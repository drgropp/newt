/* Tokens and the lexer interface. */

#ifndef NEWT_LEXER_H
#define NEWT_LEXER_H

#include "common.h"

typedef enum {
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_SEMICOLON,
    TOKEN_UNKNOWN,
    TOKEN_ERROR,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_STRING,

    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_DOUBLE,
    TOKEN_VOID,
    TOKEN_BOOL,
    TOKEN_STRING_TYPE,
    TOKEN_LIST_TYPE,
    TOKEN_CONST,
    TOKEN_START,
    TOKEN_STRUCT,
    TOKEN_EXTERN,
    TOKEN_ASM,
    TOKEN_ASM_BLOCK,
    TOKEN_SIZEOF,
    TOKEN_AMP,
    TOKEN_ARROW,
    TOKEN_LET,
    TOKEN_MUT,
    TOKEN_FN,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
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
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA,
    TOKEN_COLON,
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

void lexer_init(Lexer *lexer, const char *source);

int token_text_equals(Token token, const char *text);

int string_escape_is_supported(char escaped);

char decode_string_escape(char escaped);

Token next_token(Lexer *lexer);

void print_lexer_error(const char *path, Token token);

int print_tokens(const char *path, const char *source);

#endif

