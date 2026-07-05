#ifndef MIO_AST_H
#define MIO_AST_H

#include "types.h"
#include "token.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum {
    AST_PROGRAM,
    AST_IMPORT,
    AST_VAR_DECL,
    AST_CONST_DECL,
    AST_FUNC_DEF,
    AST_STRUCT_DEF,
    AST_ENUM_DEF,
    AST_UNION_DEF,
    AST_BLOCK,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_GOTO_STMT,
    AST_LABEL_STMT,
    AST_RETURN_STMT,
    AST_EXPR_STMT,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_INDEX_EXPR,
    AST_MEMBER_EXPR,
    AST_IDENT_EXPR,
    AST_INT_LIT,
    AST_FLOAT_LIT,
    AST_STRING_LIT,
    AST_BOOL_LIT,
    AST_CHAR_LIT,
    AST_ARRAY_LIT,
    AST_CAST_EXPR,
    AST_ASSIGN_EXPR,
    AST_MACRO_DEF,
} AstNodeKind;

typedef struct AstNode AstNode;

struct AstNode {
    AstNodeKind kind;
    MioType *type;
    int line;
    int col;

    union {
        struct { AstNode **nodes; int count; int cap; } program;
        struct { char *path; } import;
        struct {
            char *name;
            MioType *var_type;
            AstNode *init;
            bool is_static;
        } var_decl;
        struct {
            char *name;
            MioType *var_type;
            AstNode *init;
            bool is_static;
        } const_decl;
        struct {
            char *name;
            MioType *return_type;
            struct { char *name; MioType *type; } *params;
            int param_count;
            AstNode *body;
            bool is_static;
            bool is_operator;
            char *op_name;
            char *struct_name;
            struct { char *name; AstNode *expr; } *init_list;
            int init_count;
        } func_def;
        struct {
            char *name;
            struct { char *name; MioType *type; AstNode *init; } *fields;
            int field_count;
            AstNode **methods;
            int method_count;
        } struct_def;
        struct {
            char *name;
            struct { char *name; AstNode *init; } *variants;
            int variant_count;
        } enum_def;
        struct {
            char *name;
            struct { char *name; MioType *type; } *fields;
            int field_count;
        } union_def;
        struct { AstNode **stmts; int count; int cap; } block;
        struct {
            AstNode *cond;
            AstNode *then_body;
            AstNode *elif_conds;
            AstNode *elif_bodies;
            AstNode *else_body;
            AstNode **elif_list;
            int elif_count;
        } if_stmt;
        struct { AstNode *cond; AstNode *body; } while_stmt;
        struct {
            AstNode *init;
            AstNode *cond;
            AstNode *update;
            AstNode *body;
        } for_stmt;
        struct { char *label; } goto_stmt;
        struct { char *label; } label_stmt;
        struct { AstNode *value; } return_stmt;
        struct { AstNode *expr; } expr_stmt;
        struct { AstNode *left; TokenKind op; AstNode *right; } binary;
        struct { TokenKind op; AstNode *operand; } unary;
        struct { AstNode *callee; AstNode **args; int arg_count; } call;
        struct { AstNode *base; AstNode *index; } index_expr;
        struct { AstNode *base; char *member; bool arrow; } member;
        struct { char *name; } ident;
        struct { int64_t value; } int_lit;
        struct { double value; } float_lit;
        struct { char *value; } string_lit;
        struct { bool value; } bool_lit;
        struct { char value; } char_lit;
        struct { AstNode **elements; int count; } array_lit;
        struct { MioType *target_type; AstNode *expr; } cast_expr;
        struct { AstNode *left; AstNode *right; } assign;
        struct { char *name; char *value; } macro_def;
    };
};

AstNode *ast_new(AstNodeKind kind, int line, int col);
void ast_free(AstNode *node);
void ast_program_add(AstNode *program, AstNode *node);
void ast_block_add(AstNode *block, AstNode *stmt);

