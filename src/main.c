#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NEWT_VERSION "0.0.1"

typedef enum {
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_UNKNOWN,
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
    printf("  newt --help\n");
    printf("  newt --version\n");
    printf("\n");
    printf("Phase 1: reads .nt source files and can print tokens.\n");
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
    }

    return make_token(lexer, TOKEN_STRING);
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

    return make_token(lexer, TOKEN_UNKNOWN);
}

static const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF:
            return "EOF";
        case TOKEN_NEWLINE:
            return "NEWLINE";
        case TOKEN_UNKNOWN:
            return "UNKNOWN";
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

static void print_tokens(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    for (;;) {
        Token token = next_token(&lexer);

        print_token(token);

        if (token.type == TOKEN_EOF) {
            break;
        }
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

    if (strcmp(argv[1], "--tokens") == 0) {
        if (argc < 3) {
            printf("error: missing input file\n");
            printf("usage: newt --tokens <file.nt>\n");
            return 1;
        }

        print_token_mode = 1;
        path = argv[2];
    } else {
        path = argv[1];
    }

    char *source = read_file(path);
    if (source == NULL) {
        return 1;
    }

    if (print_token_mode) {
        print_tokens(source);
    } else {
        print_source(source);
    }

    free(source);

    return 0;
}
