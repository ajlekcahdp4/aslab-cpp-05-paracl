/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long
 * as you retain this notice you can do whatever you want with this stuff. If we
 * meet some day, and you think this stuff is worth it, you can buy us a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#pragma once

#include "ezvis/ezvis.hpp"

#include "bytecode_vm/bytecode_builder.hpp"
#include "bytecode_vm/decl_vm.hpp"
#include "bytecode_vm/disassembly.hpp"
#include "bytecode_vm/opcodes.hpp"
#include "bytecode_vm/virtual_machine.hpp"

#include "frontend/analysis/function_table.hpp"
#include "frontend/ast/ast_container.hpp"
#include "frontend/ast/ast_nodes.hpp"
#include "frontend/ast/ast_nodes/i_ast_node.hpp"
#include "frontend/ast/node_identifier.hpp"
#include "frontend/symtab.hpp"
#include "utils/transparent.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace paracl::codegen {

class codegen_stack_frame {
private:
  using map_type = utils::transparent::string_unordered_map<unsigned>;

  struct stack_block {
    unsigned m_top;
    map_type m_map;
  };

  std::vector<stack_block> m_blocks;

public:
  void begin_scope() {
    auto block = stack_block{size(), map_type{}};
    m_blocks.push_back(block);
  }

  void begin_scope(frontend::symtab &stab) {
    begin_scope();

    for (const auto &v : stab) {
      push_var(v.first);
    }
  }

  void end_scope() {
    assert(m_blocks.size() && "Ending nonexistent scope");
    m_blocks.pop_back();
  }

  void push_var(std::string_view name) {
    auto &back = m_blocks.back();
    [[maybe_unused]] auto res = back.m_map.emplace(name, back.m_top++);
    assert(res.second && "Reinserting var with the same label");
  }

  void pop_dummy() {
    assert(m_blocks.size() && m_blocks.back().m_top > 0 && "Ending nonexistent scope");
    m_blocks.back().m_top--;
  }

  unsigned lookup_location(std::string_view name) const {
    unsigned loc = 0;

    [[maybe_unused]] auto found = std::find_if(m_blocks.crbegin(), m_blocks.crend(), [&name, &loc](auto &block) {
      auto it = block.m_map.find(name);
      if (it == block.m_map.end()) return false;
      loc = it->second;
      return true;
    });

    assert(found != m_blocks.crend());
    return loc;
  }

  void push_dummy() { ++m_blocks.back().m_top; }

  unsigned size() const {
    if (m_blocks.empty()) return 0;
    return m_blocks.back().m_top;
  }

  unsigned names() const {
    if (m_blocks.empty()) return 0;
    return m_blocks.back().m_map.size();
  }

  void clear() { m_blocks.clear(); }
};

// It's really necessary to put all of this code inside an anonymous namespace to avoid external linkage.
// paracl_isa relies on lambda types
namespace {

class codegen_visitor final : public ezvis::visitor_base<frontend::ast::i_ast_node, codegen_visitor, void> {
  using builder_type = bytecode_vm::builder::bytecode_builder<decltype(bytecode_vm::instruction_set::paracl_isa)>;

private:
  std::unordered_map<int, unsigned> m_constant_map;

  const frontend::ast::function_definition *m_curr_function;
  struct reloc_constant {
    unsigned m_index;
    unsigned m_address;
  };

  std::vector<reloc_constant> m_return_address_constants;

  struct dyn_jump_reloc {
    unsigned m_index;
    unsigned m_address;
    frontend::ast::function_definition *m_func_ptr;
  };

  std::vector<dyn_jump_reloc> m_dynamic_jumps_constants;

  struct reloc_info {
    unsigned m_reloc_index;
    frontend::ast::function_definition *m_func_ptr;
  };

  std::vector<reloc_info> m_relocations_function_calls;
  std::vector<unsigned> m_exit_relocations;

private:
  std::unordered_map<frontend::ast::function_definition *, unsigned> m_function_defs;
  const frontend::functions_analytics *m_functions;
  codegen_stack_frame m_symtab_stack;
  builder_type m_builder;

  unsigned m_prev_stack_size = 0;

  bool m_is_currently_statement = false;

private:
  void set_currently_statement() { m_is_currently_statement = true; }
  void reset_currently_statement() { m_is_currently_statement = false; }
  bool is_currently_statement() const { return m_is_currently_statement; }

  void visit_if_no_else(frontend::ast::if_statement &);
  void visit_if_with_else(frontend::ast::if_statement &);

  unsigned lookup_or_insert_constant(int constant);