AstNode *ast_new_import(const char *path, int line, int col);
AstNode *ast_new_var_decl(const char *name, MioType *type, AstNode *init, bool is_static, int line, int col);
AstNode *ast_new_const_decl(const char *name, MioType *type, AstNode *init, bool is_static, int line, int col);
AstNode *ast_new_func_def(const char *name, MioType *return_type, AstNode *body, bool is_static, int line, int col);
AstNode *ast_new_struct_def(const char *name, int line, int col);
AstNode *ast_new_enum_def(const char *name, int line, int col);
AstNode *ast_new_union_def(const char *name, int line, int col);
AstNode *ast_new_block(int line, int col);
AstNode *ast_new_if(AstNode *cond, AstNode *then_body, AstNode *else_body, int line, int col);
AstNode *ast_new_while(AstNode *cond, AstNode *body, int line, int col);
AstNode *ast_new_for(AstNode *init, AstNode *cond, AstNode *update, AstNode *body, int line, int col);
AstNode *ast_new_break(int line, int col);
AstNode *ast_new_continue(int line, int col);
AstNode *ast_new_goto(const char *label, int line, int col);
AstNode *ast_new_label(const char *label, int line, int col);
AstNode *ast_new_return(AstNode *value, int line, int col);
AstNode *ast_new_expr_stmt(AstNode *expr, int line, int col);
AstNode *ast_new_binary(AstNode *left, TokenKind op, AstNode *right, int line, int col);
AstNode *ast_new_unary(TokenKind op, AstNode *operand, int line, int col);
AstNode *ast_new_call(AstNode *callee, int line, int col);
AstNode *ast_new_index(AstNode *base, AstNode *index, int line, int col);
AstNode *ast_new_member(AstNode *base, const char *member, bool arrow, int line, int col);
AstNode *ast_new_ident(const char *name, int line, int col);
AstNode *ast_new_int_lit(int64_t value, int line, int col);
AstNode *ast_new_float_lit(double value, int line, int col);
AstNode *ast_new_string_lit(const char *value, int line, int col);
AstNode *ast_new_bool_lit(bool value, int line, int col);
AstNode *ast_new_char_lit(char value, int line, int col);
AstNode *ast_new_array_lit(int line, int col);
AstNode *ast_new_cast(MioType *type, AstNode *expr, int line, int col);
AstNode *ast_new_assign(AstNode *left, AstNode *right, int line, int col);
AstNode *ast_new_macro_def(const char *name, const char *value, int line, int col);

void ast_call_add_arg(AstNode *call, AstNode *arg);
void ast_array_add(AstNode *array, AstNode *elem);
void ast_struct_add_field(AstNode *s, const char *name, MioType *type, AstNode *init);
void ast_struct_add_method(AstNode *s, AstNode *method);
void ast_enum_add_variant(AstNode *e, const char *name, AstNode *init);
void ast_union_add_field(AstNode *u, const char *name, MioType *type);
void ast_func_add_param(AstNode *func, const char *name, MioType *type);
void ast_func_add_init(AstNode *func, const char *field_name, AstNode *expr);
void ast_if_add_elif(AstNode *if_stmt, AstNode *cond, AstNode *body);

// ===== Implementation =====

AstNode *ast_new(AstNodeKind kind, int line, int col) {
    AstNode *n = calloc(1, sizeof(AstNode));
    if (!n) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    n->kind = kind;
    n->line = line;
    n->col = col;
    return n;
}

