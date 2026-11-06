// PS4 HEN ULTIMATE - host unit tests (no external framework).
// Build & run:  make -f Makefile.host test
#include <cstdio>
#include <string>

#include "app/services.h"
#include "cheats/cheat_naming.h"
#include "core/capability.h"
#include "core/version.h"
#include "files/path_guard.h"
#include "goldhen/goldhen_info.h"
#include "ui/screen.h"
#include "check.h"

using namespace henu;

void runPhaseCTests();

static void testVersion() {
    AppVersion v;
    CHECK(AppVersion::parse("01.05", v) && v.major == 1 && v.minor == 5);
    CHECK(v.toString() == "01.05");
    CHECK(!AppVersion::parse("1.05", v));
    CHECK(!AppVersion::parse("01.5", v));
    CHECK(!AppVersion::parse("01-05", v));
    CHECK(!AppVersion::parse("aa.bb", v));
    AppVersion a, b;
    AppVersion::parse("01.09", a);
    AppVersion::parse("01.10", b);
    CHECK(a < b && a != b);

    SemVer s, t;
    CHECK(SemVer::parse("1.0.0", s) && s.toString() == "1.0.0");
    CHECK(SemVer::parse("1.10.2", t) && s.compare(t) < 0 && t.compare(s) > 0);
    CHECK(!SemVer::parse("1.0", s));
    CHECK(!SemVer::parse("1.0.0-beta", s));
    CHECK(!SemVer::parse("", s));
}

static void testCapabilities() {
    CapabilityRegistry r;
    CHECK(r.get("unknown").status == CapStatus::NotImplemented);  // fail closed
    CHECK(!r.usable("unknown"));
    r.set("cheats.scan", CapStatus::Available);
    r.set("cheats.toggle", CapStatus::Limited, "toggle in GoldHEN menu");
    CHECK(r.usable("cheats.scan") && r.usable("cheats.toggle"));
    CHECK(r.moduleStatus("cheats") == CapStatus::Limited);
    r.set("cheats.download", CapStatus::Unavailable);
    CHECK(r.get("cheats.download").reason == "no reason given");
    CHECK(r.moduleStatus("cheats") == CapStatus::Unavailable);
    // "cheatsX.*" must not count as child of "cheats"
    r.set("cheatsx.a", CapStatus::Available);
    r.set("home", CapStatus::Available);
    CHECK(r.moduleStatus("home") == CapStatus::Available);
    CapStatus p;
    CHECK(parseCapStatus("limited", p) && p == CapStatus::Limited);
    CHECK(!parseCapStatus("bogus", p));
    CHECK(std::string(toString(CapStatus::NotImplemented)) == "not_implemented");
}

static void testPathGuard() {
    PathGuard g("/data/PS4HENULTIMATE");

    auto n = PathGuard::normalize("/data//PS4HENULTIMATE/./themes/../settings.json/");
    CHECK(n.isOk() && n.value() == "/data/PS4HENULTIMATE/settings.json");
    CHECK(PathGuard::normalize("/user/data/GoldHEN").value() == "/data/GoldHEN");
    CHECK(!PathGuard::normalize("relative/path").isOk());
    CHECK(!PathGuard::normalize("").isOk());
    CHECK(!PathGuard::normalize("/../etc").isOk());
    CHECK(PathGuard::normalize("/").value() == "/");

    // allowed
    CHECK(g.canWrite("/data/PS4HENULTIMATE/settings.json"));
    CHECK(g.canWrite("/mnt/usb0/backup/a.zip"));
    CHECK(g.canWrite("/user/data/GoldHEN/cheats/json/CUSA01234_01.00.json"));
    CHECK(g.canWrite("/data/GoldHEN/cheats/mc4/CUSA01234_01.00.mc4"));

    // denied: GoldHEN outside cheat folders
    CHECK(!g.canWrite("/data/GoldHEN/payloads/goldhen.bin"));
    CHECK(!g.canWrite("/data/GoldHEN/config.ini"));
    CHECK(!g.canWrite("/data/GoldHEN/plugins.ini"));
    CHECK(!g.canWrite("/data/GoldHEN/cheats/other/x.json"));
    // denied: traversal out of allowed root
    CHECK(!g.canWrite("/data/GoldHEN/cheats/json/../../payloads/goldhen.bin"));
    CHECK(!g.canWrite("/data/PS4HENULTIMATE/../GoldHEN/config.ini"));
    // denied: prefix trick
    CHECK(!g.canWrite("/data/PS4HENULTIMATEevil/x"));
    CHECK(!g.canWrite("/mnt/usb00/x"));
    // denied: system
    CHECK(!g.canWrite("/system/common/lib/x.sprx"));
    CHECK(!g.canWrite("/user/app/CUSA01234/app.pkg"));
    CHECK(!g.canWrite("/update/PS4UPDATE.PUP"));
    CHECK(!g.canWrite("/data/somethingelse"));
    // denied: root folders themselves
    CHECK(!g.canWrite("/data/PS4HENULTIMATE"));
    CHECK(!g.canWrite("/mnt/usb0"));
    CHECK(!g.canWrite("/data/GoldHEN/cheats/json"));
    // reads are allowed
    CHECK(g.check("/system/common/lib", AccessKind::Read).isOk());

    // dangerous app roots disable app writes
    PathGuard bad("/data");
    CHECK(!bad.canWrite("/data/x"));
    CHECK(bad.canWrite("/mnt/usb1/x"));

    CHECK(PathGuard::isGoldhenCheatPath("/data/GoldHEN/cheats/shn/a.shn"));
    CHECK(!PathGuard::isGoldhenCheatPath("/data/GoldHEN/cheats/shnx/a.shn"));
}

