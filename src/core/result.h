// OSTLERTEKKERS HEN - core/result.h
// Lightweight value-or-error type. No exceptions cross module boundaries.
#pragma once

#include <string>
#include <utility>

namespace henu {

enum class ErrorCode {
    None = 0,
    InvalidArgument,
    NotFound,
    AccessDenied,      // blocked by PathGuard or OS
    IoError,
    ParseError,
    VersionMismatch,
    Unsupported,       // no verified interface on this system
    NetworkError,
    IntegrityError,    // checksum mismatch
    CertificateError,  // TLS server certificate could not be verified
    Cancelled,
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;  // human readable, English; UI maps code -> localized text
};

template <typename T>
class Result {
public:
    static Result ok(T value) { Result r; r.ok_ = true; r.value_ = std::move(value); return r; }
    static Result fail(ErrorCode code, std::string msg) {
        Result r; r.ok_ = false; r.error_ = Error{code, std::move(msg)}; return r;
    }

    bool isOk() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const T& value() const { return value_; }
    T& value() { return value_; }
    const Error& error() const { return error_; }

private:
    Result() = default;
    bool ok_ = false;
    T value_{};
    Error error_{};
};

// Result without a payload.
class Status {
public:
    static Status ok() { return Status(); }
    static Status fail(ErrorCode code, std::string msg) {
        Status s; s.error_ = Error{code, std::move(msg)}; return s;
    }
    bool isOk() const { return error_.code == ErrorCode::None; }
    explicit operator bool() const { return isOk(); }
    const Error& error() const { return error_; }

private:
    Error error_{};
};

}  // namespace henu
