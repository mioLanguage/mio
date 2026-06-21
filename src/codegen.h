#ifndef MIO_CODEGEN_H
#define MIO_CODEGEN_H

#include "ast.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

void codegen_generate(AstNode *program, const char *input_file, FILE *output);

// ===== Implementation =====

static FILE *out;
static int indent_level = 0;
static const char *current_struct = NULL;
static bool in_constructor = false;

static const char **struct_names = NULL;
static int struct_count = 0;
static int struct_capacity = 0;

static bool is_struct_name(const char *name) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(struct_names[i], name) == 0) return true;
    }
    return false;
}

static void add_struct_name(const char *name) {
    if (is_struct_name(name)) return;
    if (struct_count >= struct_capacity) {
        int new_cap = struct_capacity ? struct_capacity * 2 : 16;
        const char **new_names = realloc(struct_names, sizeof(const char*) * new_cap);
        if (!new_names) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        struct_names = new_names;
        struct_capacity = new_cap;
    }
    struct_names[struct_count++] = name;
}

static const char **enum_names = NULL;
static int enum_count = 0;
static int enum_capacity = 0;

static bool is_enum_name(const char *name) {
    for (int i = 0; i < enum_count; i++) {
        if (strcmp(enum_names[i], name) == 0) return true;
    }
    return false;
}

static void add_enum_name(const char *name) {
    if (is_enum_name(name)) return;
    if (enum_count >= enum_capacity) {
        int new_cap = enum_capacity ? enum_capacity * 2 : 16;
        const char **new_names = realloc(enum_names, sizeof(const char*) * new_cap);
        if (!new_names) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        enum_names = new_names;
        enum_capacity = new_cap;
    }
    enum_names[enum_count++] = name;
}

typedef struct {
    const char *name;
    const char *enum_name;
} EnumVariant;

static EnumVariant *enum_variants = NULL;
static int enum_variant_count = 0;
static int enum_variant_capacity = 0;

static void add_enum_variant(const char *enum_name, const char *variant_name) {
    if (enum_variant_count >= enum_variant_capacity) {
        int new_cap = enum_variant_capacity ? enum_variant_capacity * 2 : 32;
        EnumVariant *new_variants = realloc(enum_variants, sizeof(EnumVariant) * new_cap);
        if (!new_variants) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        enum_variants = new_variants;
        enum_variant_capacity = new_cap;
    }
    enum_variants[enum_variant_count].name = variant_name;
    enum_variants[enum_variant_count].enum_name = enum_name;
    enum_variant_count++;
}

static const char *lookup_enum_variant(const char *name) {
    const char *found = NULL;
    for (int i = 0; i < enum_variant_count; i++) {
        if (strcmp(enum_variants[i].name, name) == 0) {
            if (found && strcmp(found, enum_variants[i].enum_name) != 0) {
                fprintf(stderr, "error: ambiguous enum variant '%s' (found in '%s' and '%s')\n",
                    name, found, enum_variants[i].enum_name);
                return NULL;
            }
            found = enum_variants[i].enum_name;
        }
    }
    return found;
}

typedef struct {
    const char *name;
    MioType *type;
} VarEntry;

static VarEntry *var_table = NULL;
static int var_count = 0;
static int var_capacity = 0;

static void register_var(const char *name, MioType *type) {
    if (!type || !type->name) return;
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) {
            var_table[i].type = type;
            return;
        }
    }
    if (var_count >= var_capacity) {
        int new_cap = var_capacity ? var_capacity * 2 : 32;
        VarEntry *new_table = realloc(var_table, sizeof(VarEntry) * new_cap);
        if (!new_table) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
        var_table = new_table;
        var_capacity = new_cap;
    }
    var_table[var_count].name = name;
    var_table[var_count].type = type;
    var_count++;
}

static const char *lookup_var_type_name(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0 && var_table[i].type)
            return var_table[i].type->name;
    }
    return NULL;
}

