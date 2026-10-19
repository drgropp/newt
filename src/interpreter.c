/* Interpreter: evaluates expressions and executes statements. */

#include "interpreter.h"
#include "builtins.h"

Value make_number_value(double number) {
    Value value;

    value.type = VALUE_NUMBER;
    value.number = number;
    value.boolean = 0;
    value.string.start = NULL;
    value.string.length = 0;
    value.string.line = 0;
    value.string.column = 0;
    value.string.error_message = NULL;
    value.list = NULL;
    return value;
}

static Value make_string_value(Token string) {
    Value value;

    value.type = VALUE_STRING;
    value.number = 0;
    value.boolean = 0;
    value.string = string;
    value.list = NULL;
    return value;
}

Value make_bool_value(int boolean) {
    Value value;

    value.type = VALUE_BOOL;
    value.number = 0;
    value.boolean = boolean;
    value.string.start = NULL;
    value.string.length = 0;
    value.string.line = 0;
    value.string.column = 0;
    value.string.error_message = NULL;
    value.list = NULL;
    return value;
}

static Value make_list_value(ListValue *list) {
    Value value;

    value.type = VALUE_LIST;
    value.number = 0;
    value.boolean = 0;
    value.string.start = NULL;
    value.string.length = 0;
    value.string.line = 0;
    value.string.column = 0;
    value.string.error_message = NULL;
    value.list = list;
    return value;
}

static int string_values_equal(Value left, Value right) {
    int left_length = left.string.length - 2;
    int right_length = right.string.length - 2;

    if (left_length < 0) {
        left_length = 0;
    }
    if (right_length < 0) {
        right_length = 0;
    }

    return left_length == right_length &&
           strncmp(left.string.start + 1, right.string.start + 1, (size_t)left_length) == 0;
}

static int value_is_truthy(Value value) {
    if (value.type == VALUE_BOOL) {
        return value.boolean;
    }

    if (value.type == VALUE_STRING) {
        return value.string.length > 2;
    }

    if (value.type == VALUE_LIST) {
        return value.list->count > 0;
    }

    return value.number != 0;
}

static int token_names_match(Token left, Token right) {
    return left.length == right.length &&
           strncmp(left.start, right.start, (size_t)left.length) == 0;
}

static const char *value_type_name(ValueType type) {
    switch (type) {
        case VALUE_NUMBER:
            return "number";
        case VALUE_STRING:
            return "string";
        case VALUE_BOOL:
            return "boolean";
        case VALUE_LIST:
            return "list";
    }

    return "unknown value";
}

void runtime_error(Interpreter *interpreter, Token token, const char *message) {
    if (interpreter->had_error) {
        return;
    }

    fprintf(stderr,
            "%s:%d:%d: runtime error: %s\n",
            interpreter->parser->path,
            token.line,
            token.column,
            message);
    interpreter->had_error = 1;
}

void runtime_type_error(Interpreter *interpreter,
                               Token token,
                               const char *subject,
                               const char *expected,
                               Value actual) {
    char message[160];

    snprintf(message,
             sizeof(message),
             "%s must be %s, got %s",
             subject,
             expected,
             value_type_name(actual.type));
    runtime_error(interpreter, token, message);
}

void runtime_name_error(Interpreter *interpreter, Token token, const char *message) {
    char detailed_message[160];

    snprintf(detailed_message,
             sizeof(detailed_message),
             "%s '%.*s'",
             message,
             token.length,
             token.start);
    runtime_error(interpreter, token, detailed_message);
}

int make_runtime_string(Interpreter *interpreter,
                               Token source,
                               char *characters,
                               int content_length,
                               Value *value) {
    Token string = source;

    /* The interpreter owns this heap buffer and frees it after the run. */
    if (content_length < 0 || content_length > INT_MAX - 2) {
        free(characters);
        runtime_error(interpreter, source, "runtime string is too long");
        return 0;
    }
    if (interpreter->runtime_string_count >= NEWT_MAX_RUNTIME_STRINGS) {
        free(characters);
        runtime_error(interpreter, source, "too many runtime strings");
        return 0;
    }

    string.type = TOKEN_STRING;
    string.start = characters;
    string.length = content_length + 2;
    string.error_message = NULL;
    interpreter->runtime_strings[interpreter->runtime_string_count] = characters;
    interpreter->runtime_string_count++;
    if (value != NULL) {
        *value = make_string_value(string);
    }
    return 1;
}

