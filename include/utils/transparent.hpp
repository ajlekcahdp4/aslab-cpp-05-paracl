/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace paracl::utils::transparent {

// clang-format off
template <typename T>
concept convertible_to_string_view = requires(T str) {
  { std::string_view{str} };
};
// clang-format on

struct string_equal {
  using is_transparent = void;
  bool operator()(const convertible_to_string_view auto &lhs, const convertible_to_string_view auto &rhs) const {
    return static_cast<std::string_view>(lhs) == static_cast<std::string_view>(rhs);
  }
};

struct string_hash {
  using is_transparent = string_equal;
  bool operator()(const convertible_to_string_view auto &val) const { return std::hash<std::string_view>{}(val); }
};

template <typename T> using string_unordered_map = std::unordered_map<std::string, T, string_hash, string_equal>;

} // namespace paracl::utils::transparent