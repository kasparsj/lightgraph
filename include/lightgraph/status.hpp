#pragma once

#include <string>
#include <utility>

namespace lightgraph {

/**
 * @file status.hpp
 * @brief Error/result types for the stable Lightgraph API.
 */

/**
 * @brief Stable error codes returned by high-level API operations.
 */
enum class ErrorCode {
    Ok = 0,
    InvalidArgument,
    InvalidModel,
    NoFreeLightList,
    NoEmitterAvailable,
    CapacityExceeded,
    OutOfRange,
    InternalError,
};

/**
 * @brief Operation status object.
 */
class Status {
  public:
    Status() = default;

    Status(ErrorCode code, std::string message) : code_(code), message_(std::move(message)) {}

    /**
     * @brief Construct a success status.
     */
    static Status success() { return Status(); }

    /**
     * @brief Construct an error status with code and message.
     */
    static Status error(ErrorCode code, std::string message) {
        return Status(code, std::move(message));
    }

    /**
     * @brief True when status code is `ErrorCode::Ok`.
     */
    bool ok() const { return code_ == ErrorCode::Ok; }

    explicit operator bool() const { return ok(); }

    /**
     * @brief Return the stable error code.
     */
    ErrorCode code() const { return code_; }

    /**
     * @brief Return the human-readable error message.
     */
    const std::string& message() const { return message_; }

  private:
    ErrorCode code_ = ErrorCode::Ok;
    std::string message_;
};

template <typename T> class Result {
  public:
    Result(T value, Status status = Status::success())
        : value_(std::move(value)), status_(std::move(status)) {}

    /**
     * @brief Construct an error result.
     *
     * The value is default-constructed and should be ignored when `ok()` is false.
     */
    static Result<T> error(ErrorCode code, std::string message) {
        return Result<T>(T{}, Status::error(code, std::move(message)));
    }

    /**
     * @brief True when result status is success.
     */
    bool ok() const { return status_.ok(); }

    explicit operator bool() const { return ok(); }

    /**
     * @brief Access the contained value.
     *
     * Callers should only use this when `ok()` is true.
     */
    const T& value() const { return value_; }

    T& value() { return value_; }

    /**
     * @brief Access the operation status.
     */
    const Status& status() const { return status_; }

  private:
    T value_;
    Status status_;
};

} // namespace lightgraph
