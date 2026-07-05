#ifndef MIO_PARSER_H
#define MIO_PARSER_H

#include "ast.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Lexer *lexer;
    Token *cur;
    Token *peek;
    int error_count;
    const char *filename;
    char **include_paths;
    int include_path_count;
    char **imported_files;
    int imported_count;
    int imported_capacity;
    struct { char *name; char *value; } *macros;
    int macro_count;
    int macro_cap;
} Parser;

Parser *parser_new(Lexer *lexer, const char *filename, char **include_paths, int include_path_count);
AstNode *parser_parse(Parser *p);
void parser_free(Parser *p);
int parser_error_count(Parser *p);
void parser_add_macro(Parser *p, const char *name, const char *value);

// ===== Implementation =====

static void parser_error(Parser *p, const char *msg) {
    fprintf(stderr, "%s:%d:%d: error: %s\n",
        p->filename, p->cur->line, p->cur->col, msg);
    p->error_count++;
}

static void parser_error_expected(Parser *p, const char *expected) {
    char buf[256];
    if (p->cur->kind == TOK_ERROR)
        snprintf(buf, sizeof(buf), "%s", p->cur->lexeme);
    else
        snprintf(buf, sizeof(buf), "expected %s, got '%s'",
            expected, tok_name(p->cur->kind));
    parser_error(p, buf);
}

static void parser_advance(Parser *p) {
    p->cur = lexer_next(p->lexer);
    p->peek = lexer_peek(p->lexer);
}

static bool parser_check(Parser *p, TokenKind kind) {
    return p->cur->kind == kind;
}

static bool parser_match(Parser *p, TokenKind kind) {
    if (parser_check(p, kind)) {
        parser_advance(p);
        return true;
    }
    return false;
}

static bool parser_expect(Parser *p, TokenKind kind) {
    if (parser_match(p, kind)) return true;
    parser_error_expected(p, tok_name(kind));
    return false;
}

static MioType *parse_type(Parser *p);
static AstNode *parse_expr(Parser *p);
static AstNode *parse_stmt(Parser *p);
static AstNode *parse_block(Parser *p);
static AstNode *parse_decl(Parser *p);
static char *parse_import_path(Parser *p);

static bool has_mio_extension(const char *path) {
    int len = (int)strlen(path);
    return len > 4 && strcmp(path + len - 4, ".mio") == 0;
}

static char *get_directory(const char *path) {
    const char *last_sep = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') last_sep = p;
    }
    if (!last_sep) return strdup(".");
    int len = (int)(last_sep - path);
    char *dir = malloc(len + 1);
    if (!dir) return NULL;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

static char *join_path(const char *dir, const char *file) {
    int dir_len = (int)strlen(dir);
    int file_len = (int)strlen(file);
    char *result = malloc(dir_len + 1 + file_len + 1);
    if (!result) return NULL;
    memcpy(result, dir, dir_len);
    result[dir_len] = '/';
    memcpy(result + dir_len + 1, file, file_len + 1);
    return result;
}

static bool file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

