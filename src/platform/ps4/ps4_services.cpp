// OSTLERTEKKERS HEN - platform/ps4/ps4_services.cpp
// Audio (sceAudioOut), HTTPS (sceHttp/sceSsl) and the GoldHEN SDK sandbox
// escape for reading installed titles. Only compiled by the PS4 build.
//
// Sources (docs/API_NACHWEIS.md):
//   P10 audio:  OpenOrbis samples/audio-wav (sceAudioOutInit/Open/Output,
//               user ORBIS_USER_SERVICE_USER_ID_SYSTEM, 256-frame grains, 48 kHz S16 stereo)
//   P11 https:  OpenOrbis samples/net_http (sceSysmoduleLoadModuleInternal NET/HTTP/SSL,
//               sceNetInit/PoolCreate, sceSslInit, sceHttpInit/CreateTemplate/...)
//               Difference: no "skip certificate" callback - verification stays on.
//   G8 GoldHEN: GoldHEN_Plugins_SDK (MIT) source/GoldHEN.c + Syscall.c:
//               sys_sdk_cmd = syscall(500, cmd, data); cmd 2 = jailbreak, 3 = unjailbreak
#include <orbis/AudioOut.h>
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <dirent.h>

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

#include "platform/platform.h"
#include "platform/ps4/ps4_services.h"

#ifdef HENU_HAVE_LIBJBC
#include <libjbc.h>
#endif

// ---------------------------------------------------------------- syscall
// Indirect syscall (FreeBSD syscall 0), as in GoldHEN_Plugins_SDK Syscall.c.
extern "C" long henu_syscall(long num, ...);
__asm__(
    ".att_syntax prefix\n"
    ".globl henu_syscall\n"
    "henu_syscall:\n"
    "  movq $0, %rax\n"
    "  movq %rcx, %r10\n"
    "  syscall\n"
    "  jb 1f\n"
    "  retq\n"
    "1:\n"
    "  movq $-1, %rax\n"
    "  retq\n");

