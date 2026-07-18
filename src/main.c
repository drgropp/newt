/*
Newt
A small experimental scripting language built in C.
Created by drgropp.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define NEWT_VERSION "0.1.0"
#define NEWT_MAX_WHILE_ITERATIONS 100000
#define NEWT_MAX_CALL_DEPTH 256
#define NEWT_MAX_FUNCTION_PARAMETERS 16

typedef enum {
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_UNKNOWN,
    TOKEN_ERROR,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_STRING,

    TOKEN_VAL,
    TOKEN_MUT,
    TOKEN_FN,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_END,
    TOKEN_WHILE,
    TOKEN_BREAK,
    TOKEN_PRINT,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA,
    TOKEN_DOT
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
    int column;
    const char *error_message;
} Token;

typedef struct {
    const char *source;
    const char *start;
    const char *current;
    int line;
    int column;
    int start_line;
    int start_column;
} Lexer;

static void print_help(void) {
    printf("Newt %s\n", NEWT_VERSION);
    printf("\n");
    printf("Usage:\n");
    printf("  newt.exe --run <file.nt>     Run a Newt program\n");
    printf("  newt.exe --tokens <file.nt>  Print lexer tokens\n");
    printf("  newt.exe --parse <file.nt>   Print the parse tree\n");
    printf("  newt.exe --version           Print the Newt version\n");
    printf("  newt.exe --help              Show this help\n");
    printf("\n");
    printf("Example:\n");
    printf("  newt.exe --run examples/hello.nt\n");
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        printf("error: could not open %s\n", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        printf("error: could not read %s\n", path);
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        printf("error: could not read %s\n", path);
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *source = malloc((size_t)file_size + 1);
    if (source == NULL) {
        printf("error: not enough memory to read %s\n", path);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(source, 1, (size_t)file_size, file);
    source[bytes_read] = '\0';

    if (bytes_read != (size_t)file_size && ferror(file)) {
        printf("error: could not read %s\n", path);
        free(source);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return source;
}

static void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->start_line = 1;
    lexer->start_column = 1;
}

static int lexer_is_at_end(Lexer *lexer) {
    return *lexer->current == '\0';
}

static char lexer_advance(Lexer *lexer) {
    char ch = *lexer->current;
    lexer->current++;

    if (ch == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }

    return ch;
}

static char lexer_peek(Lexer *lexer) {
    return *lexer->current;
}

static char lexer_peek_next(Lexer *lexer) {
    if (lexer_is_at_end(lexer)) {
        return '\0';
    }

    return lexer->current[1];
}

static int lexer_match(Lexer *lexer, char expected) {
    if (lexer_is_at_end(lexer)) {
        return 0;
    }

    if (*lexer->current != expected) {
        return 0;
    }

    lexer->current++;
    return 1;
}

static Token make_token(Lexer *lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->start_line;
    token.column = lexer->start_column;
    token.error_message = NULL;
    return token;
}

static Token make_error_token(Lexer *lexer, const char *message) {
    Token token = make_token(lexer, TOKEN_ERROR);
    token.error_message = message;
    return token;
}

static void lexer_mark_start(Lexer *lexer) {
    lexer->start = lexer->current;
    lexer->start_line = lexer->line;
    lexer->start_column = lexer->column;
}

static int is_ident_start(char ch) {
    return isalpha((unsigned char)ch) || ch == '_';
}

static int is_ident_char(char ch) {
    return isalnum((unsigned char)ch) || ch == '_';
}

static int token_text_equals(Token token, const char *text) {
    int length = (int)strlen(text);
    return token.length == length && strncmp(token.start, text, (size_t)length) == 0;
}

static TokenType keyword_type(Token token) {
    if (token_text_equals(token, "val")) {
        return TOKEN_VAL;
    }
    if (token_text_equals(token, "mut")) {
        return TOKEN_MUT;
    }
    if (token_text_equals(token, "fn")) {
        return TOKEN_FN;
    }
    if (token_text_equals(token, "return")) {
        return TOKEN_RETURN;
    }
    if (token_text_equals(token, "if")) {
        return TOKEN_IF;
    }
    if (token_text_equals(token, "else")) {
        return TOKEN_ELSE;
    }
    if (token_text_equals(token, "end")) {
        return TOKEN_END;
    }
    if (token_text_equals(token, "while")) {
        return TOKEN_WHILE;
    }
    if (token_text_equals(token, "break")) {
        return TOKEN_BREAK;
    }
    if (token_text_equals(token, "print")) {
        return TOKEN_PRINT;
    }
    if (token_text_equals(token, "true")) {
        return TOKEN_TRUE;
    }
    if (token_text_equals(token, "false")) {
        return TOKEN_FALSE;
    }
    if (token_text_equals(token, "and")) {
        return TOKEN_AND;
    }
    if (token_text_equals(token, "or")) {
        return TOKEN_OR;
    }
    if (token_text_equals(token, "not")) {
        return TOKEN_NOT;
    }

    return TOKEN_IDENT;
}

static Token lex_identifier(Lexer *lexer) {
    while (is_ident_char(lexer_peek(lexer))) {
        lexer_advance(lexer);
    }

    Token token = make_token(lexer, TOKEN_IDENT);
    token.type = keyword_type(token);
    return token;
}

static Token lex_number(Lexer *lexer) {
    while (isdigit((unsigned char)lexer_peek(lexer))) {
        lexer_advance(lexer);
    }

    if (lexer_peek(lexer) == '.' && isdigit((unsigned char)lexer_peek_next(lexer))) {
        lexer_advance(lexer);

        while (isdigit((unsigned char)lexer_peek(lexer))) {
            lexer_advance(lexer);
        }
    }

    return make_token(lexer, TOKEN_NUMBER);
}

static Token lex_string(Lexer *lexer) {
    while (lexer_peek(lexer) != '"' &&
           lexer_peek(lexer) != '\n' &&
           !lexer_is_at_end(lexer)) {
        lexer_advance(lexer);
    }

    if (lexer_peek(lexer) == '"') {
        lexer_advance(lexer);
        return make_token(lexer, TOKEN_STRING);
    }

    return make_error_token(lexer, "unterminated string");
}

static void lexer_skip_comment(Lexer *lexer) {
    while (lexer_peek(lexer) != '\n' && !lexer_is_at_end(lexer)) {
        lexer_advance(lexer);
    }
}

static Token next_token(Lexer *lexer) {
    for (;;) {
        lexer_mark_start(lexer);

        char ch = lexer_peek(lexer);

        if (ch == ' ' || ch == '\t' || ch == '\r') {
            lexer_advance(lexer);
            continue;
        }

        if (ch == '#') {
            lexer_skip_comment(lexer);
            continue;
        }

        break;
    }

    lexer_mark_start(lexer);

    if (lexer_is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF);
    }

    char ch = lexer_advance(lexer);

    if (is_ident_start(ch)) {
        return lex_identifier(lexer);
    }

    if (isdigit((unsigned char)ch)) {
        return lex_number(lexer);
    }

    switch (ch) {
        case '\n':
            return make_token(lexer, TOKEN_NEWLINE);
        case '"':
            return lex_string(lexer);
        case '+':
            return make_token(lexer, TOKEN_PLUS);
        case '-':
            return make_token(lexer, TOKEN_MINUS);
        case '*':
            return make_token(lexer, TOKEN_STAR);
        case '/':
            return make_token(lexer, TOKEN_SLASH);
        case '=':
            return make_token(lexer, lexer_match(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '!':
            if (lexer_match(lexer, '=')) {
                return make_token(lexer, TOKEN_BANG_EQUAL);
            }
            break;
        case '<':
            return make_token(lexer, lexer_match(lexer, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return make_token(lexer, lexer_match(lexer, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '(':
            return make_token(lexer, TOKEN_LEFT_PAREN);
        case ')':
            return make_token(lexer, TOKEN_RIGHT_PAREN);
        case '[':
            return make_token(lexer, TOKEN_LEFT_BRACKET);
        case ']':
            return make_token(lexer, TOKEN_RIGHT_BRACKET);
        case ',':
            return make_token(lexer, TOKEN_COMMA);
        case '.':
            return make_token(lexer, TOKEN_DOT);
        default:
            break;
    }

    return make_error_token(lexer, "unknown character");
}

static const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF:
            return "EOF";
        case TOKEN_NEWLINE:
            return "NEWLINE";
        case TOKEN_UNKNOWN:
            return "UNKNOWN";
        case TOKEN_ERROR:
            return "ERROR";
        case TOKEN_IDENT:
            return "IDENT";
        case TOKEN_NUMBER:
            return "NUMBER";
        case TOKEN_STRING:
            return "STRING";
        case TOKEN_VAL:
            return "VAL";
        case TOKEN_MUT:
            return "MUT";
        case TOKEN_FN:
            return "FN";
        case TOKEN_RETURN:
            return "RETURN";
        case TOKEN_IF:
            return "IF";
        case TOKEN_ELSE:
            return "ELSE";
        case TOKEN_END:
            return "END";
        case TOKEN_WHILE:
            return "WHILE";
        case TOKEN_BREAK:
            return "BREAK";
        case TOKEN_PRINT:
            return "PRINT";
        case TOKEN_TRUE:
            return "TRUE";
        case TOKEN_FALSE:
            return "FALSE";
        case TOKEN_AND:
            return "AND";
        case TOKEN_OR:
            return "OR";
        case TOKEN_NOT:
            return "NOT";
        case TOKEN_PLUS:
            return "PLUS";
        case TOKEN_MINUS:
            return "MINUS";
        case TOKEN_STAR:
            return "STAR";
        case TOKEN_SLASH:
            return "SLASH";
        case TOKEN_EQUAL:
            return "EQUAL";
        case TOKEN_EQUAL_EQUAL:
            return "EQUAL_EQUAL";
        case TOKEN_BANG_EQUAL:
            return "BANG_EQUAL";
        case TOKEN_LESS:
            return "LESS";
        case TOKEN_LESS_EQUAL:
            return "LESS_EQUAL";
        case TOKEN_GREATER:
            return "GREATER";
        case TOKEN_GREATER_EQUAL:
            return "GREATER_EQUAL";
        case TOKEN_LEFT_PAREN:
            return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN:
            return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACKET:
            return "LEFT_BRACKET";
        case TOKEN_RIGHT_BRACKET:
            return "RIGHT_BRACKET";
        case TOKEN_COMMA:
            return "COMMA";
        case TOKEN_DOT:
            return "DOT";
    }

    return "UNKNOWN";
}

static void print_token(Token token) {
    if (token.type == TOKEN_NEWLINE || token.type == TOKEN_EOF) {
        printf("%d:%d %s\n", token.line, token.column, token_type_name(token.type));
        return;
    }

    printf("%d:%d %s %.*s\n",
           token.line,
           token.column,
           token_type_name(token.type),
           token.length,
           token.start);
}

static void print_lexer_error(const char *path, Token token) {
    if (token.length == 1 && token.error_message != NULL &&
        strcmp(token.error_message, "unknown character") == 0) {
        printf("%s:%d:%d: lexer error: %s '%c'\n",
               path,
               token.line,
               token.column,
               token.error_message,
               token.start[0]);
        return;
    }

    printf("%s:%d:%d: lexer error: %s\n",
           path,
           token.line,
           token.column,
           token.error_message);
}

static void print_tokens(const char *path, const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    for (;;) {
        Token token = next_token(&lexer);

        if (token.type == TOKEN_ERROR) {
            print_lexer_error(path, token);
            break;
        }

        print_token(token);

        if (token.type == TOKEN_EOF) {
            break;
        }
    }
}

typedef enum {
    EXPR_NUMBER,
    EXPR_IDENT,
    EXPR_STRING,
    EXPR_TRUE,
    EXPR_FALSE,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_CALL,
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
    STMT_ASSIGN,
    STMT_IF,
    STMT_WHILE,
    STMT_PRINT
} StmtKind;

typedef struct {
    StmtKind kind;
    Token name;
    Expr *expression;
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
    int top_level_statements[256];
    int top_level_statement_count;
} Parser;

static void parser_init(Parser *parser, const char *path, const char *source) {
    lexer_init(&parser->lexer, source);
    parser->path = path;
    parser->had_error = 0;
    parser->expression_count = 0;
    parser->statement_count = 0;
    parser->parameter_count = 0;
    parser->call_argument_count = 0;
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

    printf("%s:%d:%d: parse error: %s\n",
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
    if (parser_match(parser, TOKEN_LEFT_PAREN)) {
        Expr *expr = parse_expression(parser);
        parser_consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after expression");
        return expr;
    }

    parser_error(parser, parser->current, "expected expression");
    return NULL;
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

    return parse_primary(parser);
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

    if (parser_match(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }

    if (parser_match(parser, TOKEN_WHILE)) {
        return parse_while_statement(parser);
    }

    if (parser_match(parser, TOKEN_PRINT)) {
        return parse_print_statement(parser);
    }

    if (parser_match(parser, TOKEN_IDENT)) {
        Token name = parser->previous;

        if (parser->current.type == TOKEN_LEFT_PAREN) {
            return parse_function_call(parser, name);
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
        case STMT_ASSIGN:
            print_indent(indent);
            printf("ASSIGN name=%.*s\n", stmt->name.length, stmt->name.start);
            print_expression_tree(parser, stmt->expression, indent + 2);
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
        case STMT_PRINT:
            print_indent(indent);
            printf("PRINT\n");
            print_expression_tree(parser, stmt->expression, indent + 2);
            break;
    }
}

static int parse_program(Parser *parser) {
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

static void print_parse_tree(const char *path, const char *source) {
    Parser parser;
    int i;

    parser_init(&parser, path, source);
    if (!parse_program(&parser)) {
        return;
    }

    printf("PROGRAM\n");
    for (i = 0; i < parser.top_level_statement_count; i++) {
        int stmt_index = parser.top_level_statements[i];
        print_statement_tree(&parser, &parser.statements[stmt_index], 2);
    }
    printf("EOF\n");
}

typedef enum {
    VALUE_NUMBER,
    VALUE_STRING,
    VALUE_BOOL
} ValueType;

typedef struct {
    ValueType type;
    double number;
    int boolean;
    Token string;
} Value;

typedef struct {
    Token name;
    Value value;
    int is_mutable;
    int scope_depth;
} Variable;

typedef struct {
    Token name;
    Stmt *declaration;
} Function;

typedef struct {
    Parser *parser;
    Variable variables[256];
    int variable_count;
    Function functions[256];
    int function_count;
    int call_depth;
    int scope_depth;
    int is_returning;
    Value return_value;
    int had_error;
} Interpreter;

static Value make_number_value(double number) {
    Value value;

    value.type = VALUE_NUMBER;
    value.number = number;
    value.boolean = 0;
    value.string.start = NULL;
    value.string.length = 0;
    value.string.line = 0;
    value.string.column = 0;
    value.string.error_message = NULL;
    return value;
}

static Value make_string_value(Token string) {
    Value value;

    value.type = VALUE_STRING;
    value.number = 0;
    value.boolean = 0;
    value.string = string;
    return value;
}

static Value make_bool_value(int boolean) {
    Value value;

    value.type = VALUE_BOOL;
    value.number = 0;
    value.boolean = boolean;
    value.string.start = NULL;
    value.string.length = 0;
    value.string.line = 0;
    value.string.column = 0;
    value.string.error_message = NULL;
    return value;
}

static int string_values_equal(Value left, Value right) {
    int left_length = left.string.length - 2;
    int right_length = right.string.length - 2;

    if (left_length < 0) {
        left_length = 0;
    }
    if (right_length < 0) {
        right_length = 0;
    }

    return left_length == right_length &&
           strncmp(left.string.start + 1, right.string.start + 1, (size_t)left_length) == 0;
}

static int value_is_truthy(Value value) {
    if (value.type == VALUE_BOOL) {
        return value.boolean;
    }

    if (value.type == VALUE_STRING) {
        return value.string.length > 2;
    }

    return value.number != 0;
}

static int token_names_match(Token left, Token right) {
    return left.length == right.length &&
           strncmp(left.start, right.start, (size_t)left.length) == 0;
}

static const char *value_type_name(ValueType type) {
    switch (type) {
        case VALUE_NUMBER:
            return "number";
        case VALUE_STRING:
            return "string";
        case VALUE_BOOL:
            return "boolean";
    }

    return "unknown value";
}

static void runtime_error(Interpreter *interpreter, Token token, const char *message) {
    if (interpreter->had_error) {
        return;
    }

    printf("%s:%d:%d: runtime error: %s\n",
           interpreter->parser->path,
           token.line,
           token.column,
           message);
    interpreter->had_error = 1;
}

static void runtime_type_error(Interpreter *interpreter,
                               Token token,
                               const char *subject,
                               const char *expected,
                               Value actual) {
    char message[160];

    snprintf(message,
             sizeof(message),
             "%s must be %s, got %s",
             subject,
             expected,
             value_type_name(actual.type));
    runtime_error(interpreter, token, message);
}

static void runtime_name_error(Interpreter *interpreter, Token token, const char *message) {
    char detailed_message[160];

    snprintf(detailed_message,
             sizeof(detailed_message),
             "%s '%.*s'",
             message,
             token.length,
             token.start);
    runtime_error(interpreter, token, detailed_message);
}

static void runtime_function_arity_error(Interpreter *interpreter,
                                         Token name,
                                         int expected,
                                         int actual) {
    char message[160];

    snprintf(message,
             sizeof(message),
             "function '%.*s' expects %d argument%s, got %d",
             name.length,
             name.start,
             expected,
             expected == 1 ? "" : "s",
             actual);
    runtime_error(interpreter, name, message);
}

static Variable *find_variable(Interpreter *interpreter, Token name) {
    int i;

    for (i = interpreter->variable_count - 1; i >= 0; i--) {
        if (token_names_match(interpreter->variables[i].name, name)) {
            return &interpreter->variables[i];
        }
    }

    return NULL;
}

static Function *find_function(Interpreter *interpreter, Token name) {
    int i;

    for (i = 0; i < interpreter->function_count; i++) {
        if (token_names_match(interpreter->functions[i].name, name)) {
            return &interpreter->functions[i];
        }
    }

    return NULL;
}

static int define_function(Interpreter *interpreter, Stmt *declaration) {
    if (find_function(interpreter, declaration->name) != NULL) {
        runtime_name_error(interpreter, declaration->name, "function already defined");
        return 0;
    }

    if (interpreter->function_count >= 256) {
        runtime_error(interpreter, declaration->name, "too many functions");
        return 0;
    }

    interpreter->functions[interpreter->function_count].name = declaration->name;
    interpreter->functions[interpreter->function_count].declaration = declaration;
    interpreter->function_count++;
    return 1;
}

static int call_function(Interpreter *interpreter,
                         Expr *call,
                         int requires_value,
                         Value *value);

static int read_number_token(Interpreter *interpreter, Token token, double *number) {
    char text[64];

    if (token.length >= (int)sizeof(text)) {
        runtime_error(interpreter, token, "number is too long");
        return 0;
    }

    memcpy(text, token.start, (size_t)token.length);
    text[token.length] = '\0';
    *number = strtod(text, NULL);
    return 1;
}

static int read_input_number(Interpreter *interpreter, Token prompt, double *number) {
    if (prompt.length >= 2) {
        printf("%.*s", prompt.length - 2, prompt.start + 1);
    }
    fflush(stdout);

    if (scanf("%lf", number) != 1) {
        runtime_error(interpreter, prompt, "expected number from input_number");
        return 0;
    }

    return 1;
}

static int eval_expression(Interpreter *interpreter, Expr *expr, Value *value) {
    Value left;
    Value right;
    double number;

    if (expr == NULL || interpreter->had_error) {
        return 0;
    }

    switch (expr->kind) {
        case EXPR_NUMBER:
            if (!read_number_token(interpreter, expr->token, &number)) {
                return 0;
            }
            *value = make_number_value(number);
            return 1;
        case EXPR_INPUT_NUMBER:
            if (!read_input_number(interpreter, expr->token, &number)) {
                return 0;
            }
            *value = make_number_value(number);
            return 1;
        case EXPR_SQRT:
            if (expr->argument_count != 1) {
                runtime_error(interpreter, expr->token, "sqrt expects exactly one argument");
                return 0;
            }
            if (!eval_expression(interpreter, expr->left, &left)) {
                return 0;
            }
            if (left.type != VALUE_NUMBER) {
                runtime_type_error(interpreter,
                                   expr->token,
                                   "sqrt argument",
                                   "a number",
                                   left);
                return 0;
            }
            if (left.number < 0) {
                runtime_error(interpreter, expr->token, "sqrt argument cannot be negative");
                return 0;
            }
            *value = make_number_value(sqrt(left.number));
            return 1;
        case EXPR_IDENT: {
            Variable *variable = find_variable(interpreter, expr->token);
            if (variable == NULL) {
                runtime_name_error(interpreter, expr->token, "undefined variable");
                return 0;
            }
            *value = variable->value;
            return 1;
        }
        case EXPR_STRING:
            *value = make_string_value(expr->token);
            return 1;
        case EXPR_CALL:
            return call_function(interpreter, expr, 1, value);
        case EXPR_UNARY:
            if (!eval_expression(interpreter, expr->left, &left)) {
                return 0;
            }
            if (expr->token.type == TOKEN_NOT) {
                if (left.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "not operand",
                                       "a boolean",
                                       left);
                    return 0;
                }
                *value = make_bool_value(!left.boolean);
                return 1;
            }
            if (left.type != VALUE_NUMBER) {
                runtime_type_error(interpreter,
                                   expr->token,
                                   "unary minus operand",
                                   "a number",
                                   left);
                return 0;
            }
            *value = make_number_value(-left.number);
            return 1;
        case EXPR_BINARY:
            if (!eval_expression(interpreter, expr->left, &left)) {
                return 0;
            }
            if (expr->token.type == TOKEN_AND) {
                if (left.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "left operand of 'and'",
                                       "a boolean",
                                       left);
                    return 0;
                }
                if (!left.boolean) {
                    *value = make_bool_value(0);
                    return 1;
                }
                if (!eval_expression(interpreter, expr->right, &right)) {
                    return 0;
                }
                if (right.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "right operand of 'and'",
                                       "a boolean",
                                       right);
                    return 0;
                }
                *value = make_bool_value(right.boolean);
                return 1;
            }
            if (expr->token.type == TOKEN_OR) {
                if (left.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "left operand of 'or'",
                                       "a boolean",
                                       left);
                    return 0;
                }
                if (left.boolean) {
                    *value = make_bool_value(1);
                    return 1;
                }
                if (!eval_expression(interpreter, expr->right, &right)) {
                    return 0;
                }
                if (right.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "right operand of 'or'",
                                       "a boolean",
                                       right);
                    return 0;
                }
                *value = make_bool_value(right.boolean);
                return 1;
            }
            if (!eval_expression(interpreter, expr->right, &right)) {
                return 0;
            }

            switch (expr->token.type) {
                case TOKEN_EQUAL_EQUAL:
                    if (left.type != right.type) {
                        *value = make_bool_value(0);
                        return 1;
                    }
                    if (left.type == VALUE_STRING) {
                        *value = make_bool_value(string_values_equal(left, right));
                        return 1;
                    }
                    if (left.type == VALUE_BOOL) {
                        *value = make_bool_value(left.boolean == right.boolean);
                        return 1;
                    }
                    *value = make_bool_value(left.number == right.number);
                    return 1;
                case TOKEN_BANG_EQUAL:
                    if (left.type != right.type) {
                        *value = make_bool_value(1);
                        return 1;
                    }
                    if (left.type == VALUE_STRING) {
                        *value = make_bool_value(!string_values_equal(left, right));
                        return 1;
                    }
                    if (left.type == VALUE_BOOL) {
                        *value = make_bool_value(left.boolean != right.boolean);
                        return 1;
                    }
                    *value = make_bool_value(left.number != right.number);
                    return 1;
                default:
                    break;
            }

            if (left.type != VALUE_NUMBER || right.type != VALUE_NUMBER) {
                if (left.type == VALUE_BOOL || right.type == VALUE_BOOL) {
                    runtime_error(interpreter, expr->token, "numeric operators require numbers, not booleans");
                    return 0;
                }

                runtime_error(interpreter, expr->token, "numeric operators require numbers");
                return 0;
            }

            switch (expr->token.type) {
                case TOKEN_PLUS:
                    *value = make_number_value(left.number + right.number);
                    return 1;
                case TOKEN_MINUS:
                    *value = make_number_value(left.number - right.number);
                    return 1;
                case TOKEN_STAR:
                    *value = make_number_value(left.number * right.number);
                    return 1;
                case TOKEN_SLASH:
                    if (right.number == 0) {
                        runtime_error(interpreter, expr->token, "division by zero");
                        return 0;
                    }
                    *value = make_number_value(left.number / right.number);
                    return 1;
                case TOKEN_LESS:
                    *value = make_bool_value(left.number < right.number);
                    return 1;
                case TOKEN_LESS_EQUAL:
                    *value = make_bool_value(left.number <= right.number);
                    return 1;
                case TOKEN_GREATER:
                    *value = make_bool_value(left.number > right.number);
                    return 1;
                case TOKEN_GREATER_EQUAL:
                    *value = make_bool_value(left.number >= right.number);
                    return 1;
                default:
                    runtime_error(interpreter, expr->token, "unsupported runtime operator");
                    return 0;
            }
        case EXPR_TRUE:
            *value = make_bool_value(1);
            return 1;
        case EXPR_FALSE:
            *value = make_bool_value(0);
            return 1;
    }

    return 0;
}

static int define_variable(Interpreter *interpreter, Token name, Value value, int is_mutable) {
    int i;

    for (i = interpreter->variable_count - 1; i >= 0; i--) {
        Variable *existing = &interpreter->variables[i];

        if (existing->scope_depth < interpreter->scope_depth) {
            break;
        }
        if (token_names_match(existing->name, name)) {
            runtime_name_error(interpreter, name, "variable already defined in this scope");
            return 0;
        }
    }

    if (interpreter->variable_count >= 256) {
        runtime_error(interpreter, name, "too many variables");
        return 0;
    }

    interpreter->variables[interpreter->variable_count].name = name;
    interpreter->variables[interpreter->variable_count].value = value;
    interpreter->variables[interpreter->variable_count].is_mutable = is_mutable;
    interpreter->variables[interpreter->variable_count].scope_depth = interpreter->scope_depth;
    interpreter->variable_count++;
    return 1;
}

static void print_number(double number) {
    long long whole = (long long)number;

    if (number == (double)whole) {
        printf("%lld\n", whole);
    } else {
        printf("%g\n", number);
    }
}

static void print_string_token(Token token) {
    if (token.length >= 2) {
        printf("%.*s\n", token.length - 2, token.start + 1);
    } else {
        printf("\n");
    }
}

static void print_value(Value value) {
    if (value.type == VALUE_STRING) {
        print_string_token(value.string);
        return;
    }

    if (value.type == VALUE_BOOL) {
        printf("%s\n", value.boolean ? "true" : "false");
        return;
    }

    print_number(value.number);
}

static int print_runtime_expression(Interpreter *interpreter, Expr *expr) {
    Value value;

    if (!eval_expression(interpreter, expr, &value)) {
        return 0;
    }

    print_value(value);
    return 1;
}

static int execute_statement(Interpreter *interpreter, Stmt *stmt);

static int execute_statement_list(Interpreter *interpreter, int statements[256], int count) {
    int i;

    for (i = 0;
         i < count && !interpreter->had_error && !interpreter->is_returning;
         i++) {
        int stmt_index = statements[i];
        if (!execute_statement(interpreter, &interpreter->parser->statements[stmt_index])) {
            return 0;
        }
    }

    return 1;
}

static int call_function(Interpreter *interpreter,
                         Expr *call,
                         int requires_value,
                         Value *value) {
    Function *function = find_function(interpreter, call->token);
    Value argument_values[NEWT_MAX_FUNCTION_PARAMETERS];
    Value returned_value = make_number_value(0);
    Value previous_return_value = interpreter->return_value;
    int previous_returning = interpreter->is_returning;
    int variable_start = interpreter->variable_count;
    int succeeded = 1;
    int did_return;
    int i;

    if (function == NULL) {
        runtime_name_error(interpreter, call->token, "undefined function");
        return 0;
    }
    if (call->argument_count != function->declaration->parameter_count) {
        runtime_function_arity_error(interpreter,
                                     call->token,
                                     function->declaration->parameter_count,
                                     call->argument_count);
        return 0;
    }
    if (interpreter->call_depth >= NEWT_MAX_CALL_DEPTH) {
        runtime_name_error(interpreter,
                           call->token,
                           "maximum call depth exceeded in function");
        return 0;
    }

    for (i = 0; i < call->argument_count; i++) {
        Expr *argument = interpreter->parser->call_arguments[call->argument_start + i];
        if (!eval_expression(interpreter, argument, &argument_values[i])) {
            return 0;
        }
    }

    interpreter->call_depth++;
    interpreter->scope_depth++;
    interpreter->is_returning = 0;

    for (i = 0; i < function->declaration->parameter_count; i++) {
        Token parameter = interpreter->parser->parameters[
            function->declaration->parameter_start + i];
        if (!define_variable(interpreter, parameter, argument_values[i], 0)) {
            succeeded = 0;
            break;
        }
    }

    if (succeeded) {
        succeeded = execute_statement_list(interpreter,
                                           function->declaration->body_statements,
                                           function->declaration->body_count);
    }

    did_return = interpreter->is_returning;
    if (did_return) {
        returned_value = interpreter->return_value;
    }

    interpreter->variable_count = variable_start;
    interpreter->scope_depth--;
    interpreter->call_depth--;
    interpreter->is_returning = previous_returning;
    interpreter->return_value = previous_return_value;

    if (!succeeded) {
        return 0;
    }
    if (requires_value && !did_return) {
        char message[160];

        snprintf(message,
                 sizeof(message),
                 "function '%.*s' did not return a value",
                 call->token.length,
                 call->token.start);
        runtime_error(interpreter, call->token, message);
        return 0;
    }
    if (value != NULL && did_return) {
        *value = returned_value;
    }

    return 1;
}

static int execute_statement(Interpreter *interpreter, Stmt *stmt) {
    Value value;

    if (interpreter->had_error) {
        return 0;
    }

    switch (stmt->kind) {
        case STMT_FN_DECL:
            return define_function(interpreter, stmt);
        case STMT_CALL:
            return call_function(interpreter, stmt->expression, 0, NULL);
        case STMT_RETURN:
            if (interpreter->call_depth == 0) {
                runtime_error(interpreter,
                              stmt->name,
                              "return can only be used inside a function");
                return 0;
            }
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            interpreter->return_value = value;
            interpreter->is_returning = 1;
            return 1;
        case STMT_VAL_DECL:
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            return define_variable(interpreter, stmt->name, value, 0);
        case STMT_MUT_DECL:
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            return define_variable(interpreter, stmt->name, value, 1);
        case STMT_ASSIGN: {
            Variable *variable = find_variable(interpreter, stmt->name);
            if (variable == NULL) {
                runtime_name_error(interpreter, stmt->name, "undefined variable");
                return 0;
            }
            if (!variable->is_mutable) {
                runtime_name_error(interpreter, stmt->name, "cannot assign to immutable val");
                return 0;
            }
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            variable->value = value;
            return 1;
        }

        case STMT_PRINT:
            return print_runtime_expression(interpreter, stmt->expression);
        case STMT_IF: {
            int *branch;
            int branch_count;

            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }

            if (value_is_truthy(value)) {
                branch = stmt->then_statements;
                branch_count = stmt->then_count;
            } else if (stmt->has_else) {
                branch = stmt->else_statements;
                branch_count = stmt->else_count;
            } else {
                return 1;
            }

            return execute_statement_list(interpreter, branch, branch_count);
        }
        case STMT_WHILE: {
            int iterations = 0;

            while (!interpreter->had_error) {
                if (iterations >= NEWT_MAX_WHILE_ITERATIONS) {
                    runtime_error(interpreter, stmt->name, "while loop exceeded max iteration limit");
                    return 0;
                }

                if (!eval_expression(interpreter, stmt->expression, &value)) {
                    return 0;
                }

                if (!value_is_truthy(value)) {
                    return 1;
                }

                iterations++;
                if (!execute_statement_list(interpreter, stmt->body_statements, stmt->body_count)) {
                    return 0;
                }
                if (interpreter->is_returning) {
                    return 1;
                }
            }

            return 0;
        }
    }

    return 0;
}
static int run_program(const char *path, const char *source) {
    Parser parser;
    Interpreter interpreter;
    int i;

    parser_init(&parser, path, source);
    if (!parse_program(&parser)) {
        return 0;
    }

    interpreter.parser = &parser;
    interpreter.variable_count = 0;
    interpreter.function_count = 0;
    interpreter.call_depth = 0;
    interpreter.scope_depth = 0;
    interpreter.is_returning = 0;
    interpreter.return_value = make_number_value(0);
    interpreter.had_error = 0;

    for (i = 0; i < parser.top_level_statement_count && !interpreter.had_error; i++) {
        int stmt_index = parser.top_level_statements[i];
        execute_statement(&interpreter, &parser.statements[stmt_index]);
    }

    return !interpreter.had_error;
}

static void print_source(const char *source) {
    fputs(source, stdout);

    if (source[0] != '\0') {
        size_t length = strlen(source);
        if (source[length - 1] != '\n') {
            putchar('\n');
        }
    }
}

int main(int argc, char **argv) {
    int print_token_mode = 0;
    int parse_mode = 0;
    int run_mode = 0;
    int success = 1;
    const char *path = NULL;

    if (argc < 2) {
        printf("error: missing input file\n");
        printf("usage: newt <file.nt>\n");
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("newt %s\n", NEWT_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "--about") == 0) {
        printf("Newt is a small experimental scripting language built in C.\n");
        printf("Created by drgropp.\n");
        return 0;
    }

    if (strcmp(argv[1], "--tokens") == 0) {
        if (argc < 3) {
            printf("error: missing input file\n");
            printf("usage: newt --tokens <file.nt>\n");
            return 1;
        }

        print_token_mode = 1;
        path = argv[2];
    } else if (strcmp(argv[1], "--parse") == 0) {
        if (argc < 3) {
            printf("error: missing input file\n");
            printf("usage: newt --parse <file.nt>\n");
            return 1;
        }

        parse_mode = 1;
        path = argv[2];
    } else if (strcmp(argv[1], "--run") == 0) {
        if (argc < 3) {
            printf("error: missing input file\n");
            printf("usage: newt --run <file.nt>\n");
            return 1;
        }

        run_mode = 1;
        path = argv[2];
    } else {
        path = argv[1];
    }

    char *source = read_file(path);
    if (source == NULL) {
        return 1;
    }

    if (print_token_mode) {
        print_tokens(path, source);
    } else if (parse_mode) {
        print_parse_tree(path, source);
    } else if (run_mode) {
        success = run_program(path, source);
    } else {
        print_source(source);
    }

    free(source);

    return success ? 0 : 1;
}
