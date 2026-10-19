/* Parser: turns tokens into statements and expressions (also powers --parse). */

#include "parser.h"

void parser_init(Parser *parser, const char *path, const char *source) {
    lexer_init(&parser->lexer, source);
    parser->path = path;
    parser->had_error = 0;
    parser->expression_count = 0;
    parser->statement_count = 0;
    parser->parameter_count = 0;
    parser->call_argument_count = 0;
    parser->list_element_count = 0;
    parser->top_level_statement_count = 0;
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

    if (parser->current.type == TOKEN_ERROR) {
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

static void parser_skip_newlines(Parser *parser) {
    while (!parser->had_error && parser->current.type == TOKEN_NEWLINE) {
        parser_advance(parser);
    }
}

static Expr *new_expr(Parser *parser, ExprKind kind, Token token) {
    Expr *expr;

    if (parser->expression_count >= 1024) {
        parser_error(parser, token, "too many expressions");
        return NULL;
    }

    expr = &parser->expressions[parser->expression_count];
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

    if (parser->statement_count >= 256) {
        parser_error(parser, name, "too many statements");
        return NULL;
    }

    stmt = &parser->statements[parser->statement_count];
    parser->statement_count++;
    stmt->kind = kind;
    stmt->name = name;
    stmt->expression = expression;
    stmt->target = NULL;
    stmt->then_count = 0;
    stmt->else_count = 0;
    stmt->has_else = 0;
    stmt->body_count = 0;
    stmt->parameter_start = 0;
    stmt->parameter_count = 0;
    return stmt;
}

static int statement_index(Parser *parser, Stmt *stmt) {
    return (int)(stmt - parser->statements);
}


static void add_top_level_statement(Parser *parser, Stmt *stmt) {
    if (stmt == NULL || parser->had_error) {
        return;
    }

    if (parser->top_level_statement_count >= 256) {
        parser_error(parser, stmt->name, "too many statements");
        return;
    }

    parser->top_level_statements[parser->top_level_statement_count] = statement_index(parser, stmt);
    parser->top_level_statement_count++;
}

static Expr *parse_expression(Parser *parser);
static Stmt *parse_statement(Parser *parser);

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

    if (parser->call_argument_count + call->argument_count > 1024) {
        parser_error(parser, name, "too many function call arguments");
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
    Expr *elements[1024];
    int i;

    if (list == NULL) {
        return NULL;
    }

    if (parser->current.type != TOKEN_RIGHT_BRACKET) {
        do {
            Expr *element;

            if (list->element_count >= 1024) {
                parser_error(parser, parser->current, "list literal has too many elements");
                return NULL;
            }

            element = parse_expression(parser);
            if (element == NULL || parser->had_error) {
                return NULL;
            }
            elements[list->element_count] = element;
            list->element_count++;
        } while (parser_match(parser, TOKEN_COMMA));
    }

    parser_consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' after list elements");
    if (parser->had_error) {
        return NULL;
    }

    if (parser->list_element_count + list->element_count > 1024) {
        parser_error(parser, opening_bracket, "too many list elements");
        return NULL;
    }
    list->element_start = parser->list_element_count;
    for (i = 0; i < list->element_count; i++) {
        parser->list_elements[parser->list_element_count] = elements[i];
        parser->list_element_count++;
    }

    return list;
}

static Expr *parse_primary(Parser *parser) {
    if (parser_match(parser, TOKEN_NUMBER)) {
        return new_expr(parser, EXPR_NUMBER, parser->previous);
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

static void parser_consume_statement_end(Parser *parser) {
    if (parser->current.type == TOKEN_NEWLINE) {
        parser_skip_newlines(parser);
        return;
    }

    if (parser->current.type == TOKEN_EOF) {
        return;
    }

    parser_error(parser, parser->current, "expected end of statement");
}

static Stmt *parse_declaration(Parser *parser, StmtKind kind, const char *keyword) {
    Token name;
    Expr *initializer;
    char message[64];

    snprintf(message, sizeof(message), "expected identifier after %s", keyword);
    parser_consume(parser, TOKEN_IDENT, message);
    if (parser->had_error) {
        return NULL;
    }
    name = parser->previous;

    parser_consume(parser, TOKEN_EQUAL, "expected '=' after variable name");
    if (parser->had_error) {
        return NULL;
    }

    initializer = parse_expression(parser);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    return new_stmt(parser, kind, name, initializer);
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
    Expr *value = parse_expression(parser);

    if (value == NULL || parser->had_error) {
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

static void parse_block(Parser *parser, TokenType stop_one, TokenType stop_two, int statements[256], int *count) {
    *count = 0;
    parser_skip_newlines(parser);

    while (!parser->had_error &&
           parser->current.type != TOKEN_EOF &&
           parser->current.type != stop_one &&
           parser->current.type != stop_two) {
        Stmt *stmt = parse_statement(parser);
        if (stmt != NULL) {
            if (*count >= 256) {
                parser_error(parser, stmt->name, "too many block statements");
                return;
            }
            statements[*count] = statement_index(parser, stmt);
            (*count)++;
        }
        parser_skip_newlines(parser);
    }
}

static Stmt *parse_function_declaration(Parser *parser) {
    Token name;
    Stmt *stmt;
    int body_statements[256];
    int body_count;
    int parameter_start;
    int parameter_count = 0;
    int i;

    parser_consume(parser, TOKEN_IDENT, "expected function name after 'fn'");
    if (parser->had_error) {
        return NULL;
    }
    name = parser->previous;

    parser_consume(parser, TOKEN_LEFT_PAREN, "expected '(' after function name");
    if (parser->had_error) {
        return NULL;
    }

    parameter_start = parser->parameter_count;
    if (parser->current.type != TOKEN_RIGHT_PAREN) {
        do {
            Token parameter;
            int parameter_index;

            if (parameter_count >= NEWT_MAX_FUNCTION_PARAMETERS) {
                parser_error(parser,
                             parser->current,
                             "function declaration has too many parameters");
                return NULL;
            }

            parser_consume(parser, TOKEN_IDENT, "expected parameter name");
            if (parser->had_error) {
                return NULL;
            }
            parameter = parser->previous;

            for (parameter_index = parameter_start;
                 parameter_index < parser->parameter_count;
                 parameter_index++) {
                Token existing = parser->parameters[parameter_index];
                if (existing.length == parameter.length &&
                    strncmp(existing.start,
                            parameter.start,
                            (size_t)parameter.length) == 0) {
                    parser_error(parser, parameter, "duplicate parameter name");
                    return NULL;
                }
            }

            if (parser->parameter_count >= 256) {
                parser_error(parser, parameter, "too many function parameters");
                return NULL;
            }
            parser->parameters[parser->parameter_count] = parameter;
            parser->parameter_count++;
            parameter_count++;
        } while (parser_match(parser, TOKEN_COMMA));
    }

    parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after function parameters");
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    parse_block(parser, TOKEN_END, TOKEN_END, body_statements, &body_count);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume(parser, TOKEN_END, "expected 'end' after function declaration");
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    stmt = new_stmt(parser, STMT_FN_DECL, name, NULL);
    if (stmt == NULL) {
        return NULL;
    }

    for (i = 0; i < body_count; i++) {
        stmt->body_statements[i] = body_statements[i];
    }
    stmt->body_count = body_count;
    stmt->parameter_start = parameter_start;
    stmt->parameter_count = parameter_count;
    return stmt;
}

static Stmt *parse_if_branch(Parser *parser, Token if_token, int consumes_end) {
    Expr *condition;
    Stmt *stmt;
    int then_statements[256];
    int then_count;
    int else_statements[256];
    int else_count = 0;
    int has_else = 0;
    int i;

    if (parser->current.type == TOKEN_NEWLINE ||
        parser->current.type == TOKEN_EOF ||
        parser->current.type == TOKEN_ELSE ||
        parser->current.type == TOKEN_END) {
        parser_error(parser,
                     parser->current,
                     consumes_end ? "expected condition after 'if'"
                                  : "expected condition after 'else if'");
        return NULL;
    }

    condition = parse_expression(parser);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    parse_block(parser, TOKEN_ELSE, TOKEN_END, then_statements, &then_count);
    if (parser->had_error) {
        return NULL;
    }

    if (parser_match(parser, TOKEN_ELSE)) {
        has_else = 1;
        if (parser_match(parser, TOKEN_IF)) {
            Stmt *else_if_stmt = parse_if_branch(parser, parser->previous, 0);

            if (else_if_stmt == NULL) {
                return NULL;
            }

            else_statements[0] = statement_index(parser, else_if_stmt);
            else_count = 1;
        } else {
            parser_consume_statement_end(parser);
            if (parser->had_error) {
                return NULL;
            }

            parse_block(parser, TOKEN_END, TOKEN_END, else_statements, &else_count);
            if (parser->had_error) {
                return NULL;
            }
        }
    }

    if (consumes_end) {
        parser_consume(parser, TOKEN_END, "expected 'end' after if statement");
        if (parser->had_error) {
            return NULL;
        }

        parser_consume_statement_end(parser);
        if (parser->had_error) {
            return NULL;
        }
    }

    stmt = new_stmt(parser, STMT_IF, if_token, condition);
    if (stmt == NULL) {
        return NULL;
    }

    for (i = 0; i < then_count; i++) {
        stmt->then_statements[i] = then_statements[i];
    }
    for (i = 0; i < else_count; i++) {
        stmt->else_statements[i] = else_statements[i];
    }
    stmt->then_count = then_count;
    stmt->else_count = else_count;
    stmt->has_else = has_else;
    return stmt;
}

static Stmt *parse_if_statement(Parser *parser) {
    return parse_if_branch(parser, parser->previous, 1);
}

static Stmt *parse_while_statement(Parser *parser) {
    Token while_token = parser->previous;
    Expr *condition;
    Stmt *stmt;
    int body_statements[256];
    int body_count;
    int i;

    condition = parse_expression(parser);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    parse_block(parser, TOKEN_END, TOKEN_END, body_statements, &body_count);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume(parser, TOKEN_END, "expected 'end' after while statement");
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    stmt = new_stmt(parser, STMT_WHILE, while_token, condition);
    if (stmt == NULL) {
        return NULL;
    }

    for (i = 0; i < body_count; i++) {
        stmt->body_statements[i] = body_statements[i];
    }
    stmt->body_count = body_count;
    return stmt;
}

static Stmt *parse_for_statement(Parser *parser) {
    Token name;
    Expr *iterable;
    Stmt *stmt;
    int body_statements[256];
    int body_count;
    int i;

    parser_consume(parser, TOKEN_IDENT, "expected loop variable after 'for'");
    if (parser->had_error) {
        return NULL;
    }
    name = parser->previous;

    parser_consume(parser, TOKEN_IN, "expected 'in' after for loop variable");
    if (parser->had_error) {
        return NULL;
    }

    if (parser->current.type == TOKEN_NEWLINE ||
        parser->current.type == TOKEN_EOF ||
        parser->current.type == TOKEN_END) {
        parser_error(parser, parser->current, "expected iterable expression after 'in'");
        return NULL;
    }
    iterable = parse_expression(parser);
    if (iterable == NULL || parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    parse_block(parser, TOKEN_END, TOKEN_END, body_statements, &body_count);
    if (parser->had_error) {
        return NULL;
    }

    parser_consume(parser, TOKEN_END, "expected 'end' after for statement");
    if (parser->had_error) {
        return NULL;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return NULL;
    }

    stmt = new_stmt(parser, STMT_FOR, name, iterable);
    if (stmt == NULL) {
        return NULL;
    }
    for (i = 0; i < body_count; i++) {
        stmt->body_statements[i] = body_statements[i];
    }
    stmt->body_count = body_count;
    return stmt;
}

static Stmt *parse_statement(Parser *parser) {
    if (parser_match(parser, TOKEN_VAL)) {
        return parse_declaration(parser, STMT_VAL_DECL, "val");
    }

    if (parser_match(parser, TOKEN_MUT)) {
        return parse_declaration(parser, STMT_MUT_DECL, "mut");
    }

    if (parser_match(parser, TOKEN_FN)) {
        return parse_function_declaration(parser);
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
        case STMT_VAL_DECL:
            print_indent(indent);
            printf("VAL_DECL name=%.*s\n", stmt->name.length, stmt->name.start);
            print_expression_tree(parser, stmt->expression, indent + 2);
            break;
        case STMT_MUT_DECL:
            print_indent(indent);
            printf("MUT_DECL name=%.*s\n", stmt->name.length, stmt->name.start);
            print_expression_tree(parser, stmt->expression, indent + 2);
            break;
        case STMT_FN_DECL:
            print_indent(indent);
            printf("FN_DECL name=%.*s\n", stmt->name.length, stmt->name.start);
            if (stmt->parameter_count > 0) {
                print_indent(indent + 2);
                printf("PARAMETERS\n");
                for (i = 0; i < stmt->parameter_count; i++) {
                    Token parameter = parser->parameters[stmt->parameter_start + i];
                    print_indent(indent + 4);
                    printf("PARAM name=%.*s\n", parameter.length, parameter.start);
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

int parse_program(Parser *parser) {
    parser_advance(parser);

    if (parser->had_error) {
        return 0;
    }

    parser_skip_newlines(parser);

    while (!parser->had_error && parser->current.type != TOKEN_EOF) {
        Stmt *stmt = parse_statement(parser);
        add_top_level_statement(parser, stmt);
        parser_skip_newlines(parser);
    }

    return !parser->had_error;
}

int print_parse_tree(const char *path, const char *source) {
    Parser parser;
    int i;

    parser_init(&parser, path, source);
    if (!parse_program(&parser)) {
        return 0;
    }

    printf("PROGRAM\n");
    for (i = 0; i < parser.top_level_statement_count; i++) {
        int stmt_index = parser.top_level_statements[i];
        print_statement_tree(&parser, &parser.statements[stmt_index], 2);
    }
    printf("EOF\n");
    return 1;
}