static char *read_file_content(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, size, f);
    if (read_bytes != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static bool is_imported(Parser *p, const char *path) {
    for (int i = 0; i < p->imported_count; i++) {
        if (strcmp(p->imported_files[i], path) == 0) return true;
    }
    return false;
}

static void mark_imported(Parser *p, const char *path) {
    if (p->imported_count >= p->imported_capacity) {
        p->imported_capacity = p->imported_capacity ? p->imported_capacity * 2 : 8;
        char **new_files = realloc(p->imported_files, sizeof(char*) * p->imported_capacity);
        if (!new_files) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        p->imported_files = new_files;
    }
    p->imported_files[p->imported_count++] = strdup(path);
}

static char *resolve_mio_file(Parser *p, const char *import_path) {
    char *dir = get_directory(p->filename);
    if (dir) {
        char *candidate = join_path(dir, import_path);
        free(dir);
        if (candidate && file_exists(candidate)) return candidate;
        free(candidate);
    }

    for (int i = 0; i < p->include_path_count; i++) {
        char *candidate = join_path(p->include_paths[i], import_path);
        if (candidate && file_exists(candidate)) return candidate;
        free(candidate);
    }

    return NULL;
}

static void add_import_to_block(AstNode *block, AstNode *node) {
    if (!node) return;
    if (node->kind == AST_BLOCK) {
        for (int i = 0; i < node->block.count; i++) {
            ast_block_add(block, node->block.stmts[i]);
        }
        free(node->block.stmts);
        free(node);
    } else {
        ast_block_add(block, node);
    }
}

static AstNode *parse_single_import(Parser *p, int line, int col) {
    if (p->cur->kind == TOK_STRING_LIT) {
        char *path = strdup(p->cur->lexeme);
        parser_advance(p);

        if (has_mio_extension(path)) {
            char *resolved = resolve_mio_file(p, path);
            if (!resolved) {
                char buf[512];
                snprintf(buf, sizeof(buf), "imported file '%s' not found", path);
                parser_error(p, buf);
                free(path);
                return NULL;
            }

            if (is_imported(p, resolved)) {
                free(path);
                free(resolved);
                return NULL;
            }
            mark_imported(p, resolved);

            char *source = read_file_content(resolved);
            if (!source) {
                char buf[512];
                snprintf(buf, sizeof(buf), "cannot read imported file '%s'", path);
                parser_error(p, buf);
                free(path);
                free(resolved);
                return NULL;
            }

            Lexer *old_lexer = p->lexer;
            Token *old_cur = p->cur;
            Token *old_peek = p->peek;
            const char *old_filename = p->filename;

            Lexer *new_lexer = lexer_new(source, resolved);
            p->lexer = new_lexer;
            p->filename = resolved;
            p->cur = new_lexer->current;
            p->peek = new_lexer->peek;

            AstNode *block = ast_new_block(line, col);
            while (!parser_check(p, TOK_EOF)) {
                AstNode *decl = parse_decl(p);
                if (decl) add_import_to_block(block, decl);
            }

            lexer_free(new_lexer);
            p->lexer = old_lexer;
            p->cur = old_cur;
            p->peek = old_peek;
            p->filename = old_filename;

            free(source);
            free(path);
            free(resolved);
            return block;
        } else {
            AstNode *node = ast_new_import(path, line, col);
            free(path);
            return node;
        }
    } else {
        char *path = parse_import_path(p);
        AstNode *node = ast_new_import(path, line, col);
        free(path);
        return node;
    }
}

static char *parse_import_path(Parser *p) {
    if (p->cur->kind == TOK_STRING_LIT) {
        char *path = strdup(p->cur->lexeme);
        parser_advance(p);
        return path;
    }
    char buf[1024];
    int pos = 0;
    while (p->cur->kind == TOK_IDENT) {
        int len = (int)strlen(p->cur->lexeme);
        if (pos + len + 2 > (int)sizeof(buf)) break;
        memcpy(buf + pos, p->cur->lexeme, len);
        pos += len;
        parser_advance(p);
        if (p->cur->kind == TOK_DOT) {
            buf[pos++] = '.';
            parser_advance(p);
        } else {
            break;
        }
    }
    buf[pos] = '\0';
    if (pos == 0) {
        parser_error_expected(p, "import path");
        return strdup("void");
    }
    char *result = malloc(pos + 3);
    result[0] = '<';
    memcpy(result + 1, buf, pos);
    result[pos + 1] = '>';
    result[pos + 2] = '\0';
    return result;
}

static MioType *parse_base_type(Parser *p) {
    switch (p->cur->kind) {
        case TOK_I32: parser_advance(p); return mio_type_new(TYPE_I32);
        case TOK_I64: parser_advance(p); return mio_type_new(TYPE_I64);
        case TOK_U32: parser_advance(p); return mio_type_new(TYPE_U32);
        case TOK_U64: parser_advance(p); return mio_type_new(TYPE_U64);
        case TOK_F32: parser_advance(p); return mio_type_new(TYPE_F32);
        case TOK_F64: parser_advance(p); return mio_type_new(TYPE_F64);
        case TOK_BOOL: parser_advance(p); return mio_type_new(TYPE_BOOL);
        case TOK_CHAR: parser_advance(p); return mio_type_new(TYPE_CHAR);
        case TOK_IDENT: {
            char *name = strdup(p->cur->lexeme);
            parser_advance(p);
            return mio_type_new_named(TYPE_STRUCT, name);
        }
        default:
            parser_error_expected(p, "type");
            return mio_type_new(TYPE_VOID);
    }
}

static MioType *parse_type(Parser *p) {
    MioType *base = parse_base_type(p);
    if (parser_match(p, TOK_LBRACKET)) {
        if (p->cur->kind == TOK_INT_LIT) {
            int size = (int)p->cur->int_val;
            parser_advance(p);
            parser_expect(p, TOK_RBRACKET);
            return mio_type_new_array(base, size);
        }
        parser_expect(p, TOK_RBRACKET);
        return mio_type_new_array(base, 0);
    }
    return base;
}

static AstNode *parse_primary(Parser *p) {
    Token *t = p->cur;
    switch (t->kind) {
        case TOK_INT_LIT: {
            parser_advance(p);
            return ast_new_int_lit(t->int_val, t->line, t->col);
        }
        case TOK_FLOAT_LIT: {
            parser_advance(p);
            return ast_new_float_lit(t->float_val, t->line, t->col);
        }
        case TOK_STRING_LIT: {
            parser_advance(p);
            return ast_new_string_lit(t->lexeme, t->line, t->col);
        }
        case TOK_CHAR_LIT: {
            parser_advance(p);
            return ast_new_char_lit(t->char_val, t->line, t->col);
        }
        case TOK_TRUE: {
            parser_advance(p);
            return ast_new_bool_lit(true, t->line, t->col);
        }
        case TOK_FALSE: {
            parser_advance(p);
            return ast_new_bool_lit(false, t->line, t->col);
        }
        case TOK_THIS: {
            parser_advance(p);
            return ast_new_ident("this", t->line, t->col);
        }
        case TOK_IDENT: {
            parser_advance(p);
            return ast_new_ident(t->lexeme, t->line, t->col);
        }
        case TOK_LPAREN: {
            parser_advance(p);
            AstNode *expr = parse_expr(p);
            parser_expect(p, TOK_RPAREN);
            return expr;
        }
        case TOK_LBRACE: {
            parser_advance(p);
            AstNode *arr = ast_new_array_lit(t->line, t->col);
            if (!parser_check(p, TOK_RBRACE)) {
                ast_array_add(arr, parse_expr(p));
                while (parser_match(p, TOK_COMMA)) {
                    if (parser_check(p, TOK_RBRACE)) break;
                    ast_array_add(arr, parse_expr(p));
                }
            }
            parser_expect(p, TOK_RBRACE);
            return arr;
        }
        case TOK_I32: case TOK_I64:
        case TOK_U32: case TOK_U64:
        case TOK_F32: case TOK_F64:
        case TOK_BOOL: case TOK_CHAR: {
            MioTypeKind k;
            switch (t->kind) {
                case TOK_I32: k = TYPE_I32; break;
                case TOK_I64: k = TYPE_I64; break;
                case TOK_U32: k = TYPE_U32; break;
                case TOK_U64: k = TYPE_U64; break;
                case TOK_F32: k = TYPE_F32; break;
                case TOK_F64: k = TYPE_F64; break;
                case TOK_BOOL: k = TYPE_BOOL; break;
                case TOK_CHAR: k = TYPE_CHAR; break;
            }
            parser_advance(p);
            if (parser_check(p, TOK_LPAREN)) {
                parser_advance(p);
                AstNode *expr = parse_expr(p);
                parser_expect(p, TOK_RPAREN);
                return ast_new_cast(mio_type_new(k), expr, t->line, t->col);
            }
            parser_error_expected(p, "expression");
            return ast_new_int_lit(0, t->line, t->col);
        }
        default:
            parser_error_expected(p, "expression");
            parser_advance(p);
            return ast_new_int_lit(0, t->line, t->col);
    }
}

static AstNode *parse_postfix(Parser *p) {
    AstNode *expr = parse_primary(p);
    while (true) {
        if (parser_match(p, TOK_LPAREN)) {
            AstNode *call = ast_new_call(expr, expr->line, expr->col);
            if (!parser_check(p, TOK_RPAREN)) {
                ast_call_add_arg(call, parse_expr(p));
                while (parser_match(p, TOK_COMMA))
                    ast_call_add_arg(call, parse_expr(p));
            }
            parser_expect(p, TOK_RPAREN);
            expr = call;
        } else if (parser_match(p, TOK_LBRACKET)) {
            AstNode *index = parse_expr(p);
            parser_expect(p, TOK_RBRACKET);
            expr = ast_new_index(expr, index, expr->line, expr->col);
        } else if (parser_match(p, TOK_DOT)) {
            if (p->cur->kind != TOK_IDENT) {
                parser_error_expected(p, "identifier after '.'");
                break;
            }
            expr = ast_new_member(expr, p->cur->lexeme, false, expr->line, expr->col);
            parser_advance(p);
        } else if (parser_match(p, TOK_ARROW)) {
            if (p->cur->kind != TOK_IDENT) {
                parser_error_expected(p, "identifier after '->'");
                break;
            }
            expr = ast_new_member(expr, p->cur->lexeme, true, expr->line, expr->col);
            parser_advance(p);
        } else {
            break;
        }
    }
    return expr;
}

static AstNode *parse_unary(Parser *p) {
    TokenKind op = p->cur->kind;
    switch (op) {
        case TOK_MINUS:
        case TOK_NOT:
        case TOK_BIT_NOT:
        case TOK_STAR:
        case TOK_BIT_AND:
            parser_advance(p);
            return ast_new_unary(op, parse_unary(p), p->cur->line, p->cur->col);
        default:
            return parse_postfix(p);
    }
}

static AstNode *parse_multiplicative(Parser *p) {
    AstNode *left = parse_unary(p);
    while (true) {
        TokenKind op = p->cur->kind;
        if (op == TOK_STAR || op == TOK_SLASH || op == TOK_PERCENT) {
            parser_advance(p);
            left = ast_new_binary(left, op, parse_unary(p), left->line, left->col);
        } else {
            break;
        }
    }
    return left;
}

static AstNode *parse_additive(Parser *p) {
    AstNode *left = parse_multiplicative(p);
    while (true) {
        TokenKind op = p->cur->kind;
        if (op == TOK_PLUS || op == TOK_MINUS) {
            parser_advance(p);
            left = ast_new_binary(left, op, parse_multiplicative(p), left->line, left->col);
        } else {
            break;
        }
    }
    return left;
}

static AstNode *parse_shift(Parser *p) {
    AstNode *left = parse_additive(p);
    while (true) {
        TokenKind op = p->cur->kind;
        if (op == TOK_LSHIFT || op == TOK_RSHIFT) {
            parser_advance(p);
            left = ast_new_binary(left, op, parse_additive(p), left->line, left->col);
        } else {
            break;
        }
    }
    return left;
}

static AstNode *parse_relational(Parser *p) {
    AstNode *left = parse_shift(p);
    while (true) {
        TokenKind op = p->cur->kind;
        if (op == TOK_LT || op == TOK_GT || op == TOK_LTE || op == TOK_GTE) {
            parser_advance(p);
            left = ast_new_binary(left, op, parse_shift(p), left->line, left->col);
        } else {
            break;
        }
    }
    return left;
}

static AstNode *parse_equality(Parser *p) {
    AstNode *left = parse_relational(p);
    while (true) {
        TokenKind op = p->cur->kind;
        if (op == TOK_EQ || op == TOK_NEQ) {
            parser_advance(p);
            left = ast_new_binary(left, op, parse_relational(p), left->line, left->col);
        } else {
            break;
        }
    }
    return left;
}

static AstNode *parse_bit_and(Parser *p) {
    AstNode *left = parse_equality(p);
    while (parser_match(p, TOK_BIT_AND))
        left = ast_new_binary(left, TOK_BIT_AND, parse_equality(p), left->line, left->col);
    return left;
}

static AstNode *parse_bit_xor(Parser *p) {
    AstNode *left = parse_bit_and(p);
    while (parser_match(p, TOK_BIT_XOR))
        left = ast_new_binary(left, TOK_BIT_XOR, parse_bit_and(p), left->line, left->col);
    return left;
}

static AstNode *parse_bit_or(Parser *p) {
    AstNode *left = parse_bit_xor(p);
    while (parser_match(p, TOK_BIT_OR))
        left = ast_new_binary(left, TOK_BIT_OR, parse_bit_xor(p), left->line, left->col);
    return left;
}

static AstNode *parse_logical_and(Parser *p) {
    AstNode *left = parse_bit_or(p);
    while (parser_match(p, TOK_AND))
        left = ast_new_binary(left, TOK_AND, parse_bit_or(p), left->line, left->col);
    return left;
}

static AstNode *parse_logical_or(Parser *p) {
    AstNode *left = parse_logical_and(p);
    while (parser_match(p, TOK_OR))
        left = ast_new_binary(left, TOK_OR, parse_logical_and(p), left->line, left->col);
    return left;
}

static AstNode *parse_assignment(Parser *p) {
    AstNode *left = parse_logical_or(p);
    if (parser_match(p, TOK_ASSIGN)) {
        AstNode *right = parse_assignment(p);
        return ast_new_assign(left, right, left->line, left->col);
    }
    return left;
}

static AstNode *parse_expr(Parser *p) {
    return parse_assignment(p);
}

static AstNode *parse_block(Parser *p) {
    int line = p->cur->line, col = p->cur->col;
    parser_expect(p, TOK_LBRACE);
    AstNode *block = ast_new_block(line, col);
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        AstNode *stmt = parse_stmt(p);
        if (stmt) ast_block_add(block, stmt);
    }
    parser_expect(p, TOK_RBRACE);
    return block;
}