void ast_free(AstNode *node) {
    if (!node) return;
    switch (node->kind) {
        case AST_PROGRAM:
            for (int i = 0; i < node->program.count; i++)
                ast_free(node->program.nodes[i]);
            free(node->program.nodes);
            break;
        case AST_IMPORT:
            free(node->import.path);
            break;
        case AST_VAR_DECL:
            free(node->var_decl.name);
            mio_type_free(node->var_decl.var_type);
            ast_free(node->var_decl.init);
            break;
        case AST_CONST_DECL:
            free(node->const_decl.name);
            mio_type_free(node->const_decl.var_type);
            ast_free(node->const_decl.init);
            break;
        case AST_FUNC_DEF:
            free(node->func_def.name);
            mio_type_free(node->func_def.return_type);
            for (int i = 0; i < node->func_def.param_count; i++) {
                free(node->func_def.params[i].name);
                mio_type_free(node->func_def.params[i].type);
            }
            free(node->func_def.params);
            for (int i = 0; i < node->func_def.init_count; i++) {
                free(node->func_def.init_list[i].name);
                ast_free(node->func_def.init_list[i].expr);
            }
            free(node->func_def.init_list);
            ast_free(node->func_def.body);
            free(node->func_def.op_name);
            free(node->func_def.struct_name);
            break;
        case AST_STRUCT_DEF:
            free(node->struct_def.name);
            for (int i = 0; i < node->struct_def.field_count; i++) {
                free(node->struct_def.fields[i].name);
                mio_type_free(node->struct_def.fields[i].type);
                ast_free(node->struct_def.fields[i].init);
            }
            free(node->struct_def.fields);
            for (int i = 0; i < node->struct_def.method_count; i++)
                ast_free(node->struct_def.methods[i]);
            free(node->struct_def.methods);
            break;
        case AST_ENUM_DEF:
            free(node->enum_def.name);
            for (int i = 0; i < node->enum_def.variant_count; i++) {
                free(node->enum_def.variants[i].name);
                ast_free(node->enum_def.variants[i].init);
            }
            free(node->enum_def.variants);
            break;
        case AST_UNION_DEF:
            free(node->union_def.name);
            for (int i = 0; i < node->union_def.field_count; i++) {
                free(node->union_def.fields[i].name);
                mio_type_free(node->union_def.fields[i].type);
            }
            free(node->union_def.fields);
            break;
        case AST_BLOCK:
            for (int i = 0; i < node->block.count; i++)
                ast_free(node->block.stmts[i]);
            free(node->block.stmts);
            break;
        case AST_IF_STMT:
            ast_free(node->if_stmt.cond);
            ast_free(node->if_stmt.then_body);
            ast_free(node->if_stmt.else_body);
            if (node->if_stmt.elif_list) {
                for (int i = 0; i < node->if_stmt.elif_count * 2; i++)
                    ast_free(node->if_stmt.elif_list[i]);
                free(node->if_stmt.elif_list);
            }
            break;
        case AST_WHILE_STMT:
            ast_free(node->while_stmt.cond);
            ast_free(node->while_stmt.body);
            break;
        case AST_FOR_STMT:
            ast_free(node->for_stmt.init);
            ast_free(node->for_stmt.cond);
            ast_free(node->for_stmt.update);
            ast_free(node->for_stmt.body);
            break;
        case AST_GOTO_STMT:
            free(node->goto_stmt.label);
            break;
        case AST_LABEL_STMT:
            free(node->label_stmt.label);
            break;
        case AST_RETURN_STMT:
            ast_free(node->return_stmt.value);
            break;
        case AST_EXPR_STMT:
            ast_free(node->expr_stmt.expr);
            break;
        case AST_BINARY_EXPR:
            ast_free(node->binary.left);
            ast_free(node->binary.right);
            break;
        case AST_UNARY_EXPR:
            ast_free(node->unary.operand);
            break;
        case AST_CALL_EXPR:
            ast_free(node->call.callee);
            for (int i = 0; i < node->call.arg_count; i++)
                ast_free(node->call.args[i]);
            free(node->call.args);
            break;
        case AST_INDEX_EXPR:
            ast_free(node->index_expr.base);
            ast_free(node->index_expr.index);
            break;
        case AST_MEMBER_EXPR:
            ast_free(node->member.base);
            free(node->member.member);
            break;
        case AST_IDENT_EXPR:
            free(node->ident.name);
            break;
        case AST_STRING_LIT:
            free(node->string_lit.value);
            break;
        case AST_ARRAY_LIT:
            for (int i = 0; i < node->array_lit.count; i++)
                ast_free(node->array_lit.elements[i]);
            free(node->array_lit.elements);
            break;
        case AST_CAST_EXPR:
            mio_type_free(node->cast_expr.target_type);
            ast_free(node->cast_expr.expr);
            break;
        case AST_ASSIGN_EXPR:
            ast_free(node->assign.left);
            ast_free(node->assign.right);
            break;
        case AST_MACRO_DEF:
            free(node->macro_def.name);
            free(node->macro_def.value);
            break;
        default:
            break;
    }
    mio_type_free(node->type);
    free(node);
}

