#ifndef MIO_LEXER_H
#define MIO_LEXER_H

#include "token.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    const char *source;
    const char *filename;
    int pos;
    int line;
    int col;
    int bol;
    Token *current;
    Token *peek;
} Lexer;

Lexer *lexer_new(const char *source, const char *filename);
Token *lexer_next(Lexer *l);
Token *lexer_peek(Lexer *l);
void lexer_free(Lexer *l);

// ===== Implementation =====

static char *mio_strndup(const char *s, int n) {
    char *buf = malloc(n + 1);
    if (!buf) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    memcpy(buf, s, n);
    buf[n] = '\0';
    return buf;
}

static char lexer_cur(Lexer *l) {
    return l->source[l->pos];
}

static char lexer_advance(Lexer *l) {
    char c = l->source[l->pos];
    if (c == '\0') return c;
    l->pos++;
    if (c == '\n') {
        l->line++;
        l->col = 1;
        l->bol = l->pos;
    } else {
        l->col++;
    }
    return c;
}

static int lexer_match(Lexer *l, char c) {
    if (lexer_cur(l) == c) {
        lexer_advance(l);
        return 1;
    }
    return 0;
}

static void lexer_skip_whitespace(Lexer *l) {
    while (1) {
        char c = lexer_cur(l);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                lexer_advance(l);
                break;
            case '\n':
                lexer_advance(l);
                break;
            case '#':
                while (lexer_cur(l) != '\n' && lexer_cur(l) != '\0')
                    lexer_advance(l);
                break;
            default:
                return;
        }
    }
}

typedef struct {
    const char *keyword;
    TokenKind kind;
} KeywordEntry;

static KeywordEntry keywords[] = {
    {"import", TOK_IMPORT}, {"var", TOK_VAR}, {"def", TOK_DEF},
    {"const", TOK_CONST}, {"if", TOK_IF}, {"else", TOK_ELSE},
    {"elif", TOK_ELIF}, {"while", TOK_WHILE}, {"for", TOK_FOR},
    {"break", TOK_BREAK}, {"continue", TOK_CONTINUE}, {"goto", TOK_GOTO},
    {"return", TOK_RETURN}, {"struct", TOK_STRUCT}, {"enum", TOK_ENUM},
    {"union", TOK_UNION}, {"static", TOK_STATIC}, {"operator", TOK_OPERATOR},
    {"true", TOK_TRUE}, {"false", TOK_FALSE},
    {"this", TOK_THIS},
    {"i32", TOK_I32}, {"i64", TOK_I64}, {"u32", TOK_U32},
    {"u64", TOK_U64}, {"f32", TOK_F32}, {"f64", TOK_F64},
    {"bool", TOK_BOOL}, {"char", TOK_CHAR},
    {NULL, TOK_EOF}
};

static TokenKind lexer_keyword_kind(const char *s) {
    for (int i = 0; keywords[i].keyword; i++) {
        if (strcmp(s, keywords[i].keyword) == 0)
            return keywords[i].kind;
    }
    return TOK_IDENT;
}

static Token *lexer_ident(Lexer *l) {
    int start = l->pos;
    int start_col = l->col;
    while (isalnum(lexer_cur(l)) || lexer_cur(l) == '_')
        lexer_advance(l);
    int len = l->pos - start;
    char *text = mio_strndup(l->source + start, len);
    TokenKind kind = lexer_keyword_kind(text);
    if (kind != TOK_IDENT) {
        free(text);
        return tok_new(kind, NULL, l->line, start_col);
    }
    Token *t = tok_new(TOK_IDENT, text, l->line, start_col);
    free(text);
    return t;
}

static Token *lexer_number(Lexer *l) {
    int start = l->pos;
    int start_col = l->col;
    bool is_float = false;

    if (lexer_cur(l) == '0' && (lexer_cur(l) == 'x' || lexer_cur(l) == 'X')) {
        lexer_advance(l);
        lexer_advance(l);
        while (isxdigit(lexer_cur(l))) lexer_advance(l);
    } else {
        while (isdigit(lexer_cur(l))) lexer_advance(l);
        if (lexer_cur(l) == '.') {
            is_float = true;
            lexer_advance(l);
            while (isdigit(lexer_cur(l))) lexer_advance(l);
        }
    }

    int len = l->pos - start;
    char *text = mio_strndup(l->source + start, len);

    Token *t;
    if (is_float) {
        t = tok_new(TOK_FLOAT_LIT, text, l->line, start_col);
        t->float_val = atof(text);
    } else {
        t = tok_new(TOK_INT_LIT, text, l->line, start_col);
        t->int_val = atoll(text);
    }
    free(text);
    return t;
}

static Token *lexer_string(Lexer *l) {
    int start_col = l->col;
    lexer_advance(l);

    char *buffer = malloc(256);
    if (!buffer) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    int capacity = 256;
    int length = 0;

    while (lexer_cur(l) != '"' && lexer_cur(l) != '\0') {
        if (length >= capacity - 1) {
            capacity *= 2;
            char *new_buffer = realloc(buffer, capacity);
            if (!new_buffer) {
                free(buffer);
                fprintf(stderr, "fatal: out of memory\n");
                exit(1);
            }
            buffer = new_buffer;
        }

        if (lexer_cur(l) == '\\') {
            lexer_advance(l);
            char next = lexer_cur(l);
            switch (next) {
                case 'n':  buffer[length++] = '\n'; break;
                case 't':  buffer[length++] = '\t'; break;
                case 'r':  buffer[length++] = '\r'; break;
                case '\\': buffer[length++] = '\\'; break;
                case '"':  buffer[length++] = '"'; break;
                case '0':  buffer[length++] = '\0'; break;
                default:   buffer[length++] = next; break;
            }
            lexer_advance(l);
        } else {
            buffer[length++] = lexer_cur(l);
            lexer_advance(l);
        }
    }

    if (lexer_cur(l) == '"') lexer_advance(l);
    buffer[length] = '\0';

    Token *t = tok_new(TOK_STRING_LIT, buffer, l->line, start_col);
    free(buffer);
    return t;
}