static bool is_var_name(const char *name) {
    return lookup_var_type_name(name) != NULL;
}

static void clear_var_table(void) {
    var_count = 0;
}

static const char *resolve_struct_name(AstNode *base_expr) {
    if (current_struct) return current_struct;
    if (base_expr && base_expr->kind == AST_IDENT_EXPR) {
        const char *tn = lookup_var_type_name(base_expr->ident.name);
        if (tn) return tn;
    }
    return "unknown";
}

static void emit_indent(void) {
    for (int i = 0; i < indent_level; i++)
        fprintf(out, "    ");
}

static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    emit_indent();
    vfprintf(out, fmt, args);
    va_end(args);
}

static void emit_raw(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
}

static void emit_escaped_string(const char *str) {
    fprintf(out, "\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '\n': fprintf(out, "\\n"); break;
            case '\t': fprintf(out, "\\t"); break;
            case '\r': fprintf(out, "\\r"); break;
            case '\\': fprintf(out, "\\\\"); break;
            case '"':  fprintf(out, "\\\""); break;
            case '\0': fprintf(out, "\\0"); break;
            default:   fprintf(out, "%c", *p); break;
        }
    }
    fprintf(out, "\"");
}

static const char *type_to_c(MioType *type) {
    if (!type) return "void";
    switch (type->kind) {
        case TYPE_VOID: return "void";
        case TYPE_I32:  return "int32_t";
        case TYPE_I64:  return "int64_t";
        case TYPE_U32:  return "uint32_t";
        case TYPE_U64:  return "uint64_t";
        case TYPE_F32:  return "float";
        case TYPE_F64:  return "double";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_ARRAY: return type_to_c(type->base_type);
        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_UNION:
            return type->name ? type->name : "void";
        default: return "void";
    }
}

static void gen_expr(AstNode *node);
static void gen_stmt(AstNode *node);
static void gen_block(AstNode *node);
static void gen_type_decl(AstNode *node);

static const char *op_to_c(TokenKind op) {
    switch (op) {
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_EQ: return "==";
        case TOK_NEQ: return "!=";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_LTE: return "<=";
        case TOK_GTE: return ">=";
        case TOK_AND: return "&&";
        case TOK_OR: return "||";
        case TOK_BIT_AND: return "&";
        case TOK_BIT_OR: return "|";
        case TOK_BIT_XOR: return "^";
        case TOK_LSHIFT: return "<<";
        case TOK_RSHIFT: return ">>";
        default: return "?";
    }
}

static const char *op_name_to_c(const char *op) {
    if (!op) return "unknown";
    if (strcmp(op, "+") == 0) return "add";
    if (strcmp(op, "-") == 0) return "sub";
    if (strcmp(op, "*") == 0) return "mul";
    if (strcmp(op, "/") == 0) return "div";
    if (strcmp(op, "%") == 0) return "mod";
    if (strcmp(op, "==") == 0) return "eq";
    if (strcmp(op, "!=") == 0) return "neq";
    if (strcmp(op, "<") == 0) return "lt";
    if (strcmp(op, ">") == 0) return "gt";
    if (strcmp(op, "<=") == 0) return "lte";
    if (strcmp(op, ">=") == 0) return "gte";
    if (strcmp(op, "&&") == 0) return "land";
    if (strcmp(op, "||") == 0) return "lor";
    if (strcmp(op, "&") == 0) return "band";
    if (strcmp(op, "|") == 0) return "bor";
    if (strcmp(op, "^") == 0) return "bxor";
    if (strcmp(op, "<<") == 0) return "lshift";
    if (strcmp(op, ">>") == 0) return "rshift";
    if (strcmp(op, "!") == 0) return "not";
    if (strcmp(op, "~") == 0) return "bnot";
    if (strcmp(op, "-u") == 0) return "neg";
    return op;
}

