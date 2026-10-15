#pragma once

#include <string>
#include <utility>
#include <variant>

namespace black::core {

/// Why an operation failed, in words a player can read.
struct Error {
    std::string message;
};

/// A value or the Error that prevented it (std::expected is C++23; the launcher targets C++20).
template <typename T>
class Result {
public:
    Result(T value) : m_state(std::move(value)) {}      // NOLINT(google-explicit-constructor): implicit by design
    Result(Error error) : m_state(std::move(error)) {}  // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool ok() const noexcept { return std::holds_alternative<T>(m_state); }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] const T& value() const& { return std::get<T>(m_state); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(m_state)); }
    [[nodiscard]] const Error& error() const& { return std::get<Error>(m_state); }

private:
    std::variant<T, Error> m_state;
};

}  // namespace black::core
