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
    STMT_LET_DECL,
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
/* Block statement lists live in one shared pool (Parser.block_items). Each
   block records where its run starts; once parsing succeeds, the *_statements
   pointers are aimed into the finished pool, which never moves again. */
typedef struct {
    StmtKind kind;
    Token name;
    Expr *expression;
    Expr *target;
    int index;
    int *then_statements;
    int then_start;
    int then_count;
    int *else_statements;
    int else_start;
    int else_count;
    int has_else;
    int *body_statements;
    int body_start;
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
/* Expressions point at each other by address, so they are allocated in
   fixed-size blocks that never move (a growable array could move them). */
typedef struct ExprBlock ExprBlock;

/* Every array below grows as needed and is owned by the parser;
   parser_free releases all of it, even after a parse error. */
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    const char *path;
    int had_error;
    ExprBlock *expr_blocks;
    int expr_block_used;
    int expression_count;
    Stmt *statements;
    int statement_count;
    int statement_capacity;
    Token *parameters;
    int parameter_count;
    int parameter_capacity;
    Expr **call_arguments;
    int call_argument_count;
    int call_argument_capacity;
    Expr **list_elements;
    int list_element_count;
    int list_element_capacity;
    int *top_level_statements;
    int top_level_statement_count;
    int top_level_statement_capacity;
    int *block_items;
    int block_item_count;
    int block_item_capacity;
    /* Temporary stacks for blocks and list literals that are still being
       parsed; nested ones push above and pop back before the outer continues. */
    int *block_scratch;
    int block_scratch_count;
    int block_scratch_capacity;
    Expr **element_scratch;
    int element_scratch_count;
    int element_scratch_capacity;
} Parser;

void parser_init(Parser *parser, const char *path, const char *source);

int parse_program(Parser *parser);

void parser_free(Parser *parser);

int print_parse_tree(const char *path, const char *source);

#endif