static void gen_expr(AstNode *node) {
    if (!node) return;

    switch (node->kind) {
        case AST_INT_LIT:
            fprintf(out, "%lld", (long long)node->int_lit.value);
            break;
        case AST_FLOAT_LIT:
            fprintf(out, "%g", node->float_lit.value);
            break;
        case AST_STRING_LIT:
            emit_escaped_string(node->string_lit.value);
            break;
        case AST_CHAR_LIT:
            fprintf(out, "'");
            switch (node->char_lit.value) {
                case '\n': fprintf(out, "\\n"); break;
                case '\t': fprintf(out, "\\t"); break;
                case '\r': fprintf(out, "\\r"); break;
                case '\\': fprintf(out, "\\\\"); break;
                case '\'': fprintf(out, "\\'"); break;
                case '\0': fprintf(out, "\\0"); break;
                default:   fprintf(out, "%c", node->char_lit.value); break;
            }
            fprintf(out, "'");
            break;
        case AST_BOOL_LIT:
            fprintf(out, "%s", node->bool_lit.value ? "true" : "false");
            break;
        case AST_IDENT_EXPR: {
            const char *name = node->ident.name;
            if (!is_var_name(name)) {
                const char *enum_name = lookup_enum_variant(name);
                if (enum_name) {
                    fprintf(out, "%s_%s", enum_name, name);
                    break;
                }
            }
            fprintf(out, "%s", name);
            break;
        }
        case AST_BINARY_EXPR:
            fprintf(out, "(");
            gen_expr(node->binary.left);
            fprintf(out, " %s ", op_to_c(node->binary.op));
            gen_expr(node->binary.right);
            fprintf(out, ")");
            break;
        case AST_UNARY_EXPR:
            switch (node->unary.op) {
                case TOK_MINUS:
                    fprintf(out, "-");
                    gen_expr(node->unary.operand);
                    break;
                case TOK_NOT:
                    fprintf(out, "!");
                    gen_expr(node->unary.operand);
                    break;
                case TOK_BIT_NOT:
                    fprintf(out, "~");
                    gen_expr(node->unary.operand);
                    break;
                case TOK_STAR:
                    fprintf(out, "*");
                    gen_expr(node->unary.operand);
                    break;
                case TOK_BIT_AND:
                    fprintf(out, "&");
                    gen_expr(node->unary.operand);
                    break;
                default:
                    gen_expr(node->unary.operand);
                    break;
            }
            break;
        case AST_CALL_EXPR: {
            if (node->call.callee->kind == AST_MEMBER_EXPR &&
                node->call.callee->member.arrow) {
                AstNode *member = node->call.callee;
                const char *struct_name = resolve_struct_name(member->member.base);
                fprintf(out, "_mio_%s_", struct_name);
                fprintf(out, "%s(", member->member.member);
                gen_expr(member->member.base);
                for (int i = 0; i < node->call.arg_count; i++) {
                    fprintf(out, ", ");
                    gen_expr(node->call.args[i]);
                }
                fprintf(out, ")");
            } else if (node->call.callee->kind == AST_MEMBER_EXPR) {
                AstNode *member = node->call.callee;
                const char *struct_name = resolve_struct_name(member->member.base);
                fprintf(out, "_mio_%s_", struct_name);
                fprintf(out, "%s(&", member->member.member);
                gen_expr(member->member.base);
                for (int i = 0; i < node->call.arg_count; i++) {
                    fprintf(out, ", ");
                    gen_expr(node->call.args[i]);
                }
                fprintf(out, ")");
            } else {
                if (node->call.callee->kind == AST_IDENT_EXPR &&
                    is_struct_name(node->call.callee->ident.name)) {
                    fprintf(out, "_mio_%s_%s",
                        node->call.callee->ident.name,
                        node->call.callee->ident.name);
                } else {
                    gen_expr(node->call.callee);
                }
                fprintf(out, "(");
                for (int i = 0; i < node->call.arg_count; i++) {
                    if (i > 0) fprintf(out, ", ");
                    gen_expr(node->call.args[i]);
                }
                fprintf(out, ")");
            }
            break;
        }
        case AST_INDEX_EXPR:
            gen_expr(node->index_expr.base);
            fprintf(out, "[");
            gen_expr(node->index_expr.index);
            fprintf(out, "]");
            break;
        case AST_MEMBER_EXPR:
            if (node->member.base->kind == AST_IDENT_EXPR &&
                is_enum_name(node->member.base->ident.name)) {
                fprintf(out, "%s_%s", node->member.base->ident.name, node->member.member);
            } else {
                gen_expr(node->member.base);
                bool use_arrow = node->member.arrow;
                if (!use_arrow && !in_constructor &&
                    node->member.base->kind == AST_IDENT_EXPR &&
                    strcmp(node->member.base->ident.name, "this") == 0) {
                    use_arrow = true;
                }
                fprintf(out, "%s%s", use_arrow ? "->" : ".", node->member.member);
            }
            break;
        case AST_ARRAY_LIT:
            fprintf(out, "{");
            for (int i = 0; i < node->array_lit.count; i++) {
                if (i > 0) fprintf(out, ", ");
                gen_expr(node->array_lit.elements[i]);
            }
            fprintf(out, "}");
            break;
        case AST_ASSIGN_EXPR:
            gen_expr(node->assign.left);
            fprintf(out, " = ");
            gen_expr(node->assign.right);
            break;
        case AST_CAST_EXPR:
            fprintf(out, "(%s)(", type_to_c(node->cast_expr.target_type));
            gen_expr(node->cast_expr.expr);
            fprintf(out, ")");
            break;
        default:
            fprintf(out, "/* unknown expr */");
            break;
    }
}

