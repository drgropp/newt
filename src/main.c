/*
Newt
A small experimental scripting language built in C.
Created by drgropp.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NEWT_VERSION "0.1.0-dev"

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
    printf("  newt <file.nt>\n");
    printf("  newt --tokens <file.nt>\n");
    printf("  newt --parse <file.nt>\n");
    printf("  newt --help\n");
    printf("  newt --version\n");
    printf("  newt --about\n");
    printf("\n");
    printf("Phase 2: reads .nt source files, can print tokens, and can debug-print a parse tree.\n");
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

static Token next_token(Lexer *lexer) {
    for (;;) {
        lexer_mark_start(lexer);

        char ch = lexer_peek(lexer);

        if (ch == ' ' || ch == '\t' || ch == '\r') {
            lexer_advance(lexer);
            continue;
        }

        if (ch == '#') {
            while (lexer_peek(lexer) != '\n' && !lexer_is_at_end(lexer)) {
                lexer_advance(lexer);
            }
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
    EXPR_BINARY
} ExprKind;

typedef struct Expr Expr;

typedef enum {
    STMT_VAL_DECL,
    STMT_PRINT
} StmtKind;

typedef struct {
    StmtKind kind;
    Token name;
    Expr *expression;
} Stmt;

struct Expr {
    ExprKind kind;
    Token token;
    Expr *left;
    Expr *right;
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
} Parser;

static void parser_init(Parser *parser, const char *path, const char *source) {
    lexer_init(&parser->lexer, source);
    parser->path = path;
    parser->had_error = 0;
    parser->expression_count = 0;
    parser->statement_count = 0;
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
    return stmt;
}

static Expr *parse_expression(Parser *parser);

static Expr *parse_primary(Parser *parser) {
    if (parser_match(parser, TOKEN_NUMBER)) {
        return new_expr(parser, EXPR_NUMBER, parser->previous);
    }
    if (parser_match(parser, TOKEN_IDENT)) {
        return new_expr(parser, EXPR_IDENT, parser->previous);
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

static Expr *parse_factor(Parser *parser) {
    Expr *left = parse_primary(parser);

    while (!parser->had_error &&
           (parser->current.type == TOKEN_STAR || parser->current.type == TOKEN_SLASH)) {
        Token operator_token;
        Expr *binary;
        Expr *right;

        parser_advance(parser);
        operator_token = parser->previous;
        right = parse_primary(parser);
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

static Expr *parse_expression(Parser *parser) {
    return parse_term(parser);
}

static void print_indent(int spaces) {
    int i;

    for (i = 0; i < spaces; i++) {
        putchar(' ');
    }
}

static void print_expression_tree(Expr *expr, int indent) {
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
        case EXPR_BINARY:
            printf("BINARY %.*s\n", expr->token.length, expr->token.start);
            print_expression_tree(expr->left, indent + 2);
            print_expression_tree(expr->right, indent + 2);
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

static void parse_val_declaration(Parser *parser) {
    Token name;
    Expr *initializer;

    parser_consume(parser, TOKEN_IDENT, "expected identifier after val");
    if (parser->had_error) {
        return;
    }
    name = parser->previous;

    parser_consume(parser, TOKEN_EQUAL, "expected '=' after variable name");
    if (parser->had_error) {
        return;
    }

    initializer = parse_expression(parser);
    if (parser->had_error) {
        return;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return;
    }

    new_stmt(parser, STMT_VAL_DECL, name, initializer);
}

static void parse_print_statement(Parser *parser) {
    Expr *expr = parse_expression(parser);

    if (parser->had_error) {
        return;
    }

    parser_consume_statement_end(parser);
    if (parser->had_error) {
        return;
    }

    new_stmt(parser, STMT_PRINT, parser->previous, expr);
}

static void parse_statement(Parser *parser) {
    if (parser_match(parser, TOKEN_VAL)) {
        parse_val_declaration(parser);
        return;
    }

    if (parser_match(parser, TOKEN_PRINT)) {
        parse_print_statement(parser);
        return;
    }

    parser_error(parser, parser->current, "expected statement");
}

static void print_statement_tree(Stmt *stmt) {
    switch (stmt->kind) {
        case STMT_VAL_DECL:
            printf("  VAL_DECL name=%.*s\n", stmt->name.length, stmt->name.start);
            print_expression_tree(stmt->expression, 4);
            break;
        case STMT_PRINT:
            printf("  PRINT\n");
            print_expression_tree(stmt->expression, 4);
            break;
    }
}

static void print_parse_tree(const char *path, const char *source) {
    Parser parser;

    parser_init(&parser, path, source);
    parser_advance(&parser);

    if (parser.had_error) {
        return;
    }

    parser_skip_newlines(&parser);

    while (!parser.had_error && parser.current.type != TOKEN_EOF) {
        parse_statement(&parser);
        parser_skip_newlines(&parser);
    }

    if (!parser.had_error) {
        int i;

        printf("PROGRAM\n");
        for (i = 0; i < parser.statement_count; i++) {
            print_statement_tree(&parser.statements[i]);
        }
        printf("EOF\n");
    }
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
        printf("Newt %s\n", NEWT_VERSION);
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
    } else {
        print_source(source);
    }

    free(source);

    return 0;
}
