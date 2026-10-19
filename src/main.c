/*
Newt
A small experimental scripting language built in C.
Created by drgropp.
*/

#include "interpreter.h"

static void print_help(void) {
    printf("Newt %s\n", NEWT_VERSION);
    printf("\n");
    printf("Usage:\n");
    printf("  newt.exe <file.nt> [args...]        Run a Newt program\n");
    printf("  newt.exe --run <file.nt> [args...]  Run a Newt program\n");
    printf("  newt.exe --tokens <file.nt>  Print lexer tokens\n");
    printf("  newt.exe --parse <file.nt>   Print the parse tree\n");
    printf("  newt.exe --version           Print the Newt version\n");
    printf("  newt.exe --help              Show this help\n");
    printf("\n");
    printf("Example:\n");
    printf("  newt.exe examples/hello.nt\n");
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "error: could not open %s\n", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: could not read %s\n", path);
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "error: could not read %s\n", path);
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *source = malloc((size_t)file_size + 1);
    if (source == NULL) {
        fprintf(stderr, "error: not enough memory to read %s\n", path);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(source, 1, (size_t)file_size, file);
    source[bytes_read] = '\0';

    if (bytes_read != (size_t)file_size && ferror(file)) {
        fprintf(stderr, "error: could not read %s\n", path);
        free(source);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return source;
}

int main(int argc, char **argv) {
    int print_token_mode = 0;
    int parse_mode = 0;
    int run_mode = 0;
    int success = 1;
    int script_argument_count = 0;
    char **script_arguments = NULL;
    const char *path = NULL;

    if (argc < 2) {
        fprintf(stderr, "error: missing input file\n");
        fprintf(stderr, "usage: newt <file.nt>\n");
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("newt %s\n", NEWT_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "--about") == 0) {
        printf("Newt is a small experimental scripting language built in C.\n");
        printf("Created by drgropp.\n");
        return 0;
    }

    if (strcmp(argv[1], "--tokens") == 0) {
        if (argc < 3) {
            fprintf(stderr, "error: missing input file\n");
            fprintf(stderr, "usage: newt --tokens <file.nt>\n");
            return 1;
        }

        print_token_mode = 1;
        path = argv[2];
    } else if (strcmp(argv[1], "--parse") == 0) {
        if (argc < 3) {
            fprintf(stderr, "error: missing input file\n");
            fprintf(stderr, "usage: newt --parse <file.nt>\n");
            return 1;
        }

        parse_mode = 1;
        path = argv[2];
    } else if (strcmp(argv[1], "--run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "error: missing input file\n");
            fprintf(stderr, "usage: newt --run <file.nt> [args...]\n");
            return 1;
        }

        run_mode = 1;
        path = argv[2];
        script_argument_count = argc - 3;
        script_arguments = argv + 3;
    } else {
        run_mode = 1;
        path = argv[1];
        script_argument_count = argc - 2;
        script_arguments = argv + 2;
    }

    char *source = read_file(path);
    if (source == NULL) {
        return 1;
    }

    if (print_token_mode) {
        success = print_tokens(path, source);
    } else if (parse_mode) {
        success = print_parse_tree(path, source);
    } else if (run_mode) {
        success = run_program(path,
                              source,
                              script_argument_count,
                              script_arguments);
    }

    free(source);

    return success ? 0 : 1;
}
