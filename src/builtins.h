/* Built-in functions callable from Newt programs. */

#ifndef NEWT_BUILTINS_H
#define NEWT_BUILTINS_H

#include "interpreter.h"

typedef enum {
    SCIENTIFIC_DOMAIN_ALL,
    SCIENTIFIC_DOMAIN_UNIT,
    SCIENTIFIC_DOMAIN_POSITIVE
} ScientificMathDomain;

int call_file_read(Interpreter *interpreter, Expr *call, Value *value);

int call_file_write(Interpreter *interpreter,
                           Expr *call,
                           const char *mode,
                           const char *name,
                           Value *value);

int call_arg_count(Interpreter *interpreter, Expr *call, Value *value);

int call_arg(Interpreter *interpreter, Expr *call, Value *value);

int call_text(Interpreter *interpreter, Expr *call, Value *value);

int call_len(Interpreter *interpreter, Expr *call, Value *value);

int call_length(Interpreter *interpreter, Expr *call, Value *value);

int call_append(Interpreter *interpreter, Expr *call, Value *value);

int call_contains(Interpreter *interpreter, Expr *call, Value *value);

int call_change_case(Interpreter *interpreter,
                            Expr *call,
                            const char *name,
                            int to_upper,
                            Value *value);

int call_trim(Interpreter *interpreter, Expr *call, Value *value);

int call_unary_math(Interpreter *interpreter,
                           Expr *call,
                           const char *name,
                           double (*operation)(double),
                           Value *value);

int call_scientific_unary(Interpreter *interpreter,
                                 Expr *call,
                                 const char *name,
                                 double (*operation)(double),
                                 ScientificMathDomain domain,
                                 Value *value);

int call_atan2(Interpreter *interpreter, Expr *call, Value *value);

int call_min_max(Interpreter *interpreter,
                        Expr *call,
                        const char *name,
                        int choose_max,
                        Value *value);

int call_pow(Interpreter *interpreter, Expr *call, Value *value);

#endif