  unsigned current_constant_index() const {
    return m_constant_map.size() + m_return_address_constants.size() + m_dynamic_jumps_constants.size();
  }

private:
  void emit_pop();

  auto emit_with_increment(auto &&desc) {
    increment_stack();
    return m_builder.emit_operation(desc);
  }

  auto emit_with_decrement(auto &&desc) {
    decrement_stack();
    return m_builder.emit_operation(desc);
  }

  // clang-format off
  void increment_stack() { m_symtab_stack.push_dummy(); }
  void decrement_stack() { m_symtab_stack.pop_dummy(); }
  // clang-format on

private:
  using to_visit = std::tuple<
      frontend::ast::assignment_statement, frontend::ast::binary_expression, frontend::ast::constant_expression,
      frontend::ast::if_statement, frontend::ast::print_statement, frontend::ast::read_expression,
      frontend::ast::statement_block, frontend::ast::unary_expression, frontend::ast::variable_expression,
      frontend::ast::while_statement, frontend::ast::function_call, frontend::ast::return_statement,
      frontend::ast::function_definition_to_ptr_conv>;

public:
  EZVIS_VISIT_CT(to_visit);

  codegen_visitor() = default;

  void generate(frontend::ast::assignment_statement &);
  void generate(frontend::ast::binary_expression &);
  void generate(frontend::ast::constant_expression &);
  void generate(frontend::ast::if_statement &);
  void generate(frontend::ast::print_statement &);
  void generate(frontend::ast::read_expression &);
  void generate(frontend::ast::statement_block &);
  void generate(frontend::ast::unary_expression &);
  void generate(frontend::ast::variable_expression &);
  void generate(frontend::ast::while_statement &);
  void generate(frontend::ast::function_call &);
  void generate(frontend::ast::return_statement &);
  void generate(frontend::ast::function_definition_to_ptr_conv &);

  EZVIS_VISIT_INVOKER(generate);

