/* Expression and statement trees, and the parser interface. */

#ifndef NEWT_PARSER_H
#define NEWT_PARSER_H

#include "lexer.h"

typedef enum {
    EXPR_NUMBER,
    EXPR_IDENT,
    EXPR_STRING,
    EXPR_TRUE,
    EXPR_FALSE,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_CALL,
    EXPR_LIST,
    EXPR_INDEX,
    EXPR_INPUT_NUMBER,
    EXPR_SQRT
} ExprKind;
typedef struct Expr Expr;
typedef enum {
    STMT_VAL_DECL,
    STMT_MUT_DECL,
    STMT_FN_DECL,
    STMT_CALL,
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_ASSIGN,
    STMT_INDEX_ASSIGN,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR,
    STMT_PRINT
} StmtKind;
typedef struct {
    StmtKind kind;
    Token name;
    Expr *expression;
    Expr *target;
    int then_statements[256];
    int then_count;
    int else_statements[256];
    int else_count;
    int has_else;
    int body_statements[256];
    int body_count;
    int parameter_start;
    int parameter_count;
} Stmt;
struct Expr {
    ExprKind kind;
    Token token;
    Expr *left;
    Expr *right;
    int argument_start;
    int argument_count;
    int element_start;
    int element_count;
};
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    const char *path;
    int had_error;
    Expr expressions[1024];
    int expression_count;
    Stmt statements[256];
    int statement_count;
    Token parameters[256];
    int parameter_count;
    Expr *call_arguments[1024];
    int call_argument_count;
    Expr *list_elements[1024];
    int list_element_count;
    int top_level_statements[256];
    int top_level_statement_count;
} Parser;

void parser_init(Parser *parser, const char *path, const char *source);

int parse_program(Parser *parser);

int print_parse_tree(const char *path, const char *source);

#endif
