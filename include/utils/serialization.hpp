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

#include "utils/algorithm.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>

namespace paracl::utils {

namespace detail {

inline auto get_iterator_endian(std::span<char> raw_bytes) {
  static_assert(
      std::endian::native == std::endian::little || std::endian::native == std::endian::big, "Mixed endian, bailing out"
  );
  if constexpr (std::endian::native == std::endian::little) {
    return raw_bytes.begin();
  } else {
    return raw_bytes.rbegin();
  }
}

} // namespace detail

template <typename T>
concept integral_or_floating = std::integral<T> || std::floating_point<T>;

template <integral_or_floating T, std::input_iterator iter>
std::pair<std::optional<T>, iter> read_little_endian(iter first, iter last) {
  std::array<char, sizeof(T)> raw_bytes;

  const std::input_iterator auto input_iter = detail::get_iterator_endian(raw_bytes);
  auto size = sizeof(T);
  first = copy_while(first, last, input_iter, [&size](auto) { return size && size--; });

  if (size != 0) return std::pair{std::nullopt, first};
  return std::pair{std::bit_cast<T>(raw_bytes), first};
}

template <integral_or_floating T> void write_little_endian(T val, std::output_iterator<char> auto oput) {
  std::array<char, sizeof(T)> raw_bytes = std::bit_cast<decltype(raw_bytes)>(val);
  const std::input_iterator auto input_iter = detail::get_iterator_endian(raw_bytes);
  auto size = sizeof(T);
  std::copy_n(input_iter, size, oput);
}

struct padded_hex {
  auto &operator()(auto &os, auto val, unsigned padding = 8, char fill = '0') const {
    os << "0x" << std::setfill(fill) << std::setw(padding) << std::hex << val;
    return os;
  }
};

constexpr auto padded_hex_printer = padded_hex{};

auto pointer_to_uintptr(auto *pointer) {
  return std::bit_cast<uintptr_t>(pointer);
}

inline std::string read_file(const std::filesystem::path &input_path) {
  std::ifstream ifs;
  ifs.exceptions(ifs.exceptions() | std::ios::failbit | std::ios::badbit);
  ifs.open(input_path, std::ios::binary);

  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

} // namespace paracl::utils