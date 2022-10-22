#include <iostream>

#include "bytecode_vm.hpp"
#include "utils/serialization.hpp"
#include "bytecode_vm/disassembly.hpp"

int main() {
  using namespace paracl::bytecode_vm;

  constant_pool pool;
  pool.push_back(0);
  pool.push_back(42);
  pool.push_back(11);

  binary_code_buffer buf;

  chunk ch{std::move(buf), std::move(pool)};
  ch.push_byte(opcode::E_PUSH_CONST_UNARY);
  ch.push_byte(1);
  ch.push_byte(0);
  ch.push_byte(0);
  ch.push_byte(0);
  ch.push_byte(opcode::E_JMP_GT_ABS_UNARY);
  
  ch.push_byte(5);
  ch.push_byte(0);
  ch.push_byte(0);
  ch.push_byte(0);
  
  disassembly::chunk_complete_disassembler{}(std::cout, ch);
}