/* Parser: turns tokens into statements and expressions (also powers --parse). */

#include "parser.h"

#define EXPR_BLOCK_SIZE 256

struct ExprBlock {
    ExprBlock *next;
    Expr items[EXPR_BLOCK_SIZE];
};

void parser_init(Parser *parser, const char *path, const char *source) {
    lexer_init(&parser->lexer, source);
    parser->path = path;
    parser->had_error = 0;
    parser->entry_statement = -1;
    parser->expr_blocks = NULL;
    parser->expr_block_used = 0;
    parser->expression_count = 0;
    parser->statements = NULL;
    parser->statement_count = 0;
    parser->statement_capacity = 0;
    parser->parameters = NULL;
    parser->parameter_count = 0;
    parser->parameter_capacity = 0;
    parser->call_arguments = NULL;
    parser->call_argument_count = 0;
    parser->call_argument_capacity = 0;
    parser->list_elements = NULL;
    parser->list_element_count = 0;
    parser->list_element_capacity = 0;
    parser->top_level_statements = NULL;
    parser->top_level_statement_count = 0;
    parser->top_level_statement_capacity = 0;
    parser->block_items = NULL;
    parser->block_item_count = 0;
    parser->block_item_capacity = 0;
    parser->block_scratch = NULL;
    parser->block_scratch_count = 0;
    parser->block_scratch_capacity = 0;
    parser->element_scratch = NULL;
    parser->element_scratch_count = 0;
    parser->element_scratch_capacity = 0;
    parser->current.type = TOKEN_EOF;
    parser->current.start = source;
    parser->current.length = 0;
    parser->current.line = 1;
    parser->current.column = 1;
    parser->current.error_message = NULL;
    parser->previous = parser->current;
}

static void parser_error(Parser *parser, Token token, const char *message) {
    if (parser->had_error) {
        return;
    }

    fprintf(stderr,
            "%s:%d:%d: parse error: %s\n",
            parser->path,
            token.line,
            token.column,
            message);
    parser->had_error = 1;
}

static void parser_advance(Parser *parser) {
    parser->previous = parser->current;
    parser->current = next_token(&parser->lexer);

    if (parser->current.type == TOKEN_ERROR && !parser->had_error) {
        print_lexer_error(parser->path, parser->current);
        parser->had_error = 1;
    }
}

static int parser_match(Parser *parser, TokenType type) {
    if (parser->current.type != type) {
        return 0;
    }

    parser_advance(parser);
    return 1;
}

static void parser_consume(Parser *parser, TokenType type, const char *message) {
    if (parser->current.type == type) {
        parser_advance(parser);
        return;
    }

    parser_error(parser, parser->current, message);
}

static void parser_skip_separators(Parser *parser) {
    while (!parser->had_error && (parser->current.type == TOKEN_NEWLINE ||
                                    parser->current.type == TOKEN_SEMICOLON)) {
        parser_advance(parser);
    }
}

void parser_free(Parser *parser) {
    while (parser->expr_blocks != NULL) {
        ExprBlock *block = parser->expr_blocks;

        parser->expr_blocks = block->next;
        free(block);
    }
    free(parser->statements);
    free(parser->parameters);
    free(parser->call_arguments);
    free(parser->list_elements);
    free(parser->top_level_statements);
    free(parser->block_items);
    free(parser->block_scratch);
    free(parser->element_scratch);
    parser->statements = NULL;
    parser->parameters = NULL;
    parser->call_arguments = NULL;
    parser->list_elements = NULL;
    parser->top_level_statements = NULL;
    parser->block_items = NULL;
    parser->block_scratch = NULL;
    parser->element_scratch = NULL;
}

/* Make room for `needed` more items in a growable array, doubling its
   capacity. On failure it reports a parse error at `token` and returns 0. */
static int ensure_capacity(Parser *parser,
                           void **items,
                           int *capacity,
                           int count,
                           int needed,
                           size_t item_size,
                           Token token) {
    int new_capacity;
    void *grown;

    if (count > INT_MAX - needed) {
        parser_error(parser, token, "program is too large");
        return 0;
    }
    if (count + needed <= *capacity) {
        return 1;
    }

    new_capacity = *capacity == 0 ? 64 : *capacity;
    while (new_capacity < count + needed) {
        if (new_capacity > INT_MAX / 2) {
            new_capacity = count + needed;
            break;
        }
        new_capacity *= 2;
    }
    if ((size_t)new_capacity > (size_t)-1 / item_size) {
        parser_error(parser, token, "program is too large");
        return 0;
    }

    grown = realloc(*items, (size_t)new_capacity * item_size);
    if (grown == NULL) {
        parser_error(parser, token, "not enough memory to parse program");
        return 0;
    }
    *items = grown;
    *capacity = new_capacity;
    return 1;
}

static Expr *new_expr(Parser *parser, ExprKind kind, Token token) {
    Expr *expr;

    if (parser->expr_blocks == NULL || parser->expr_block_used == EXPR_BLOCK_SIZE) {
        ExprBlock *block = malloc(sizeof(ExprBlock));

        if (block == NULL) {
            parser_error(parser, token, "not enough memory to parse program");
            return NULL;
        }
        block->next = parser->expr_blocks;
        parser->expr_blocks = block;
        parser->expr_block_used = 0;
    }

    expr = &parser->expr_blocks->items[parser->expr_block_used];
    parser->expr_block_used++;
    parser->expression_count++;
    expr->kind = kind;
    expr->token = token;
    expr->left = NULL;
    expr->right = NULL;
    expr->argument_start = 0;
    expr->argument_count = 0;
    expr->element_start = 0;
    expr->element_count = 0;
    return expr;
}