static void gen_stmt(AstNode *node) {
    if (!node) return;

    switch (node->kind) {
        case AST_EXPR_STMT:
            emit("");
            gen_expr(node->expr_stmt.expr);
            fprintf(out, ";\n");
            break;
        case AST_RETURN_STMT:
            emit("return");
            if (node->return_stmt.value) {
                fprintf(out, " ");
                gen_expr(node->return_stmt.value);
            }
            fprintf(out, ";\n");
            break;
        case AST_BLOCK:
            gen_block(node);
            break;
        case AST_IF_STMT:
            emit("if (");
            gen_expr(node->if_stmt.cond);
            fprintf(out, ") ");
            if (node->if_stmt.then_body->kind != AST_BLOCK) {
                fprintf(out, "{\n");
                indent_level++;
                gen_stmt(node->if_stmt.then_body);
                indent_level--;
                emit("}\n");
            } else {
                gen_stmt(node->if_stmt.then_body);
            }
            for (int i = 0; i < node->if_stmt.elif_count; i++) {
                emit("else if (");
                gen_expr(node->if_stmt.elif_list[i * 2]);
                fprintf(out, ") ");
                AstNode *body = node->if_stmt.elif_list[i * 2 + 1];
                if (body->kind != AST_BLOCK) {
                    fprintf(out, "{\n");
                    indent_level++;
                    gen_stmt(body);
                    indent_level--;
                    emit("}\n");
                } else {
                    gen_stmt(body);
                }
            }
            if (node->if_stmt.else_body) {
                emit("else ");
                if (node->if_stmt.else_body->kind != AST_BLOCK) {
                    fprintf(out, "{\n");
                    indent_level++;
                    gen_stmt(node->if_stmt.else_body);
                    indent_level--;
                    emit("}\n");
                } else {
                    gen_stmt(node->if_stmt.else_body);
                }
            }
            break;
        case AST_WHILE_STMT:
            emit("while (");
            gen_expr(node->while_stmt.cond);
            fprintf(out, ") ");
            if (node->while_stmt.body->kind != AST_BLOCK) {
                fprintf(out, "{\n");
                indent_level++;
                gen_stmt(node->while_stmt.body);
                indent_level--;
                emit("}\n");
            } else {
                gen_stmt(node->while_stmt.body);
            }
            break;
        case AST_FOR_STMT:
            emit("for (");
            if (node->for_stmt.init) gen_expr(node->for_stmt.init);
            fprintf(out, "; ");
            if (node->for_stmt.cond) gen_expr(node->for_stmt.cond);
            fprintf(out, "; ");
            if (node->for_stmt.update) gen_expr(node->for_stmt.update);
            fprintf(out, ") ");
            if (node->for_stmt.body->kind != AST_BLOCK) {
                fprintf(out, "{\n");
                indent_level++;
                gen_stmt(node->for_stmt.body);
                indent_level--;
                emit("}\n");
            } else {
                gen_stmt(node->for_stmt.body);
            }
            break;
        case AST_BREAK_STMT:
            emit("break;\n");
            break;
        case AST_CONTINUE_STMT:
            emit("continue;\n");
            break;
        case AST_GOTO_STMT:
            emit("goto %s;\n", node->goto_stmt.label);
            break;
        case AST_LABEL_STMT:
            emit_raw("%s:\n", node->label_stmt.label);
            break;
        case AST_VAR_DECL: {
            emit("%s%s ", node->var_decl.is_static ? "static " : "",
                 node->var_decl.var_type ? type_to_c(node->var_decl.var_type) : "__auto_type");
            fprintf(out, "%s", node->var_decl.name);
            if (node->var_decl.var_type && node->var_decl.var_type->kind == TYPE_ARRAY) {
                fprintf(out, "[%d]", node->var_decl.var_type->array_size);
            }
            if (node->var_decl.init) {
                fprintf(out, " = ");
                gen_expr(node->var_decl.init);
            }
            fprintf(out, ";\n");
            register_var(node->var_decl.name, node->var_decl.var_type);
            break;
        }
        case AST_CONST_DECL: {
            emit("const %s %s",
                 node->const_decl.var_type ? type_to_c(node->const_decl.var_type) : "int",
                 node->const_decl.name);
            if (node->const_decl.var_type && node->const_decl.var_type->kind == TYPE_ARRAY) {
                fprintf(out, "[%d]", node->const_decl.var_type->array_size);
            }
            if (node->const_decl.init) {
                fprintf(out, " = ");
                gen_expr(node->const_decl.init);
            }
            fprintf(out, ";\n");
            break;
        }
        default:
            break;
    }
}

