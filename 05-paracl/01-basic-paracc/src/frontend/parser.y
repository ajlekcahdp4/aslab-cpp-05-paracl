/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy me a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

%skeleton "lalr1.cc"
%require "3.8"

%defines

%define api.token.raw
%define api.parser.class { parser }
%define api.token.constructor
%define api.value.type variant
%define api.namespace { paracl::frontend }
%define parse.error custom

%code requires {
#include <iostream>
#include <string>
#include <vector>

#include "frontend/ast.hpp"

namespace paracl::frontend {
  class scanner;
  class frontend_driver;
}

using namespace paracl::frontend;

}

%code top
{

#include <iostream>
#include <string>
#include <sstream>

#include "frontend/scanner.hpp"
#include "bison_paracl_parser.hpp"
#include "frontend/frontend_driver.hpp"

static paracl::frontend::parser::symbol_type yylex(paracl::frontend::scanner &p_scanner, paracl::frontend::frontend_driver &p_driver) {
  return p_scanner.get_next_token();
}

}

%lex-param { paracl::frontend::scanner &scanner }
%lex-param { paracl::frontend::frontend_driver &driver }
%parse-param { paracl::frontend::scanner &scanner }
%parse-param { paracl::frontend::frontend_driver &driver }

%define parse.trace
%define api.token.prefix {TOKEN_}

%locations
%define api.location.file "location.hpp"

/* Signle letter tokens */
%token LPAREN   "lparen"
%token RPAREN   "rparen"
%token LBRACE   "lbrace"
%token RBRACE   "rbrace"

%token ASSIGN   "assign"

%token COMP_EQ  "=="
%token COMP_NE  "!="
%token COMP_GT  "GT"
%token COMP_LS  "LS"
%token COMP_GE  "GE"
%token COMP_LE  "LE"

%token QMARK    "?"
%token BANG     "!"

%token PLUS       "plus"
%token MINUS      "minus"
%token MULTIPLY   "multiply"
%token DIVIDE     "divide"
%token MODULUS    "modulus"

%token LOGICAL_AND  "and"
%token LOGICAL_OR   "or"

%token SEMICOL  "semicol"
%token EOF 0    "eof"

/* Keywords */
%token WHILE  "while"
%token IF     "if"
%token ELSE   "else"
%token PRINT  "print"

/* Terminals */
%token <int> INTEGER_CONSTANT "integer_constant"
%token <std::string> IDENTIFIER "identifier"

/* Rules that model the AST */
%type <ast::i_ast_node_uptr> primary_expression    
%type <ast::i_ast_node_uptr> multiplicative_expression
%type <ast::i_ast_node_uptr> unary_expression
%type <ast::i_ast_node_uptr> additive_expression
%type <ast::i_ast_node_uptr> comparison_expression
%type <ast::i_ast_node_uptr> equality_expression
%type <ast::i_ast_node_uptr> expression

%type <ast::i_ast_node_uptr> assignment_expression_statement

%type <ast::i_ast_node_uptr> print_statement
%type <ast::i_ast_node_uptr> assignment_statement
%type <ast::i_ast_node_uptr> statement_block
%type <ast::i_ast_node_uptr> statement
%type <std::vector<ast::i_ast_node_uptr>> statements
%type <ast::i_ast_node_uptr> if_statement
%type <ast::i_ast_node_uptr> while_statement

/* Utility rules */
%type <ast::unary_operation> unary_operator

%precedence THEN
%precedence ELSE

%start program

%%

program: statements { driver.m_ast = ast::make_statement_block(std::move($1)); }

primary_expression: INTEGER_CONSTANT            { $$ = ast::make_constant_expression($1); }
                    | IDENTIFIER                { $$ = ast::make_variable_expression($1); }
                    | QMARK                     { $$ = ast::make_read_expression(); }
                    | LPAREN expression RPAREN  { $$ = std::move($2); }
                    | LPAREN error RPAREN       { auto error = driver.take_error(); $$ = ast::make_error_node(error.error_message, error.loc);}

