/* Lexer: turns source text into tokens (also powers --tokens). */

#include "lexer.h"

void lexer_init(Lexer *lexer, const char *source) {
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

    lexer_advance(lexer);
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

static Token make_error_token_at(Lexer *lexer,
                                 const char *start,
                                 int line,
                                 int column,
                                 const char *message) {
    Token token = make_error_token(lexer, message);

    token.start = start;
    token.length = (int)(lexer->current - start);
    token.line = line;
    token.column = column;
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

int token_text_equals(Token token, const char *text) {
    int length = (int)strlen(text);
    return token.length == length && strncmp(token.start, text, (size_t)length) == 0;
}

static TokenType keyword_type(Token token) {
    if (token_text_equals(token, "let")) {
        return TOKEN_LET;
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
    if (token_text_equals(token, "for")) {
        return TOKEN_FOR;
    }
    if (token_text_equals(token, "in")) {
        return TOKEN_IN;
    }
    if (token_text_equals(token, "break")) {
        return TOKEN_BREAK;
    }
    if (token_text_equals(token, "continue")) {
        return TOKEN_CONTINUE;
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
    if (token_text_equals(token, "&&")) {
        return TOKEN_AND;
    }
    if (token_text_equals(token, "||")) {
        return TOKEN_OR;
    }
    if (token_text_equals(token, "!")) {
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

int string_escape_is_supported(char escaped) {
    return escaped == 'n' ||
           escaped == 'r' ||
           escaped == 't' ||
           escaped == '\\' ||
           escaped == '"';
}

char decode_string_escape(char escaped) {
    switch (escaped) {
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case '\\':
            return '\\';
        case '"':
            return '"';
        default:
            return escaped;
    }
}

static Token lex_string(Lexer *lexer) {
    while (!lexer_is_at_end(lexer)) {
        char ch = lexer_peek(lexer);

        if (ch == '"') {
            lexer_advance(lexer);
            return make_token(lexer, TOKEN_STRING);
        }
        if (ch == '\n' || ch == '\r') {
            return make_error_token(lexer, "unterminated string");
        }
        if (ch == '\\') {
            const char *escape_start = lexer->current;
            int escape_line = lexer->line;
            int escape_column = lexer->column;

            lexer_advance(lexer);
            if (lexer_is_at_end(lexer) ||
                lexer_peek(lexer) == '\n' ||
                lexer_peek(lexer) == '\r') {
                return make_error_token_at(lexer,
                                           escape_start,
                                           escape_line,
                                           escape_column,
                                           "incomplete escape sequence");
            }

            ch = lexer_advance(lexer);
            if (!string_escape_is_supported(ch)) {
                return make_error_token_at(lexer,
                                           escape_start,
                                           escape_line,
                                           escape_column,
                                           "invalid escape sequence");
            }
            continue;
        }

        lexer_advance(lexer);
    }

    return make_error_token(lexer, "unterminated string");
}

static void lexer_skip_comment(Lexer *lexer) {
    while (lexer_peek(lexer) != '\n' && !lexer_is_at_end(lexer)) {
        lexer_advance(lexer);
    }
}

Token next_token(Lexer *lexer) {
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
            return make_token(lexer, TOKEN_NOT);
        case '&':
            if (lexer_match(lexer, '&')) {
                return make_token(lexer, TOKEN_AND);
            }
            break;
        case '|':
            if (lexer_match(lexer, '|')) {
                return make_token(lexer, TOKEN_OR);
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
        case TOKEN_LET:
            return "LET";
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
        case TOKEN_FOR:
            return "FOR";
        case TOKEN_IN:
            return "IN";
        case TOKEN_BREAK:
            return "BREAK";
        case TOKEN_CONTINUE:
            return "CONTINUE";
        case TOKEN_PRINT:
            return "PRINT";
        case TOKEN_TRUE:
            return "TRUE";
        case TOKEN_FALSE:
            return "FALSE";
        case TOKEN_AND:
            return "&&";
        case TOKEN_OR:
            return "||";
        case TOKEN_NOT:
            return "!";
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

void print_lexer_error(const char *path, Token token) {
    if (token.length == 2 && token.error_message != NULL &&
        strcmp(token.error_message, "invalid escape sequence") == 0) {
        fprintf(stderr,
                "%s:%d:%d: lexer error: %s '\\%c'\n",
                path,
                token.line,
                token.column,
                token.error_message,
                token.start[1]);
        return;
    }

    if (token.length == 1 && token.error_message != NULL &&
        strcmp(token.error_message, "unknown character") == 0) {
        fprintf(stderr,
                "%s:%d:%d: lexer error: %s '%c'\n",
                path,
                token.line,
                token.column,
                token.error_message,
                token.start[0]);
        return;
    }

    fprintf(stderr,
            "%s:%d:%d: lexer error: %s\n",
            path,
            token.line,
            token.column,
            token.error_message);
}

int print_tokens(const char *path, const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    for (;;) {
        Token token = next_token(&lexer);

        if (token.type == TOKEN_ERROR) {
            print_lexer_error(path, token);
            return 0;
        }

        print_token(token);

        if (token.type == TOKEN_EOF) {
            return 1;
        }
    }
}