static void gen_block(AstNode *node) {
    if (node->kind != AST_BLOCK) {
        indent_level++;
        gen_stmt(node);
        indent_level--;
        return;
    }
    bool all_decls = true;
    for (int i = 0; i < node->block.count; i++) {
        AstNodeKind k = node->block.stmts[i]->kind;
        if (k != AST_VAR_DECL && k != AST_CONST_DECL) {
            all_decls = false;
            break;
        }
    }
    if (all_decls) {
        if (node->block.count == 0) {
            emit("{\n");
            emit("}\n");
            return;
        }
        for (int i = 0; i < node->block.count; i++)
            gen_stmt(node->block.stmts[i]);
        return;
    }
    emit("{\n");
    indent_level++;
    for (int i = 0; i < node->block.count; i++)
        gen_stmt(node->block.stmts[i]);
    indent_level--;
    emit("}\n");
}

static void gen_func_def(AstNode *node, const char *struct_name) {
    const char *prev_struct = current_struct;
    current_struct = struct_name;
    clear_var_table();

    fprintf(out, "\n");
    if (node->func_def.is_static)
        fprintf(out, "static ");

    bool is_main = (strcmp(node->func_def.name, "main") == 0 && !struct_name);

    if (is_main) {
        fprintf(out, "int main(void) {\n");
        indent_level++;
        gen_block(node->func_def.body);
        emit("return 0;\n");
        indent_level--;
        emit("}\n");
        current_struct = prev_struct;
        return;
    }

    const char *ret = type_to_c(node->func_def.return_type);
    fprintf(out, "%s ", ret);

    if (struct_name && !node->func_def.is_operator && node->func_def.name) {
        fprintf(out, "_mio_%s_%s(", struct_name, node->func_def.name);
    } else if (node->func_def.is_operator && struct_name) {
        fprintf(out, "_mio_%s_operator_%s(",
            struct_name, op_name_to_c(node->func_def.op_name));
    } else {
        fprintf(out, "%s(", node->func_def.name);
    }

    int param_start = 0;
    bool is_constructor = (struct_name && node->func_def.name &&
        strcmp(node->func_def.name, struct_name) == 0);

    if (is_constructor) {
        if (node->func_def.param_count > 0 &&
            strcmp(node->func_def.params[0].name, "this") == 0) {
            param_start = 1;
        } else {
            param_start = 0;
        }
    } else if (struct_name && !node->func_def.is_static) {
        if (node->func_def.is_operator && node->func_def.param_count >= 2) {
            param_start = 0;
        } else if (node->func_def.param_count > 0 &&
            strcmp(node->func_def.params[0].name, "this") == 0) {
            fprintf(out, "%s* this", struct_name);
            param_start = 1;
            if (node->func_def.param_count > 1) fprintf(out, ", ");
        } else {
            fprintf(out, "%s* this", struct_name);
            param_start = 0;
            if (node->func_def.param_count > 0) fprintf(out, ", ");
        }
    }

    for (int i = param_start; i < node->func_def.param_count; i++) {
        if (i > param_start) fprintf(out, ", ");
        fprintf(out, "%s %s",
            type_to_c(node->func_def.params[i].type),
            node->func_def.params[i].name);
    }
    fprintf(out, ") ");

    if (is_constructor) {
        in_constructor = true;
        emit("{\n");
        indent_level++;
        emit("%s this;\n", struct_name);
        for (int i = 0; i < node->func_def.init_count; i++) {
            emit("this.%s = ", node->func_def.init_list[i].name);
            gen_expr(node->func_def.init_list[i].expr);
            fprintf(out, ";\n");
        }
        for (int i = 0; i < node->func_def.body->block.count; i++)
            gen_stmt(node->func_def.body->block.stmts[i]);
        emit("return this;\n");
        indent_level--;
        emit("}\n");
        in_constructor = false;
    } else {
        gen_block(node->func_def.body);
    }

    current_struct = prev_struct;
}

