/* A deliberately small x86-64 System V backend. Values live in stack slots,
   so calls never destroy a live expression temporary. No optimizer is used. */
#include "compiler.h"
#include "parser.h"
#include <stdarg.h>

typedef struct { DeclType base; int pointers; Token record; } CType;
typedef struct { Token name; CType type; int offset, readonly, depth; } Local;
typedef struct {
    Parser *parser;
    const char *path;
    FILE *text, *data, *body;
    int failed, label, bytes, count, depth, break_label, continue_label, return_label;
    Stmt *function;
    Local locals[NEWT_MAX_VARIABLES];
} Codegen;

static CType scalar(DeclType base) { CType t = {0}; t.base = base; return t; }
static int same_name(Token a, Token b) {
    return a.length == b.length && strncmp(a.start, b.start, (size_t)a.length) == 0;
}
static void error(Codegen *g, Token at, const char *message) {
    if (!g->failed) fprintf(stderr, "%s:%d:%d: compile error: %s\n", g->path, at.line, at.column, message);
    g->failed = 1;
}
static void out(Codegen *g, const char *format, ...) {
    va_list args; va_start(args, format);
    if (vfprintf(g->body, format, args) < 0) g->failed = 1;
    va_end(args);
}
static int slot(Codegen *g, Token at) {
    if (g->bytes > 1024 * 1024 - 8) { error(g, at, "native stack frame is too large"); return 8; }
    g->bytes += 8; return g->bytes;
}
static Local *local(Codegen *g, Token name) {
    int i;
    for (i = g->count - 1; i >= 0; --i) if (same_name(g->locals[i].name, name)) return &g->locals[i];
    error(g, name, "undefined native variable (global variables are not supported)"); return NULL;
}
static Local *declare(Codegen *g, Token name, CType type, int readonly) {
    Local *v; int i;
    for (i = g->count - 1; i >= 0 && g->locals[i].depth == g->depth; --i)
        if (same_name(g->locals[i].name, name)) { error(g, name, "duplicate local variable"); return NULL; }
    if (g->count == NEWT_MAX_VARIABLES) { error(g, name, "too many native variables"); return NULL; }
    v = &g->locals[g->count++]; v->name = name; v->type = type;
    v->offset = slot(g, name); v->readonly = readonly; v->depth = g->depth; return v;
}
static Stmt *function(Codegen *g, Token name) {
    int i;
    for (i = 0; i < g->parser->top_level_statement_count; ++i) {
        Stmt *s = &g->parser->statements[g->parser->top_level_statements[i]];
        if (s->kind == STMT_FN_DECL && same_name(s->name, name)) return s;
    }
    error(g, name, "undefined native function"); return NULL;
}
static void symbol(Codegen *g, Stmt *f) {
    if (f->is_entry) out(g, "main"); else out(g, "newt_%.*s", f->name.length, f->name.start);
}
static int supported(Codegen *g, CType t, Token at, int allow_void) {
    if (t.base == TYPE_INT || t.base == TYPE_BOOL || (allow_void && t.base == TYPE_VOID)) return 1;
    error(g, at, "native subset supports typed int, bool, and void; this construct is not compiled"); return 0;
}
static int integer_token(Codegen *g, Token t, int negative, int *result) {
    char buffer[64], *end; long long n;
    if (t.length >= (int)sizeof(buffer)) { error(g, t, "integer literal is too long"); return 0; }
    memcpy(buffer, t.start, (size_t)t.length); buffer[t.length] = 0; errno = 0;
    n = strtoll(buffer, &end, 10);
    if (*end || errno == ERANGE) { error(g, t, "native integer literal must be a decimal whole number"); return 0; }
    if (negative) n = -n;
    if (n < INT_MIN || n > INT_MAX) { error(g, t, "integer literal is out of int range"); return 0; }
    *result = (int)n; return 1;
}
static CType infer(Codegen *g, Expr *e) {
    if (!e) return scalar(TYPE_VOID);
    switch (e->kind) {
        case EXPR_NUMBER: return scalar(TYPE_AUTO);
        case EXPR_TRUE: case EXPR_FALSE: return scalar(TYPE_BOOL);
        case EXPR_IDENT: { Local *v = local(g, e->token); return v ? v->type : scalar(TYPE_AUTO); }
        case EXPR_CALL: {
            Stmt *f;
            if (e->token.type == TOKEN_INT) return scalar(TYPE_INT);
            f = function(g, e->token); return scalar(f ? f->declared_type : TYPE_AUTO);
        }
        case EXPR_UNARY: return e->token.type == TOKEN_NOT ? scalar(TYPE_BOOL) : infer(g, e->left);
        case EXPR_BINARY: {
            CType a = infer(g, e->left), b = infer(g, e->right);
            switch (e->token.type) {
                case TOKEN_PLUS: case TOKEN_MINUS: case TOKEN_STAR: case TOKEN_SLASH:
                    return a.base == TYPE_AUTO ? b : a;
                default: return scalar(TYPE_BOOL);
            }
        }
        default: error(g, e->token, "expression is outside the native subset"); return scalar(TYPE_AUTO);
    }
}
static CType expression(Codegen *, Expr *, CType);
static void block(Codegen *, int *, int);