namespace henu {

namespace {

// ---------------------------------------------------------------- GoldHEN SDK
struct JailbreakBackup {   // struct jailbreak_backup (GoldHEN.h)
    uint32_t cr_uid, cr_ruid, cr_rgid, cr_groups;
    uint64_t cr_paid;
    uint64_t cr_caps[2];
    void* cr_prison;
    void* fd_cdir;
    void* fd_jdir;
    void* fd_rdir;
};

constexpr long kGoldhenSyscall = 500;
constexpr long kCmdVersion = 0, kCmdJailbreak = 2, kCmdUnjailbreak = 3;

// Probe: can this process list /user/app? (read-only, no side effects)
// True only if /user/app lists at least one title folder: inside the app
// sandbox the folder can exist but be empty.
bool canSeeUserApp() {
    for (const char* dir : {"/user/app", "/system_data/priv/appmeta"}) {
        DIR* d = opendir(dir);
        if (!d) continue;
        bool found = false;
        while (struct dirent* e = readdir(d)) {
            const char* n = e->d_name;
            if (std::strlen(n) == 9 && std::isalpha(static_cast<unsigned char>(n[0])) &&
                std::isdigit(static_cast<unsigned char>(n[4])) && std::isdigit(static_cast<unsigned char>(n[8]))) {
                found = true;
                break;
            }
        }
        closedir(d);
        if (found) return true;
    }
    return false;
}

// Three ways out of the app sandbox, tried in order (docs/API_NACHWEIS.md G8):
//   1. none needed (already outside, e.g. GoldHEN setting)
//   2. GoldHEN SDK syscall 500 jailbreak / unjailbreak
//   3. libjbc (same method as the official GoldHEN Cheat Manager)
// Every attempt is reported in `why`, so the games screen can show it.
class Ps4Privilege : public IPrivilege {
public:
    bool elevate(std::string& why) override {
        std::lock_guard<std::mutex> lock(m_);
        if (depth_ > 0) { ++depth_; return true; }
        why.clear();
        if (canSeeUserApp()) {
            method_ = Method::None;
            ++depth_;
            why = "direct";
            return true;
        }
        why += "direct: no; ";
        // 2. GoldHEN SDK
        long v = henu_syscall(kGoldhenSyscall, kCmdVersion, nullptr);
        if (v > 0) {
            std::memset(&backup_, 0, sizeof(backup_));
            long r = henu_syscall(kGoldhenSyscall, kCmdJailbreak, &backup_);
            if (r == 0 && canSeeUserApp()) {
                method_ = Method::GoldhenSdk;
                ++depth_;
                why += "goldhen-sdk v" + std::to_string(v);
                return true;
            }
            if (r == 0) henu_syscall(kGoldhenSyscall, kCmdUnjailbreak, &backup_);
            why += "goldhen-sdk v" + std::to_string(v) + ": jailbreak=" + std::to_string(r) + ", /user/app still closed; ";
        } else {
            why += "goldhen-sdk: no answer; ";
        }
#ifdef HENU_HAVE_LIBJBC
        // 3. libjbc
        if (jbc_get_cred(&saved_) == 0) {
            jbc_cred root = saved_;
            if (jbc_jailbreak_cred(&root) == 0 && jbc_set_cred(&root) == 0) {
                if (canSeeUserApp()) {
                    method_ = Method::Libjbc;
                    ++depth_;
                    why += "libjbc";
                    return true;
                }
                jbc_set_cred(&saved_);
                why += "libjbc: creds set, /user/app still closed";
            } else {
                jbc_set_cred(&saved_);
                why += "libjbc: jailbreak failed";
            }
        } else {
            why += "libjbc: cannot read creds";
        }
#else
        why += "libjbc: not built in";
#endif
        return false;
    }
    void restore() override {
        std::lock_guard<std::mutex> lock(m_);
        if (depth_ <= 0) return;
        if (--depth_ > 0) return;
        if (method_ == Method::GoldhenSdk) henu_syscall(kGoldhenSyscall, kCmdUnjailbreak, &backup_);
#ifdef HENU_HAVE_LIBJBC
        if (method_ == Method::Libjbc) jbc_set_cred(&saved_);
#endif
        method_ = Method::None;
    }

private:
    enum class Method { None, GoldhenSdk, Libjbc };
    std::mutex m_;
    int depth_ = 0;
    Method method_ = Method::None;
    JailbreakBackup backup_;
#ifdef HENU_HAVE_LIBJBC
    jbc_cred saved_;
#endif
};

// ---------------------------------------------------------------- audio
class Ps4Audio : public IAudio {
public:
    Ps4Audio() {
        if (sceAudioOutInit() != 0) { std::printf("[henu] sceAudioOutInit failed\n"); return; }
        port_ = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, kGrain, 48000,
                                ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        if (port_ <= 0) std::printf("[henu] sceAudioOutOpen failed: 0x%08x\n", static_cast<unsigned>(port_));
    }
    ~Ps4Audio() override {
        stop();
        if (port_ > 0) sceAudioOutClose(port_);
    }
    void playStereo48k(std::vector<int16_t> samples) override {
        stop();
        if (port_ <= 0 || samples.empty()) return;
        stopFlag_ = false;
        playing_ = true;
        worker_ = std::thread([this, s = std::move(samples)]() mutable {
            s.resize(((s.size() / 2 + kGrain - 1) / kGrain) * kGrain * 2, 0);   // whole grains
            for (size_t off = 0; off < s.size() && !stopFlag_; off += kGrain * 2)
                if (sceAudioOutOutput(port_, &s[off]) < 0) break;
            sceAudioOutOutput(port_, nullptr);   // wait until the last grain is out
            playing_ = false;
        });
    }
    void stop() override {
        stopFlag_ = true;
        if (worker_.joinable()) worker_.join();
        playing_ = false;
    }
    bool playing() const override { return playing_; }

private:
    static constexpr uint32_t kGrain = 256;
    int32_t port_ = -1;
    std::thread worker_;
    std::atomic<bool> stopFlag_{false};
    std::atomic<bool> playing_{false};
};

// ---------------------------------------------------------------- HTTPS
class Ps4Network : public INetwork {
public:
    bool init(std::string& err) {
        if (static_cast<int32_t>(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET)) < 0 ||
            static_cast<int32_t>(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP)) < 0 ||
            static_cast<int32_t>(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL)) < 0) {
            err = "loading net/http/ssl modules failed";
            return false;
        }
        sceNetInit();
        int32_t mem = sceNetPoolCreate("henuNet", 16 * 1024, 0);
        if (mem < 0) { err = "sceNetPoolCreate failed"; return false; }
        int32_t ssl = sceSslInit(SSL_POOLSIZE);
        if (ssl < 0) { err = "sceSslInit failed"; return false; }
        http_ = sceHttpInit(mem, ssl, LIBHTTP_POOLSIZE);
        if (http_ < 0) { err = "sceHttpInit failed"; return false; }
        return true;
    }

    Result<HttpResponse> get(const std::string& url, size_t maxBytes) override {
        HttpResponse resp;
        Status st = run(url, maxBytes, [&](const char* d, size_t n, uint64_t, uint64_t) {
            if (resp.body.size() + n > maxBytes) return false;
            resp.body.append(d, n);
            return true;
        }, &resp.status, true);
        if (!st) return Result<HttpResponse>::fail(st.error().code, st.error().message);
        return Result<HttpResponse>::ok(std::move(resp));
    }

    Status stream(const std::string& url, uint64_t maxBytes, const Sink& sink) override {
        int status = 0;
        return run(url, maxBytes, sink, &status, false);
    }

