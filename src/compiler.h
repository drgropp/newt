/* Native assembly generation; the assembler/linker remain external tools. */
#ifndef NEWT_COMPILER_H
#define NEWT_COMPILER_H
int emit_native_assembly(const char *path, const char *source,
                         const char *output_path, const char *target);
#endif