static AstNode *parse_var_items(Parser *p, bool is_const, bool is_static) {
    int line = p->cur->line, col = p->cur->col;
    char *name = strdup(p->cur->lexeme);
    parser_expect(p, TOK_IDENT);

    MioType *type = NULL;
    AstNode *init = NULL;

    if (parser_match(p, TOK_COLON))
        type = parse_type(p);

    if (parser_match(p, TOK_ASSIGN))
        init = parse_expr(p);

    if (init && init->kind == AST_ARRAY_LIT) {
        if (type && type->kind == TYPE_ARRAY && type->array_size == 0) {
            type->array_size = init->array_lit.count;
        } else if (!type) {
            MioType *base = NULL;
            if (init->array_lit.count > 0) {
                AstNode *first = init->array_lit.elements[0];
                switch (first->kind) {
                    case AST_INT_LIT:   base = mio_type_new(TYPE_I32); break;
                    case AST_FLOAT_LIT: base = mio_type_new(TYPE_F64); break;
                    case AST_BOOL_LIT:  base = mio_type_new(TYPE_BOOL); break;
                    case AST_CHAR_LIT:  base = mio_type_new(TYPE_CHAR); break;
                    case AST_STRING_LIT:base = mio_type_new(TYPE_CHAR); break;
                    default:            base = mio_type_new(TYPE_I32); break;
                }
            }
            if (!base) base = mio_type_new(TYPE_I32);
            type = mio_type_new_array(base, init->array_lit.count);
        }
    }

    if (is_const)
        return ast_new_const_decl(name, type, init, is_static, line, col);
    else
        return ast_new_var_decl(name, type, init, is_static, line, col);
}

