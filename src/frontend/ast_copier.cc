/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#include "frontend/ast/ast_copier.hpp"
#include "frontend/ast/ast_nodes.hpp"

#include <cassert>
#include <memory>
#include <variant>

namespace paracl::frontend::ast {

template <typename T> T &trivial_ast_node_copy(const T &ref, ast_container &cont) {
  return cont.make_node<T>(ref);
}

subscript &ast_copier::copy(const subscript &ref) {
  return trivial_ast_node_copy(ref, m_container);
}

read_expression &ast_copier::copy(const read_expression &ref) {
  return trivial_ast_node_copy(ref, m_container);
}

variable_expression &ast_copier::copy(const variable_expression &ref) {
  return trivial_ast_node_copy(ref, m_container);
}

error_node &ast_copier::copy(const error_node &ref) {
  return trivial_ast_node_copy(ref, m_container);
}

constant_expression &ast_copier::copy(const constant_expression &ref) {
  return trivial_ast_node_copy(ref, m_container);
}

binary_expression &ast_copier::copy(const binary_expression &ref) {
  return m_container.make_node<binary_expression>(
      ref.op_type(), copy_expr(ref.left()), copy_expr(ref.right()), ref.loc()
  );
}

print_statement &ast_copier::copy(const print_statement &ref) {
  return m_container.make_node<print_statement>(copy_expr(ref.expr()), ref.loc());
}

unary_expression &ast_copier::copy(const unary_expression &ref) {
  return m_container.make_node<unary_expression>(ref.op_type(), copy_expr(ref.expr()), ref.loc());
}

while_statement &ast_copier::copy(const while_statement &ref) {
  return m_container.make_node<while_statement>(
      static_cast<i_expression &>(apply(*ref.cond())),
      static_cast<statement_block &>(apply(*ref.block())), ref.loc()
  );
}

function_definition &ast_copier::copy(const function_definition &ref) {
  std::vector<variable_expression> arguments{ref.begin(), ref.end()};
  return m_container.make_node<function_definition>(ref.name, ref.body(), ref.loc(), arguments);
}

function_definition_to_ptr_conv &ast_copier::copy(const function_definition_to_ptr_conv &ref) {
  return m_container.make_node<function_definition_to_ptr_conv>(copy(ref.definition()), ref.loc());
}

return_statement &ast_copier::copy(const return_statement &ref) {
  if (!ref.empty())
    return m_container.make_node<return_statement>(&copy_expr(ref.expr()), ref.loc());
  return m_container.make_node<return_statement>(nullptr, ref.loc());
}

assignment_statement &ast_copier::copy(const assignment_statement &ref) {
  auto &copy =
      m_container.make_node<assignment_statement>(*ref.rbegin(), copy_expr(ref.right()), ref.loc());

  for (auto start = std::next(ref.rbegin()), finish = ref.rend(); start != finish; ++start) {
    if (std::holds_alternative<variable_expression>(*start))
      copy.append(std::get<variable_expression>(*start));
    else if (std::holds_alternative<subscript>(*start)) copy.append(std::get<subscript>(*start));
  }

  return copy;
}

if_statement &ast_copier::copy(const if_statement &ref) {
  if (ref.else_block()) {
    return m_container.make_node<if_statement>(
        copy_expr(*ref.cond()), copy(*ref.true_block()), copy(*ref.else_block()), ref.loc()
    );
  }

  return m_container.make_node<if_statement>(
      copy_expr(*ref.cond()), copy(*ref.true_block()), ref.loc()
  );
}

value_block &ast_copier::copy(const value_block &ref) {
  auto &copy = m_container.make_node<value_block>();

  for (const auto &v : ref) {
    copy.append_statement(apply(*v));
  }

  return copy;
}

statement_block &ast_copier::copy(const statement_block &ref) {
  auto &copy = m_container.make_node<statement_block>();

  for (const auto &v : ref) {
    copy.append_statement(apply(*v));
  }

  return copy;
}

function_call &ast_copier::copy(const function_call &ref) {
  auto &copy = m_container.make_node<function_call>(std::string{ref.name()}, ref.loc());

  for (const auto v : ref) {
    copy.append_parameter(&copy_expr(*v));
  }

  return copy;
}

} // namespace paracl::frontend::ast
