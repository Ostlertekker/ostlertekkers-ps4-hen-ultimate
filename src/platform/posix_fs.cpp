// OSTLERTEKKERS HEN - platform/posix_fs.cpp
#include "platform/posix_fs.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace henu {

namespace {
std::string errText(const std::string& what) { return what + ": " + std::strerror(errno); }
ErrorCode errCode() {
    switch (errno) {
        case ENOENT: return ErrorCode::NotFound;
        case EACCES:
        case EPERM:
        case EROFS: return ErrorCode::AccessDenied;
        default: return ErrorCode::IoError;
    }
}
}  // namespace

bool PosixFileSystem::exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

Result<std::vector<DirEntry>> PosixFileSystem::list(const std::string& dir) {
    using R = Result<std::vector<DirEntry>>;
    DIR* d = ::opendir(dir.c_str());
    if (!d) return R::fail(errCode(), errText("opendir " + dir));
    std::vector<DirEntry> out;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        DirEntry de;
        de.name = name;
        struct stat st;
        std::string full = dir == "/" ? "/" + name : dir + "/" + name;
        if (::stat(full.c_str(), &st) == 0) {
            de.isDir = S_ISDIR(st.st_mode);
            de.size = static_cast<uint64_t>(st.st_size);
            de.mtime = static_cast<int64_t>(st.st_mtime);
        }
        out.push_back(de);
        if (out.size() >= 100000) break;  // sanity cap
    }
    ::closedir(d);
    return R::ok(out);
}

Result<std::string> PosixFileSystem::readFile(const std::string& path, size_t maxBytes) {
    using R = Result<std::string>;
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return R::fail(errCode(), errText("open " + path));
    std::string data;
    char buf[16384];
    while (true) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return R::fail(ErrorCode::IoError, errText("read " + path));
        }
        if (n == 0) break;
        if (data.size() + static_cast<size_t>(n) > maxBytes) {
            ::close(fd);
            return R::fail(ErrorCode::IoError, "file larger than limit: " + path);
        }
        data.append(buf, static_cast<size_t>(n));
    }
    ::close(fd);
    return R::ok(data);
}

Status PosixFileSystem::writeFile(const std::string& path, const std::string& data) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return Status::fail(errCode(), errText("open " + path));
    size_t off = 0;
    while (off < data.size()) {
        ssize_t n = ::write(fd, data.data() + off, data.size() - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return Status::fail(ErrorCode::IoError, errText("write " + path));
        }
        off += static_cast<size_t>(n);
    }
    if (::fsync(fd) != 0) {
        ::close(fd);
        return Status::fail(ErrorCode::IoError, errText("fsync " + path));
    }
    if (::close(fd) != 0) return Status::fail(ErrorCode::IoError, errText("close " + path));
    return Status::ok();
}

Status PosixFileSystem::appendFile(const std::string& path, const std::string& data) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) return Status::fail(errCode(), errText("open " + path));
    size_t off = 0;
    while (off < data.size()) {
        ssize_t n = ::write(fd, data.data() + off, data.size() - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return Status::fail(ErrorCode::IoError, errText("write " + path));
        }
        off += static_cast<size_t>(n);
    }
    if (::close(fd) != 0) return Status::fail(ErrorCode::IoError, errText("close " + path));
    return Status::ok();
}

Status PosixFileSystem::makeDir(const std::string& path) {
    if (::mkdir(path.c_str(), 0777) == 0 || errno == EEXIST) return Status::ok();
    return Status::fail(errCode(), errText("mkdir " + path));
}

Status PosixFileSystem::rename(const std::string& from, const std::string& to) {
    if (::rename(from.c_str(), to.c_str()) == 0) return Status::ok();
    return Status::fail(errCode(), errText("rename " + from));
}

Status PosixFileSystem::remove(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return Status::fail(errCode(), errText("stat " + path));
    int rc = S_ISDIR(st.st_mode) ? ::rmdir(path.c_str()) : ::unlink(path.c_str());
    if (rc == 0) return Status::ok();
    return Status::fail(errCode(), errText("remove " + path));
}

Result<SpaceInfo> PosixFileSystem::space(const std::string& mountPath) {
    struct statvfs v;
    if (::statvfs(mountPath.c_str(), &v) != 0)
        return Result<SpaceInfo>::fail(ErrorCode::Unsupported, errText("statvfs " + mountPath));
    SpaceInfo s;
    s.total = static_cast<uint64_t>(v.f_blocks) * v.f_frsize;
    s.free = static_cast<uint64_t>(v.f_bavail) * v.f_frsize;
    return Result<SpaceInfo>::ok(s);
}

Status PosixFileSystem::copyFile(const std::string& from, const std::string& to, uint64_t maxBytes) {
    int in = ::open(from.c_str(), O_RDONLY);
    if (in < 0) return Status::fail(errCode(), errText("open " + from));
    int out = ::open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) {
        ::close(in);
        return Status::fail(errCode(), errText("open " + to));
    }
    static char buf[1 << 20];
    uint64_t total = 0;
    Status result = Status::ok();
    while (true) {
        ssize_t n = ::read(in, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            result = Status::fail(ErrorCode::IoError, errText("read " + from));
            break;
        }
        if (n == 0) break;
        total += static_cast<uint64_t>(n);
        if (total > maxBytes) { result = Status::fail(ErrorCode::IoError, "file larger than limit: " + from); break; }
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = ::write(out, buf + off, static_cast<size_t>(n - off));
            if (w < 0) {
                if (errno == EINTR) continue;
                result = Status::fail(ErrorCode::IoError, errText("write " + to));
                break;
            }
            off += w;
        }
        if (!result) break;
    }
    ::close(in);
    if (::fsync(out) != 0 && result) result = Status::fail(ErrorCode::IoError, errText("fsync " + to));
    ::close(out);
    if (!result) ::unlink(to.c_str());
    return result;
}

}  // namespace henu