static void testCheatNaming() {
    auto r = parseCheatFileName("CUSA01234_01.01.json");
    CHECK(r.isOk());
    CHECK(r.value().titleId == "CUSA01234" && r.value().version.toString() == "01.01");
    CHECK(r.value().process.empty() && r.value().format == CheatFormat::Json);
    CHECK(makeCheatFileName(r.value()) == "CUSA01234_01.01.json");

    auto p = parseCheatFileName("CUSA01234_01.01_example.elf.shn");
    CHECK(p.isOk() && p.value().process == "example.elf" && p.value().format == CheatFormat::Shn);
    CHECK(makeCheatFileName(p.value()) == "CUSA01234_01.01_example.elf.shn");

    CHECK(parseCheatFileName("CUSA01234_01.01.MC4").isOk());
    CHECK(!parseCheatFileName("CUSA01234_01.01.txt").isOk());
    CHECK(!parseCheatFileName("CUSA1234_01.01.json").isOk());
    CHECK(!parseCheatFileName("cusa01234_01.01.json").isOk());
    CHECK(!parseCheatFileName("CUSA01234_1.01.json").isOk());
    CHECK(!parseCheatFileName("CUSA01234_01.01_.json").isOk());
    CHECK(!parseCheatFileName("CUSA01234_01.01x.json").isOk());
    CHECK(!parseCheatFileName("../CUSA01234_01.01.json").isOk());
    CHECK(!parseCheatFileName("CUSA01234").isOk());

    CHECK(isValidTitleId("CUSA00552") && !isValidTitleId("CUSA0055") && !isValidTitleId("CUSA0055A"));

    CHECK(regionFromContentId("UP9000-CUSA00552_00-THELASTOFUS00000") == Region::US);
    CHECK(regionFromContentId("EP9000-CUSA00556_00-THELASTOFUS00000") == Region::EU);
    CHECK(regionFromContentId("JP9000-X") == Region::JP);
    CHECK(regionFromContentId("HP9000-X") == Region::Asia);
    CHECK(regionFromContentId("IV0000-HENU00001_00-PS4HENULTIMATE00") == Region::Unknown);
    CHECK(regionFromContentId("") == Region::Unknown);

    AppVersion v100, v101;
    AppVersion::parse("01.00", v100);
    AppVersion::parse("01.01", v101);
    CHECK(checkCompatibility(r.value(), "CUSA01234", v101) == Compatibility::Compatible);
    CHECK(checkCompatibility(r.value(), "CUSA01234", v100) == Compatibility::VersionMismatch);
    CHECK(checkCompatibility(r.value(), "CUSA09999", v101) == Compatibility::WrongTitle);
    CHECK(checkCompatibility(r.value(), "", v101) == Compatibility::GameNotInstalled);

    CHECK(categorize("Infinite Health") == CheatCategory::Health);
    CHECK(categorize("Unendlich Leben") == CheatCategory::Health);
    CHECK(categorize("Infinite Ammo") == CheatCategory::Ammo);
    CHECK(categorize("Unendlich Munition") == CheatCategory::Ammo);
    CHECK(categorize("Max Money") == CheatCategory::Money);
    CHECK(categorize("Unendlich Geld") == CheatCategory::Money);
    CHECK(categorize("Unlimited Stamina") == CheatCategory::Stamina);
    CHECK(categorize("Max XP") == CheatCategory::Experience);
    CHECK(categorize("Unlock All Weapons") == CheatCategory::Unlocks);
    CHECK(categorize("Infinite Crafting Materials") == CheatCategory::Resources);
    CHECK(categorize("Max Strength") == CheatCategory::Stats);
    CHECK(categorize("Super Jump") == CheatCategory::Gameplay);
    CHECK(categorize("Infinite HP") == CheatCategory::Health);
    CHECK(categorize("Mystery Code") == CheatCategory::Other);
}

static void testGoldhenSupport() {
    // User's system
    CHECK(lookupSupport("2.4b18.12", "13.52") == FirmwareSupport::SupportedBeta);
    CHECK(lookupSupport("2.4b18.11", "13.52") == FirmwareSupport::SupportedBeta);
    CHECK(lookupSupport("2.4b18.12", "13.50") == FirmwareSupport::SupportedBeta);
    CHECK(lookupSupport("2.4b18.11", "13.50") == FirmwareSupport::NotListed);
    CHECK(lookupSupport("2.4b18.10", "13.52") == FirmwareSupport::NotListed);
    CHECK(lookupSupport("2.4b18", "11.00") == FirmwareSupport::SupportedBeta);
    CHECK(lookupSupport("2.4b18", "13.52") == FirmwareSupport::NotListed);
    CHECK(lookupSupport("2.4b18.12", "14.00") == FirmwareSupport::NotListed);
    CHECK(lookupSupport("2.4b18.12", "") == FirmwareSupport::Unknown);
    CHECK(lookupSupport("garbage", "13.52") == FirmwareSupport::Unknown);
    CHECK(lookupSupport("2.4b18.12x", "13.52") == FirmwareSupport::Unknown);
    CHECK(lookupSupport("2.3", "9.00") == FirmwareSupport::Unknown);
}

int main() {
    testVersion();
    testCapabilities();
    testPathGuard();
    testCheatNaming();
    testGoldhenSupport();
    runPhaseCTests();
    const int pass = henu_test::passCount(), fail = henu_test::failCount();
    std::printf("\n%d passed, %d failed\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