static Token *lexer_char(Lexer *l) {
    int start_col = l->col;
    lexer_advance(l);
    char c = lexer_cur(l);

    if (c == '\\') {
        lexer_advance(l);
        char next = lexer_cur(l);
        switch (next) {
            case 'n':  c = '\n'; break;
            case 't':  c = '\t'; break;
            case 'r':  c = '\r'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            case '0':  c = '\0'; break;
            default:   c = next; break;
        }
        lexer_advance(l);
    } else {
        lexer_advance(l);
    }

    if (lexer_cur(l) == '\'') lexer_advance(l);
    Token *t = tok_new(TOK_CHAR_LIT, NULL, l->line, start_col);
    t->char_val = c;
    return t;
}

static Token *lexer_token(Lexer *l) {
    lexer_skip_whitespace(l);
    if (lexer_cur(l) == '\0') return tok_new(TOK_EOF, NULL, l->line, l->col);

    int line = l->line;
    int col = l->col;
    char c = lexer_advance(l);

    if (isalpha(c) || c == '_') {
        l->pos--;
        l->col--;
        return lexer_ident(l);
    }
    if (isdigit(c)) {
        l->pos--;
        l->col--;
        return lexer_number(l);
    }

    switch (c) {
        case '"': l->pos--; l->col--; return lexer_string(l);
        case '\'': l->pos--; l->col--; return lexer_char(l);
        case '+': return tok_new(TOK_PLUS, NULL, line, col);
        case '-':
            if (lexer_match(l, '>')) return tok_new(TOK_ARROW, NULL, line, col);
            return tok_new(TOK_MINUS, NULL, line, col);
        case '*': return tok_new(TOK_STAR, NULL, line, col);
        case '/': return tok_new(TOK_SLASH, NULL, line, col);
        case '%': return tok_new(TOK_PERCENT, NULL, line, col);
        case '(': return tok_new(TOK_LPAREN, NULL, line, col);
        case ')': return tok_new(TOK_RPAREN, NULL, line, col);
        case '{': return tok_new(TOK_LBRACE, NULL, line, col);
        case '}': return tok_new(TOK_RBRACE, NULL, line, col);
        case '[': return tok_new(TOK_LBRACKET, NULL, line, col);
        case ']': return tok_new(TOK_RBRACKET, NULL, line, col);
        case ';': return tok_new(TOK_SEMICOLON, NULL, line, col);
        case ':': return tok_new(TOK_COLON, NULL, line, col);
        case ',': return tok_new(TOK_COMMA, NULL, line, col);
        case '.': return tok_new(TOK_DOT, NULL, line, col);
        case '=':
            if (lexer_match(l, '=')) return tok_new(TOK_EQ, NULL, line, col);
            return tok_new(TOK_ASSIGN, NULL, line, col);
        case '!':
            if (lexer_match(l, '=')) return tok_new(TOK_NEQ, NULL, line, col);
            return tok_new(TOK_NOT, NULL, line, col);
        case '<':
            if (lexer_match(l, '=')) return tok_new(TOK_LTE, NULL, line, col);
            if (lexer_match(l, '<')) return tok_new(TOK_LSHIFT, NULL, line, col);
            return tok_new(TOK_LT, NULL, line, col);
        case '>':
            if (lexer_match(l, '=')) return tok_new(TOK_GTE, NULL, line, col);
            if (lexer_match(l, '>')) return tok_new(TOK_RSHIFT, NULL, line, col);
            return tok_new(TOK_GT, NULL, line, col);
        case '&':
            if (lexer_match(l, '&')) return tok_new(TOK_AND, NULL, line, col);
            return tok_new(TOK_BIT_AND, NULL, line, col);
        case '|':
            if (lexer_match(l, '|')) return tok_new(TOK_OR, NULL, line, col);
            return tok_new(TOK_BIT_OR, NULL, line, col);
        case '^': return tok_new(TOK_BIT_XOR, NULL, line, col);
        case '~': return tok_new(TOK_BIT_NOT, NULL, line, col);
        default: {
            char buf[64];
            snprintf(buf, sizeof(buf), "unexpected character '%c'", c);
            Token *t = tok_new(TOK_ERROR, buf, line, col);
            return t;
        }
    }
}

Lexer *lexer_new(const char *source, const char *filename) {
    Lexer *l = calloc(1, sizeof(Lexer));
    if (!l) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    l->source = source;
    l->filename = filename;
    l->line = 1;
    l->col = 1;
    l->current = lexer_token(l);
    l->peek = lexer_token(l);
    return l;
}

Token *lexer_next(Lexer *l) {
    l->current = l->peek;
    l->peek = lexer_token(l);
    return l->current;
}

Token *lexer_peek(Lexer *l) {
    return l->peek;
}

void lexer_free(Lexer *l) {
    if (!l) return;
    tok_free(l->current);
    tok_free(l->peek);
    free(l);
}

#endif