/* Runtime values, interpreter state, and helpers shared with builtins. */

#ifndef NEWT_INTERPRETER_H
#define NEWT_INTERPRETER_H

#include "parser.h"

typedef enum {
    VALUE_NUMBER,
    VALUE_STRING,
    VALUE_BOOL,
    VALUE_LIST,
    VALUE_VOID
} ValueType;
typedef struct Value Value;

/* Heap strings and lists are reference counted. Every stored copy (a
   variable, a list element) and every temporary returned by
   eval_expression owns one reference. Live objects are also linked into a
   registry so anything still alive at exit (such as a list that contains
   itself) is freed. */
typedef struct StringObject {
    int ref_count;
    char *characters;
    struct StringObject *previous;
    struct StringObject *next;
} StringObject;
typedef struct ListValue {
    Value *elements;
    int count;
    int capacity;
    int ref_count;
    struct ListValue *previous;
    struct ListValue *next;
} ListValue;
struct Value {
    ValueType type;
    /* Numeric payloads retain their actual host C representation.
       number is also the double view used by existing math built-ins. */
    double number;
    DeclType numeric_type;
    int integer;
    float single;
    int boolean;
    Token string;
    /* NULL when the string points into the source text. */
    StringObject *string_object;
    ListValue *list;
};
typedef struct {
    Token name;
    Value value;
    int is_mutable;
    int scope_depth;
    DeclType declared_type;
} Variable;
typedef struct {
    Token name;
    Stmt *declaration;
} Function;
typedef struct {
    Parser *parser;
    /* Allocated once at NEWT_MAX_VARIABLES and never moved, so Variable
       pointers stay valid while an expression is being evaluated. */
    Variable *variables;
    int variable_count;
    /* Lexical scoping: the first variable slot of the running function's
       frame, and how many slots existed when the outermost call began
       (top-level variables live below that). */
    int frame_base;
    int global_limit;
    Function functions[256];
    int function_count;
    int call_depth;
    int scope_depth;
    int is_returning;
    int is_breaking;
    int is_continuing;
    int loop_depth;
    Value return_value;
    DeclType return_type;
    DeclType literal_type;
    /* Script arguments borrow the argv strings owned by the C runtime. */
    int script_argument_count;
    char **script_arguments;
    StringObject *live_strings;
    ListValue *live_lists;
    /* Work stack used to free nested lists without recursion. */
    ListValue **release_stack;
    int release_stack_count;
    int release_stack_capacity;
    int had_error;
} Interpreter;

Value make_number_value(double number);

Value make_bool_value(int boolean);

void value_retain_object(Value value);

void value_release_object(Interpreter *interpreter, Value value);

/* Numbers, booleans, and strings that point into the source own nothing,
   so these inline checks skip the function call for them. */
static inline int value_has_object(const Value *value) {
    return (value->type == VALUE_STRING && value->string_object != NULL) ||
           value->type == VALUE_LIST;
}

static inline void value_retain(Value value) {
    if (value_has_object(&value)) {
        value_retain_object(value);
    }
}

static inline void value_release(Interpreter *interpreter, Value value) {
    if (value_has_object(&value)) {
        value_release_object(interpreter, value);
    }
}

void runtime_error(Interpreter *interpreter, Token token, const char *message);

void runtime_type_error(Interpreter *interpreter,
                               Token token,
                               const char *subject,
                               const char *expected,
                               Value actual);

void runtime_name_error(Interpreter *interpreter, Token token, const char *message);

int make_runtime_string(Interpreter *interpreter,
                               Token source,
                               char *characters,
                               int content_length,
                               Value *value);

Variable *find_variable(Interpreter *interpreter, Token name);

int eval_expression(Interpreter *interpreter, Expr *expr, Value *value);

/* Returns interpreter success separately from the entry function's host exit status. */
int run_program(const char *path,
                       const char *source,
                       int script_argument_count,
                       char **script_arguments,
                       int *program_exit_status);

#endif