static int concatenate_strings(Interpreter *interpreter,
                               Token operator_token,
                               Value left,
                               Value right,
                               Value *value) {
    int left_length = left.string.length - 2;
    int right_length = right.string.length - 2;
    int content_length;
    char *characters;

    if (left_length < 0) {
        left_length = 0;
    }
    if (right_length < 0) {
        right_length = 0;
    }
    if (left_length > INT_MAX - right_length - 2) {
        runtime_error(interpreter, operator_token, "concatenated string is too long");
        return 0;
    }

    content_length = left_length + right_length;
    characters = malloc((size_t)content_length + 3);
    if (characters == NULL) {
        runtime_error(interpreter, operator_token, "not enough memory to concatenate strings");
        return 0;
    }

    characters[0] = '"';
    memcpy(characters + 1, left.string.start + 1, (size_t)left_length);
    memcpy(characters + 1 + left_length,
           right.string.start + 1,
           (size_t)right_length);
    characters[content_length + 1] = '"';
    characters[content_length + 2] = '\0';
    return make_runtime_string(interpreter,
                               operator_token,
                               characters,
                               content_length,
                               value);
}

static int eval_string_literal(Interpreter *interpreter, Token token, Value *value) {
    char *characters;
    int input_index;
    int output_index = 1;
    int has_escape = 0;

    if (token.length < 2) {
        runtime_error(interpreter, token, "invalid string token");
        return 0;
    }

    for (input_index = 1; input_index < token.length - 1; input_index++) {
        if (token.start[input_index] == '\\') {
            has_escape = 1;
            break;
        }
    }
    if (!has_escape) {
        *value = make_string_value(token);
        return 1;
    }

    characters = malloc((size_t)token.length + 1);
    if (characters == NULL) {
        runtime_error(interpreter, token, "not enough memory for string");
        return 0;
    }
    characters[0] = '"';

    for (input_index = 1; input_index < token.length - 1; input_index++) {
        char ch = token.start[input_index];

        if (ch == '\\') {
            char escaped;

            if (input_index + 1 >= token.length - 1) {
                free(characters);
                runtime_error(interpreter, token, "incomplete escape sequence in string");
                return 0;
            }
            escaped = token.start[++input_index];
            if (!string_escape_is_supported(escaped)) {
                free(characters);
                runtime_error(interpreter, token, "invalid escape sequence in string");
                return 0;
            }

            characters[output_index++] = decode_string_escape(escaped);
            continue;
        }

        characters[output_index++] = ch;
    }

    characters[output_index++] = '"';
    characters[output_index] = '\0';
    return make_runtime_string(interpreter,
                               token,
                               characters,
                               output_index - 2,
                               value);
}

static void runtime_function_arity_error(Interpreter *interpreter,
                                         Token name,
                                         int expected,
                                         int actual) {
    char message[160];

    snprintf(message,
             sizeof(message),
             "function '%.*s' expects %d argument%s, got %d",
             name.length,
             name.start,
             expected,
             expected == 1 ? "" : "s",
             actual);
    runtime_error(interpreter, name, message);
}

Variable *find_variable(Interpreter *interpreter, Token name) {
    int i;

    for (i = interpreter->variable_count - 1; i >= 0; i--) {
        if (token_names_match(interpreter->variables[i].name, name)) {
            return &interpreter->variables[i];
        }
    }

    return NULL;
}

static Function *find_function(Interpreter *interpreter, Token name) {
    int i;

    for (i = 0; i < interpreter->function_count; i++) {
        if (token_names_match(interpreter->functions[i].name, name)) {
            return &interpreter->functions[i];
        }
    }

    return NULL;
}

static int define_function(Interpreter *interpreter, Stmt *declaration) {
    if (find_function(interpreter, declaration->name) != NULL) {
        runtime_name_error(interpreter, declaration->name, "function already defined");
        return 0;
    }

    if (interpreter->function_count >= 256) {
        runtime_error(interpreter, declaration->name, "too many functions");
        return 0;
    }

    interpreter->functions[interpreter->function_count].name = declaration->name;
    interpreter->functions[interpreter->function_count].declaration = declaration;
    interpreter->function_count++;
    return 1;
}

static int call_function(Interpreter *interpreter,
                         Expr *call,
                         Value *value);

static int read_number_token(Interpreter *interpreter, Token token, double *number) {
    char text[64];

    if (token.length >= (int)sizeof(text)) {
        runtime_error(interpreter, token, "number is too long");
        return 0;
    }

    memcpy(text, token.start, (size_t)token.length);
    text[token.length] = '\0';
    *number = strtod(text, NULL);
    return 1;
}

static int read_input_number(Interpreter *interpreter, Token prompt, double *number) {
    Value decoded_prompt;

    if (!eval_string_literal(interpreter, prompt, &decoded_prompt)) {
        return 0;
    }
    if (decoded_prompt.string.length >= 2) {
        printf("%.*s",
               decoded_prompt.string.length - 2,
               decoded_prompt.string.start + 1);
    }
    fflush(stdout);

    if (scanf("%lf", number) != 1) {
        runtime_error(interpreter, prompt, "expected number from input_number");
        return 0;
    }

    return 1;
}