unary_operator: PLUS    { $$ = ast::unary_operation::E_UN_OP_POS; }
                | MINUS { $$ = ast::unary_operation::E_UN_OP_NEG; }
                | BANG  { $$ = ast::unary_operation::E_UN_OP_NOT; }

unary_expression: unary_operator unary_expression { $$ = ast::make_unary_expression($1, std::move($2)); }
                  | primary_expression            { $$ = std::move($1); }

multiplicative_expression:  multiplicative_expression MULTIPLY unary_expression   { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_MUL, std::move($1), std::move($3)); }
                            | multiplicative_expression DIVIDE unary_expression   { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_DIV, std::move($1), std::move($3)); }
                            | multiplicative_expression MODULUS unary_expression  { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_MOD, std::move($1), std::move($3)); }
                            | unary_expression                                    { $$ = std::move($1); }

additive_expression:  additive_expression PLUS multiplicative_expression      { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_ADD, std::move($1), std::move($3)); }
                      | additive_expression MINUS multiplicative_expression   { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_SUB, std::move($1), std::move($3)); }
                      | multiplicative_expression                             { $$ = std::move($1); }

comparison_expression:  comparison_expression COMP_GT additive_expression     { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_GT, std::move($1), std::move($3)); }
                        | comparison_expression COMP_LS additive_expression   { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_LS, std::move($1), std::move($3));  }
                        | comparison_expression COMP_GE additive_expression   { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_GE, std::move($1), std::move($3)); }
                        | comparison_expression COMP_LE additive_expression   { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_LE, std::move($1), std::move($3)); }
                        | additive_expression                                 { $$ = std::move($1); }


equality_expression:  equality_expression COMP_EQ comparison_expression   { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_EQ, std::move($1), std::move($3)); }
                      | equality_expression COMP_NE comparison_expression { $$ = ast::make_binary_expression(ast::binary_operation::E_BIN_OP_NE, std::move($1), std::move($3)); }
                      | comparison_expression                             { $$ = std::move($1); }

expression: equality_expression                 { $$ = std::move($1); }
            | assignment_expression_statement   { $$ = std::move($1); }

assignment_expression_statement: IDENTIFIER ASSIGN expression             { $$ = ast::make_assignment_statement(ast::make_variable_expression($1), std::move($3)); }

assignment_statement: assignment_expression_statement SEMICOL             { $$ = std::move($1); }

print_statement: PRINT expression SEMICOL { $$ = make_print_statement(std::move($2)); }

statements: statements statement        { $$ = std::move($1); $$.push_back(std::move($2)); }
            | statements error SEMICOL  { $$ = std::move($1); auto error = driver.take_error(); $$.push_back(ast::make_error_node(error.error_message, error.loc)); }
            | %empty                    { }

statement_block: LBRACE statements RBRACE   { $$ = ast::make_statement_block(std::move($2)); }

while_statement: WHILE LPAREN expression RPAREN statement { $$ = ast::make_while_statement(std::move($3), std::move($5)); }

if_statement: IF LPAREN expression RPAREN statement %prec THEN        { $$ = ast::make_if_statement(std::move($3), std::move($5)); }
              | IF LPAREN expression RPAREN statement ELSE statement  { $$ = ast::make_if_statement(std::move($3), std::move($5), std::move($7)); }

statement:  assignment_statement  { $$ = std::move($1); }
            | print_statement     { $$ = std::move($1); }
            | statement_block     { $$ = std::move($1); }
            | while_statement     { $$ = std::move($1); }
            | if_statement        { $$ = std::move($1); }
%%

// Bison expects us to provide implementation - otherwise linker complains
void paracl::frontend::parser::report_syntax_error(const context& ctx) const {
  location loc = ctx.location();
  std::stringstream error_message;
  const auto &lookahead = ctx.lookahead();
  error_message << "Syntax error at " << loc << " : " << "Unexpected " << lookahead.name();

  driver.report_error(error_message.str(), loc);
}

void paracl::frontend::parser::error(const location &loc, const std::string &message) {
  /* This never gets called, this is just here so that linker doesn't complain */
}