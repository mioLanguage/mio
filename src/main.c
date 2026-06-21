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
    fprintf(stderr, "Usage: %s <input.mio> [-o <output.c>]\n", prog);
    fprintf(stderr, "  Mio compiler - compiles Mio source to C code\n");
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
		printf("mio version 2.0.7\n");
        return 0;
    }

    const char *input_file = NULL;
    const char *output_file = NULL;
    bool output_file_allocated = false;

    for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (!input_file) {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        usage(argv[0]);
        return 1;
    }

    char *source = read_file(input_file);
    if (!source) return 1;

    Lexer *lexer = lexer_new(source, input_file);
    Parser *parser = parser_new(lexer, input_file);
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

    return 0;
}