static AstNode *parse_var_decl(Parser *p, bool is_const, bool is_static) {
    parser_advance(p);
    AstNode *block = ast_new_block(p->cur->line, p->cur->col);
    ast_block_add(block, parse_var_items(p, is_const, is_static));

    while (parser_match(p, TOK_COMMA))
        ast_block_add(block, parse_var_items(p, is_const, is_static));

    parser_expect(p, TOK_SEMICOLON);
    return block;
}

static AstNode *parse_if_stmt(Parser *p) {
    int line = p->cur->line, col = p->cur->col;
    parser_advance(p);
    parser_expect(p, TOK_COLON);

    AstNode *cond = parse_expr(p);
    AstNode *then_body = parse_stmt(p);
    AstNode *if_node = ast_new_if(cond, then_body, NULL, line, col);

    while (parser_match(p, TOK_ELIF)) {
        parser_expect(p, TOK_COLON);
        AstNode *elif_cond = parse_expr(p);
        AstNode *elif_body = parse_stmt(p);
        ast_if_add_elif(if_node, elif_cond, elif_body);
    }

    if (parser_match(p, TOK_ELSE)) {
        if_node->if_stmt.else_body = parse_stmt(p);
    }

    return if_node;
}

static AstNode *parse_while_stmt(Parser *p) {
    int line = p->cur->line, col = p->cur->col;
    parser_advance(p);
    parser_expect(p, TOK_COLON);
    AstNode *cond = parse_expr(p);
    AstNode *body = parse_stmt(p);
    return ast_new_while(cond, body, line, col);
}

