// Error handling without exceptions: a fallible function returns a Result
// holding either a value or an error message.
#pragma once

#include <string>
#include <utility>

namespace core {

template <class T>
struct Result {
    T value{};
    std::string error;  // empty on success

    bool ok() const { return error.empty(); }

    static Result success(T v) { return Result{std::move(v), {}}; }
    static Result failure(std::string e) { return Result{T{}, std::move(e)}; }
};

// For operations that produce no value.
struct Status {
    std::string error;
    bool ok() const { return error.empty(); }
    static Status success() { return {}; }
    static Status failure(std::string e) { return Status{std::move(e)}; }
};

} // namespace core
