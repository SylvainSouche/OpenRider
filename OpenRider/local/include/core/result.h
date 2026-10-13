/// @file
/// Error handling without exceptions: a fallible function returns a Result
/// holding either a value or an error message.
#pragma once

#include <string>
#include <utility>

namespace core {

/// The outcome of an operation producing a `T`: a value, or an error.
template <class T>
struct Result {
    T value{};          ///< meaningful on success only
    std::string error;  ///< empty on success

    /// The operation succeeded.
    bool Ok() const { return error.empty(); }

    /// A success holding `value`.
    static Result Success(T value) { return Result{std::move(value), {}}; }

    /// A failure with an error message.
    static Result Failure(std::string error) { return Result{T{}, std::move(error)}; }
};

/// The outcome of an operation producing no value.
struct Status {
    std::string error;  ///< empty on success

    /// The operation succeeded.
    bool Ok() const { return error.empty(); }

    /// A success.
    static Status Success() { return {}; }

    /// A failure with an error message.
    static Status Failure(std::string error) { return Status{std::move(error)}; }
};

} // namespace core