static AstNode *parse_for_stmt(Parser *p) {
    int line = p->cur->line, col = p->cur->col;
    parser_advance(p);
    parser_expect(p, TOK_COLON);

    AstNode *init = NULL, *cond = NULL, *update = NULL;

    if (!parser_check(p, TOK_SEMICOLON))
        init = parse_expr(p);
    parser_expect(p, TOK_SEMICOLON);

    if (!parser_check(p, TOK_SEMICOLON))
        cond = parse_expr(p);
    parser_expect(p, TOK_SEMICOLON);

    if (!parser_check(p, TOK_LBRACE))
        update = parse_expr(p);

    AstNode *body = parse_stmt(p);
    return ast_new_for(init, cond, update, body, line, col);
}

static AstNode *parse_return_stmt(Parser *p) {
    int line = p->cur->line, col = p->cur->col;
    parser_advance(p);

    AstNode *value = NULL;
    if (!parser_check(p, TOK_SEMICOLON) && !parser_check(p, TOK_RBRACE))
        value = parse_expr(p);

    parser_expect(p, TOK_SEMICOLON);

    return ast_new_return(value, line, col);
}

static AstNode *parse_stmt(Parser *p) {
    switch (p->cur->kind) {
        case TOK_VAR:
            return parse_var_decl(p, false, false);
        case TOK_CONST:
            return parse_var_decl(p, true, false);
        case TOK_IF:
            return parse_if_stmt(p);
        case TOK_WHILE:
            return parse_while_stmt(p);
        case TOK_FOR:
            return parse_for_stmt(p);
        case TOK_BREAK:
            parser_advance(p);
            parser_expect(p, TOK_SEMICOLON);
            return ast_new_break(p->cur->line, p->cur->col);
        case TOK_CONTINUE:
            parser_advance(p);
            parser_expect(p, TOK_SEMICOLON);
            return ast_new_continue(p->cur->line, p->cur->col);
        case TOK_GOTO: {
            parser_advance(p);
            int line = p->cur->line, col = p->cur->col;
            char *label = strdup(p->cur->lexeme);
            parser_expect(p, TOK_IDENT);
            parser_expect(p, TOK_SEMICOLON);
            AstNode *n = ast_new_goto(label, line, col);
            free(label);
            return n;
        }
        case TOK_COLON: {
            parser_advance(p);
            int line = p->cur->line, col = p->cur->col;
            char *label = strdup(p->cur->lexeme);
            parser_expect(p, TOK_IDENT);
            AstNode *n = ast_new_label(label, line, col);
            free(label);
            return n;
        }
        case TOK_RETURN:
            return parse_return_stmt(p);
        case TOK_LBRACE:
            return parse_block(p);
        case TOK_SEMICOLON:
            parser_advance(p);
            return NULL;
        default: {
            AstNode *expr = parse_expr(p);
            if (parser_match(p, TOK_SEMICOLON))
                return ast_new_expr_stmt(expr, expr->line, expr->col);
            return ast_new_return(expr, expr->line, expr->col);
        }
    }
}

