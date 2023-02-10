/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

#include "frontend/semantic_analyzer.hpp"
#include "utils.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace paracl::frontend {

// clang-format off

void semantic_analyzer_visitor::visit(ast::constant_expression *) { /* Do nothing */ }
void semantic_analyzer_visitor::visit(ast::read_expression *) { /* Do nothing */ }

// clang-format on

void semantic_analyzer_visitor::report_error(std::string msg, location loc) {
  m_valid = false; // As we have encountered an error the program is ill-formed
  std::cerr << "Error at " << loc << " : " << msg << "\n";
}

void semantic_analyzer_visitor::visit(ast::assignment_statement *ptr) {
  assert(ptr);
  set_state(semantic_analysis_state::E_LVALUE);
  ast_node_visit(*this, ptr->left());
  set_state(semantic_analysis_state::E_RVALUE);
  ast_node_visit(*this, ptr->right());
  reset_state();
}

void semantic_analyzer_visitor::visit(ast::binary_expression *ptr) {
  assert(ptr);
  set_state(semantic_analysis_state::E_RVALUE);
  ast_node_visit(*this, ptr->right());
  ast_node_visit(*this, ptr->left());
  reset_state();
}

void semantic_analyzer_visitor::visit(ast::print_statement *ptr) {
  assert(ptr);
  set_state(semantic_analysis_state::E_RVALUE);
  ast_node_visit(*this, ptr->expr());
  reset_state();
}

void semantic_analyzer_visitor::visit(ast::error_node *ptr) {
  assert(ptr);
  report_error(ptr->error_msg(), ptr->loc());
}

void semantic_analyzer_visitor::visit(ast::statement_block *ptr) {
  assert(ptr);
  m_scopes.begin_scope(ptr->symbol_table());

  for (auto &statement : *ptr) {
    ast_node_visit(*this, statement);
  }

  m_scopes.end_scope();
}

void semantic_analyzer_visitor::visit(ast::if_statement *ptr) {
  assert(ptr);
  m_scopes.begin_scope(ptr->control_block_symtab());
  ast_node_visit(*this, ptr->cond());

  m_scopes.begin_scope(ptr->true_symtab());
  ast_node_visit(*this, ptr->true_block());
  m_scopes.end_scope();

  if (ptr->else_block() != nullptr) {
    m_scopes.begin_scope(ptr->else_symtab());
    ast_node_visit(*this, ptr->else_block());
    m_scopes.end_scope();
  }

  m_scopes.end_scope();
}

void semantic_analyzer_visitor::visit(ast::while_statement *ptr) {
  assert(ptr);
  m_scopes.begin_scope(ptr->symbol_table());

  ast_node_visit(*this, ptr->cond());
  ast_node_visit(*this, ptr->block());

  m_scopes.end_scope();
}

void semantic_analyzer_visitor::visit(ast::unary_expression *ptr) {
  assert(ptr);
  ast_node_visit(*this, ptr->expr());
}

void semantic_analyzer_visitor::visit(ast::variable_expression *ptr) {
  assert(ptr);
  if (!m_scopes.declared(ptr->name())) {
    if (current_state == semantic_analysis_state::E_LVALUE) {
      m_scopes.declare(ptr->name());
    } else {
      report_error("Use of undeclared variable", ptr->loc());
    }
  }
  /* TODO[Sergei]: Bind the variable to it's declaration symbol table */
}

bool ast_analyze(ast::i_ast_node *node) {
  assert(node);
  semantic_analyzer_visitor analyzer;
  ast_node_visit(analyzer, node);
  return analyzer.valid();
}

} // namespace paracl::frontend