static int resolve_list_index(Interpreter *interpreter,
                              Value list,
                              Token target_token,
                              Expr *index_expr,
                              int *resolved_index) {
    Value index;
    char message[160];

    if (list.type != VALUE_LIST) {
        runtime_type_error(interpreter,
                           target_token,
                           "indexed value",
                           "a list",
                           list);
        return 0;
    }
    if (!eval_expression(interpreter, index_expr, &index)) {
        return 0;
    }
    if (index.type != VALUE_NUMBER) {
        runtime_type_error(interpreter,
                           index_expr->token,
                           "list index",
                           "an integer",
                           index);
        return 0;
    }
    if (!isfinite(index.number) || floor(index.number) != index.number) {
        runtime_error(interpreter,
                      index_expr->token,
                      "list index must be an integer");
        return 0;
    }
    if (index.number < 0 || index.number >= (double)list.list->count) {
        snprintf(message,
                 sizeof(message),
                 "list index %.0f out of range for list of length %d",
                 index.number,
                 list.list->count);
        runtime_error(interpreter, index_expr->token, message);
        return 0;
    }

    *resolved_index = (int)index.number;
    return 1;
}

int eval_expression(Interpreter *interpreter, Expr *expr, Value *value) {
    Value left;
    Value right;
    double number;

    if (expr == NULL || interpreter->had_error) {
        return 0;
    }

    switch (expr->kind) {
        case EXPR_NUMBER:
            if (!read_number_token(interpreter, expr->token, &number)) {
                return 0;
            }
            *value = make_number_value(number);
            return 1;
        case EXPR_INPUT_NUMBER:
            if (!read_input_number(interpreter, expr->token, &number)) {
                return 0;
            }
            *value = make_number_value(number);
            return 1;
        case EXPR_SQRT:
            if (expr->argument_count != 1) {
                runtime_error(interpreter, expr->token, "sqrt expects exactly one argument");
                return 0;
            }
            if (!eval_expression(interpreter, expr->left, &left)) {
                return 0;
            }
            if (left.type != VALUE_NUMBER) {
                runtime_type_error(interpreter,
                                   expr->token,
                                   "sqrt argument",
                                   "a number",
                                   left);
                return 0;
            }
            if (left.number < 0) {
                runtime_error(interpreter, expr->token, "sqrt argument cannot be negative");
                return 0;
            }
            *value = make_number_value(sqrt(left.number));
            return 1;
        case EXPR_IDENT: {
            Variable *variable = find_variable(interpreter, expr->token);
            if (variable == NULL) {
                runtime_name_error(interpreter, expr->token, "undefined variable");
                return 0;
            }
            *value = variable->value;
            return 1;
        }
        case EXPR_STRING:
            return eval_string_literal(interpreter, expr->token, value);
        case EXPR_CALL:
            return call_function(interpreter, expr, value);
        case EXPR_LIST: {
            ListValue *list;
            int i;

            if (interpreter->runtime_list_count >= NEWT_MAX_RUNTIME_LISTS) {
                runtime_error(interpreter, expr->token, "too many runtime lists");
                return 0;
            }
            list = malloc(sizeof(ListValue));
            if (list == NULL) {
                runtime_error(interpreter, expr->token, "not enough memory for list");
                return 0;
            }
            list->elements = NULL;
            list->count = expr->element_count;
            list->capacity = expr->element_count;
            if (list->capacity > 0) {
                list->elements = malloc((size_t)list->capacity * sizeof(Value));
                if (list->elements == NULL) {
                    free(list);
                    runtime_error(interpreter, expr->token, "not enough memory for list");
                    return 0;
                }
            }
            for (i = 0; i < list->count; i++) {
                Expr *element = interpreter->parser->list_elements[expr->element_start + i];

                if (!eval_expression(interpreter, element, &list->elements[i])) {
                    free(list->elements);
                    free(list);
                    return 0;
                }
            }
            if (interpreter->runtime_list_count >= NEWT_MAX_RUNTIME_LISTS) {
                free(list->elements);
                free(list);
                runtime_error(interpreter, expr->token, "too many runtime lists");
                return 0;
            }
            interpreter->runtime_lists[interpreter->runtime_list_count] = list;
            interpreter->runtime_list_count++;
            *value = make_list_value(list);
            return 1;
        }
        case EXPR_INDEX: {
            Value list;
            int index;

            if (!eval_expression(interpreter, expr->left, &list)) {
                return 0;
            }
            if (!resolve_list_index(interpreter,
                                    list,
                                    expr->token,
                                    expr->right,
                                    &index)) {
                return 0;
            }
            *value = list.list->elements[index];
            return 1;
        }
        case EXPR_UNARY:
            if (!eval_expression(interpreter, expr->left, &left)) {
                return 0;
            }
            if (expr->token.type == TOKEN_NOT) {
                if (left.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "not operand",
                                       "a boolean",
                                       left);
                    return 0;
                }
                *value = make_bool_value(!left.boolean);
                return 1;
            }
            if (left.type != VALUE_NUMBER) {
                runtime_type_error(interpreter,
                                   expr->token,
                                   "unary minus operand",
                                   "a number",
                                   left);
                return 0;
            }
            *value = make_number_value(-left.number);
            return 1;
        case EXPR_BINARY:
            if (!eval_expression(interpreter, expr->left, &left)) {
                return 0;
            }
            if (expr->token.type == TOKEN_AND) {
                if (left.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "left operand of 'and'",
                                       "a boolean",
                                       left);
                    return 0;
                }
                if (!left.boolean) {
                    *value = make_bool_value(0);
                    return 1;
                }
                if (!eval_expression(interpreter, expr->right, &right)) {
                    return 0;
                }
                if (right.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "right operand of 'and'",
                                       "a boolean",
                                       right);
                    return 0;
                }
                *value = make_bool_value(right.boolean);
                return 1;
            }
            if (expr->token.type == TOKEN_OR) {
                if (left.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "left operand of 'or'",
                                       "a boolean",
                                       left);
                    return 0;
                }
                if (left.boolean) {
                    *value = make_bool_value(1);
                    return 1;
                }
                if (!eval_expression(interpreter, expr->right, &right)) {
                    return 0;
                }
                if (right.type != VALUE_BOOL) {
                    runtime_type_error(interpreter,
                                       expr->token,
                                       "right operand of 'or'",
                                       "a boolean",
                                       right);
                    return 0;
                }
                *value = make_bool_value(right.boolean);
                return 1;
            }
            if (!eval_expression(interpreter, expr->right, &right)) {
                return 0;
            }

            switch (expr->token.type) {
                case TOKEN_EQUAL_EQUAL:
                    if (left.type != right.type) {
                        *value = make_bool_value(0);
                        return 1;
                    }
                    if (left.type == VALUE_STRING) {
                        *value = make_bool_value(string_values_equal(left, right));
                        return 1;
                    }
                    if (left.type == VALUE_BOOL) {
                        *value = make_bool_value(left.boolean == right.boolean);
                        return 1;
                    }
                    if (left.type == VALUE_LIST) {
                        runtime_error(interpreter, expr->token, "list comparison is not supported");
                        return 0;
                    }
                    *value = make_bool_value(left.number == right.number);
                    return 1;
                case TOKEN_BANG_EQUAL:
                    if (left.type != right.type) {
                        *value = make_bool_value(1);
                        return 1;
                    }
                    if (left.type == VALUE_STRING) {
                        *value = make_bool_value(!string_values_equal(left, right));
                        return 1;
                    }
                    if (left.type == VALUE_BOOL) {
                        *value = make_bool_value(left.boolean != right.boolean);
                        return 1;
                    }
                    if (left.type == VALUE_LIST) {
                        runtime_error(interpreter, expr->token, "list comparison is not supported");
                        return 0;
                    }
                    *value = make_bool_value(left.number != right.number);
                    return 1;
                default:
                    break;
            }

            if (expr->token.type == TOKEN_PLUS &&
                left.type == VALUE_STRING &&
                right.type == VALUE_STRING) {
                return concatenate_strings(interpreter,
                                           expr->token,
                                           left,
                                           right,
                                           value);
            }

            if (left.type != VALUE_NUMBER || right.type != VALUE_NUMBER) {
                if (left.type == VALUE_BOOL || right.type == VALUE_BOOL) {
                    runtime_error(interpreter, expr->token, "numeric operators require numbers, not booleans");
                    return 0;
                }

                runtime_error(interpreter, expr->token, "numeric operators require numbers");
                return 0;
            }

            switch (expr->token.type) {
                case TOKEN_PLUS:
                    *value = make_number_value(left.number + right.number);
                    return 1;
                case TOKEN_MINUS:
                    *value = make_number_value(left.number - right.number);
                    return 1;
                case TOKEN_STAR:
                    *value = make_number_value(left.number * right.number);
                    return 1;
                case TOKEN_SLASH:
                    if (right.number == 0) {
                        runtime_error(interpreter, expr->token, "division by zero");
                        return 0;
                    }
                    *value = make_number_value(left.number / right.number);
                    return 1;
                case TOKEN_LESS:
                    *value = make_bool_value(left.number < right.number);
                    return 1;
                case TOKEN_LESS_EQUAL:
                    *value = make_bool_value(left.number <= right.number);
                    return 1;
                case TOKEN_GREATER:
                    *value = make_bool_value(left.number > right.number);
                    return 1;
                case TOKEN_GREATER_EQUAL:
                    *value = make_bool_value(left.number >= right.number);
                    return 1;
                default:
                    runtime_error(interpreter, expr->token, "unsupported runtime operator");
                    return 0;
            }
        case EXPR_TRUE:
            *value = make_bool_value(1);
            return 1;
        case EXPR_FALSE:
            *value = make_bool_value(0);
            return 1;
    }

    return 0;
}

