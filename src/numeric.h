/* Numeric operations shared by expression evaluation and typed bindings. */
#ifndef NEWT_NUMERIC_H
#define NEWT_NUMERIC_H

#include "interpreter.h"

const char *decl_type_name(DeclType type);

int convert_numeric(Interpreter *interpreter, Token token, Value source,
                    DeclType type, int explicit_cast, Value *out);

int typed_numeric_operator(Interpreter *interpreter, Expr *expr,
                           Value left, Value right, Value *out);

#endif
