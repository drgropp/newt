/* Typed numeric representation, checked conversions, and arithmetic. */

#include "numeric.h"

/* The initial interpreter target uses the common 32/32/64 C numeric model. */
_Static_assert(CHAR_BIT == 8, "Newt requires 8-bit bytes");
_Static_assert(INT_MAX == 2147483647 && INT_MIN == (-2147483647 - 1), "Newt requires 32-bit int");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128 && sizeof(float) == 4, "Newt requires binary32 float");
_Static_assert(DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024 && sizeof(double) == 8, "Newt requires binary64 double");

const char *decl_type_name(DeclType type) {
    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_VOID: return "void";
        case TYPE_BOOL: return "bool";
        case TYPE_STRING: return "string";
        case TYPE_LIST: return "list";
        default: return "untyped";
    }
}

int convert_numeric(Interpreter *interpreter, Token token, Value source,
                           DeclType type, int explicit_cast, Value *out) {
    double n;
    if (source.type != VALUE_NUMBER) {
        runtime_error(interpreter, token, "numeric conversion requires a number");
        return 0;
    }
    n = source.number;
    if (!isfinite(n)) {
        runtime_error(interpreter, token, "numeric value must be finite");
        return 0;
    }
    *out = make_number_value(n);
    out->numeric_type = type;
    if (type == TYPE_INT) {
        double whole = trunc(n);
        if ((!explicit_cast && whole != n) || whole < INT_MIN || whole > INT_MAX) {
            runtime_error(interpreter, token, "value is not representable as int");
            return 0;
        }
        out->integer = (int)whole;
        out->number = (double)out->integer;
    } else if (type == TYPE_FLOAT) {
        if (fabs(n) > FLT_MAX) {
            runtime_error(interpreter, token, "value is not representable as float");
            return 0;
        }
        out->single = (float)n;
        out->number = (double)out->single;
    }
    return 1;
}

int typed_numeric_operator(Interpreter *interpreter, Expr *expr, Value left, Value right, Value *out) {
    DeclType type = left.numeric_type;
    double result;
    if (type != right.numeric_type) {
        runtime_error(interpreter, expr->token, "mixed numeric types require an explicit conversion");
        return 0;
    }
    switch (expr->token.type) {
        case TOKEN_EQUAL_EQUAL: *out = make_bool_value(left.number == right.number); return 1;
        case TOKEN_BANG_EQUAL: *out = make_bool_value(left.number != right.number); return 1;
        case TOKEN_LESS: *out = make_bool_value(left.number < right.number); return 1;
        case TOKEN_LESS_EQUAL: *out = make_bool_value(left.number <= right.number); return 1;
        case TOKEN_GREATER: *out = make_bool_value(left.number > right.number); return 1;
        case TOKEN_GREATER_EQUAL: *out = make_bool_value(left.number >= right.number); return 1;
        default: break;
    }
    if (expr->token.type == TOKEN_SLASH && right.number == 0) {
        runtime_error(interpreter, expr->token, "division by zero");
        return 0;
    }
    if (type == TYPE_INT) {
        long long a = left.integer, b = right.integer, n;
        switch (expr->token.type) {
            case TOKEN_PLUS: n = a + b; break;
            case TOKEN_MINUS: n = a - b; break;
            case TOKEN_STAR: n = a * b; break;
            case TOKEN_SLASH: n = a / b; break;
            default: runtime_error(interpreter, expr->token, "unsupported numeric operator"); return 0;
        }
        if (n < INT_MIN || n > INT_MAX) {
            runtime_error(interpreter, expr->token, "int arithmetic overflow");
            return 0;
        }
        return convert_numeric(interpreter, expr->token, make_number_value((double)n), type, 0, out);
    }
    if (type == TYPE_FLOAT) {
        float a = left.single, b = right.single, n;
        switch (expr->token.type) {
            case TOKEN_PLUS: n = a + b; break;
            case TOKEN_MINUS: n = a - b; break;
            case TOKEN_STAR: n = a * b; break;
            case TOKEN_SLASH: n = a / b; break;
            default: runtime_error(interpreter, expr->token, "unsupported numeric operator"); return 0;
        }
        result = n;
    } else {
        switch (expr->token.type) {
            case TOKEN_PLUS: result = left.number + right.number; break;
            case TOKEN_MINUS: result = left.number - right.number; break;
            case TOKEN_STAR: result = left.number * right.number; break;
            case TOKEN_SLASH: result = left.number / right.number; break;
            default: runtime_error(interpreter, expr->token, "unsupported numeric operator"); return 0;
        }
    }
    return convert_numeric(interpreter, expr->token, make_number_value(result), type, 0, out);
}