static int define_variable(Interpreter *interpreter, Token name, Value value, int is_mutable) {
    int i;

    for (i = interpreter->variable_count - 1; i >= 0; i--) {
        Variable *existing = &interpreter->variables[i];

        if (existing->scope_depth < interpreter->scope_depth) {
            break;
        }
        if (token_names_match(existing->name, name)) {
            runtime_name_error(interpreter, name, "variable already defined in this scope");
            return 0;
        }
    }

    if (interpreter->variable_count >= NEWT_MAX_VARIABLES) {
        runtime_error(interpreter, name, "too many variables");
        return 0;
    }

    interpreter->variables[interpreter->variable_count].name = name;
    interpreter->variables[interpreter->variable_count].value = value;
    interpreter->variables[interpreter->variable_count].is_mutable = is_mutable;
    interpreter->variables[interpreter->variable_count].scope_depth = interpreter->scope_depth;
    interpreter->variable_count++;
    return 1;
}

static void print_number(double number) {
    long long whole = (long long)number;

    if (number == (double)whole) {
        printf("%lld\n", whole);
    } else {
        printf("%g\n", number);
    }
}

static void print_string_token(Token token) {
    if (token.length >= 2) {
        int content_length = token.length - 2;

        printf("%.*s", content_length, token.start + 1);
        if (content_length == 0 || token.start[token.length - 2] != '\n') {
            putchar('\n');
        }
    } else {
        printf("\n");
    }
}

