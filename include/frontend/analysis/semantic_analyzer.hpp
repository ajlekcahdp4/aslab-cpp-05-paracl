/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long
 * as you retain this notice you can do whatever you want with this stuff. If we
 * meet some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#pragma once

#include "ezvis/ezvis.hpp"
#include "location.hpp"

#include "frontend/analysis/augmented_ast.hpp"
#include "frontend/ast/ast_container.hpp"
#include "frontend/ast/ast_nodes.hpp"

#include "frontend/error.hpp"
#include "frontend/symtab.hpp"
#include "frontend/types/types.hpp"

#include <iostream>

namespace paracl::frontend {

class semantic_analyzer final : public ezvis::visitor_base<ast::i_ast_node, semantic_analyzer, void> {
private:
  symtab_stack m_scopes;
  functions_analytics *m_functions;
  ast::ast_container *m_ast;

public:
private:
  error_queue_type *m_error_queue;
  error_queue_type m_default_error_queue;

private:
  // Vector of return statements in the current functions.
  std::vector<const ast::return_statement *> m_return_statements;

private:
  enum class semantic_analysis_state {
    E_LVALUE,
    E_RVALUE,
    E_DEFAULT,
  } current_state = semantic_analysis_state::E_DEFAULT;

  bool m_in_function_body = false;
  bool m_type_errors_allowed = false; // Flag used to indicate that a type mismatch is not an error.
  // Set this flag to true when doing a first pass on recurisive functions.

  bool m_in_void_block = false; // Flag used to indicate that we are in guaranteed to be void block (e.g. 'while' body,
                                // 'if' body or function body)

private:
  void set_state(semantic_analysis_state s) { current_state = s; }
  void reset_state() { current_state = semantic_analysis_state::E_DEFAULT; }

private:
  void report_error(std::string msg, location loc) const {
    m_error_queue->push_back({
        error_kind{msg, loc}
    });
  }

  void report_error(error_report report) const { m_error_queue->push_back(std::move(report)); }

  bool expect_type_eq(const ast::i_expression &ref, const types::i_type &rhs) const {
    if (m_type_errors_allowed) return false;

    auto &&type = ref.type;
    if (!type || !(type == rhs)) {
      report_error("Expression is not of type '" + rhs.to_string() + "'", ref.loc());
      return false;
    }

    return true;
  }

  bool expect_type_eq(const ast::i_expression &ref, const types::generic_type &rhs) const {
    return expect_type_eq(ref, rhs.base());
  }

private:
  void check_return_types_matches(ast::function_definition &ref);
  void begin_scope(symtab &stab) { m_scopes.begin_scope(&stab); }
  void end_scope() { m_scopes.end_scope(); }

public:
  EZVIS_VISIT_CT(ast::tuple_all_nodes);

  void analyze_node(ast::error_node &ref) { report_error(ref.error_msg(), ref.loc()); }

  // clang-format off
  void analyze_node(ast::read_expression &) {}
  void analyze_node(ast::constant_expression &) {}
  // clang-format on

  void analyze_node(ast::assignment_statement &);
  void analyze_node(ast::binary_expression &);

  void analyze_node(ast::if_statement &);
  void analyze_node(ast::print_statement &);

  void analyze_node(ast::statement_block &, bool function_body = false);
  void analyze_node(ast::unary_expression &);
  bool analyze_node(ast::variable_expression &);
  void analyze_node(ast::while_statement &);

  void analyze_node(ast::function_call &);
  void analyze_node(ast::function_definition &);
  void analyze_node(ast::function_definition_to_ptr_conv &);
  void analyze_node(ast::return_statement &);

  EZVIS_VISIT_INVOKER(analyze_node);

  bool analyze_main(ast::i_ast_node &);
  bool analyze_func(ast::function_definition &ref, bool is_recursive);

public:
  semantic_analyzer() : m_error_queue{&m_default_error_queue} {}
  semantic_analyzer(functions_analytics &functions) { m_functions = &functions; } // Temporary

  void set_error_queue(std::vector<error_report> &errors) { m_error_queue = &errors; }
  error_queue_type &get_error_queue() & { return *m_error_queue; }
  void set_functions(functions_analytics &functions) { m_functions = &functions; }
  void set_ast(ast::ast_container &ast) { m_ast = &ast; }
};

} // namespace paracl::frontend