static void gen_type_decl(AstNode *node) {
    switch (node->kind) {
        case AST_STRUCT_DEF: {
            add_struct_name(node->struct_def.name);
            fprintf(out, "typedef struct %s {\n", node->struct_def.name);
            for (int i = 0; i < node->struct_def.field_count; i++) {
                fprintf(out, "    %s %s",
                    type_to_c(node->struct_def.fields[i].type),
                    node->struct_def.fields[i].name);
                if (node->struct_def.fields[i].type->kind == TYPE_ARRAY &&
                    node->struct_def.fields[i].type->array_size > 0) {
                    fprintf(out, "[%d]", node->struct_def.fields[i].type->array_size);
                }
                fprintf(out, ";\n");
            }
            fprintf(out, "} %s;\n", node->struct_def.name);
            break;
        }
        case AST_ENUM_DEF: {
            add_enum_name(node->enum_def.name);
            fprintf(out, "typedef enum {\n");
            for (int i = 0; i < node->enum_def.variant_count; i++) {
                add_enum_variant(node->enum_def.name, node->enum_def.variants[i].name);
                fprintf(out, "    %s_%s",
                    node->enum_def.name, node->enum_def.variants[i].name);
                if (node->enum_def.variants[i].init) {
                    fprintf(out, " = ");
                    gen_expr(node->enum_def.variants[i].init);
                }
                fprintf(out, ",\n");
            }
            fprintf(out, "} %s;\n", node->enum_def.name);
            break;
        }
        case AST_UNION_DEF: {
            fprintf(out, "typedef union %s {\n", node->union_def.name);
            for (int i = 0; i < node->union_def.field_count; i++) {
                fprintf(out, "    %s %s;\n",
                    type_to_c(node->union_def.fields[i].type),
                    node->union_def.fields[i].name);
            }
            fprintf(out, "} %s;\n", node->union_def.name);
            break;
        }
        default:
            break;
    }
}