void ast_program_add(AstNode *program, AstNode *node) {
    if (program->program.count >= program->program.cap) {
        int new_cap = program->program.cap ? program->program.cap * 2 : 8;
        if (new_cap < program->program.cap || new_cap > INT_MAX / (int)sizeof(AstNode*)) {
            fprintf(stderr, "fatal: program too large\n");
            exit(1);
        }
        AstNode **new_nodes = realloc(program->program.nodes, sizeof(AstNode*) * new_cap);
        if (!new_nodes) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        program->program.nodes = new_nodes;
        program->program.cap = new_cap;
    }
    program->program.nodes[program->program.count++] = node;
}

void ast_block_add(AstNode *block, AstNode *stmt) {
    if (block->block.count >= block->block.cap) {
        int new_cap = block->block.cap ? block->block.cap * 2 : 8;
        if (new_cap < block->block.cap || new_cap > INT_MAX / (int)sizeof(AstNode*)) {
            fprintf(stderr, "fatal: block too large\n");
            exit(1);
        }
        AstNode **new_stmts = realloc(block->block.stmts, sizeof(AstNode*) * new_cap);
        if (!new_stmts) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        block->block.stmts = new_stmts;
        block->block.cap = new_cap;
    }
    block->block.stmts[block->block.count++] = stmt;
}

AstNode *ast_new_import(const char *path, int line, int col) {
    AstNode *n = ast_new(AST_IMPORT, line, col);
    n->import.path = strdup(path);
    return n;
}

AstNode *ast_new_var_decl(const char *name, MioType *type, AstNode *init, bool is_static, int line, int col) {
    AstNode *n = ast_new(AST_VAR_DECL, line, col);
    n->var_decl.name = strdup(name);
    n->var_decl.var_type = type;
    n->var_decl.init = init;
    n->var_decl.is_static = is_static;
    return n;
}

AstNode *ast_new_const_decl(const char *name, MioType *type, AstNode *init, bool is_static, int line, int col) {
    AstNode *n = ast_new(AST_CONST_DECL, line, col);
    n->const_decl.name = strdup(name);
    n->const_decl.var_type = type;
    n->const_decl.init = init;
    n->const_decl.is_static = is_static;
    return n;
}

AstNode *ast_new_func_def(const char *name, MioType *return_type, AstNode *body, bool is_static, int line, int col) {
    AstNode *n = ast_new(AST_FUNC_DEF, line, col);
    n->func_def.name = strdup(name);
    n->func_def.return_type = return_type;
    n->func_def.body = body;
    n->func_def.is_static = is_static;
    n->func_def.is_operator = false;
    return n;
}

AstNode *ast_new_struct_def(const char *name, int line, int col) {
    AstNode *n = ast_new(AST_STRUCT_DEF, line, col);
    n->struct_def.name = strdup(name);
    return n;
}

AstNode *ast_new_enum_def(const char *name, int line, int col) {
    AstNode *n = ast_new(AST_ENUM_DEF, line, col);
    n->enum_def.name = strdup(name);
    return n;
}

AstNode *ast_new_union_def(const char *name, int line, int col) {
    AstNode *n = ast_new(AST_UNION_DEF, line, col);
    n->union_def.name = strdup(name);
    return n;
}

AstNode *ast_new_block(int line, int col) {
    return ast_new(AST_BLOCK, line, col);
}

AstNode *ast_new_if(AstNode *cond, AstNode *then_body, AstNode *else_body, int line, int col) {
    AstNode *n = ast_new(AST_IF_STMT, line, col);
    n->if_stmt.cond = cond;
    n->if_stmt.then_body = then_body;
    n->if_stmt.else_body = else_body;
    return n;
}

AstNode *ast_new_while(AstNode *cond, AstNode *body, int line, int col) {
    AstNode *n = ast_new(AST_WHILE_STMT, line, col);
    n->while_stmt.cond = cond;
    n->while_stmt.body = body;
    return n;
}

AstNode *ast_new_for(AstNode *init, AstNode *cond, AstNode *update, AstNode *body, int line, int col) {
    AstNode *n = ast_new(AST_FOR_STMT, line, col);
    n->for_stmt.init = init;
    n->for_stmt.cond = cond;
    n->for_stmt.update = update;
    n->for_stmt.body = body;
    return n;
}

AstNode *ast_new_break(int line, int col) {
    return ast_new(AST_BREAK_STMT, line, col);
}