  unsigned generate_function(frontend::ast::function_definition &);
  void generate_all(const frontend::ast::ast_container &ast, const frontend::functions_analytics &functions);
  bytecode_vm::decl_vm::chunk to_chunk();
};

namespace vm_instruction_set = bytecode_vm::instruction_set;
namespace vm_builder = bytecode_vm::builder;
namespace ast = frontend::ast;

using vm_builder::encoded_instruction;

void codegen_visitor::emit_pop() {
  emit_with_decrement(vm_instruction_set::pop_desc);
}

void codegen_visitor::generate(ast::constant_expression &ref) {
  auto index = lookup_or_insert_constant(ref.value());
  emit_with_increment(encoded_instruction{vm_instruction_set::push_const_desc, index});
}

void codegen_visitor::generate(ast::read_expression &) {
  emit_with_increment(vm_instruction_set::push_read_desc);
}

void codegen_visitor::generate(ast::variable_expression &ref) {
  auto index = m_symtab_stack.lookup_location(ref.name());
  emit_with_increment(encoded_instruction{vm_instruction_set::push_local_rel_desc, index});
}

void codegen_visitor::generate(ast::print_statement &ref) {
  reset_currently_statement();
  apply(ref.expr());
  emit_with_decrement(vm_instruction_set::print_desc);
}

void codegen_visitor::generate(ast::assignment_statement &ref) {
  const bool emit_push = !is_currently_statement();
  apply(ref.right());

  const auto last_it = std::prev(ref.rend());
  for (auto start = ref.rbegin(), finish = last_it; start != finish; ++start) {
    const auto left_index = m_symtab_stack.lookup_location(start->name());
    emit_with_decrement(encoded_instruction{vm_instruction_set::mov_local_rel_desc, left_index});
    emit_with_increment(encoded_instruction{vm_instruction_set::push_local_rel_desc, left_index});
  }

  // Last iteration:
  const auto left_index = m_symtab_stack.lookup_location(last_it->name());
  emit_with_decrement(encoded_instruction{vm_instruction_set::mov_local_rel_desc, left_index});
  if (emit_push) {
    emit_with_increment(encoded_instruction{vm_instruction_set::push_local_rel_desc, left_index});
  }
}

void codegen_visitor::generate(ast::binary_expression &ref) {
  reset_currently_statement();
  apply(ref.left());

  reset_currently_statement();

  apply(ref.right());

  using bin_op = ast::binary_operation;

  switch (ref.op_type()) {
  case bin_op::E_BIN_OP_ADD: emit_with_decrement(vm_instruction_set::add_desc); break;
  case bin_op::E_BIN_OP_SUB: emit_with_decrement(vm_instruction_set::sub_desc); break;
  case bin_op::E_BIN_OP_MUL: emit_with_decrement(vm_instruction_set::mul_desc); break;
  case bin_op::E_BIN_OP_DIV: emit_with_decrement(vm_instruction_set::div_desc); break;
  case bin_op::E_BIN_OP_MOD: emit_with_decrement(vm_instruction_set::mod_desc); break;
  case bin_op::E_BIN_OP_EQ: emit_with_decrement(vm_instruction_set::cmp_eq_desc); break;
  case bin_op::E_BIN_OP_NE: emit_with_decrement(vm_instruction_set::cmp_ne_desc); break;
  case bin_op::E_BIN_OP_GT: emit_with_decrement(vm_instruction_set::cmp_gt_desc); break;
  case bin_op::E_BIN_OP_LS: emit_with_decrement(vm_instruction_set::cmp_ls_desc); break;
  case bin_op::E_BIN_OP_GE: emit_with_decrement(vm_instruction_set::cmp_ge_desc); break;
  case bin_op::E_BIN_OP_LE: emit_with_decrement(vm_instruction_set::cmp_le_desc); break;
  case bin_op::E_BIN_OP_AND: emit_with_decrement(vm_instruction_set::and_desc); break;
  case bin_op::E_BIN_OP_OR: emit_with_decrement(vm_instruction_set::or_desc); break;
  default: std::terminate();
  }
}

void codegen_visitor::generate(ast::statement_block &ref) {
  bool should_return = ref.type && ref.type != frontend::types::type_builtin::type_void();

  unsigned ret_addr_index = 0;
  unsigned prev_stack_size = m_prev_stack_size;

  if (should_return) {
    const auto const_index = current_constant_index();
    m_return_address_constants.push_back({const_index, 0}); // Dummy address
    ret_addr_index = m_return_address_constants.size() - 1;

    m_symtab_stack.begin_scope(); // scope to isolate IP and SP
    emit_with_increment(encoded_instruction{vm_instruction_set::push_const_desc, const_index});
    emit_with_increment(vm_instruction_set::setup_call_desc);

    m_prev_stack_size = m_symtab_stack.size();
  }

  m_symtab_stack.begin_scope(ref.stab);

  auto n_symbols = ref.stab.size();

  for (unsigned i = 0; i < n_symbols; ++i) {
    // no need to increment there cause it is already done in begin_scope
    m_builder.emit_operation(encoded_instruction{vm_instruction_set::push_const_desc, lookup_or_insert_constant(0)});
  }

  if (ref.size()) {
    for (auto start = ref.cbegin(), finish = ref.cend(); start != finish; ++start) {
      auto &&last = std::prev(finish);
      auto &&statement = *start;

      assert(statement && "Broken statement pointer");
      using frontend::ast::ast_expression_types;

      const auto node_type = frontend::ast::identify_node(*statement);
      const auto is_raw_expression =
          std::find(ast_expression_types.begin(), ast_expression_types.end(), node_type) != ast_expression_types.end();
      bool is_assignment = (node_type == frontend::ast::ast_node_type::E_ASSIGNMENT_STATEMENT);
      bool is_return = (node_type == frontend::ast::ast_node_type::E_RETURN_STATEMENT);
      bool is_last_iteration = start == last;
      bool pop_unused_result = (!is_last_iteration || !should_return) && is_raw_expression && !is_return;

      if (is_assignment && pop_unused_result) {
        set_currently_statement();
      } else {
        reset_currently_statement();
      }

      if (node_type != ast::ast_node_type::E_FUNCTION_DEFINITION) {
        apply(*statement);
      }

      if (!is_assignment && pop_unused_result) {
        if (!(node_type == frontend::ast::ast_node_type::E_FUNCTION_CALL &&
              static_cast<frontend::ast::function_call &>(*statement).type == frontend::types::type_builtin::type_void()
            )) {
          emit_pop();
        }
      }
      if (is_last_iteration && should_return && !is_return && is_raw_expression)
        emit_with_decrement(vm_instruction_set::load_r0_desc);
    }
  }
  if (!should_return) {
    for (unsigned i = 0; i < n_symbols; ++i) {
      emit_pop();
    }
  }

  m_symtab_stack.end_scope();

  if (should_return) {
    m_return_address_constants.at(ret_addr_index).m_address = m_builder.current_loc();
    m_prev_stack_size = prev_stack_size;
    m_symtab_stack.end_scope(); // end scope for IP and SP
    emit_with_increment(vm_instruction_set::store_r0_desc);
  }
}

void codegen_visitor::visit_if_no_else(ast::if_statement &ref) {
  reset_currently_statement();
  apply(ref.cond());

  auto index_jmp_to_false_block = emit_with_decrement(encoded_instruction{vm_instruction_set::jmp_false_desc, 0});

  set_currently_statement();
  apply(ref.true_block());

  auto jump_to_index = m_builder.current_loc();
  auto &to_relocate = m_builder.get_as(vm_instruction_set::jmp_false_desc, index_jmp_to_false_block);

  std::get<0>(to_relocate.m_attr) = jump_to_index;
}

void codegen_visitor::visit_if_with_else(ast::if_statement &ref) {
  reset_currently_statement();
  apply(ref.cond());

  auto index_jmp_to_false_block = emit_with_decrement(encoded_instruction{vm_instruction_set::jmp_false_desc, 0});

  set_currently_statement();
  apply(ref.true_block());
  auto index_jmp_to_after_true_block = m_builder.emit_operation(encoded_instruction{vm_instruction_set::jmp_desc, 0});

  auto &to_relocate_else_jump = m_builder.get_as(vm_instruction_set::jmp_false_desc, index_jmp_to_false_block);
  std::get<0>(to_relocate_else_jump.m_attr) = m_builder.current_loc();

  set_currently_statement();
  apply(*ref.else_block());

  auto &to_relocate_after_true_block = m_builder.get_as(vm_instruction_set::jmp_desc, index_jmp_to_after_true_block);
  std::get<0>(to_relocate_after_true_block.m_attr) = m_builder.current_loc();
}

void codegen_visitor::generate(ast::if_statement &ref) {
  m_symtab_stack.begin_scope(ref.control_block_symtab());

  for (unsigned i = 0; i < ref.control_block_symtab().size(); ++i) {
    // no need to increment there cause it is already done in begin_scope
    m_builder.emit_operation(encoded_instruction{vm_instruction_set::push_const_desc, lookup_or_insert_constant(0)});
  }

  if (!ref.else_block()) {
    visit_if_no_else(ref);
  } else {
    visit_if_with_else(ref);
  }

  for (unsigned i = 0; i < ref.control_block_symtab().size(); ++i) {
    emit_pop();
  }

  m_symtab_stack.end_scope();
}

void codegen_visitor::generate(ast::while_statement &ref) {
  m_symtab_stack.begin_scope(ref.symbol_table());

  for (unsigned i = 0; i < ref.symbol_table().size(); ++i) {
    // no need to increment there cause it is already done in begin_scope
    m_builder.emit_operation(encoded_instruction{vm_instruction_set::push_const_desc, lookup_or_insert_constant(0)});
  }

  auto while_location_start = m_builder.current_loc();
  reset_currently_statement();
  apply(ref.cond());

  auto index_jmp_to_after_loop = emit_with_decrement(encoded_instruction{vm_instruction_set::jmp_false_desc, 0});
  set_currently_statement();

  apply(ref.block());

  m_builder.emit_operation(encoded_instruction{vm_instruction_set::jmp_desc, while_location_start});

  auto &to_relocate_after_loop_jump = m_builder.get_as(vm_instruction_set::jmp_false_desc, index_jmp_to_after_loop);
  std::get<0>(to_relocate_after_loop_jump.m_attr) = m_builder.current_loc();

  for (unsigned i = 0; i < ref.symbol_table().size(); ++i) {
    emit_pop();
  }

  m_symtab_stack.end_scope();
}

void codegen_visitor::generate(ast::unary_expression &ref) {
  using unary_op = ast::unary_operation;

  reset_currently_statement();

  switch (ref.op_type()) {
  case unary_op::E_UN_OP_NEG: {
    emit_with_increment(encoded_instruction{vm_instruction_set::push_const_desc, lookup_or_insert_constant(0)});
    apply(ref.expr());
    emit_with_decrement(vm_instruction_set::sub_desc);
    break;
  }

  case unary_op::E_UN_OP_POS: {
    apply(ref.expr()); /* Do nothing */
    break;
  }

  case unary_op::E_UN_OP_NOT: {
    apply(ref.expr());
    m_builder.emit_operation(vm_instruction_set::not_desc);
    break;

  default: std::terminate();
  }
  }
}

void codegen_visitor::generate(ast::function_call &ref) {
  bool is_return = false;
  if (ref.type && ref.type != frontend::types::type_builtin::type_void()) {
    is_return = true;
  }
  const auto const_index = current_constant_index();
  m_return_address_constants.push_back({const_index, 0}); // Dummy address
  const auto ret_addr_index = m_return_address_constants.size() - 1;

  m_symtab_stack.begin_scope(); // scope to isolate IP and SP
  emit_with_increment(encoded_instruction{vm_instruction_set::push_const_desc, const_index});
  emit_with_increment(vm_instruction_set::setup_call_desc);

  auto &&n_args = ref.size();
  for (auto &&e : ref) {
    assert(e);
    apply(*e);
  }
  m_builder.emit_operation(encoded_instruction{vm_instruction_set::update_sp_desc, n_args});

  if (ref.m_def) {
    auto relocate_index = m_builder.emit_operation(encoded_instruction{vm_instruction_set::jmp_desc});
    m_relocations_function_calls.push_back({relocate_index, ref.m_def});
  }

  else {
    int total_depth = m_symtab_stack.size() - n_args;
    int rel_pos = m_symtab_stack.lookup_location(ref.name()) - total_depth;

    emit_with_increment(encoded_instruction{vm_instruction_set::push_local_rel_desc, rel_pos});
    emit_with_decrement(vm_instruction_set::jmp_dynamic_desc);
  }

  m_return_address_constants.at(ret_addr_index).m_address = m_builder.current_loc();
  m_symtab_stack.end_scope();
  if (is_return) emit_with_increment(encoded_instruction{vm_instruction_set::store_r0_desc});
}

void codegen_visitor::generate(frontend::ast::return_statement &ref) {
  if (!ref.empty()) {
    apply(ref.expr());
    emit_with_decrement(vm_instruction_set::load_r0_desc);
  }

  // clean up local variables
  unsigned local_var_n = m_symtab_stack.size() - m_prev_stack_size;
  for (unsigned i = 0; i < local_var_n; ++i) {
    m_builder.emit_operation(encoded_instruction{vm_instruction_set::pop_desc});
  }

  m_builder.emit_operation(encoded_instruction{vm_instruction_set::return_desc});
}

void codegen_visitor::generate(frontend::ast::function_definition_to_ptr_conv &ref) {
  const auto const_index = current_constant_index();
  m_dynamic_jumps_constants.push_back({const_index, 0, &ref.definition()}); // Dummy address
  emit_with_increment(encoded_instruction{vm_instruction_set::push_const_desc, const_index});
}

unsigned codegen_visitor::generate_function(frontend::ast::function_definition &ref) {
  m_symtab_stack.clear();

  m_curr_function = &ref;
  m_symtab_stack.begin_scope();
  for (auto &&var : ref) {
    m_symtab_stack.push_var(var.name());
  }

  auto &&function_pos = m_builder.current_loc();
  m_function_defs.insert({&ref, function_pos});
  apply(ref.body());

  for (unsigned i = 0; i < ref.param_symtab().size(); ++i) {
    emit_pop();
  }

  m_builder.emit_operation(encoded_instruction{vm_instruction_set::return_desc});

  m_symtab_stack.end_scope();

  return function_pos;
}

unsigned codegen_visitor::lookup_or_insert_constant(int constant) {
  auto found = m_constant_map.find(constant);
  unsigned index;

  if (found == m_constant_map.end()) {
    index = current_constant_index();
    m_constant_map.insert({constant, index});
  }

  else {
    index = found->second;
  }

  return index;
}

void codegen_visitor::generate_all(
    const frontend::ast::ast_container &ast, const frontend::functions_analytics &functions
) {
  m_return_address_constants.clear();
  m_functions = &functions;

  if (ast.get_root_ptr()) {
    apply(*ast.get_root_ptr()); // Last instruction is ret
  }

  m_builder.emit_operation(vm_instruction_set::return_desc);
  for (auto &&[name, attr] : functions.m_named) {
    assert(attr.definition && "Attribute definition pointer can't be nullptr");
    generate_function(*attr.definition);
  }

  for (auto &&reloc : m_relocations_function_calls) {
    auto &relocate_instruction = m_builder.get_as(vm_instruction_set::jmp_desc, reloc.m_reloc_index);
    relocate_instruction.m_attr = m_function_defs.at(reloc.m_func_ptr);
  }

  for (auto &&dynjmp : m_dynamic_jumps_constants) {
    assert(dynjmp.m_func_ptr);
    dynjmp.m_address = m_function_defs.at(dynjmp.m_func_ptr);
  }
}

paracl::bytecode_vm::decl_vm::chunk codegen_visitor::to_chunk() {
  auto ch = m_builder.to_chunk();

  std::vector<int> constants;
  constants.resize(current_constant_index());

  for (auto &&[constant, index] : m_constant_map) {
    constants[index] = constant;
  }

  for (auto &&v : m_return_address_constants) {
    constants[v.m_index] = v.m_address;
  }

  for (auto &&v : m_dynamic_jumps_constants) {
    constants[v.m_index] = v.m_address;
  }

  ch.set_constant_pool(std::move(constants));
  return ch;
}

} // namespace

} // namespace paracl::codegen