/* Runtime values, interpreter state, and helpers shared with builtins. */

#ifndef NEWT_INTERPRETER_H
#define NEWT_INTERPRETER_H

#include "parser.h"

typedef enum {
    VALUE_NUMBER,
    VALUE_STRING,
    VALUE_BOOL,
    VALUE_LIST
} ValueType;
typedef struct Value Value;
typedef struct ListValue {
    Value *elements;
    int count;
    int capacity;
} ListValue;
struct Value {
    ValueType type;
    double number;
    int boolean;
    Token string;
    ListValue *list;
};
typedef struct {
    Token name;
    Value value;
    int is_mutable;
    int scope_depth;
} Variable;
typedef struct {
    Token name;
    Stmt *declaration;
} Function;
typedef struct {
    Parser *parser;
    Variable variables[256];
    int variable_count;
    Function functions[256];
    int function_count;
    int call_depth;
    int scope_depth;
    int is_returning;
    int is_breaking;
    int is_continuing;
    int loop_depth;
    Value return_value;
    /* Script arguments borrow the argv strings owned by the C runtime. */
    int script_argument_count;
    char **script_arguments;
    char *runtime_strings[NEWT_MAX_RUNTIME_STRINGS];
    int runtime_string_count;
    ListValue *runtime_lists[NEWT_MAX_RUNTIME_LISTS];
    int runtime_list_count;
    int had_error;
} Interpreter;

Value make_number_value(double number);

Value make_bool_value(int boolean);

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

int run_program(const char *path,
                       const char *source,
                       int script_argument_count,
                       char **script_arguments);

#endif