static Stmt *new_stmt(Parser *parser, StmtKind kind, Token name, Expr *expression) {
    Stmt *stmt;

    /* The statement array may move when it grows, so callers keep a
       statement's index (stmt->index), never its address, past the next
       new_stmt call. */
    if (!ensure_capacity(parser,
                         (void **)&parser->statements,
                         &parser->statement_capacity,
                         parser->statement_count,
                         1,
                         sizeof(Stmt),
                         name)) {
        return NULL;
    }

    stmt = &parser->statements[parser->statement_count];
    stmt->index = parser->statement_count;
    parser->statement_count++;
    stmt->kind = kind;
    stmt->name = name;
    stmt->expression = expression;
    stmt->target = NULL;
    stmt->then_statements = NULL;
    stmt->then_start = 0;
    stmt->then_count = 0;
    stmt->else_statements = NULL;
    stmt->else_start = 0;
    stmt->else_count = 0;
    stmt->has_else = 0;
    stmt->body_statements = NULL;
    stmt->body_start = 0;
    stmt->body_count = 0;
    stmt->parameter_start = 0;
    stmt->parameter_count = 0;
    stmt->declared_type = TYPE_AUTO;
    stmt->is_entry = 0;
    return stmt;
}

static int statement_index(Parser *parser, Stmt *stmt) {
    (void)parser;
    return stmt->index;
}


static void add_top_level_statement(Parser *parser, Stmt *stmt) {
    if (stmt == NULL || parser->had_error) {
        return;
    }

    if (!ensure_capacity(parser,
                         (void **)&parser->top_level_statements,
                         &parser->top_level_statement_capacity,
                         parser->top_level_statement_count,
                         1,
                         sizeof(int),
                         stmt->name)) {
        return;
    }

    parser->top_level_statements[parser->top_level_statement_count] = statement_index(parser, stmt);
    parser->top_level_statement_count++;
}

static Expr *parse_expression(Parser *parser);
static Stmt *parse_statement(Parser *parser);
static Stmt *parse_function_declaration(Parser *parser, DeclType result_type, Token name, int typed);

static Expr *finish_function_call(Parser *parser, Token name) {
    Expr *call = new_expr(parser, EXPR_CALL, name);
    Expr *arguments[NEWT_MAX_FUNCTION_PARAMETERS];
    int i;

    if (call == NULL) {
        return NULL;
    }

    if (parser->current.type != TOKEN_RIGHT_PAREN) {
        do {
            Expr *argument;

            if (call->argument_count >= NEWT_MAX_FUNCTION_PARAMETERS) {
                parser_error(parser,
                             parser->current,
                             "function call has too many arguments");
                return NULL;
            }

            argument = parse_expression(parser);
            if (argument == NULL || parser->had_error) {
                return NULL;
            }
            arguments[call->argument_count] = argument;
            call->argument_count++;
        } while (parser_match(parser, TOKEN_COMMA));
    }

    parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after function arguments");
    if (parser->had_error) {
        return NULL;
    }

    if (!ensure_capacity(parser,
                         (void **)&parser->call_arguments,
                         &parser->call_argument_capacity,
                         parser->call_argument_count,
                         call->argument_count,
                         sizeof(Expr *),
                         name)) {
        return NULL;
    }
    call->argument_start = parser->call_argument_count;
    for (i = 0; i < call->argument_count; i++) {
        parser->call_arguments[parser->call_argument_count] = arguments[i];
        parser->call_argument_count++;
    }

    return call;
}

static Expr *parse_list_literal(Parser *parser, Token opening_bracket) {
    Expr *list = new_expr(parser, EXPR_LIST, opening_bracket);
    /* Elements collect on a scratch stack; a nested list literal pushes its
       own elements above ours and pops them before we continue, so ours stay
       contiguous from `base`. */
    int base = parser->element_scratch_count;
    int i;

    if (list == NULL) {
        return NULL;
    }

    if (parser->current.type != TOKEN_RIGHT_BRACKET) {
        do {
            Expr *element = parse_expression(parser);

            if (element == NULL || parser->had_error) {
                return NULL;
            }
            if (!ensure_capacity(parser,
                                 (void **)&parser->element_scratch,
                                 &parser->element_scratch_capacity,
                                 parser->element_scratch_count,
                                 1,
                                 sizeof(Expr *),
                                 opening_bracket)) {
                return NULL;
            }
            parser->element_scratch[parser->element_scratch_count] = element;
            parser->element_scratch_count++;
            list->element_count++;
        } while (parser_match(parser, TOKEN_COMMA));
    }

    parser_consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' after list elements");
    if (parser->had_error) {
        return NULL;
    }

    if (!ensure_capacity(parser,
                         (void **)&parser->list_elements,
                         &parser->list_element_capacity,
                         parser->list_element_count,
                         list->element_count,
                         sizeof(Expr *),
                         opening_bracket)) {
        return NULL;
    }
    list->element_start = parser->list_element_count;
    for (i = 0; i < list->element_count; i++) {
        parser->list_elements[parser->list_element_count] = parser->element_scratch[base + i];
        parser->list_element_count++;
    }
    parser->element_scratch_count = base;

    return list;
}

static DeclType token_decl_type(TokenType token) {
    switch (token) {
        case TOKEN_INT: return TYPE_INT;
        case TOKEN_FLOAT: return TYPE_FLOAT;
        case TOKEN_DOUBLE: return TYPE_DOUBLE;
        case TOKEN_VOID: return TYPE_VOID;
        case TOKEN_BOOL: return TYPE_BOOL;
        case TOKEN_STRING_TYPE: return TYPE_STRING;
        case TOKEN_LIST_TYPE: return TYPE_LIST;
        default: return TYPE_AUTO;
    }
}

static const char *parser_type_name(DeclType type) {
    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_VOID: return "void";
        case TYPE_BOOL: return "bool";
        case TYPE_STRING: return "string";
        case TYPE_LIST: return "list";
        default: return "untyped";
    }
}