static CType call(Codegen *g, Expr *e) {
    static const char *regs[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
    Stmt *f; int offsets[6], i;
    if (e->token.type == TOKEN_INT) {
        Expr *a;
        if (e->argument_count != 1) { error(g, e->token, "int conversion requires one argument"); return scalar(TYPE_INT); }
        a = g->parser->call_arguments[e->argument_start];
        if (a->kind != EXPR_NUMBER && infer(g, a).base != TYPE_INT) {
            error(g, a->token, "native int conversion currently accepts integer literals or typed ints"); return scalar(TYPE_INT);
        }
        expression(g, a, scalar(TYPE_INT)); return scalar(TYPE_INT);
    }
    f = function(g, e->token); if (!f) return scalar(TYPE_AUTO);
    if (e->argument_count != f->parameter_count || e->argument_count > 6) {
        error(g, e->token, "native calls require matching signatures and at most six arguments"); return scalar(TYPE_AUTO);
    }
    for (i = 0; i < e->argument_count; ++i) {
        Parameter p = g->parser->parameters[f->parameter_start + i];
        expression(g, g->parser->call_arguments[e->argument_start + i], scalar(p.type));
        offsets[i] = slot(g, e->token); out(g, "  mov DWORD PTR [rbp-%d], eax\n", offsets[i]);
    }
    for (i = 0; i < e->argument_count; ++i) out(g, "  mov %s, DWORD PTR [rbp-%d]\n", regs[i], offsets[i]);
    out(g, "  call "); symbol(g, f); out(g, "\n"); return scalar(f->declared_type);
}
static CType expression(Codegen *g, Expr *e, CType expected) {
    CType type = scalar(TYPE_AUTO); int value, saved, label; CType context;
    if (!e || g->failed) return type;
    switch (e->kind) {
        case EXPR_NUMBER:
            if (integer_token(g, e->token, 0, &value)) out(g, "  mov eax, %d\n", value);
            type = scalar(expected.base == TYPE_INT ? TYPE_INT : TYPE_AUTO); break;
        case EXPR_TRUE: case EXPR_FALSE:
            out(g, "  mov eax, %d\n", e->kind == EXPR_TRUE); type = scalar(TYPE_BOOL); break;
        case EXPR_IDENT: {
            Local *v = local(g, e->token); if (!v) break;
            out(g, "  mov eax, DWORD PTR [rbp-%d]\n", v->offset); type = v->type; break;
        }
        case EXPR_CALL: type = call(g, e); break;
        case EXPR_UNARY:
            if (e->token.type == TOKEN_MINUS && e->left->kind == EXPR_NUMBER) {
                if (integer_token(g, e->left->token, 1, &value)) out(g, "  mov eax, %d\n", value);
                type = scalar(expected.base == TYPE_INT ? TYPE_INT : TYPE_AUTO); break;
            }
            type = expression(g, e->left, e->token.type == TOKEN_NOT ? scalar(TYPE_BOOL) : scalar(TYPE_INT));
            if (e->token.type == TOKEN_NOT) out(g, "  test eax, eax\n  sete al\n  movzx eax, al\n");
            else out(g, "  neg eax\n  jo .Lnewt_overflow\n");
            break;
        case EXPR_BINARY:
            if (e->token.type == TOKEN_AND || e->token.type == TOKEN_OR) {
                label = g->label++;
                expression(g, e->left, scalar(TYPE_BOOL));
                out(g, "  test eax, eax\n  %s .Llogic%d\n", e->token.type == TOKEN_AND ? "jz" : "jnz", label);
                expression(g, e->right, scalar(TYPE_BOOL)); out(g, ".Llogic%d:\n", label);
                type = scalar(TYPE_BOOL); break;
            }
            context = expected.base == TYPE_INT ? expected : infer(g, e->left);
            if (context.base == TYPE_AUTO) context = infer(g, e->right);
            if (context.base == TYPE_AUTO) {
                if (e->token.type == TOKEN_PLUS || e->token.type == TOKEN_MINUS || e->token.type == TOKEN_STAR || e->token.type == TOKEN_SLASH) {
                    error(g, e->token, "native arithmetic needs a typed int context or operand"); break;
                }
                context = scalar(TYPE_INT);
            }
            if (context.base == TYPE_BOOL && e->token.type != TOKEN_EQUAL_EQUAL && e->token.type != TOKEN_BANG_EQUAL) {
                error(g, e->token, "booleans only support equality and logical operators"); break;
            }
            if (context.base != TYPE_INT && context.base != TYPE_BOOL) { error(g, e->token, "unsupported native operand type"); break; }
            expression(g, e->left, context); saved = slot(g, e->token);
            out(g, "  mov DWORD PTR [rbp-%d], eax\n", saved);
            expression(g, e->right, context); out(g, "  mov ecx, eax\n  mov eax, DWORD PTR [rbp-%d]\n", saved);
            type = scalar(TYPE_INT);
            switch (e->token.type) {
                case TOKEN_PLUS: out(g, "  add eax, ecx\n  jo .Lnewt_overflow\n"); break;
                case TOKEN_MINUS: out(g, "  sub eax, ecx\n  jo .Lnewt_overflow\n"); break;
                case TOKEN_STAR: out(g, "  imul eax, ecx\n  jo .Lnewt_overflow\n"); break;
                case TOKEN_SLASH:
                    label = g->label++;
                    out(g, "  test ecx, ecx\n  jz .Lnewt_divzero\n  cmp eax, -2147483648\n  jne .Ldivide%d\n  cmp ecx, -1\n  je .Lnewt_overflow\n.Ldivide%d:\n  cdq\n  idiv ecx\n", label, label); break;
                default: {
                    const char *condition = NULL;
                    switch (e->token.type) {
                        case TOKEN_EQUAL_EQUAL: condition = "e"; break; case TOKEN_BANG_EQUAL: condition = "ne"; break;
                        case TOKEN_LESS: condition = "l"; break; case TOKEN_LESS_EQUAL: condition = "le"; break;
                        case TOKEN_GREATER: condition = "g"; break; case TOKEN_GREATER_EQUAL: condition = "ge"; break;
                        default: error(g, e->token, "unsupported native operator"); break;
                    }
                    if (condition) out(g, "  cmp eax, ecx\n  set%s al\n  movzx eax, al\n", condition);
                    type = scalar(TYPE_BOOL); break;
                }
            }
            break;
        default: error(g, e->token, "expression is outside the native subset"); break;
    }
    if (expected.base != TYPE_AUTO && type.base != expected.base) error(g, e->token, "native expression does not match the declared type");
    return type;
}
static void print_expression(Codegen *g, Expr *e) {
    if (e->kind == EXPR_STRING) {
        int label = g->label++, trailing_newline = 0;
        /* Source escapes are also understood by the GNU assembler. */
        int i; char last = 0;
        for (i = 1; i < e->token.length - 1; ++i) {
            last = e->token.start[i];
            if (last == '\\' && i + 1 < e->token.length - 1) last = decode_string_escape(e->token.start[++i]);
        }
        trailing_newline = last == '\n';
        fprintf(g->data, ".Lstring%d: .asciz %.*s\n", label, e->token.length, e->token.start);
        out(g, "  lea rsi, [rip+.Lstring%d]\n  lea rdi, [rip+.Lfmt_%s]\n  xor eax, eax\n  call printf@PLT\n", label, trailing_newline ? "raw" : "string");
    } else {
        CType t = expression(g, e, scalar(TYPE_AUTO));
        if (t.base == TYPE_VOID) { error(g, e->token, "void functions do not produce values"); return; }
        if (t.base == TYPE_BOOL) {
            out(g, "  test eax, eax\n  lea rsi, [rip+.Ltrue]\n  lea rdx, [rip+.Lfalse]\n  cmovz rsi, rdx\n  lea rdi, [rip+.Lfmt_string]\n");
        } else out(g, "  mov esi, eax\n  lea rdi, [rip+.Lfmt_int]\n");
        out(g, "  xor eax, eax\n  call printf@PLT\n");
    }
}
static void statement(Codegen *g, Stmt *s) {
    int a, b, previous_break, previous_continue;
    Local *v;
    if (g->failed) return;
    switch (s->kind) {
        case STMT_LET_DECL: case STMT_MUT_DECL:
            supported(g, scalar(s->declared_type), s->name, 0);
            expression(g, s->expression, scalar(s->declared_type));
            v = declare(g, s->name, scalar(s->declared_type), s->kind == STMT_LET_DECL);
            if (v) out(g, "  mov DWORD PTR [rbp-%d], eax\n", v->offset);
            break;
        case STMT_ASSIGN:
            v = local(g, s->name); if (!v) break;
            if (v->readonly) { error(g, s->name, "cannot assign to immutable variable"); break; }
            expression(g, s->expression, v->type); out(g, "  mov DWORD PTR [rbp-%d], eax\n", v->offset); break;
        case STMT_CALL: call(g, s->expression); break;
        case STMT_PRINT: print_expression(g, s->expression); break;
        case STMT_RETURN:
            if (g->function->declared_type == TYPE_VOID) {
                if (s->expression) error(g, s->name, "void function cannot return a value");
                out(g, "  xor eax, eax\n");
            } else {
                if (!s->expression) { error(g, s->name, "typed return requires a value"); break; }
                expression(g, s->expression, scalar(g->function->declared_type));
            }
            out(g, "  jmp .Lreturn%d\n", g->return_label); break;
        case STMT_IF:
            a = g->label++; b = g->label++;
            if (expression(g, s->expression, scalar(TYPE_AUTO)).base == TYPE_VOID) error(g, s->name, "void value cannot be a condition");
            out(g, "  test eax, eax\n  jz .Lif%d\n", a);
            block(g, s->then_statements, s->then_count); out(g, "  jmp .Lif%d\n.Lif%d:\n", b, a);
            if (s->has_else) block(g, s->else_statements, s->else_count);
            out(g, ".Lif%d:\n", b); break;
        case STMT_WHILE:
            a = g->label++; b = g->label++; previous_break = g->break_label; previous_continue = g->continue_label;
            g->break_label = b; g->continue_label = a;
            out(g, ".Lloop%d:\n", a);
            if (expression(g, s->expression, scalar(TYPE_AUTO)).base == TYPE_VOID) error(g, s->name, "void value cannot be a condition");
            out(g, "  test eax, eax\n  jz .Lloop%d\n", b); block(g, s->body_statements, s->body_count);
            out(g, "  jmp .Lloop%d\n.Lloop%d:\n", a, b);
            g->break_label = previous_break; g->continue_label = previous_continue; break;
        case STMT_BREAK: case STMT_CONTINUE:
            a = s->kind == STMT_BREAK ? g->break_label : g->continue_label;
            if (a < 0) error(g, s->name, "loop control used outside a loop"); else out(g, "  jmp .Lloop%d\n", a);
            break;
        default: error(g, s->name, "statement is outside the native subset"); break;
    }
}
static void block(Codegen *g, int *items, int count) {
    int i, old_count = g->count; ++g->depth;
    for (i = 0; i < count && !g->failed; ++i) statement(g, &g->parser->statements[items[i]]);
    g->count = old_count; --g->depth;
}
static int copy_stream(FILE *destination, FILE *source) {
    char buffer[4096]; size_t n; rewind(source);
    while ((n = fread(buffer, 1, sizeof(buffer), source)) != 0)
        if (fwrite(buffer, 1, n, destination) != n) return 0;
    return !ferror(source);
}
static void compile_function(Codegen *g, Stmt *s) {
    static const char *regs[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
    int i; FILE *body = tmpfile();
    if (!body) { error(g, s->name, "cannot create compiler temporary stream"); return; }
    g->body = body; g->function = s; g->bytes = 0; g->count = 0; g->depth = 0;
    g->return_label = g->label++; g->break_label = g->continue_label = -1;
    supported(g, scalar(s->declared_type), s->name, 1);
    if (s->parameter_count > 6) error(g, s->name, "native functions support at most six parameters");
    for (i = 0; i < s->parameter_count && !g->failed; ++i) {
        Parameter p = g->parser->parameters[s->parameter_start + i]; Local *v;
        supported(g, scalar(p.type), p.name, 0); v = declare(g, p.name, scalar(p.type), !p.is_mutable);
        if (v) out(g, "  mov DWORD PTR [rbp-%d], %s\n", v->offset, regs[i]);
    }
    /* The outer function body shares the parameter scope, as in C. */
    for (i = 0; i < s->body_count && !g->failed; ++i) statement(g, &g->parser->statements[s->body_statements[i]]);
    if (s->declared_type == TYPE_VOID) out(g, "  xor eax, eax\n"); else out(g, "  jmp .Lnewt_missing_return\n");
    out(g, ".Lreturn%d:\n  leave\n  ret\n", g->return_label);
    g->body = g->text;
    if (s->is_entry) out(g, ".globl main\n");
    out(g, ".type "); symbol(g, s); out(g, ", @function\n"); symbol(g, s);
    out(g, ":\n  push rbp\n  mov rbp, rsp\n  sub rsp, %d\n", (g->bytes + 15) & ~15);
    if (!copy_stream(g->text, body)) error(g, s->name, "cannot write assembly stream");
    fclose(body);
}
static void runtime_failure(Codegen *g, const char *name, const char *message) {
    size_t length = strlen(message);
    fprintf(g->data, ".Lmsg_%s: .asciz \"%s\\n\"\n", name, message);
    fprintf(g->text, ".Lnewt_%s:\n  mov edi, 2\n  lea rsi, [rip+.Lmsg_%s]\n  mov edx, %lu\n  call write@PLT\n  mov edi, 1\n  call exit@PLT\n", name, name, (unsigned long)length + 1);
}
int emit_native_assembly(const char *path, const char *source, const char *output_path, const char *target) {
    Parser parser; Codegen *g; FILE *output; int i, ok = 0;
    if (strcmp(target, "x86_64-linux") != 0) { fprintf(stderr, "compile error: unsupported target '%s'; supported target: x86_64-linux\n", target); return 0; }
    if (strcmp(path, output_path) == 0) { fprintf(stderr, "compile error: output must not overwrite the input source\n"); return 0; }
    parser_init(&parser, path, source);
    if (!parse_program(&parser)) { parser_free(&parser); return 0; }
    g = calloc(1, sizeof(*g)); if (!g) { parser_free(&parser); return 0; }
    g->parser = &parser; g->path = path; g->text = tmpfile(); g->data = tmpfile();
    if (!g->text || !g->data) { fprintf(stderr, "compile error: cannot create temporary streams\n"); goto done; }
    if (parser.entry_statement < 0) { fprintf(stderr, "compile error: native programs require start: and a typed main\n"); goto done; }
    fprintf(g->text, ".intel_syntax noprefix\n.text\n");
    fprintf(g->data, ".section .rodata\n.Lfmt_int: .asciz \"%%d\\n\"\n.Lfmt_string: .asciz \"%%s\\n\"\n.Lfmt_raw: .asciz \"%%s\"\n.Ltrue: .asciz \"true\"\n.Lfalse: .asciz \"false\"\n");
    for (i = 0; i < parser.top_level_statement_count && !g->failed; ++i) {
        int j; Stmt *s = &parser.statements[parser.top_level_statements[i]];
        if (s->kind != STMT_FN_DECL) { error(g, s->name, "native subset requires globals to be moved into functions"); break; }
        for (j = 0; j < i; ++j) {
            Stmt *previous = &parser.statements[parser.top_level_statements[j]];
            if (same_name(previous->name, s->name)) error(g, s->name, "duplicate native function");
        }
        compile_function(g, s);
    }
    runtime_failure(g, "overflow", "native error: int arithmetic overflow");
    runtime_failure(g, "divzero", "native error: division by zero");
    runtime_failure(g, "missing_return", "native error: typed function completed without returning a value");
    if (g->failed) goto done;
    output = fopen(output_path, "wb");
    if (!output) { fprintf(stderr, "compile error: cannot open output '%s'\n", output_path); goto done; }
    ok = copy_stream(output, g->text) && copy_stream(output, g->data) &&
         fprintf(output, ".section .note.GNU-stack,\"\",@progbits\n") >= 0;
    if (fclose(output) != 0) ok = 0;
    if (!ok) fprintf(stderr, "compile error: cannot write assembly output\n");
done:
    if (g->text) fclose(g->text);
    if (g->data) fclose(g->data);
    free(g); parser_free(&parser); return ok;
}
