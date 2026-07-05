#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open file '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "fatal: out of memory\n");
        return NULL;
    }
    size_t read = fread(buf, 1, size, f);
    if (read != (size_t)size) {
        fprintf(stderr, "error: failed to read file '%s'\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <input.mio> [-o <output.c>] [-I <include_path>] [-D <macro>[=<value>]]\n", prog);
    fprintf(stderr, "  Mio compiler - compiles Mio source to C code\n");
    fprintf(stderr, "  -o <file>   specify output C file\n");
    fprintf(stderr, "  -I <path>   add include path for .mio file resolution\n");
    fprintf(stderr, "  -D <macro>  define a macro (e.g. -D DEBUG or -D VERSION=2)\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (argc == 2 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
		printf("License: MIT License\n");
        printf("\n");
        printf("MIT License\n");
        printf("\n");
        printf("Copyright (c) 2026 mioLanguage\n");
        printf("\n");
        printf("Permission is hereby granted, free of charge, to any person obtaining a copy\n");
        printf("of this software and associated documentation files (the \"Software\"), to deal\n");
        printf("in the Software without restriction, including without limitation the rights\n");
        printf("to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
        printf("copies of the Software, and to permit persons to whom the Software is\n");
        printf("furnished to do so, subject to the following conditions:\n");
        printf("\n");
        printf("The above copyright notice and this permission notice shall be included in all\n");
        printf("copies or substantial portions of the Software.\n");
        printf("\n");
        printf("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
        printf("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
        printf("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
        printf("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
        printf("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
        printf("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n");
        printf("SOFTWARE.\n");
		printf("mio version 2.1.2\n");
        return 0;
    }

    const char *input_file = NULL;
    const char *output_file = NULL;
    bool output_file_allocated = false;
    char **include_paths = NULL;
    int include_path_count = 0;
    int include_path_cap = 0;
    char **defines = NULL;
    int define_count = 0;
    int define_cap = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            if (include_path_count >= include_path_cap) {
                include_path_cap = include_path_cap ? include_path_cap * 2 : 4;
                char **new_paths = realloc(include_paths, sizeof(char*) * include_path_cap);
                if (!new_paths) {
                    fprintf(stderr, "fatal: out of memory\n");
                    free(include_paths);
                    return 1;
                }
                include_paths = new_paths;
            }
            include_paths[include_path_count++] = strdup(argv[++i]);
        } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            if (define_count >= define_cap) {
                define_cap = define_cap ? define_cap * 2 : 4;
                char **new_defs = realloc(defines, sizeof(char*) * define_cap);
                if (!new_defs) {
                    fprintf(stderr, "fatal: out of memory\n");
                    free(include_paths);
                    free(defines);
                    return 1;
                }
                defines = new_defs;
            }
            defines[define_count++] = strdup(argv[++i]);
        } else if (!input_file) {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        usage(argv[0]);
        for (int i = 0; i < include_path_count; i++) free(include_paths[i]);
        free(include_paths);
        return 1;
    }

    {
        const char *last_sep = NULL;
        for (const char *p = argv[0]; *p; p++) {
            if (*p == '/' || *p == '\\') last_sep = p;
        }
        if (last_sep) {
            int dir_len = (int)(last_sep - argv[0]);
            char *default_include = malloc(dir_len + 12);
            if (default_include) {
                memcpy(default_include, argv[0], dir_len);
                memcpy(default_include + dir_len, "/../include", 11);
                default_include[dir_len + 11] = '\0';
                if (include_path_count >= include_path_cap) {
                    include_path_cap = include_path_cap ? include_path_cap * 2 : 4;
                    char **new_paths = realloc(include_paths, sizeof(char*) * include_path_cap);
                    if (!new_paths) {
                        fprintf(stderr, "fatal: out of memory\n");
                        free(default_include);
                        for (int i = 0; i < include_path_count; i++) free(include_paths[i]);
                        free(include_paths);
                        return 1;
                    }
                    include_paths = new_paths;
                }
                include_paths[include_path_count++] = default_include;
            }
        }
    }

    char *source = read_file(input_file);
    if (!source) return 1;

    Lexer *lexer = lexer_new(source, input_file);
    Parser *parser = parser_new(lexer, input_file, include_paths, include_path_count);

    for (int i = 0; i < define_count; i++) {
        char *eq = strchr(defines[i], '=');
        if (eq) {
            *eq = '\0';
            parser_add_macro(parser, defines[i], eq + 1);
        } else {
            parser_add_macro(parser, defines[i], "1");
        }
    }

    AstNode *program = parser_parse(parser);

    if (parser_error_count(parser) > 0) {
        fprintf(stderr, "%d error(s) found.\n", parser_error_count(parser));
        ast_free(program);
        parser_free(parser);
        lexer_free(lexer);
        free(source);
        return 1;
    }

    if (!output_file) {
        char *dot = strrchr(input_file, '.');
        if (dot && strcmp(dot, ".mio") == 0) {
            int len = dot - input_file;
            char *buf = malloc(len + 3);
            if (!buf) {
                fprintf(stderr, "fatal: out of memory\n");
                ast_free(program);
                parser_free(parser);
                lexer_free(lexer);
                free(source);
                return 1;
            }
            memcpy(buf, input_file, len);
            strcpy(buf + len, ".c");
            output_file = buf;
            output_file_allocated = true;
        } else {
            output_file = "out.c";
        }
    }

    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "error: cannot open output file '%s'\n", output_file);
        ast_free(program);
        parser_free(parser);
        lexer_free(lexer);
        free(source);
        return 1;
    }

    codegen_generate(program, input_file, out);
    fclose(out);

    printf("Compiled '%s' -> '%s'\n", input_file, output_file);

    ast_free(program);
    parser_free(parser);
    lexer_free(lexer);
    free(source);

    if (output_file_allocated) {
        free((void*)output_file);
    }

    for (int i = 0; i < include_path_count; i++) free(include_paths[i]);
    free(include_paths);

    for (int i = 0; i < define_count; i++) free(defines[i]);
    free(defines);

    return 0;
}