static Expr *parse_primary(Parser *parser) {
    if (parser_match(parser, TOKEN_NUMBER)) {
        return new_expr(parser, EXPR_NUMBER, parser->previous);
    }
    if (parser->current.type == TOKEN_INT || parser->current.type == TOKEN_FLOAT ||
        parser->current.type == TOKEN_DOUBLE) {
        Token name = parser->current;
        parser_advance(parser);
        parser_consume(parser, TOKEN_LEFT_PAREN, "expected '(' after conversion type");
        return finish_function_call(parser, name);
    }
    if (parser_match(parser, TOKEN_IDENT)) {
        Token name = parser->previous;

        if (token_text_equals(name, "sqrt") && parser_match(parser, TOKEN_LEFT_PAREN)) {
            Expr *expr = new_expr(parser, EXPR_SQRT, name);

            if (expr == NULL) {
                return NULL;
            }

            if (parser->current.type != TOKEN_RIGHT_PAREN) {
                expr->left = parse_expression(parser);
                expr->argument_count = 1;

                while (!parser->had_error && parser_match(parser, TOKEN_COMMA)) {
                    parse_expression(parser);
                    expr->argument_count++;
                }
            }

            parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after sqrt arguments");
            return expr;
        }

        if (token_text_equals(name, "input_number") && parser_match(parser, TOKEN_LEFT_PAREN)) {
            Expr *expr;

            parser_consume(parser, TOKEN_STRING, "expected string literal prompt for input_number");
            if (parser->had_error) {
                return NULL;
            }

            expr = new_expr(parser, EXPR_INPUT_NUMBER, parser->previous);
            if (expr == NULL) {
                return NULL;
            }

            parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after input_number prompt");
            if (parser->had_error) {
                return NULL;
            }

            return expr;
        }

        if (parser_match(parser, TOKEN_LEFT_PAREN)) {
            return finish_function_call(parser, name);
        }

        return new_expr(parser, EXPR_IDENT, name);
    }
    if (parser_match(parser, TOKEN_STRING)) {
        return new_expr(parser, EXPR_STRING, parser->previous);
    }
    if (parser_match(parser, TOKEN_TRUE)) {
        return new_expr(parser, EXPR_TRUE, parser->previous);
    }
    if (parser_match(parser, TOKEN_FALSE)) {
        return new_expr(parser, EXPR_FALSE, parser->previous);
    }
    if (parser_match(parser, TOKEN_LEFT_BRACKET)) {
        return parse_list_literal(parser, parser->previous);
    }
    if (parser_match(parser, TOKEN_LEFT_PAREN)) {
        Expr *expr = parse_expression(parser);
        parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after expression");
        return expr;
    }

    parser_error(parser, parser->current, "expected expression");
    return NULL;
}

static Expr *finish_list_indexes(Parser *parser, Expr *expr) {
    while (expr != NULL &&
           !parser->had_error &&
           parser_match(parser, TOKEN_LEFT_BRACKET)) {
        Token opening_bracket = parser->previous;
        Expr *index;
        Expr *index_expr;

        if (parser->current.type == TOKEN_RIGHT_BRACKET) {
            parser_error(parser, parser->current, "expected index expression");
            return NULL;
        }
        index = parse_expression(parser);
        if (index == NULL || parser->had_error) {
            return NULL;
        }
        parser_consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' after list index");
        if (parser->had_error) {
            return NULL;
        }

        index_expr = new_expr(parser, EXPR_INDEX, opening_bracket);
        if (index_expr == NULL) {
            return NULL;
        }
        index_expr->left = expr;
        index_expr->right = index;
        expr = index_expr;
    }

    return expr;
}

static Expr *parse_postfix(Parser *parser) {
    return finish_list_indexes(parser, parse_primary(parser));
}

static Expr *parse_unary(Parser *parser) {
    if (parser->current.type == TOKEN_MINUS || parser->current.type == TOKEN_NOT) {
        Token operator_token;
        Expr *unary;

        parser_advance(parser);
        operator_token = parser->previous;
        unary = new_expr(parser, EXPR_UNARY, operator_token);
        if (unary == NULL) {
            return NULL;
        }

        unary->left = parse_unary(parser);
        return unary;
    }

    return parse_postfix(parser);
}

