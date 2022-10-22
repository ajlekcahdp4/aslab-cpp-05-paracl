#include "bytecode_vm/chunk.hpp"
#include "utils/serialization.hpp"
#include "bytecode_vm/disassembly.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace paracl::bytecode_vm {

static std::vector<uint8_t> read_raw_data(std::istream &is) {
  if (!is) throw std::runtime_error("Invalid istream");

  std::vector<uint8_t> raw_data;

  is.seekg(is.end);
  auto length = is.tellg();
  is.seekg(is.beg);

  raw_data.resize(length);
  is.read(reinterpret_cast<char *>(raw_data.data()), length); // This is ugly.

  return raw_data;
}

chunk read_chunk(std::istream &is) {}

} // namespace paracl::bytecode_vm