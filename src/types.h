#ifndef MIO_TYPES_H
#define MIO_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TYPE_VOID,
    TYPE_I32,
    TYPE_I64,
    TYPE_U32,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_UNION,
    TYPE_ARRAY,
    TYPE_FUNC,
} MioTypeKind;

typedef struct MioType {
    MioTypeKind kind;
    char *name;
    int array_size;
    struct MioType *base_type;
    struct MioType **param_types;
    int param_count;
} MioType;

MioType *mio_type_new(MioTypeKind kind);
MioType *mio_type_new_named(MioTypeKind kind, const char *name);
MioType *mio_type_new_array(MioType *base, int size);
MioType *mio_type_clone(MioType *type);
void mio_type_free(MioType *type);
const char *mio_type_c_name(MioType *type);

// ===== Implementation =====

MioType *mio_type_new(MioTypeKind kind) {
    MioType *t = calloc(1, sizeof(MioType));
    if (!t) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    t->kind = kind;
    t->array_size = 0;
    return t;
}

MioType *mio_type_new_named(MioTypeKind kind, const char *name) {
    MioType *t = mio_type_new(kind);
    if (name) {
        t->name = strdup(name);
        if (!t->name) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
    } else {
        t->name = NULL;
    }
    return t;
}

MioType *mio_type_new_array(MioType *base, int size) {
    MioType *t = mio_type_new(TYPE_ARRAY);
    t->base_type = base;
    t->array_size = size;
    return t;
}

MioType *mio_type_clone(MioType *type) {
    if (!type) return NULL;
    MioType *t = calloc(1, sizeof(MioType));
    if (!t) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(1);
    }
    t->kind = type->kind;
    if (type->name) {
        t->name = strdup(type->name);
        if (!t->name) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(1);
        }
    } else {
        t->name = NULL;
    }
    t->array_size = type->array_size;
    if (type->base_type) t->base_type = mio_type_clone(type->base_type);
    return t;
}

void mio_type_free(MioType *type) {
    if (!type) return;
    free(type->name);
    if (type->base_type) mio_type_free(type->base_type);
    if (type->param_types) {
        for (int i = 0; i < type->param_count; i++)
            mio_type_free(type->param_types[i]);
        free(type->param_types);
    }
    free(type);
}

const char *mio_type_c_name(MioType *type) {
    if (!type) return "void";
    switch (type->kind) {
        case TYPE_VOID:  return "void";
        case TYPE_I32:   return "int32_t";
        case TYPE_I64:   return "int64_t";
        case TYPE_U32:   return "uint32_t";
        case TYPE_U64:   return "uint64_t";
        case TYPE_F32:   return "float";
        case TYPE_F64:   return "double";
        case TYPE_BOOL:  return "bool";
        case TYPE_CHAR:  return "char";
        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_UNION:
            return type->name ? type->name : "void";
        default: return "void";
    }
}

#endif