private:
    static std::string hex(int32_t v) {
        char b[16];
        std::snprintf(b, sizeof(b), "0x%08X", static_cast<unsigned>(v));
        return b;
    }

    // One GET with manual redirect handling (Location header), max 6 hops.
    Status run(std::string url, uint64_t maxBytes, const Sink& sink, int* statusOut, bool anyStatus) {
        std::lock_guard<std::mutex> lock(m_);
        for (int hop = 0; hop < 6; ++hop) {
            if (url.compare(0, 8, "https://") != 0)
                return Status::fail(ErrorCode::NetworkError, "only https:// URLs are allowed");
            int32_t tpl = sceHttpCreateTemplate(http_, "OSTLERTEKKERS-HEN/0.5 (PlayStation 4)", ORBIS_HTTP_VERSION_1_1, 1);
            if (tpl < 0) return Status::fail(ErrorCode::NetworkError, "sceHttpCreateTemplate " + hex(tpl));
            sceHttpSetConnectTimeOut(tpl, 15u * 1000000u);
            int32_t conn = sceHttpCreateConnectionWithURL(tpl, url.c_str(), true);
            int32_t req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url.c_str(), 0) : conn;
            auto cleanup = [&]() {
                if (req >= 0) sceHttpDeleteRequest(req);
                if (conn >= 0) sceHttpDeleteConnection(conn);
                sceHttpDeleteTemplate(tpl);
            };
            if (req < 0) { cleanup(); return Status::fail(ErrorCode::NetworkError, "sceHttpCreateRequest " + hex(req)); }
            sceHttpAddRequestHeader(req, "Accept", "*/*", 0);
            int32_t ret = sceHttpSendRequest(req, nullptr, 0);
            if (ret < 0) {
                cleanup();
                return Status::fail(ErrorCode::NetworkError, "sceHttpSendRequest " + hex(ret) +
                                    " (no connection, DNS or TLS certificate problem)");
            }
            int32_t code = 0;
            sceHttpGetStatusCode(req, &code);
            *statusOut = code;
            if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                char* hdr = nullptr;
                size_t hdrLen = 0;
                std::string next;
                if (sceHttpGetAllResponseHeaders(req, &hdr, &hdrLen) >= 0 && hdr) next = findLocation(std::string(hdr, hdrLen));
                cleanup();
                if (next.empty()) return Status::fail(ErrorCode::NetworkError, "redirect without Location");
                url = next;
                continue;
            }
            if (code != 200 && !anyStatus) {
                cleanup();
                return Status::fail(ErrorCode::NetworkError, "HTTP " + std::to_string(code));
            }
            int32_t lenType = 0;
            size_t len = 0;
            uint64_t total = 0;
            if (sceHttpGetResponseContentLength(req, &lenType, &len) >= 0 && lenType == ORBIS_HTTP_CONTENTLEN_EXIST) total = len;
            if (total > maxBytes) { cleanup(); return Status::fail(ErrorCode::NetworkError, "response too large"); }
            static char buf[64 * 1024];
            uint64_t done = 0;
            for (;;) {
                int32_t n = sceHttpReadData(req, buf, sizeof(buf));
                if (n < 0) { cleanup(); return Status::fail(ErrorCode::NetworkError, "sceHttpReadData " + hex(n)); }
                if (n == 0) break;
                done += static_cast<uint64_t>(n);
                if (done > maxBytes) { cleanup(); return Status::fail(ErrorCode::NetworkError, "response too large"); }
                if (!sink(buf, static_cast<size_t>(n), done, total)) {
                    cleanup();
                    return Status::fail(ErrorCode::Cancelled, "cancelled");
                }
            }
            cleanup();
            if (total && done != total) return Status::fail(ErrorCode::NetworkError, "download incomplete");
            return Status::ok();
        }
        return Status::fail(ErrorCode::NetworkError, "too many redirects");
    }

    static std::string findLocation(const std::string& headers) {
        size_t pos = 0;
        while (pos < headers.size()) {
            size_t eol = headers.find('\n', pos);
            if (eol == std::string::npos) eol = headers.size();
            std::string line = headers.substr(pos, eol - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.size() > 9) {
                std::string key = line.substr(0, 9);
                for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (key == "location:") {
                    size_t v = 9;
                    while (v < line.size() && line[v] == ' ') ++v;
                    return line.substr(v);
                }
            }
            pos = eol + 1;
        }
        return "";
    }

    std::mutex m_;
    int32_t http_ = -1;
};

}  // namespace