static void print_value(Value value) {
    int i;

    if (value.type == VALUE_LIST) {
        putchar('[');
        for (i = 0; i < value.list->count; i++) {
            Value element = value.list->elements[i];

            if (i > 0) {
                printf(", ");
            }
            if (element.type == VALUE_STRING) {
                printf("%.*s", element.string.length, element.string.start);
            } else if (element.type == VALUE_BOOL) {
                printf("%s", element.boolean ? "true" : "false");
            } else if (element.type == VALUE_LIST) {
                printf("<list>");
            } else {
                long long whole = (long long)element.number;

                if (element.number == (double)whole) {
                    printf("%lld", whole);
                } else {
                    printf("%g", element.number);
                }
            }
        }
        printf("]\n");
        return;
    }

    if (value.type == VALUE_STRING) {
        print_string_token(value.string);
        return;
    }

    if (value.type == VALUE_BOOL) {
        printf("%s\n", value.boolean ? "true" : "false");
        return;
    }

    print_number(value.number);
}

static int print_runtime_expression(Interpreter *interpreter, Expr *expr) {
    Value value;

    if (!eval_expression(interpreter, expr, &value)) {
        return 0;
    }

    print_value(value);
    return 1;
}

static int execute_statement(Interpreter *interpreter, Stmt *stmt);

static int execute_statement_list(Interpreter *interpreter, int statements[256], int count) {
    int i;

    for (i = 0;
         i < count &&
         !interpreter->had_error &&
         !interpreter->is_returning &&
         !interpreter->is_breaking &&
         !interpreter->is_continuing;
         i++) {
        int stmt_index = statements[i];
        if (!execute_statement(interpreter, &interpreter->parser->statements[stmt_index])) {
            return 0;
        }
    }

    return 1;
}

static int execute_scoped_statement_list(Interpreter *interpreter,
                                         int statements[256],
                                         int count) {
    int variable_start = interpreter->variable_count;
    int succeeded;

    interpreter->scope_depth++;
    succeeded = execute_statement_list(interpreter, statements, count);
    interpreter->variable_count = variable_start;
    interpreter->scope_depth--;

    return succeeded;
}

