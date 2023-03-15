/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#pragma once

#include "frontend/symtab.hpp"
#include "frontend/types/types.hpp"

#include "i_ast_node.hpp"
#include "variable_expression.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace paracl::frontend::ast {

class function_definition final : public i_ast_node, private std::vector<variable_expression> {
private:
  // An optional function name. Those functions that don't have a name will be called anonymous functions
  std::optional<std::string> m_name;

  symtab m_symtab;
  i_ast_node *m_block;

private:
  types::type_composite_function
  make_func_type(const std::vector<variable_expression> &vars, const types::generic_type &return_type) {
    std::vector<types::generic_type> arg_types;

    for (const auto &v : vars) {
      assert(v.m_type);
      arg_types.push_back(v.type);
    }

    return types::type_composite_function{arg_types, return_type};
  }

public:
  types::type_composite_function m_type;

  EZVIS_VISITABLE();

  function_definition(
      std::optional<std::string> name, i_ast_node &body, location l, std::vector<variable_expression> vars = {},
      types::generic_type return_type = {}
  )
      : i_ast_node{l}, vector{std::move(vars)}, m_name{name}, m_block{&body}, m_type{
                                                                                  make_func_type(vars, return_type)} {}

  using vector::begin;
  using vector::cbegin;
  using vector::cend;
  using vector::crbegin;
  using vector::crend;
  using vector::end;
  using vector::size;

  symtab *param_symtab() { return &m_symtab; }

  i_ast_node &body() const { return *m_block; }
  std::optional<std::string> name() const { return m_name; }
};

class function_definition_to_ptr_conv final : public i_expression {
  function_definition *m_definition;

  EZVIS_VISITABLE();

public:
  function_definition_to_ptr_conv(location l, function_definition &def) : i_expression{l}, m_definition{&def} {}
  function_definition &definition() const { return *m_definition; }
};

} // namespace paracl::frontend::ast