static AstNode *parse_func_def(Parser *p, bool is_static) {
    int line = p->cur->line, col = p->cur->col;

    MioType *return_type = parse_type(p);

    bool is_operator = false;
    char *op_name = NULL;
    char *func_name = NULL;

    if (parser_match(p, TOK_OPERATOR)) {
        is_operator = true;
        func_name = strdup(tok_name(p->cur->kind));
        op_name = strdup(tok_name(p->cur->kind));
        parser_advance(p);
    } else if (p->cur->kind == TOK_IDENT) {
        func_name = strdup(p->cur->lexeme);
        parser_advance(p);
    } else if (p->cur->kind == TOK_LPAREN) {
        func_name = return_type->name ? strdup(return_type->name) : strdup("constructor");
    } else {
        parser_error_expected(p, "function name");
        mio_type_free(return_type);
        return NULL;
    }

    AstNode *func = ast_new_func_def(func_name, return_type, NULL, is_static, line, col);
    func->func_def.is_operator = is_operator;
    if (is_operator) {
        func->func_def.op_name = op_name;
    }
    free(func_name);

    parser_expect(p, TOK_LPAREN);

    if (!parser_check(p, TOK_RPAREN)) {
        do {
            char *pname = strdup(p->cur->lexeme);
            parser_expect(p, TOK_IDENT);
            parser_expect(p, TOK_COLON);
            MioType *ptype = parse_type(p);
            ast_func_add_param(func, pname, ptype);
            free(pname);
        } while (parser_match(p, TOK_COMMA));
    }

    parser_expect(p, TOK_RPAREN);

    if (!is_operator && parser_match(p, TOK_COLON)) {
        func->func_def.struct_name = strdup(func->func_def.name);
        while (!parser_check(p, TOK_LBRACE) && !parser_check(p, TOK_EOF)) {
            if (p->cur->kind == TOK_IDENT) {
                char *field_name = strdup(p->cur->lexeme);
                parser_advance(p);
                if (parser_match(p, TOK_LPAREN)) {
                    AstNode *init_expr = parse_expr(p);
                    parser_expect(p, TOK_RPAREN);
                    ast_func_add_init(func, field_name, init_expr);
                } else if (parser_match(p, TOK_ASSIGN)) {
                    AstNode *init_expr = parse_expr(p);
                    ast_func_add_init(func, field_name, init_expr);
                } else {
                    free(field_name);
                }
            }
            if (!parser_match(p, TOK_COMMA)) break;
        }
    }

    func->func_def.body = parse_block(p);
    return func;
}

static AstNode *parse_struct_def(Parser *p) {
    int line = p->cur->line, col = p->cur->col;
    parser_advance(p);

    char *name = strdup(p->cur->lexeme);
    parser_expect(p, TOK_IDENT);
    parser_expect(p, TOK_LBRACE);

    AstNode *s = ast_new_struct_def(name, line, col);
    free(name);

    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        if (parser_match(p, TOK_DEF)) {
            if (parser_match(p, TOK_STATIC)) {
                AstNode *method = parse_func_def(p, true);
                if (method) {
                    method->func_def.struct_name = strdup(s->struct_def.name);
                    ast_struct_add_method(s, method);
                }
            } else {
                AstNode *method = parse_func_def(p, false);
                if (method) {
                    method->func_def.struct_name = strdup(s->struct_def.name);
                    ast_struct_add_method(s, method);
                }
            }
        } else if (p->cur->kind == TOK_IDENT) {
            char *fname = strdup(p->cur->lexeme);
            parser_advance(p);
            parser_expect(p, TOK_COLON);
            MioType *ftype = parse_type(p);
            AstNode *init = NULL;
            if (parser_match(p, TOK_ASSIGN))
                init = parse_expr(p);
            parser_expect(p, TOK_SEMICOLON);
            ast_struct_add_field(s, fname, ftype, init);
            free(fname);
        } else {
            parser_error_expected(p, "field or method");
            parser_advance(p);
        }
    }

    parser_expect(p, TOK_RBRACE);
    return s;
}

static AstNode *parse_enum_def(Parser *p) {
    int line = p->cur->line, col = p->cur->col;
    parser_advance(p);

    char *name = strdup(p->cur->lexeme);
    parser_expect(p, TOK_IDENT);
    parser_expect(p, TOK_LBRACE);

    AstNode *e = ast_new_enum_def(name, line, col);
    free(name);

    if (!parser_check(p, TOK_RBRACE)) {
        do {
            char *vname = strdup(p->cur->lexeme);
            parser_expect(p, TOK_IDENT);
            AstNode *init = NULL;
            if (parser_match(p, TOK_ASSIGN))
                init = parse_expr(p);
            ast_enum_add_variant(e, vname, init);
            free(vname);
        } while (parser_match(p, TOK_COMMA));
    }

    parser_expect(p, TOK_RBRACE);
    return e;
}