static int call_function(Interpreter *interpreter,
                         Expr *call,
                         Value *value) {
    Function *function;
    Value argument_values[NEWT_MAX_FUNCTION_PARAMETERS];
    Value returned_value = make_number_value(0);
    Value previous_return_value = interpreter->return_value;
    int previous_returning = interpreter->is_returning;
    int previous_breaking = interpreter->is_breaking;
    int previous_continuing = interpreter->is_continuing;
    int previous_loop_depth = interpreter->loop_depth;
    int variable_start = interpreter->variable_count;
    int succeeded = 1;
    int did_return;
    int i;

    if (token_text_equals(call->token, "arg_count")) {
        return call_arg_count(interpreter, call, value);
    }
    if (token_text_equals(call->token, "arg")) {
        return call_arg(interpreter, call, value);
    }
    if (token_text_equals(call->token, "file_read")) {
        return call_file_read(interpreter, call, value);
    }
    if (token_text_equals(call->token, "file_write")) {
        return call_file_write(interpreter, call, "wb", "file_write", value);
    }
    if (token_text_equals(call->token, "file_append")) {
        return call_file_write(interpreter, call, "ab", "file_append", value);
    }
    if (token_text_equals(call->token, "text")) {
        return call_text(interpreter, call, value);
    }
    if (token_text_equals(call->token, "len")) {
        return call_len(interpreter, call, value);
    }
    if (token_text_equals(call->token, "length")) {
        return call_length(interpreter, call, value);
    }
    if (token_text_equals(call->token, "append")) {
        return call_append(interpreter, call, value);
    }
    if (token_text_equals(call->token, "contains")) {
        return call_contains(interpreter, call, value);
    }
    if (token_text_equals(call->token, "upper")) {
        return call_change_case(interpreter, call, "upper", 1, value);
    }
    if (token_text_equals(call->token, "lower")) {
        return call_change_case(interpreter, call, "lower", 0, value);
    }
    if (token_text_equals(call->token, "trim")) {
        return call_trim(interpreter, call, value);
    }
    if (token_text_equals(call->token, "abs")) {
        return call_unary_math(interpreter, call, "abs", fabs, value);
    }
    if (token_text_equals(call->token, "floor")) {
        return call_unary_math(interpreter, call, "floor", floor, value);
    }
    if (token_text_equals(call->token, "ceil")) {
        return call_unary_math(interpreter, call, "ceil", ceil, value);
    }
    if (token_text_equals(call->token, "sin")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "sin",
                                     sin,
                                     SCIENTIFIC_DOMAIN_ALL,
                                     value);
    }
    if (token_text_equals(call->token, "cos")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "cos",
                                     cos,
                                     SCIENTIFIC_DOMAIN_ALL,
                                     value);
    }
    if (token_text_equals(call->token, "tan")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "tan",
                                     tan,
                                     SCIENTIFIC_DOMAIN_ALL,
                                     value);
    }
    if (token_text_equals(call->token, "asin")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "asin",
                                     asin,
                                     SCIENTIFIC_DOMAIN_UNIT,
                                     value);
    }
    if (token_text_equals(call->token, "acos")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "acos",
                                     acos,
                                     SCIENTIFIC_DOMAIN_UNIT,
                                     value);
    }
    if (token_text_equals(call->token, "atan")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "atan",
                                     atan,
                                     SCIENTIFIC_DOMAIN_ALL,
                                     value);
    }
    if (token_text_equals(call->token, "atan2")) {
        return call_atan2(interpreter, call, value);
    }
    if (token_text_equals(call->token, "log")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "log",
                                     log,
                                     SCIENTIFIC_DOMAIN_POSITIVE,
                                     value);
    }
    if (token_text_equals(call->token, "log10")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "log10",
                                     log10,
                                     SCIENTIFIC_DOMAIN_POSITIVE,
                                     value);
    }
    if (token_text_equals(call->token, "exp")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "exp",
                                     exp,
                                     SCIENTIFIC_DOMAIN_ALL,
                                     value);
    }
    if (token_text_equals(call->token, "round")) {
        return call_scientific_unary(interpreter,
                                     call,
                                     "round",
                                     round,
                                     SCIENTIFIC_DOMAIN_ALL,
                                     value);
    }
    if (token_text_equals(call->token, "min")) {
        return call_min_max(interpreter, call, "min", 0, value);
    }
    if (token_text_equals(call->token, "max")) {
        return call_min_max(interpreter, call, "max", 1, value);
    }
    if (token_text_equals(call->token, "pow")) {
        return call_pow(interpreter, call, value);
    }

    function = find_function(interpreter, call->token);

    if (function == NULL) {
        runtime_name_error(interpreter, call->token, "undefined function");
        return 0;
    }
    if (call->argument_count != function->declaration->parameter_count) {
        runtime_function_arity_error(interpreter,
                                     call->token,
                                     function->declaration->parameter_count,
                                     call->argument_count);
        return 0;
    }
    if (interpreter->call_depth >= NEWT_MAX_CALL_DEPTH) {
        runtime_name_error(interpreter,
                           call->token,
                           "maximum call depth exceeded in function");
        return 0;
    }

    for (i = 0; i < call->argument_count; i++) {
        Expr *argument = interpreter->parser->call_arguments[call->argument_start + i];
        if (!eval_expression(interpreter, argument, &argument_values[i])) {
            return 0;
        }
    }

    interpreter->call_depth++;
    interpreter->scope_depth++;
    interpreter->is_returning = 0;
    interpreter->is_breaking = 0;
    interpreter->is_continuing = 0;
    interpreter->loop_depth = 0;

    for (i = 0; i < function->declaration->parameter_count; i++) {
        Token parameter = interpreter->parser->parameters[
            function->declaration->parameter_start + i];
        if (!define_variable(interpreter, parameter, argument_values[i], 0)) {
            succeeded = 0;
            break;
        }
    }

    if (succeeded) {
        succeeded = execute_statement_list(interpreter,
                                           function->declaration->body_statements,
                                           function->declaration->body_count);
    }

    did_return = interpreter->is_returning;
    if (did_return) {
        returned_value = interpreter->return_value;
    }

    interpreter->variable_count = variable_start;
    interpreter->scope_depth--;
    interpreter->call_depth--;
    interpreter->is_returning = previous_returning;
    interpreter->is_breaking = previous_breaking;
    interpreter->is_continuing = previous_continuing;
    interpreter->loop_depth = previous_loop_depth;
    interpreter->return_value = previous_return_value;

    if (!succeeded) {
        return 0;
    }
    if (value != NULL) {
        *value = returned_value;
    }

    return 1;
}

