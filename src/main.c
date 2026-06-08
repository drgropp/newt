#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEWT_VERSION "0.0.1"

static void print_help(void) {
    printf("Newt %s\n", NEWT_VERSION);
    printf("\n");
    printf("Usage:\n");
    printf("  newt <file.nt>\n");
    printf("  newt --help\n");
    printf("  newt --version\n");
    printf("\n");
    printf("Phase 0: reads and prints .nt source files.\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("error: missing input file\n");
        printf("usage: newt <file.nt>\n");
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("Newt %s\n", NEWT_VERSION);
        return 0;
    }

    const char *path = argv[1];

    FILE *file = fopen(path, "r");

    if (file == NULL) {
        printf("error: could not open %s\n", path);
        return 1;
    }

    int ch;
    int last_ch = '\n';

    while ((ch = fgetc(file)) != EOF) {
        putchar(ch);
        last_ch = ch;
    }

    if (last_ch != '\n') {
        putchar('\n');
    }

    fclose(file);

    return 0;
}