AstNode *ast_new_continue(int line, int col) {
    return ast_new(AST_CONTINUE_STMT, line, col);
}

AstNode *ast_new_goto(const char *label, int line, int col) {
    AstNode *n = ast_new(AST_GOTO_STMT, line, col);
    n->goto_stmt.label = strdup(label);
    return n;
}

AstNode *ast_new_label(const char *label, int line, int col) {
    AstNode *n = ast_new(AST_LABEL_STMT, line, col);
    n->label_stmt.label = strdup(label);
    return n;
}

AstNode *ast_new_return(AstNode *value, int line, int col) {
    AstNode *n = ast_new(AST_RETURN_STMT, line, col);
    n->return_stmt.value = value;
    return n;
}

AstNode *ast_new_expr_stmt(AstNode *expr, int line, int col) {
    AstNode *n = ast_new(AST_EXPR_STMT, line, col);
    n->expr_stmt.expr = expr;
    return n;
}

AstNode *ast_new_binary(AstNode *left, TokenKind op, AstNode *right, int line, int col) {
    AstNode *n = ast_new(AST_BINARY_EXPR, line, col);
    n->binary.left = left;
    n->binary.op = op;
    n->binary.right = right;
    return n;
}

AstNode *ast_new_unary(TokenKind op, AstNode *operand, int line, int col) {
    AstNode *n = ast_new(AST_UNARY_EXPR, line, col);
    n->unary.op = op;
    n->unary.operand = operand;
    return n;
}

AstNode *ast_new_call(AstNode *callee, int line, int col) {
    AstNode *n = ast_new(AST_CALL_EXPR, line, col);
    n->call.callee = callee;
    return n;
}

AstNode *ast_new_index(AstNode *base, AstNode *index, int line, int col) {
    AstNode *n = ast_new(AST_INDEX_EXPR, line, col);
    n->index_expr.base = base;
    n->index_expr.index = index;
    return n;
}

AstNode *ast_new_member(AstNode *base, const char *member, bool arrow, int line, int col) {
    AstNode *n = ast_new(AST_MEMBER_EXPR, line, col);
    n->member.base = base;
    n->member.member = strdup(member);
    n->member.arrow = arrow;
    return n;
}

AstNode *ast_new_ident(const char *name, int line, int col) {
    AstNode *n = ast_new(AST_IDENT_EXPR, line, col);
    n->ident.name = strdup(name);
    return n;
}

AstNode *ast_new_int_lit(int64_t value, int line, int col) {
    AstNode *n = ast_new(AST_INT_LIT, line, col);
    n->int_lit.value = value;
    return n;
}

AstNode *ast_new_float_lit(double value, int line, int col) {
    AstNode *n = ast_new(AST_FLOAT_LIT, line, col);
    n->float_lit.value = value;
    return n;
}

AstNode *ast_new_string_lit(const char *value, int line, int col) {
    AstNode *n = ast_new(AST_STRING_LIT, line, col);
    n->string_lit.value = strdup(value);
    return n;
}

AstNode *ast_new_bool_lit(bool value, int line, int col) {
    AstNode *n = ast_new(AST_BOOL_LIT, line, col);
    n->bool_lit.value = value;
    return n;
}

AstNode *ast_new_char_lit(char value, int line, int col) {
    AstNode *n = ast_new(AST_CHAR_LIT, line, col);
    n->char_lit.value = value;
    return n;
}

AstNode *ast_new_array_lit(int line, int col) {
    return ast_new(AST_ARRAY_LIT, line, col);
}

AstNode *ast_new_cast(MioType *type, AstNode *expr, int line, int col) {
    AstNode *n = ast_new(AST_CAST_EXPR, line, col);
    n->cast_expr.target_type = type;
    n->cast_expr.expr = expr;
    return n;
}

AstNode *ast_new_assign(AstNode *left, AstNode *right, int line, int col) {
    AstNode *n = ast_new(AST_ASSIGN_EXPR, line, col);
    n->assign.left = left;
    n->assign.right = right;
    return n;
}