void codegen_generate(AstNode *program, const char *input_file, FILE *output) {
    out = output;

    fprintf(out, "/* Generated by Mio compiler from %s */\n", input_file);
    fprintf(out, "#include <stdint.h>\n");
    fprintf(out, "#include <stdbool.h>\n");
    fprintf(out, "#include <stddef.h>\n");
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <string.h>\n\n");

    for (int i = 0; i < program->program.count; i++) {
        AstNode *node = program->program.nodes[i];
        if (node->kind == AST_IMPORT) {
            fprintf(out, "#include %s\n", node->import.path);
        } else if (node->kind == AST_BLOCK) {
            for (int j = 0; j < node->block.count; j++) {
                AstNode *child = node->block.stmts[j];
                if (child->kind == AST_IMPORT) {
                    fprintf(out, "#include %s\n", child->import.path);
                }
            }
        }
    }
    fprintf(out, "\n");

    for (int i = 0; i < program->program.count; i++) {
        AstNode *node = program->program.nodes[i];
        if (node->kind == AST_STRUCT_DEF ||
            node->kind == AST_ENUM_DEF ||
            node->kind == AST_UNION_DEF) {
            gen_type_decl(node);
            fprintf(out, "\n");
        }
    }

    for (int i = 0; i < program->program.count; i++) {
        AstNode *node = program->program.nodes[i];
        if (node->kind == AST_STRUCT_DEF) {
            for (int j = 0; j < node->struct_def.method_count; j++) {
                gen_func_def(node->struct_def.methods[j], node->struct_def.name);
            }
        }
    }

    for (int i = 0; i < program->program.count; i++) {
        AstNode *node = program->program.nodes[i];
        if (node->kind == AST_BLOCK) {
            bool all_imports = true;
            for (int j = 0; j < node->block.count; j++) {
                if (node->block.stmts[j]->kind != AST_IMPORT) {
                    all_imports = false;
                    break;
                }
            }
            if (all_imports) continue;
            gen_block(node);
            fprintf(out, "\n");
        }
    }

    for (int i = 0; i < program->program.count; i++) {
        AstNode *node = program->program.nodes[i];
        if (node->kind == AST_FUNC_DEF) {
            gen_func_def(node, NULL);
        }
    }

    bool has_main = false;
    for (int i = 0; i < program->program.count; i++) {
        AstNode *node = program->program.nodes[i];
        if (node->kind == AST_FUNC_DEF &&
            strcmp(node->func_def.name, "main") == 0) {
            has_main = true;
            break;
        }
    }

    if (!has_main) {
        for (int i = 0; i < program->program.count; i++) {
            AstNode *node = program->program.nodes[i];
            if (node->kind == AST_EXPR_STMT ||
                node->kind == AST_VAR_DECL ||
                node->kind == AST_CONST_DECL ||
                node->kind == AST_BLOCK ||
                node->kind == AST_ASSIGN_EXPR) {
                fprintf(out, "\nint main(void) {\n");
                for (int j = i; j < program->program.count; j++) {
                    AstNode *n = program->program.nodes[j];
                    if (n->kind == AST_EXPR_STMT ||
                        n->kind == AST_VAR_DECL ||
                        n->kind == AST_CONST_DECL ||
                        n->kind == AST_BLOCK) {
                        emit("    ");
                        gen_stmt(n);
                    }
                }
                fprintf(out, "    return 0;\n}\n");
                break;
            }
        }
    }
}

#endif