static Expr *parse_factor(Parser *parser) {
    Expr *left = parse_unary(parser);

    while (!parser->had_error &&
           (parser->current.type == TOKEN_STAR || parser->current.type == TOKEN_SLASH)) {
        Token operator_token;
        Expr *binary;
        Expr *right;

        parser_advance(parser);
        operator_token = parser->previous;
        right = parse_unary(parser);
        if (right == NULL) {
            return NULL;
        }

        binary = new_expr(parser, EXPR_BINARY, operator_token);
        if (binary == NULL) {
            return NULL;
        }
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

static Expr *parse_term(Parser *parser) {
    Expr *left = parse_factor(parser);

    while (!parser->had_error &&
           (parser->current.type == TOKEN_PLUS || parser->current.type == TOKEN_MINUS)) {
        Token operator_token;
        Expr *binary;
        Expr *right;

        parser_advance(parser);
        operator_token = parser->previous;
        right = parse_factor(parser);
        if (right == NULL) {
            return NULL;
        }

        binary = new_expr(parser, EXPR_BINARY, operator_token);
        if (binary == NULL) {
            return NULL;
        }
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

static Expr *parse_comparison(Parser *parser) {
    Expr *left = parse_term(parser);

    while (!parser->had_error &&
           (parser->current.type == TOKEN_LESS ||
            parser->current.type == TOKEN_LESS_EQUAL ||
            parser->current.type == TOKEN_GREATER ||
            parser->current.type == TOKEN_GREATER_EQUAL)) {
        Token operator_token;
        Expr *binary;
        Expr *right;

        parser_advance(parser);
        operator_token = parser->previous;
        right = parse_term(parser);
        if (right == NULL) {
            return NULL;
        }

        binary = new_expr(parser, EXPR_BINARY, operator_token);
        if (binary == NULL) {
            return NULL;
        }
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

static Expr *parse_equality(Parser *parser) {
    Expr *left = parse_comparison(parser);

    while (!parser->had_error &&
           (parser->current.type == TOKEN_EQUAL_EQUAL || parser->current.type == TOKEN_BANG_EQUAL)) {
        Token operator_token;
        Expr *binary;
        Expr *right;

        parser_advance(parser);
        operator_token = parser->previous;
        right = parse_comparison(parser);
        if (right == NULL) {
            return NULL;
        }

        binary = new_expr(parser, EXPR_BINARY, operator_token);
        if (binary == NULL) {
            return NULL;
        }
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

static Expr *parse_and(Parser *parser) {
    Expr *left = parse_equality(parser);

    while (!parser->had_error && parser->current.type == TOKEN_AND) {
        Token operator_token;
        Expr *binary;
        Expr *right;

        parser_advance(parser);
        operator_token = parser->previous;
        right = parse_equality(parser);
        if (right == NULL) {
            return NULL;
        }

        binary = new_expr(parser, EXPR_BINARY, operator_token);
        if (binary == NULL) {
            return NULL;
        }
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

static Expr *parse_expression(Parser *parser) {
    Expr *left = parse_and(parser);

    while (!parser->had_error && parser->current.type == TOKEN_OR) {
        Token operator_token;
        Expr *binary;
        Expr *right;

        parser_advance(parser);
        operator_token = parser->previous;
        right = parse_and(parser);
        if (right == NULL) {
            return NULL;
        }

        binary = new_expr(parser, EXPR_BINARY, operator_token);
        if (binary == NULL) {
            return NULL;
        }
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

static void print_indent(int spaces) {
    int i;

    for (i = 0; i < spaces; i++) {
        putchar(' ');
    }
}

static void print_expression_tree(Parser *parser, Expr *expr, int indent) {
    if (expr == NULL) {
        return;
    }

    print_indent(indent);

    switch (expr->kind) {
        case EXPR_NUMBER:
            printf("NUMBER %.*s\n", expr->token.length, expr->token.start);
            break;
        case EXPR_IDENT:
            printf("IDENT %.*s\n", expr->token.length, expr->token.start);
            break;
        case EXPR_STRING:
            printf("STRING %.*s\n", expr->token.length, expr->token.start);
            break;
        case EXPR_TRUE:
            printf("TRUE %.*s\n", expr->token.length, expr->token.start);
            break;
        case EXPR_FALSE:
            printf("FALSE %.*s\n", expr->token.length, expr->token.start);
            break;
        case EXPR_UNARY:
            printf("UNARY %.*s\n", expr->token.length, expr->token.start);
            print_expression_tree(parser, expr->left, indent + 2);
            break;
        case EXPR_BINARY:
            printf("BINARY %.*s\n", expr->token.length, expr->token.start);
            print_expression_tree(parser, expr->left, indent + 2);
            print_expression_tree(parser, expr->right, indent + 2);
            break;
        case EXPR_CALL: {
            int i;

            printf("CALL name=%.*s arguments=%d\n",
                   expr->token.length,
                   expr->token.start,
                   expr->argument_count);
            for (i = 0; i < expr->argument_count; i++) {
                Expr *argument = parser->call_arguments[expr->argument_start + i];
                print_expression_tree(parser, argument, indent + 2);
            }
            break;
        }
        case EXPR_LIST: {
            int i;

            printf("LIST elements=%d\n", expr->element_count);
            for (i = 0; i < expr->element_count; i++) {
                Expr *element = parser->list_elements[expr->element_start + i];
                print_expression_tree(parser, element, indent + 2);
            }
            break;
        }
        case EXPR_INDEX:
            printf("INDEX\n");
            print_indent(indent + 2);
            printf("TARGET\n");
            print_expression_tree(parser, expr->left, indent + 4);
            print_indent(indent + 2);
            printf("POSITION\n");
            print_expression_tree(parser, expr->right, indent + 4);
            break;
        case EXPR_INPUT_NUMBER:
            printf("INPUT_NUMBER %.*s\n", expr->token.length, expr->token.start);
            break;
        case EXPR_SQRT:
            printf("SQRT arguments=%d\n", expr->argument_count);
            if (expr->left != NULL) {
                print_expression_tree(parser, expr->left, indent + 2);
            }
            break;
    }
}

/* Simple statements require an explicit terminator; braces close blocks. */
static void parser_consume_statement_end(Parser *parser) {
    parser_consume(parser, TOKEN_SEMICOLON, "expected ';' after statement");
    parser_skip_separators(parser);
}

/* Permit either brace placement without making semicolons optional. */
static void parser_skip_newlines(Parser *parser) {
    while (!parser->had_error &&
           (parser->current.type == TOKEN_NEWLINE ||
            (parser->current.type == TOKEN_SEMICOLON &&
             parser->current.start[1] == '#'))) {
        parser_advance(parser);
    }
}

static void parser_consume_block_open(Parser *parser) {
    parser_skip_newlines(parser);
    parser_consume(parser, TOKEN_LEFT_BRACE, "expected '{' to open block");
    parser_skip_separators(parser);
}

static Stmt *parse_named_declaration(Parser *parser, StmtKind kind, Token name, DeclType type) {
    Expr *initializer;
    Stmt *stmt;
    if (type == TYPE_VOID) {
        parser_error(parser, name, "variables cannot have type void");
        return NULL;
    }
    parser_consume(parser, TOKEN_EQUAL, "expected '=' after variable name");
    initializer = parse_expression(parser);
    parser_consume_statement_end(parser);
    if (initializer == NULL || parser->had_error) return NULL;
    stmt = new_stmt(parser, kind, name, initializer);
    if (stmt != NULL) stmt->declared_type = type;
    return stmt;
}

static Stmt *parse_declaration(Parser *parser, StmtKind kind, const char *keyword) {
    DeclType type = token_decl_type(parser->current.type);
    Token name;
    char message[64];
    if (type != TYPE_AUTO) parser_advance(parser);
    else if (strcmp(keyword, "const") == 0) {
        parser_error(parser, parser->current, "expected type after const");
        return NULL;
    }
    snprintf(message, sizeof(message), "expected identifier after %s", keyword);
    parser_consume(parser, TOKEN_IDENT, message);
    if (parser->had_error) return NULL;
    name = parser->previous;
    return parse_named_declaration(parser, kind, name, type);
}

static Stmt *parse_print_statement(Parser *parser) {
    Expr *expr = parse_expression(parser);

    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    return new_stmt(parser, STMT_PRINT, parser->previous, expr);
}

static Stmt *parse_assignment_statement(Parser *parser) {
    Token name = parser->previous;
    Expr *expr;

    parser_consume(parser, TOKEN_EQUAL, "expected '=' after assignment name");
    if (parser->had_error) {
        return NULL;
    }

    expr = parse_expression(parser);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    return new_stmt(parser, STMT_ASSIGN, name, expr);
}

static Stmt *parse_index_assignment_statement(Parser *parser, Token name) {
    Expr *target = new_expr(parser, EXPR_IDENT, name);
    Expr *value;
    Stmt *stmt;

    target = finish_list_indexes(parser, target);
    parser_consume(parser, TOKEN_EQUAL, "expected '=' after indexed assignment target");
    value = parse_expression(parser);
    parser_consume_statement_end(parser);
    if (target == NULL || value == NULL || parser->had_error) {
        return NULL;
    }

    stmt = new_stmt(parser, STMT_INDEX_ASSIGN, name, value);
    if (stmt != NULL) {
        stmt->target = target;
    }
    return stmt;
}

static Stmt *parse_function_call(Parser *parser, Token name) {
    Expr *call;

    parser_consume(parser, TOKEN_LEFT_PAREN, "expected '(' after function name");
    if (parser->had_error) {
        return NULL;
    }

    call = finish_function_call(parser, name);
    if (call == NULL || parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    return new_stmt(parser, STMT_CALL, name, call);
}

static Stmt *parse_return_statement(Parser *parser) {
    Token return_token = parser->previous;
    Expr *value = NULL;

    if (parser->current.type != TOKEN_SEMICOLON) value = parse_expression(parser);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    return new_stmt(parser, STMT_RETURN, return_token, value);
}

static Stmt *parse_break_statement(Parser *parser) {
    Token break_token = parser->previous;

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    return new_stmt(parser, STMT_BREAK, break_token, NULL);
}

static Stmt *parse_continue_statement(Parser *parser) {
    Token continue_token = parser->previous;

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    return new_stmt(parser, STMT_CONTINUE, continue_token, NULL);
}

/* Append `count` statement indexes to the shared block pool and return
   where they start, or -1 on failure. */
static int store_block(Parser *parser, const int *indexes, int count, Token token) {
    int start;
    int i;

    if (!ensure_capacity(parser,
                         (void **)&parser->block_items,
                         &parser->block_item_capacity,
                         parser->block_item_count,
                         count,
                         sizeof(int),
                         token)) {
        return -1;
    }
    start = parser->block_item_count;
    for (i = 0; i < count; i++) {
        parser->block_items[start + i] = indexes[i];
    }
    parser->block_item_count += count;
    return start;
}

/* Parse statements until a stop token. Child indexes collect on the block
   scratch stack: a nested block pushes above ours and pops back before we
   push again, so ours stay contiguous from `base`. At the end they are
   copied into the block pool and popped. */
static void parse_block(Parser *parser, TokenType stop_one, TokenType stop_two, int *start, int *count) {
    int base = parser->block_scratch_count;

    *start = 0;
    *count = 0;
    parser_skip_separators(parser);

    while (!parser->had_error &&
           parser->current.type != TOKEN_EOF &&
           parser->current.type != stop_one &&
           parser->current.type != stop_two) {
        Stmt *stmt = parse_statement(parser);
        if (stmt != NULL) {
            if (!ensure_capacity(parser,
                                 (void **)&parser->block_scratch,
                                 &parser->block_scratch_capacity,
                                 parser->block_scratch_count,
                                 1,
                                 sizeof(int),
                                 stmt->name)) {
                return;
            }
            parser->block_scratch[parser->block_scratch_count] = statement_index(parser, stmt);
            parser->block_scratch_count++;
        }
        parser_skip_separators(parser);
    }
    if (parser->had_error) {
        return;
    }

    *count = parser->block_scratch_count - base;
    *start = store_block(parser, parser->block_scratch + base, *count, parser->current);
    parser->block_scratch_count = base;
    if (*start < 0) {
        *start = 0;
        *count = 0;
    }
}

static Stmt *parse_function_declaration(Parser *parser, DeclType result_type, Token name, int typed) {
    Stmt *stmt;
    int body_start;
    int body_count;
    int parameter_start;
    int parameter_count = 0;

    if (!typed) {
        parser_consume(parser, TOKEN_IDENT, "expected function name after 'fn'");
        if (parser->had_error) return NULL;
        name = parser->previous;
    }

    parser_consume(parser, TOKEN_LEFT_PAREN, "expected '(' after function name");
    if (parser->had_error) {
        return NULL;
    }

    parameter_start = parser->parameter_count;
    if (typed && parser_match(parser, TOKEN_VOID)) {
        if (parser->current.type != TOKEN_RIGHT_PAREN) {
            parser_error(parser, parser->current, "void must be the only parameter marker");
            return NULL;
        }
    }
    if (parser->current.type != TOKEN_RIGHT_PAREN) {
        do {
            Token parameter;
            DeclType parameter_type = TYPE_AUTO;
            int parameter_mutable = typed;
            int parameter_index;

            if (parameter_count >= NEWT_MAX_FUNCTION_PARAMETERS) {
                parser_error(parser,
                             parser->current,
                             "function declaration has too many parameters");
                return NULL;
            }

            if (typed) {
                if (parser_match(parser, TOKEN_CONST) || parser_match(parser, TOKEN_LET)) parameter_mutable = 0;
                else parser_match(parser, TOKEN_MUT);
                parameter_type = token_decl_type(parser->current.type);
                if (parameter_type == TYPE_AUTO || parameter_type == TYPE_VOID) {
                    parser_error(parser, parser->current, "expected non-void parameter type");
                    return NULL;
                }
                parser_advance(parser);
            }
            parser_consume(parser, TOKEN_IDENT, "expected parameter name");
            if (parser->had_error) {
                return NULL;
            }
            parameter = parser->previous;

            for (parameter_index = parameter_start;
                 parameter_index < parser->parameter_count;
                 parameter_index++) {
                Token existing = parser->parameters[parameter_index].name;
                if (existing.length == parameter.length &&
                    strncmp(existing.start,
                            parameter.start,
                            (size_t)parameter.length) == 0) {
                    parser_error(parser, parameter, "duplicate parameter name");
                    return NULL;
                }
            }

            if (!ensure_capacity(parser,
                                 (void **)&parser->parameters,
                                 &parser->parameter_capacity,
                                 parser->parameter_count,
                                 1,
                                 sizeof(Parameter),
                                 parameter)) {
                return NULL;
            }
            parser->parameters[parser->parameter_count].name = parameter;
            parser->parameters[parser->parameter_count].type = parameter_type;
            parser->parameters[parser->parameter_count].is_mutable = parameter_mutable;
            parser->parameter_count++;
            parameter_count++;
        } while (parser_match(parser, TOKEN_COMMA));
    }

    parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after function parameters");
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_block_open(parser);
    if (parser->had_error) {
        return NULL;
    }

    parse_block(parser, TOKEN_RIGHT_BRACE, TOKEN_RIGHT_BRACE, &body_start, &body_count);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after function declaration");
    if (parser->had_error) {
        return NULL;
    }

    stmt = new_stmt(parser, STMT_FN_DECL, name, NULL);
    if (stmt == NULL) {
        return NULL;
    }

    stmt->body_start = body_start;
    stmt->body_count = body_count;
    stmt->parameter_start = parameter_start;
    stmt->parameter_count = parameter_count;
    stmt->declared_type = result_type;
    return stmt;
}

static Stmt *parse_if_branch(Parser *parser, Token if_token) {
    Expr *condition;
    Stmt *stmt;
    int then_start;
    int then_count;
    int else_start = 0;
    int else_count = 0;
    int has_else = 0;

    condition = parse_expression(parser);
    if (condition == NULL || parser->had_error) {
        return NULL;
    }
    parser_consume_block_open(parser);
    if (parser->had_error) {
        return NULL;
    }
    parse_block(parser, TOKEN_RIGHT_BRACE, TOKEN_RIGHT_BRACE, &then_start, &then_count);
    parser_consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after if branch");
    if (parser->had_error) {
        return NULL;
    }

    parser_skip_separators(parser);
    if (parser_match(parser, TOKEN_ELSE)) {
        has_else = 1;
        parser_skip_separators(parser);
        if (parser_match(parser, TOKEN_IF)) {
            Stmt *else_if_stmt = parse_if_branch(parser, parser->previous);
            int else_if_index;

            if (else_if_stmt == NULL) {
                return NULL;
            }
            else_if_index = statement_index(parser, else_if_stmt);
            else_start = store_block(parser, &else_if_index, 1, if_token);
            if (else_start < 0) {
                return NULL;
            }
            else_count = 1;
        } else {
            parser_consume_block_open(parser);
            if (parser->had_error) {
                return NULL;
            }
            parse_block(parser, TOKEN_RIGHT_BRACE, TOKEN_RIGHT_BRACE, &else_start, &else_count);
            parser_consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after else branch");
            if (parser->had_error) {
                return NULL;
            }
        }
    }

    stmt = new_stmt(parser, STMT_IF, if_token, condition);
    if (stmt == NULL) {
        return NULL;
    }
    stmt->then_start = then_start;
    stmt->then_count = then_count;
    stmt->else_start = else_start;
    stmt->else_count = else_count;
    stmt->has_else = has_else;
    return stmt;
}

static Stmt *parse_if_statement(Parser *parser) {
    return parse_if_branch(parser, parser->previous);
}

static Stmt *parse_while_statement(Parser *parser) {
    Token while_token = parser->previous;
    Expr *condition;
    Stmt *stmt;
    int body_start;
    int body_count;

    condition = parse_expression(parser);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_block_open(parser);
    if (parser->had_error) {
        return NULL;
    }

    parse_block(parser, TOKEN_RIGHT_BRACE, TOKEN_RIGHT_BRACE, &body_start, &body_count);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after while statement");
    if (parser->had_error) {
        return NULL;
    }

    stmt = new_stmt(parser, STMT_WHILE, while_token, condition);
    if (stmt == NULL) {
        return NULL;
    }

    stmt->body_start = body_start;
    stmt->body_count = body_count;
    return stmt;
}

static Stmt *parse_for_statement(Parser *parser) {
    Token name;
    Expr *iterable;
    Stmt *stmt;
    int body_start;
    int body_count;

    parser_consume(parser, TOKEN_IDENT, "expected loop variable after 'for'");
    if (parser->had_error) {
        return NULL;
    }
    name = parser->previous;

    parser_consume(parser, TOKEN_IN, "expected 'in' after for loop variable");
    if (parser->had_error) {
        return NULL;
    }

    if (parser->current.type == TOKEN_SEMICOLON ||
        parser->current.type == TOKEN_NEWLINE ||
        parser->current.type == TOKEN_EOF ||
        parser->current.type == TOKEN_RIGHT_BRACE) {
        parser_error(parser, parser->current, "expected iterable expression after 'in'");
        return NULL;
    }
    iterable = parse_expression(parser);
    if (iterable == NULL || parser->had_error) {
        return NULL;
    }

    parser_consume_block_open(parser);
    if (parser->had_error) {
        return NULL;
    }

    parse_block(parser, TOKEN_RIGHT_BRACE, TOKEN_RIGHT_BRACE, &body_start, &body_count);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after for statement");
    if (parser->had_error) {
        return NULL;
    }

    stmt = new_stmt(parser, STMT_FOR, name, iterable);
    if (stmt == NULL) {
        return NULL;
    }
    stmt->body_start = body_start;
    stmt->body_count = body_count;
    return stmt;
}

static Stmt *parse_statement(Parser *parser) {
    if (parser->current.type == TOKEN_START) {
        parser_error(parser, parser->current, "start: is only allowed at top level");
        return NULL;
    }
    DeclType type = token_decl_type(parser->current.type);
    if (type != TYPE_AUTO) {
        Token name;
        parser_advance(parser);
        parser_consume(parser, TOKEN_IDENT, "expected name after type");
        if (parser->had_error) return NULL;
        name = parser->previous;
        if (parser->current.type == TOKEN_LEFT_PAREN) {
            return parse_function_declaration(parser, type, name, 1);
        }
        return parse_named_declaration(parser, STMT_MUT_DECL, name, type);
    }
    if (parser_match(parser, TOKEN_CONST)) {
        return parse_declaration(parser, STMT_LET_DECL, "const");
    }
    if (parser_match(parser, TOKEN_LET)) {
        return parse_declaration(parser, STMT_LET_DECL, "let");
    }

    if (parser_match(parser, TOKEN_MUT)) {
        return parse_declaration(parser, STMT_MUT_DECL, "mut");
    }

    if (parser_match(parser, TOKEN_FN)) {
        return parse_function_declaration(parser, TYPE_AUTO, parser->previous, 0);
    }

    if (parser_match(parser, TOKEN_RETURN)) {
        return parse_return_statement(parser);
    }

    if (parser_match(parser, TOKEN_BREAK)) {
        return parse_break_statement(parser);
    }

    if (parser_match(parser, TOKEN_CONTINUE)) {
        return parse_continue_statement(parser);
    }

    if (parser_match(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }

    if (parser_match(parser, TOKEN_WHILE)) {
        return parse_while_statement(parser);
    }

    if (parser_match(parser, TOKEN_FOR)) {
        return parse_for_statement(parser);
    }

    if (parser_match(parser, TOKEN_PRINT)) {
        return parse_print_statement(parser);
    }

    if (parser_match(parser, TOKEN_IDENT)) {
        Token name = parser->previous;

        if (parser->current.type == TOKEN_LEFT_PAREN) {
            return parse_function_call(parser, name);
        }
        if (parser->current.type == TOKEN_LEFT_BRACKET) {
            return parse_index_assignment_statement(parser, name);
        }

        return parse_assignment_statement(parser);
    }

    parser_error(parser, parser->current, "expected statement");
    return NULL;
}

static void print_statement_tree(Parser *parser, Stmt *stmt, int indent) {
    int i;

    switch (stmt->kind) {
        case STMT_LET_DECL:
            print_indent(indent);
            printf("LET_DECL name=%.*s\n", stmt->name.length, stmt->name.start);
            if (stmt->declared_type != TYPE_AUTO) {
                print_indent(indent + 2);
                printf("TYPE %s\n", parser_type_name(stmt->declared_type));
            }
            print_expression_tree(parser, stmt->expression, indent + 2);
            break;
        case STMT_MUT_DECL:
            print_indent(indent);
            printf("MUT_DECL name=%.*s\n", stmt->name.length, stmt->name.start);
            if (stmt->declared_type != TYPE_AUTO) {
                print_indent(indent + 2);
                printf("TYPE %s\n", parser_type_name(stmt->declared_type));
            }
            print_expression_tree(parser, stmt->expression, indent + 2);
            break;
        case STMT_FN_DECL:
            if (stmt->is_entry) {
                print_indent(indent);
                printf("ENTRY start:\n");
            }
            print_indent(indent);
            printf("FN_DECL name=%.*s\n", stmt->name.length, stmt->name.start);
            if (stmt->declared_type != TYPE_AUTO) {
                print_indent(indent + 2);
                printf("RETURNS %s\n", parser_type_name(stmt->declared_type));
            }
            if (stmt->parameter_count > 0) {
                print_indent(indent + 2);
                printf("PARAMETERS\n");
                for (i = 0; i < stmt->parameter_count; i++) {
                    Parameter metadata = parser->parameters[stmt->parameter_start + i];
                    Token parameter = metadata.name;
                    print_indent(indent + 4);
                    printf("PARAM name=%.*s", parameter.length, parameter.start);
                    if (metadata.type != TYPE_AUTO) printf(" type=%s mutable=%d", parser_type_name(metadata.type), metadata.is_mutable);
                    printf("\n");
                }
            }
            print_indent(indent + 2);
            printf("BODY\n");
            for (i = 0; i < stmt->body_count; i++) {
                int stmt_index = stmt->body_statements[i];
                print_statement_tree(parser, &parser->statements[stmt_index], indent + 4);
            }
            break;
        case STMT_CALL:
            print_expression_tree(parser, stmt->expression, indent);
            break;
        case STMT_RETURN:
            print_indent(indent);
            printf("RETURN\n");
            print_expression_tree(parser, stmt->expression, indent + 2);
            break;
        case STMT_BREAK:
            print_indent(indent);
            printf("BREAK\n");
            break;
        case STMT_CONTINUE:
            print_indent(indent);
            printf("CONTINUE\n");
            break;
        case STMT_ASSIGN:
            print_indent(indent);
            printf("ASSIGN name=%.*s\n", stmt->name.length, stmt->name.start);
            print_expression_tree(parser, stmt->expression, indent + 2);
            break;
        case STMT_INDEX_ASSIGN:
            print_indent(indent);
            printf("INDEX_ASSIGN name=%.*s\n", stmt->name.length, stmt->name.start);
            print_indent(indent + 2);
            printf("TARGET\n");
            print_expression_tree(parser, stmt->target, indent + 4);
            print_indent(indent + 2);
            printf("VALUE\n");
            print_expression_tree(parser, stmt->expression, indent + 4);
            break;
        case STMT_IF:
            print_indent(indent);
            printf("IF\n");
            print_indent(indent + 2);
            printf("CONDITION\n");
            print_expression_tree(parser, stmt->expression, indent + 4);
            print_indent(indent + 2);
            printf("THEN\n");
            for (i = 0; i < stmt->then_count; i++) {
                int stmt_index = stmt->then_statements[i];
                print_statement_tree(parser, &parser->statements[stmt_index], indent + 4);
            }
            if (stmt->has_else) {
                print_indent(indent + 2);
                printf("ELSE\n");
                for (i = 0; i < stmt->else_count; i++) {
                    int stmt_index = stmt->else_statements[i];
                    print_statement_tree(parser, &parser->statements[stmt_index], indent + 4);
                }
            }
            break;

        case STMT_WHILE:
            print_indent(indent);
            printf("WHILE\n");
            print_indent(indent + 2);
            printf("CONDITION\n");
            print_expression_tree(parser, stmt->expression, indent + 4);
            print_indent(indent + 2);
            printf("BODY\n");
            for (i = 0; i < stmt->body_count; i++) {
                int stmt_index = stmt->body_statements[i];
                print_statement_tree(parser, &parser->statements[stmt_index], indent + 4);
            }
            break;
        case STMT_FOR:
            print_indent(indent);
            printf("FOR name=%.*s\n", stmt->name.length, stmt->name.start);
            print_indent(indent + 2);
            printf("ITERABLE\n");
            print_expression_tree(parser, stmt->expression, indent + 4);
            print_indent(indent + 2);
            printf("BODY\n");
            for (i = 0; i < stmt->body_count; i++) {
                int stmt_index = stmt->body_statements[i];
                print_statement_tree(parser, &parser->statements[stmt_index], indent + 4);
            }
            break;
        case STMT_PRINT:
            print_indent(indent);
            printf("PRINT\n");
            print_expression_tree(parser, stmt->expression, indent + 2);
            break;
    }
}

/* start: is an entry directive, not a function call or an arbitrary jump. */
static Stmt *parse_entry_declaration(Parser *parser) {
    Token start_token = parser->current;
    DeclType type;
    Token name;
    Stmt *stmt;
    if (parser->entry_statement >= 0) {
        parser_error(parser, start_token, "only one start: entry is allowed");
        return NULL;
    }
    parser_advance(parser);
    parser_consume(parser, TOKEN_COLON, "expected ':' after start");
    if (parser->had_error) return NULL;
    parser_skip_newlines(parser);
    type = token_decl_type(parser->current.type);
    if (type != TYPE_INT && type != TYPE_VOID) {
        parser_error(parser, parser->current, "start: requires int main(void) or void main(void)");
        return NULL;
    }
    parser_advance(parser);
    parser_consume(parser, TOKEN_IDENT, "expected main after entry return type");
    if (parser->had_error) return NULL;
    name = parser->previous;
    if (!token_text_equals(name, "main")) {
        parser_error(parser, name, "start: entry function must be named main");
        return NULL;
    }
    stmt = parse_function_declaration(parser, type, name, 1);
    if (stmt == NULL || parser->had_error) return NULL;
    if (stmt->parameter_count != 0) {
        parser_error(parser, name, "start: main must have no parameters; use arg_count() and arg()");
        return NULL;
    }
    stmt->is_entry = 1;
    parser->entry_statement = stmt->index;
    return stmt;
}

int parse_program(Parser *parser) {
    parser_advance(parser);

    if (parser->had_error) {
        return 0;
    }

    parser_skip_separators(parser);

    while (!parser->had_error && parser->current.type != TOKEN_EOF) {
        Stmt *stmt = parser->current.type == TOKEN_START
                   ? parse_entry_declaration(parser) : parse_statement(parser);
        add_top_level_statement(parser, stmt);
        parser_skip_separators(parser);
    }

    if (parser->had_error) {
        return 0;
    }

    if (parser->entry_statement >= 0) {
        int i;
        for (i = 0; i < parser->top_level_statement_count; i++) {
            Stmt *stmt = &parser->statements[parser->top_level_statements[i]];
            if (stmt->kind != STMT_FN_DECL && stmt->kind != STMT_LET_DECL && stmt->kind != STMT_MUT_DECL) {
                parser_error(parser, stmt->name, "programs with start: allow only declarations at top level; put executable statements inside main");
                return 0;
            }
        }
    }

    /* Parsing is finished, so the block pool will not move again: aim each
       statement's block pointers into it. */
    {
        int i;

        for (i = 0; i < parser->statement_count; i++) {
            Stmt *stmt = &parser->statements[i];

            if (stmt->then_count > 0) {
                stmt->then_statements = parser->block_items + stmt->then_start;
            }
            if (stmt->else_count > 0) {
                stmt->else_statements = parser->block_items + stmt->else_start;
            }
            if (stmt->body_count > 0) {
                stmt->body_statements = parser->block_items + stmt->body_start;
            }
        }
    }

    return 1;
}

int print_parse_tree(const char *path, const char *source) {
    Parser parser;
    int i;

    parser_init(&parser, path, source);
    if (!parse_program(&parser)) {
        parser_free(&parser);
        return 0;
    }

    printf("PROGRAM\n");
    for (i = 0; i < parser.top_level_statement_count; i++) {
        int stmt_index = parser.top_level_statements[i];
        print_statement_tree(&parser, &parser.statements[stmt_index], 2);
    }
    printf("EOF\n");
    parser_free(&parser);
    return 1;
}