static AstNode *parse_union_def(Parser *p) {
    int line = p->cur->line, col = p->cur->col;
    parser_advance(p);

    char *name = strdup(p->cur->lexeme);
    parser_expect(p, TOK_IDENT);
    parser_expect(p, TOK_LBRACE);

    AstNode *u = ast_new_union_def(name, line, col);
    free(name);

    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        char *fname = strdup(p->cur->lexeme);
        parser_expect(p, TOK_IDENT);
        parser_expect(p, TOK_COLON);
        MioType *ftype = parse_type(p);
        parser_expect(p, TOK_SEMICOLON);
        ast_union_add_field(u, fname, ftype);
        free(fname);
    }

    parser_expect(p, TOK_RBRACE);
    return u;
}

void parser_add_macro(Parser *p, const char *name, const char *value) {
    for (int i = 0; i < p->macro_count; i++) {
        if (strcmp(p->macros[i].name, name) == 0) {
            free(p->macros[i].value);
            p->macros[i].value = strdup(value);
            return;
        }
    }
    if (p->macro_count >= p->macro_cap) {
        p->macro_cap = p->macro_cap ? p->macro_cap * 2 : 8;
        void *new_macros = realloc(p->macros, sizeof(p->macros[0]) * p->macro_cap);
        if (!new_macros) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        p->macros = new_macros;
    }
    p->macros[p->macro_count].name = strdup(name);
    p->macros[p->macro_count].value = strdup(value);
    p->macro_count++;
}

static bool parser_is_macro_defined(Parser *p, const char *name) {
    for (int i = 0; i < p->macro_count; i++) {
        if (strcmp(p->macros[i].name, name) == 0) return true;
    }
    return false;
}

static void parser_skip_cond_block(Parser *p) {
    while (!parser_check(p, TOK_EOF) &&
           !parser_check(p, TOK_AT_ELIF) &&
           !parser_check(p, TOK_AT_ELSE) &&
           !parser_check(p, TOK_AT_END)) {
        parser_advance(p);
    }
}

static void parser_skip_cond_to_end(Parser *p) {
    int depth = 0;
    while (!parser_check(p, TOK_EOF)) {
        if (parser_check(p, TOK_AT_IF)) depth++;
        if (parser_check(p, TOK_AT_ELIF) && depth == 0) return;
        if (parser_check(p, TOK_AT_ELSE) && depth == 0) return;
        if (parser_check(p, TOK_AT_END)) {
            if (depth == 0) return;
            depth--;
        }
        parser_advance(p);
    }
}

static AstNode *parse_cond_comp(Parser *p, int line, int col) {
    parser_advance(p);
    bool negate = false;
    if (parser_check(p, TOK_NOT)) {
        parser_advance(p);
        negate = true;
    }
    if (parser_check(p, TOK_IDENT)) {
        bool defined = parser_is_macro_defined(p, p->cur->lexeme);
        parser_advance(p);
        bool result = negate ? !defined : defined;

        if (result) {
            AstNode *block = ast_new_block(line, col);
            while (!parser_check(p, TOK_EOF) &&
                   !parser_check(p, TOK_AT_ELIF) &&
                   !parser_check(p, TOK_AT_ELSE) &&
                   !parser_check(p, TOK_AT_END)) {
                AstNode *decl = parse_decl(p);
                if (decl) {
                    ast_block_add(block, decl);
                }
            }
            parser_skip_cond_to_end(p);
            if (parser_check(p, TOK_AT_ELIF)) parse_cond_comp(p, p->cur->line, p->cur->col);
            if (parser_check(p, TOK_AT_ELSE)) {
                parser_advance(p);
                parser_skip_cond_block(p);
            }
            if (parser_check(p, TOK_AT_END)) parser_advance(p);
            return block;
        } else {
            parser_skip_cond_block(p);
            if (parser_check(p, TOK_AT_ELIF)) {
                return parse_cond_comp(p, p->cur->line, p->cur->col);
            }
            if (parser_check(p, TOK_AT_ELSE)) {
                parser_advance(p);
                AstNode *block = ast_new_block(line, col);
                while (!parser_check(p, TOK_EOF) &&
                       !parser_check(p, TOK_AT_ELIF) &&
                       !parser_check(p, TOK_AT_ELSE) &&
                       !parser_check(p, TOK_AT_END)) {
                    AstNode *decl = parse_decl(p);
                    if (decl) {
                        ast_block_add(block, decl);
                    }
                }
                if (parser_check(p, TOK_AT_END)) parser_advance(p);
                return block;
            }
            if (parser_check(p, TOK_AT_END)) parser_advance(p);
            return NULL;
        }
    } else {
        parser_error(p, "expected identifier in @if condition");
        parser_skip_cond_to_end(p);
        if (parser_check(p, TOK_AT_END)) parser_advance(p);
        return NULL;
    }
}