AstNode *ast_new_macro_def(const char *name, const char *value, int line, int col) {
    AstNode *n = ast_new(AST_MACRO_DEF, line, col);
    n->macro_def.name = strdup(name);
    n->macro_def.value = value ? strdup(value) : strdup("1");
    if (!n->macro_def.name || !n->macro_def.value) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    return n;
}

void ast_call_add_arg(AstNode *call, AstNode *arg) {
    AstNode **new_args = realloc(call->call.args, sizeof(AstNode*) * (call->call.arg_count + 1));
    if (!new_args) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    call->call.args = new_args;
    call->call.args[call->call.arg_count++] = arg;
}

void ast_array_add(AstNode *array, AstNode *elem) {
    AstNode **new_elements = realloc(array->array_lit.elements,
        sizeof(AstNode*) * (array->array_lit.count + 1));
    if (!new_elements) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    array->array_lit.elements = new_elements;
    array->array_lit.elements[array->array_lit.count++] = elem;
}

void ast_struct_add_field(AstNode *s, const char *name, MioType *type, AstNode *init) {
    int i = s->struct_def.field_count++;
    void *new_fields = realloc(s->struct_def.fields,
        sizeof(s->struct_def.fields[0]) * s->struct_def.field_count);
    if (!new_fields) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    s->struct_def.fields = new_fields;
    s->struct_def.fields[i].name = strdup(name);
    if (!s->struct_def.fields[i].name) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    s->struct_def.fields[i].type = type;
    s->struct_def.fields[i].init = init;
}

void ast_struct_add_method(AstNode *s, AstNode *method) {
    int i = s->struct_def.method_count++;
    AstNode **new_methods = realloc(s->struct_def.methods,
        sizeof(AstNode*) * s->struct_def.method_count);
    if (!new_methods) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    s->struct_def.methods = new_methods;
    s->struct_def.methods[i] = method;
}

void ast_enum_add_variant(AstNode *e, const char *name, AstNode *init) {
    int i = e->enum_def.variant_count++;
    void *new_variants = realloc(e->enum_def.variants,
        sizeof(e->enum_def.variants[0]) * e->enum_def.variant_count);
    if (!new_variants) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    e->enum_def.variants = new_variants;
    e->enum_def.variants[i].name = strdup(name);
    if (!e->enum_def.variants[i].name) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    e->enum_def.variants[i].init = init;
}

void ast_union_add_field(AstNode *u, const char *name, MioType *type) {
    int i = u->union_def.field_count++;
    void *new_fields = realloc(u->union_def.fields,
        sizeof(u->union_def.fields[0]) * u->union_def.field_count);
    if (!new_fields) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    u->union_def.fields = new_fields;
    u->union_def.fields[i].name = strdup(name);
    if (!u->union_def.fields[i].name) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    u->union_def.fields[i].type = type;
}

void ast_func_add_param(AstNode *func, const char *name, MioType *type) {
    int i = func->func_def.param_count++;
    void *new_params = realloc(func->func_def.params,
        sizeof(func->func_def.params[0]) * func->func_def.param_count);
    if (!new_params) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    func->func_def.params = new_params;
    func->func_def.params[i].name = strdup(name);
    if (!func->func_def.params[i].name) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    func->func_def.params[i].type = type;
}

void ast_func_add_init(AstNode *func, const char *field_name, AstNode *expr) {
    int i = func->func_def.init_count++;
    void *new_init_list = realloc(func->func_def.init_list,
        sizeof(func->func_def.init_list[0]) * func->func_def.init_count);
    if (!new_init_list) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    func->func_def.init_list = new_init_list;
    func->func_def.init_list[i].name = strdup(field_name);
    if (!func->func_def.init_list[i].name) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    func->func_def.init_list[i].expr = expr;
}

void ast_if_add_elif(AstNode *if_stmt, AstNode *cond, AstNode *body) {
    int i = if_stmt->if_stmt.elif_count++;
    AstNode **new_elif_list = realloc(if_stmt->if_stmt.elif_list,
        sizeof(AstNode*) * if_stmt->if_stmt.elif_count * 2);
    if (!new_elif_list) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    if_stmt->if_stmt.elif_list = new_elif_list;
    if_stmt->if_stmt.elif_list[i * 2] = cond;
    if_stmt->if_stmt.elif_list[i * 2 + 1] = body;
}

#endif