static int execute_statement(Interpreter *interpreter, Stmt *stmt) {
    Value value;

    if (interpreter->had_error) {
        return 0;
    }

    switch (stmt->kind) {
        case STMT_FN_DECL:
            return define_function(interpreter, stmt);
        case STMT_CALL:
            return call_function(interpreter, stmt->expression, NULL);
        case STMT_RETURN:
            if (interpreter->call_depth == 0) {
                runtime_error(interpreter,
                              stmt->name,
                              "return can only be used inside a function");
                return 0;
            }
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            interpreter->return_value = value;
            interpreter->is_returning = 1;
            return 1;
        case STMT_BREAK:
            if (interpreter->loop_depth == 0) {
                runtime_error(interpreter,
                              stmt->name,
                              "break can only be used inside a loop");
                return 0;
            }
            interpreter->is_breaking = 1;
            return 1;
        case STMT_CONTINUE:
            if (interpreter->loop_depth == 0) {
                runtime_error(interpreter,
                              stmt->name,
                              "continue can only be used inside a loop");
                return 0;
            }
            interpreter->is_continuing = 1;
            return 1;
        case STMT_VAL_DECL:
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            return define_variable(interpreter, stmt->name, value, 0);
        case STMT_MUT_DECL:
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            return define_variable(interpreter, stmt->name, value, 1);
        case STMT_ASSIGN: {
            Variable *variable = find_variable(interpreter, stmt->name);
            if (variable == NULL) {
                runtime_name_error(interpreter, stmt->name, "undefined variable");
                return 0;
            }
            if (!variable->is_mutable) {
                runtime_name_error(interpreter, stmt->name, "cannot assign to immutable val");
                return 0;
            }
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            variable->value = value;
            return 1;
        }
        case STMT_INDEX_ASSIGN: {
            Variable *variable = find_variable(interpreter, stmt->name);
            Expr *target = stmt->target;
            Value list;
            int index;

            if (variable == NULL) {
                runtime_name_error(interpreter, stmt->name, "undefined variable");
                return 0;
            }
            if (!variable->is_mutable) {
                runtime_name_error(interpreter, stmt->name, "cannot mutate immutable val");
                return 0;
            }
            if (!eval_expression(interpreter, target->left, &list)) {
                return 0;
            }
            if (!resolve_list_index(interpreter,
                                    list,
                                    target->token,
                                    target->right,
                                    &index)) {
                return 0;
            }
            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }
            list.list->elements[index] = value;
            return 1;
        }

        case STMT_PRINT:
            return print_runtime_expression(interpreter, stmt->expression);
        case STMT_IF: {
            int *branch;
            int branch_count;

            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }

            if (value_is_truthy(value)) {
                branch = stmt->then_statements;
                branch_count = stmt->then_count;
            } else if (stmt->has_else) {
                branch = stmt->else_statements;
                branch_count = stmt->else_count;
            } else {
                return 1;
            }

            return execute_scoped_statement_list(interpreter, branch, branch_count);
        }
        case STMT_WHILE: {
            int iterations = 0;
            int succeeded = 1;

            interpreter->loop_depth++;
            while (!interpreter->had_error) {
                if (iterations >= NEWT_MAX_WHILE_ITERATIONS) {
                    runtime_error(interpreter, stmt->name, "while loop exceeded max iteration limit");
                    succeeded = 0;
                    break;
                }

                if (!eval_expression(interpreter, stmt->expression, &value)) {
                    succeeded = 0;
                    break;
                }

                if (!value_is_truthy(value)) {
                    break;
                }

                iterations++;
                if (!execute_scoped_statement_list(interpreter,
                                                   stmt->body_statements,
                                                   stmt->body_count)) {
                    succeeded = 0;
                    break;
                }
                if (interpreter->is_returning) {
                    break;
                }
                if (interpreter->is_breaking) {
                    interpreter->is_breaking = 0;
                    break;
                }
                if (interpreter->is_continuing) {
                    interpreter->is_continuing = 0;
                    continue;
                }
            }

            interpreter->loop_depth--;
            return succeeded && !interpreter->had_error;
        }
        case STMT_FOR: {
            Value iterable;
            int iterable_count;
            int index;
            int succeeded = 1;

            if (!eval_expression(interpreter, stmt->expression, &iterable)) {
                return 0;
            }
            if (iterable.type != VALUE_LIST) {
                runtime_type_error(interpreter,
                                   stmt->expression->token,
                                   "for iterable",
                                   "a list",
                                   iterable);
                return 0;
            }

            iterable_count = iterable.list->count;
            interpreter->loop_depth++;
            for (index = 0; index < iterable_count; index++) {
                int variable_start = interpreter->variable_count;

                interpreter->scope_depth++;
                if (!define_variable(interpreter,
                                     stmt->name,
                                     iterable.list->elements[index],
                                     0)) {
                    succeeded = 0;
                } else if (!execute_statement_list(interpreter,
                                                   stmt->body_statements,
                                                   stmt->body_count)) {
                    succeeded = 0;
                }
                interpreter->variable_count = variable_start;
                interpreter->scope_depth--;

                if (!succeeded || interpreter->is_returning) {
                    break;
                }
                if (interpreter->is_breaking) {
                    interpreter->is_breaking = 0;
                    break;
                }
                if (interpreter->is_continuing) {
                    interpreter->is_continuing = 0;
                    continue;
                }
            }
            interpreter->loop_depth--;
            return succeeded && !interpreter->had_error;
        }
    }

    return 0;
}
int run_program(const char *path,
                       const char *source,
                       int script_argument_count,
                       char **script_arguments) {
    Parser parser;
    Interpreter interpreter;
    int i;

    parser_init(&parser, path, source);
    if (!parse_program(&parser)) {
        return 0;
    }

    interpreter.parser = &parser;
    interpreter.variables = malloc(NEWT_MAX_VARIABLES * sizeof(Variable));
    if (interpreter.variables == NULL) {
        fprintf(stderr, "error: not enough memory for variables\n");
        return 0;
    }
    interpreter.variable_count = 0;
    interpreter.function_count = 0;
    interpreter.call_depth = 0;
    interpreter.scope_depth = 0;
    interpreter.is_returning = 0;
    interpreter.is_breaking = 0;
    interpreter.is_continuing = 0;
    interpreter.loop_depth = 0;
    interpreter.return_value = make_number_value(0);
    interpreter.script_argument_count = script_argument_count;
    interpreter.script_arguments = script_arguments;
    interpreter.runtime_string_count = 0;
    interpreter.runtime_list_count = 0;
    interpreter.had_error = 0;

    {
        Token pi_name = { TOKEN_IDENT, "pi", 2, 0, 0, NULL };
        Token e_name = { TOKEN_IDENT, "e", 1, 0, 0, NULL };

        if (!define_variable(&interpreter,
                             pi_name,
                             make_number_value(NEWT_PI),
                             0) ||
            !define_variable(&interpreter,
                             e_name,
                             make_number_value(NEWT_E),
                             0)) {
            free(interpreter.variables);
            return 0;
        }
    }

    for (i = 0; i < parser.top_level_statement_count && !interpreter.had_error; i++) {
        int stmt_index = parser.top_level_statements[i];
        execute_statement(&interpreter, &parser.statements[stmt_index]);
    }

    {
        int succeeded = !interpreter.had_error;

        for (i = 0; i < interpreter.runtime_list_count; i++) {
            free(interpreter.runtime_lists[i]->elements);
            free(interpreter.runtime_lists[i]);
        }
        for (i = 0; i < interpreter.runtime_string_count; i++) {
            free(interpreter.runtime_strings[i]);
        }
        free(interpreter.variables);
        return succeeded;
    }
}