static AstNode *parse_decl(Parser *p) {
    while (parser_match(p, TOK_SEMICOLON));

    switch (p->cur->kind) {
        case TOK_IMPORT: {
            parser_advance(p);
            int line = p->cur->line, col = p->cur->col;

            AstNode *first = parse_single_import(p, line, col);

            if (!parser_match(p, TOK_COMMA)) {
                parser_expect(p, TOK_SEMICOLON);
                return first;
            }

            AstNode *block = ast_new_block(line, col);
            add_import_to_block(block, first);

            do {
                AstNode *import = parse_single_import(p, line, col);
                if (import) add_import_to_block(block, import);
            } while (parser_match(p, TOK_COMMA));

            parser_expect(p, TOK_SEMICOLON);
            return block->block.count > 0 ? block : (ast_free(block), NULL);
        }
        case TOK_VAR:
            return parse_var_decl(p, false, false);
        case TOK_CONST:
            return parse_var_decl(p, true, false);
        case TOK_DEF:
            parser_advance(p);
            return parse_func_def(p, false);
        case TOK_STATIC: {
            parser_advance(p);
            if (parser_match(p, TOK_DEF))
                return parse_func_def(p, true);
            if (parser_match(p, TOK_VAR))
                return parse_var_decl(p, false, true);
            if (parser_match(p, TOK_CONST))
                return parse_var_decl(p, true, true);
            parser_error(p, "expected 'def', 'var', or 'const' after 'static'");
            return NULL;
        }
        case TOK_STRUCT:
            return parse_struct_def(p);
        case TOK_ENUM:
            return parse_enum_def(p);
        case TOK_UNION:
            return parse_union_def(p);
        case TOK_MACRO: {
            parser_advance(p);
            if (!parser_check(p, TOK_IDENT)) {
                parser_error(p, "expected macro name");
                return NULL;
            }
            char *name = strdup(p->cur->lexeme);
            int line = p->cur->line, col = p->cur->col;
            parser_advance(p);

            char *value = NULL;
            if (!parser_check(p, TOK_SEMICOLON)) {
                if (parser_check(p, TOK_STRING_LIT)) {
                    value = strdup(p->cur->lexeme);
                    parser_advance(p);
                } else if (parser_check(p, TOK_INT_LIT) || parser_check(p, TOK_FLOAT_LIT)) {
                    value = strdup(p->cur->lexeme);
                    parser_advance(p);
                } else if (parser_check(p, TOK_TRUE)) {
                    value = strdup("1");
                    parser_advance(p);
                } else if (parser_check(p, TOK_FALSE)) {
                    value = strdup("0");
                    parser_advance(p);
                } else if (parser_check(p, TOK_IDENT)) {
                    value = strdup(p->cur->lexeme);
                    parser_advance(p);
                } else {
                    parser_error(p, "expected macro value or ';'");
                    free(name);
                    return NULL;
                }
            }

            if (parser_is_macro_defined(p, name)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "macro '%s' is already defined", name);
                parser_error(p, buf);
            }
            parser_add_macro(p, name, value ? value : "1");
            AstNode *node = ast_new_macro_def(name, value ? value : "1", line, col);
            free(name);
            free(value);
            parser_expect(p, TOK_SEMICOLON);
            return node;
        }
        case TOK_AT_IF:
            return parse_cond_comp(p, p->cur->line, p->cur->col);
        case TOK_EOF:
            return NULL;
        default: {
            parser_error(p, "expected declaration");
            while (!parser_check(p, TOK_EOF) && !parser_check(p, TOK_SEMICOLON)
                   && !parser_check(p, TOK_RBRACE))
                parser_advance(p);
            parser_advance(p);
            return NULL;
        }
    }
}

Parser *parser_new(Lexer *lexer, const char *filename, char **include_paths, int include_path_count) {
    Parser *p = calloc(1, sizeof(Parser));
    if (!p) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    p->lexer = lexer;
    p->filename = filename;
    p->include_paths = include_paths;
    p->include_path_count = include_path_count;
    p->cur = lexer->current;
    p->peek = lexer->peek;
    return p;
}

AstNode *parser_parse(Parser *p) {
    AstNode *program = ast_new(AST_PROGRAM, 0, 0);
    while (!parser_check(p, TOK_EOF)) {
        AstNode *decl = parse_decl(p);
        if (decl) ast_program_add(program, decl);
    }
    return program;
}

void parser_free(Parser *p) {
    if (!p) return;
    for (int i = 0; i < p->imported_count; i++) {
        free(p->imported_files[i]);
    }
    free(p->imported_files);
    for (int i = 0; i < p->macro_count; i++) {
        free(p->macros[i].name);
        free(p->macros[i].value);
    }
    free(p->macros);
    free(p);
}

int parser_error_count(Parser *p) {
    return p->error_count;
}

#endif