// ---------------------------------------------------------------- launcher
// Same call sequence as the open-source Itemzflow game manager
// (itemzflow/source/utils.cpp, Launch_App): load libSceSystemService.sprx,
// take the foreground user, sceLncUtilLaunchApp with SkipSystemUpdate.
// Declarations: OpenOrbis include/orbis/SystemService.h, UserService.h.
// Error codes: Itemzflow include/defines.h.
const char* lncErrorText(uint32_t e) {
    switch (e) {
        case 0x80940031: return "app not found in the app database";
        case 0x8094000c: return "already running";
        case 0x80940010: return "another app must be closed first";
        case 0x80940011: return "another app must be suspended first";
        case 0x80940008: return "eboot.bin missing";
        case 0x80940009: return "param.sfo missing";
        case 0x8094000a: return "no user logged in";
        case 0x8094000d: return "not allowed in the current memory mode";
        case 0x80940004: return "no app info";
        case 0x8094001c: return "invalid title id";
        case 0x80940005: return "invalid parameter";
        default: return nullptr;
    }
}

class Ps4Launcher : public ILauncher {
public:
    Status launch(const std::string& titleId) override {
        if (!moduleLoaded_) {
            const uint32_t m = sceKernelLoadStartModule("/system/common/lib/libSceSystemService.sprx", 0, nullptr, 0, nullptr, nullptr);
            moduleLoaded_ = static_cast<int32_t>(m) > 0;
        }
        int32_t user = -1;
        sceUserServiceGetForegroundUser(&user);
        LncAppParam param;
        std::memset(&param, 0, sizeof(param));
        param.size = sizeof(LncAppParam);
        param.user_id = static_cast<uint32_t>(user);
        param.app_opt = 0;
        param.crash_report = 0;
        param.LaunchAppCheck_flag = LaunchApp_SkipSystemUpdate;
        const uint32_t r = static_cast<uint32_t>(sceLncUtilLaunchApp(titleId.c_str(), nullptr, &param));
        if ((r & 0x80000000u) == 0) return Status::ok();
        char hex[16];
        std::snprintf(hex, sizeof(hex), "0x%08X", r);
        const char* txt = lncErrorText(r);
        return Status::fail(ErrorCode::IoError, std::string(hex) + (txt ? std::string(" (") + txt + ")" : ""));
    }

private:
    bool moduleLoaded_ = false;
};

std::unique_ptr<IAudio> createPs4Audio() { return std::unique_ptr<IAudio>(new Ps4Audio); }
std::unique_ptr<IPrivilege> createPs4Privilege() { return std::unique_ptr<IPrivilege>(new Ps4Privilege); }
std::unique_ptr<ILauncher> createPs4Launcher() { return std::unique_ptr<ILauncher>(new Ps4Launcher); }
std::unique_ptr<INetwork> createPs4Network(std::string& err) {
    std::unique_ptr<Ps4Network> n(new Ps4Network);
    if (!n->init(err)) return nullptr;
    return std::unique_ptr<INetwork>(n.release());
}

}  // namespace henu
