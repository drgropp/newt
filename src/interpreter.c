/* Interpreter: evaluates expressions and executes statements. */

#include "interpreter.h"
#include "builtins.h"
#include "numeric.h"

Value make_number_value(double number) {
    Value value;

    value.type = VALUE_NUMBER;
    value.number = number;
    value.numeric_type = TYPE_AUTO;
    value.integer = 0;
    value.single = 0;
    value.boolean = 0;
    value.string.start = NULL;
    value.string.length = 0;
    value.string.line = 0;
    value.string.column = 0;
    value.string.error_message = NULL;
    value.string_object = NULL;
    value.list = NULL;
    return value;
}

static Value make_string_value(Token string) {
    Value value;

    value.type = VALUE_STRING;
    value.number = 0;
    value.numeric_type = TYPE_AUTO;
    value.integer = 0;
    value.single = 0;
    value.boolean = 0;
    value.string = string;
    value.string_object = NULL;
    value.list = NULL;
    return value;
}

Value make_bool_value(int boolean) {
    Value value;

    value.type = VALUE_BOOL;
    value.number = 0;
    value.numeric_type = TYPE_AUTO;
    value.integer = 0;
    value.single = 0;
    value.boolean = boolean;
    value.string.start = NULL;
    value.string.length = 0;
    value.string.line = 0;
    value.string.column = 0;
    value.string.error_message = NULL;
    value.string_object = NULL;
    value.list = NULL;
    return value;
}

static Value make_list_value(ListValue *list) {
    Value value;

    value.type = VALUE_LIST;
    value.number = 0;
    value.numeric_type = TYPE_AUTO;
    value.integer = 0;
    value.single = 0;
    value.boolean = 0;
    value.string.start = NULL;
    value.string.length = 0;
    value.string.line = 0;
    value.string.column = 0;
    value.string.error_message = NULL;
    value.string_object = NULL;
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
        case VALUE_VOID:
            return "void";
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

void value_retain_object(Value value) {
    if (value.type == VALUE_STRING && value.string_object != NULL) {
        value.string_object->ref_count++;
    } else if (value.type == VALUE_LIST) {
        value.list->ref_count++;
    }
}

static void free_string_object(Interpreter *interpreter, StringObject *object) {
    if (object->previous != NULL) {
        object->previous->next = object->next;
    } else {
        interpreter->live_strings = object->next;
    }
    if (object->next != NULL) {
        object->next->previous = object->previous;
    }
    free(object->characters);
    free(object);
}

static void unlink_list(Interpreter *interpreter, ListValue *list) {
    if (list->previous != NULL) {
        list->previous->next = list->next;
    } else {
        interpreter->live_lists = list->next;
    }
    if (list->next != NULL) {
        list->next->previous = list->previous;
    }
}

static void link_list(Interpreter *interpreter, ListValue *list) {
    list->ref_count = 1;
    list->previous = NULL;
    list->next = interpreter->live_lists;
    if (interpreter->live_lists != NULL) {
        interpreter->live_lists->previous = list;
    }
    interpreter->live_lists = list;
}

static int push_release_stack(Interpreter *interpreter, ListValue *list) {
    if (interpreter->release_stack_count == interpreter->release_stack_capacity) {
        int new_capacity = interpreter->release_stack_capacity == 0
                               ? 64
                               : interpreter->release_stack_capacity * 2;
        ListValue **grown;

        if (interpreter->release_stack_capacity > INT_MAX / 2) {
            return 0;
        }
        grown = realloc(interpreter->release_stack,
                        (size_t)new_capacity * sizeof(ListValue *));
        if (grown == NULL) {
            return 0;
        }
        interpreter->release_stack = grown;
        interpreter->release_stack_capacity = new_capacity;
    }
    interpreter->release_stack[interpreter->release_stack_count++] = list;
    return 1;
}

/* Drop one reference. Lists whose count reaches zero go on a work stack
   instead of being freed recursively, so deeply nested lists cannot
   overflow the C stack. If the stack cannot grow, the list simply stays in
   the registry and is freed when the program ends. */
void value_release_object(Interpreter *interpreter, Value value) {
    if (value.type == VALUE_STRING && value.string_object != NULL) {
        if (--value.string_object->ref_count == 0) {
            free_string_object(interpreter, value.string_object);
        }
        return;
    }
    if (value.type != VALUE_LIST) {
        return;
    }
    if (--value.list->ref_count > 0 || !push_release_stack(interpreter, value.list)) {
        return;
    }

    while (interpreter->release_stack_count > 0) {
        ListValue *list = interpreter->release_stack[--interpreter->release_stack_count];
        int i;

        for (i = 0; i < list->count; i++) {
            Value element = list->elements[i];

            if (element.type == VALUE_STRING && element.string_object != NULL) {
                if (--element.string_object->ref_count == 0) {
                    free_string_object(interpreter, element.string_object);
                }
            } else if (element.type == VALUE_LIST) {
                if (--element.list->ref_count == 0 &&
                    !push_release_stack(interpreter, element.list)) {
                    element.list->ref_count = 1;
                }
            }
        }
        unlink_list(interpreter, list);
        free(list->elements);
        free(list);
    }
}

int make_runtime_string(Interpreter *interpreter,
                               Token source,
                               char *characters,
                               int content_length,
                               Value *value) {
    Token string = source;

    StringObject *object;

    /* The new string starts with one reference, owned by the caller. */
    if (content_length < 0 || content_length > INT_MAX - 2) {
        free(characters);
        runtime_error(interpreter, source, "runtime string is too long");
        return 0;
    }
    if (value == NULL) {
        free(characters);
        return 1;
    }
    object = malloc(sizeof(StringObject));
    if (object == NULL) {
        free(characters);
        runtime_error(interpreter, source, "not enough memory for string");
        return 0;
    }
    object->ref_count = 1;
    object->characters = characters;
    object->previous = NULL;
    object->next = interpreter->live_strings;
    if (interpreter->live_strings != NULL) {
        interpreter->live_strings->previous = object;
    }
    interpreter->live_strings = object;

    string.type = TOKEN_STRING;
    string.start = characters;
    string.length = content_length + 2;
    string.error_message = NULL;
    *value = make_string_value(string);
    value->string_object = object;
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

/* Lexical scoping. Top-level code sees every variable in scope. Inside a
   function, lookup checks that call's own frame, then top-level (global)
   variables; it never sees variables belonging to the caller. */
Variable *find_variable(Interpreter *interpreter, Token name) {
    int stop = interpreter->call_depth > 0 ? interpreter->frame_base : 0;
    int i;

    for (i = interpreter->variable_count - 1; i >= stop; i--) {
        if (token_names_match(interpreter->variables[i].name, name)) {
            return &interpreter->variables[i];
        }
    }

    if (interpreter->call_depth > 0) {
        for (i = interpreter->global_limit - 1; i >= 0; i--) {
            if (interpreter->variables[i].scope_depth == 0 &&
                token_names_match(interpreter->variables[i].name, name)) {
                return &interpreter->variables[i];
            }
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
    value_release(interpreter, decoded_prompt);
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

static int require_decl_type(Interpreter *interpreter, Token token, DeclType type, Value value) {
    int matches = type == TYPE_AUTO ||
        (type == TYPE_INT && value.type == VALUE_NUMBER && value.numeric_type == TYPE_INT) ||
        (type == TYPE_FLOAT && value.type == VALUE_NUMBER && value.numeric_type == TYPE_FLOAT) ||
        (type == TYPE_DOUBLE && value.type == VALUE_NUMBER && value.numeric_type == TYPE_DOUBLE) ||
        (type == TYPE_BOOL && value.type == VALUE_BOOL) ||
        (type == TYPE_STRING && value.type == VALUE_STRING) ||
        (type == TYPE_LIST && value.type == VALUE_LIST) ||
        (type == TYPE_VOID && value.type == VALUE_VOID);
    if (!matches) {
        char message[160];
        const char *actual = value.type == VALUE_NUMBER ? decl_type_name(value.numeric_type) : value_type_name(value.type);
        snprintf(message, sizeof(message), "expected %s, got %s (use an explicit numeric conversion when needed)", decl_type_name(type), actual);
        runtime_error(interpreter, token, message);
    }
    return matches;
}

static int eval_decl_expression(Interpreter *interpreter, Expr *expr, DeclType type, Value *value) {
    DeclType previous = interpreter->literal_type;
    int ok;
    interpreter->literal_type = type;
    ok = eval_expression(interpreter, expr, value);
    interpreter->literal_type = previous;
    if (!ok) return 0;
    if (!require_decl_type(interpreter, expr->token, type, *value)) {
        value_release(interpreter, *value);
        return 0;
    }
    return 1;
}

static int is_numeric_literal_expression(const Expr *expr) {
    if (expr == NULL) return 0;
    if (expr->kind == EXPR_NUMBER) return 1;
    if (expr->kind == EXPR_UNARY && expr->token.type == TOKEN_MINUS) return is_numeric_literal_expression(expr->left);
    if (expr->kind == EXPR_BINARY && (expr->token.type == TOKEN_PLUS || expr->token.type == TOKEN_MINUS ||
        expr->token.type == TOKEN_STAR || expr->token.type == TOKEN_SLASH)) {
        return is_numeric_literal_expression(expr->left) && is_numeric_literal_expression(expr->right);
    }
    return 0;
}

static int eval_binary_operator(Interpreter *interpreter,
                                Expr *expr,
                                Value left,
                                Value right,
                                Value *value) {
    if (left.type == VALUE_NUMBER && right.type == VALUE_NUMBER &&
        (left.numeric_type != TYPE_AUTO || right.numeric_type != TYPE_AUTO)) {
        /* A literal beside a typed operand adopts that operand's type. */
        if (left.numeric_type == TYPE_AUTO && is_numeric_literal_expression(expr->left)) {
            if (!eval_decl_expression(interpreter, expr->left, right.numeric_type, &left)) return 0;
        }
        if (right.numeric_type == TYPE_AUTO && is_numeric_literal_expression(expr->right)) {
            if (!eval_decl_expression(interpreter, expr->right, left.numeric_type, &right)) return 0;
        }
        return typed_numeric_operator(interpreter, expr, left, right, value);
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
}

int eval_expression(Interpreter *interpreter, Expr *expr, Value *value) {
    Value left;
    Value right;
    double number;

    if (expr == NULL || interpreter->had_error) {
        return 0;
    }

    switch (expr->kind) {
        case EXPR_MEMBER: case EXPR_AGGREGATE: case EXPR_SIZEOF:
            runtime_error(interpreter, expr->token, "this systems feature requires --emit-asm"); return 0;
        case EXPR_NUMBER:
            if (!read_number_token(interpreter, expr->token, &number)) {
                return 0;
            }
            *value = make_number_value(number);
            if (interpreter->literal_type == TYPE_INT || interpreter->literal_type == TYPE_FLOAT || interpreter->literal_type == TYPE_DOUBLE) {
                return convert_numeric(interpreter, expr->token, *value, interpreter->literal_type, 0, value);
            }
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
            value_retain(*value);
            return 1;
        }
        case EXPR_STRING:
            return eval_string_literal(interpreter, expr->token, value);
        case EXPR_CALL:
            return call_function(interpreter, expr, value);
        case EXPR_LIST: {
            ListValue *list;
            int i;
            const int count = expr->element_count;

            list = malloc(sizeof(ListValue));
            if (list == NULL) {
                runtime_error(interpreter, expr->token, "not enough memory for list");
                return 0;
            }
            list->elements = NULL;
            list->count = count;
            list->capacity = count;
            if (count > 0) {
                list->elements = malloc((size_t)count * sizeof(Value));
                if (list->elements == NULL) {
                    free(list);
                    runtime_error(interpreter, expr->token, "not enough memory for list");
                    return 0;
                }
                for (i = 0; i < count; i++) {
                    Expr *element = interpreter->parser->list_elements[expr->element_start + i];

                    if (!eval_expression(interpreter, element, &list->elements[i])) {
                        free(list->elements);
                        free(list);
                        return 0;
                    }
                }
            }
            link_list(interpreter, list);
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
            /* Take the element's reference before dropping the list's. */
            *value = list.list->elements[index];
            value_retain(*value);
            value_release(interpreter, list);
            return 1;
        }
        case EXPR_UNARY:
            if (expr->token.type == TOKEN_AMP || expr->token.type == TOKEN_STAR) {
                runtime_error(interpreter, expr->token, "pointer operations require --emit-asm"); return 0;
            }
            if (expr->token.type == TOKEN_MINUS && interpreter->literal_type == TYPE_INT &&
                expr->left->kind == EXPR_NUMBER) {
                double magnitude;
                if (!read_number_token(interpreter, expr->left->token, &magnitude)) return 0;
                return convert_numeric(interpreter, expr->token, make_number_value(-magnitude), TYPE_INT, 0, value);
            }
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
            if (left.numeric_type == TYPE_INT && left.integer == INT_MIN) {
                runtime_error(interpreter, expr->token, "int arithmetic overflow");
                return 0;
            }
            if (left.numeric_type != TYPE_AUTO) {
                return convert_numeric(interpreter, expr->token, make_number_value(-left.number), left.numeric_type, 0, value);
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
            {
                int succeeded = eval_binary_operator(interpreter, expr, left, right, value);

                value_release(interpreter, left);
                value_release(interpreter, right);
                return succeeded;
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
    interpreter->variables[interpreter->variable_count].declared_type = TYPE_AUTO;
    interpreter->variable_count++;
    return 1;
}

static void print_number_inline(double number) {
    if (isfinite(number) && number >= (double)LLONG_MIN && number < 9223372036854775808.0 && trunc(number) == number) {
        long long whole = (long long)number;
        printf("%lld", whole);
    } else {
        printf("%g", number);
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
                print_number_inline(element.number);
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

    print_number_inline(value.number);
    putchar('\n');
}

static int print_runtime_expression(Interpreter *interpreter, Expr *expr) {
    Value value;

    if (!eval_expression(interpreter, expr, &value)) {
        return 0;
    }

    print_value(value);
    value_release(interpreter, value);
    return 1;
}

/* Leave a scope: drop the references held by its variables. */
static void pop_variables(Interpreter *interpreter, int variable_start) {
    while (interpreter->variable_count > variable_start) {
        interpreter->variable_count--;
        value_release(interpreter,
                      interpreter->variables[interpreter->variable_count].value);
    }
}

static int execute_statement(Interpreter *interpreter, Stmt *stmt);

static int execute_statement_list(Interpreter *interpreter, int *statements, int count) {
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
                                         int *statements,
                                         int count) {
    int variable_start = interpreter->variable_count;
    int succeeded;

    interpreter->scope_depth++;
    succeeded = execute_statement_list(interpreter, statements, count);
    pop_variables(interpreter, variable_start);
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
    DeclType previous_return_type = interpreter->return_type;
    int previous_returning = interpreter->is_returning;
    int previous_breaking = interpreter->is_breaking;
    int previous_continuing = interpreter->is_continuing;
    int previous_loop_depth = interpreter->loop_depth;
    int previous_frame_base;
    int previous_global_limit;
    int variable_start = interpreter->variable_count;
    int succeeded = 1;
    int did_return;
    int i;

    if (call->token.type == TOKEN_INT || call->token.type == TOKEN_FLOAT || call->token.type == TOKEN_DOUBLE) {
        Value source, converted;
        DeclType previous = interpreter->literal_type;
        DeclType target = call->token.type == TOKEN_INT ? TYPE_INT : call->token.type == TOKEN_FLOAT ? TYPE_FLOAT : TYPE_DOUBLE;
        int ok;
        if (call->argument_count != 1) {
            runtime_error(interpreter, call->token, "numeric conversion expects exactly one argument");
            return 0;
        }
        interpreter->literal_type = TYPE_AUTO;
        ok = eval_expression(interpreter, interpreter->parser->call_arguments[call->argument_start], &source);
        interpreter->literal_type = previous;
        if (!ok) return 0;
        ok = convert_numeric(interpreter, call->token, source, target, 1, &converted);
        value_release(interpreter, source);
        if (ok && value != NULL) *value = converted;
        return ok;
    }

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
    if (function->declaration->is_extern || function->declaration->native_type.pointers || function->declaration->declared_type == TYPE_STRUCT) {
        runtime_error(interpreter, call->token, "native function signature requires --emit-asm"); return 0;
    }
    for (i = 0; i < function->declaration->parameter_count; ++i) {
        TypeSpec t = interpreter->parser->parameters[function->declaration->parameter_start + i].native_type;
        if (t.pointers || t.base == TYPE_STRUCT) {
            runtime_error(interpreter, call->token, "native parameter types require --emit-asm"); return 0;
        }
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
        if (!eval_decl_expression(interpreter, argument, interpreter->parser->parameters[function->declaration->parameter_start + i].type, &argument_values[i])) {
            return 0;
        }
    }

    previous_frame_base = interpreter->frame_base;
    previous_global_limit = interpreter->global_limit;
    if (interpreter->call_depth == 0) {
        interpreter->global_limit = interpreter->variable_count;
    }
    interpreter->frame_base = interpreter->variable_count;
    interpreter->call_depth++;
    interpreter->scope_depth++;
    interpreter->is_returning = 0;
    interpreter->return_type = function->declaration->declared_type;
    interpreter->is_breaking = 0;
    interpreter->is_continuing = 0;
    interpreter->loop_depth = 0;

    for (i = 0; i < function->declaration->parameter_count; i++) {
        Parameter parameter = interpreter->parser->parameters[function->declaration->parameter_start + i];
        if (!define_variable(interpreter, parameter.name, argument_values[i], parameter.is_mutable)) {
            succeeded = 0;
            break;
        }
        interpreter->variables[interpreter->variable_count - 1].declared_type = parameter.type;
    }

    if (succeeded) {
        succeeded = execute_statement_list(interpreter,
                                           function->declaration->body_statements,
                                           function->declaration->body_count);
    }

    did_return = interpreter->is_returning;
    if (did_return) {
        returned_value = interpreter->return_value;
    } else if (succeeded && function->declaration->declared_type != TYPE_AUTO && function->declaration->declared_type != TYPE_VOID) {
        runtime_error(interpreter, call->token, "typed function completed without returning a value");
        succeeded = 0;
    } else if (function->declaration->declared_type == TYPE_VOID) {
        returned_value.type = VALUE_VOID;
    }

    pop_variables(interpreter, variable_start);
    interpreter->scope_depth--;
    interpreter->call_depth--;
    interpreter->is_returning = previous_returning;
    interpreter->is_breaking = previous_breaking;
    interpreter->is_continuing = previous_continuing;
    interpreter->loop_depth = previous_loop_depth;
    interpreter->return_value = previous_return_value;
    interpreter->return_type = previous_return_type;
    interpreter->frame_base = previous_frame_base;
    interpreter->global_limit = previous_global_limit;

    if (!succeeded) {
        return 0;
    }
    if (value != NULL && returned_value.type == VALUE_VOID) {
        runtime_error(interpreter, call->token, "void function cannot be used as a value");
        return 0;
    }
    /* The returned value's reference passes to the caller, or is dropped
       when the call is used as a statement. */
    if (value != NULL) {
        *value = returned_value;
    } else {
        value_release(interpreter, returned_value);
    }

    return 1;
}

static int execute_statement(Interpreter *interpreter, Stmt *stmt) {
    Value value;

    if (interpreter->had_error) {
        return 0;
    }

    switch (stmt->kind) {
        case STMT_STRUCT: case STMT_MEMORY_ASSIGN: case STMT_ASM:
            runtime_error(interpreter, stmt->name, "this systems feature requires --emit-asm"); return 0;
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
            if (interpreter->return_type == TYPE_VOID) {
                if (stmt->expression != NULL) {
                    runtime_error(interpreter, stmt->name, "void function cannot return a value");
                    return 0;
                }
                value = make_number_value(0);
                value.type = VALUE_VOID;
            } else {
                if (stmt->expression == NULL) {
                    runtime_error(interpreter, stmt->name, "return requires a value in this function");
                    return 0;
                }
                if (!eval_decl_expression(interpreter, stmt->expression, interpreter->return_type, &value)) return 0;
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
        case STMT_LET_DECL:
        case STMT_MUT_DECL:
            if (stmt->native_type.pointers || stmt->declared_type == TYPE_STRUCT) {
                runtime_error(interpreter, stmt->name, "native object types require --emit-asm"); return 0;
            }
            if (!eval_decl_expression(interpreter, stmt->expression, stmt->declared_type, &value)) return 0;
            if (!define_variable(interpreter, stmt->name, value, stmt->kind == STMT_MUT_DECL)) {
                value_release(interpreter, value);
                return 0;
            }
            interpreter->variables[interpreter->variable_count - 1].declared_type = stmt->declared_type;
            return 1;
        case STMT_ASSIGN: {
            Variable *variable = find_variable(interpreter, stmt->name);
            if (variable == NULL) {
                runtime_name_error(interpreter, stmt->name, "undefined variable");
                return 0;
            }
            if (!variable->is_mutable) {
                runtime_name_error(interpreter, stmt->name, "cannot assign to immutable variable");
                return 0;
            }
            if (!eval_decl_expression(interpreter, stmt->expression, variable->declared_type, &value)) {
                return 0;
            }
            {
                Value old_value = variable->value;

                variable->value = value;
                value_release(interpreter, old_value);
            }
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
                runtime_name_error(interpreter, stmt->name, "cannot mutate immutable variable");
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
            {
                Value old_value = list.list->elements[index];

                list.list->elements[index] = value;
                value_release(interpreter, old_value);
            }
            value_release(interpreter, list);
            return 1;
        }

        case STMT_PRINT:
            return print_runtime_expression(interpreter, stmt->expression);
        case STMT_IF: {
            int *branch;
            int branch_count;
            int truthy;

            if (!eval_expression(interpreter, stmt->expression, &value)) {
                return 0;
            }

            truthy = value_is_truthy(value);
            value_release(interpreter, value);
            if (truthy) {
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

                {
                    int truthy = value_is_truthy(value);

                    value_release(interpreter, value);
                    if (!truthy) {
                        break;
                    }
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
                value_retain(iterable.list->elements[index]);
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
                pop_variables(interpreter, variable_start);
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
            value_release(interpreter, iterable);
            return succeeded && !interpreter->had_error;
        }
    }

    return 0;
}
int run_program(const char *path,
                       const char *source,
                       int script_argument_count,
                       char **script_arguments,
                       int *program_exit_status) {
    Parser parser;
    Interpreter interpreter;
    int i;

    if (program_exit_status != NULL) *program_exit_status = 0;
    parser_init(&parser, path, source);
    if (!parse_program(&parser)) {
        parser_free(&parser);
        return 0;
    }

    interpreter.parser = &parser;
    interpreter.variables = malloc(NEWT_MAX_VARIABLES * sizeof(Variable));
    if (interpreter.variables == NULL) {
        fprintf(stderr, "error: not enough memory for variables\n");
        parser_free(&parser);
        return 0;
    }
    interpreter.variable_count = 0;
    interpreter.frame_base = 0;
    interpreter.global_limit = 0;
    interpreter.function_count = 0;
    interpreter.call_depth = 0;
    interpreter.scope_depth = 0;
    interpreter.is_returning = 0;
    interpreter.is_breaking = 0;
    interpreter.is_continuing = 0;
    interpreter.loop_depth = 0;
    interpreter.return_value = make_number_value(0);
    interpreter.return_type = TYPE_AUTO;
    interpreter.literal_type = TYPE_AUTO;
    interpreter.script_argument_count = script_argument_count;
    interpreter.script_arguments = script_arguments;
    interpreter.live_strings = NULL;
    interpreter.live_lists = NULL;
    interpreter.release_stack = NULL;
    interpreter.release_stack_count = 0;
    interpreter.release_stack_capacity = 0;
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
            parser_free(&parser);
            return 0;
        }
    }

    for (i = 0; i < parser.top_level_statement_count && !interpreter.had_error; i++) {
        int stmt_index = parser.top_level_statements[i];
        execute_statement(&interpreter, &parser.statements[stmt_index]);
    }

    if (!interpreter.had_error && parser.entry_statement >= 0) {
        Stmt *entry = &parser.statements[parser.entry_statement];
        Expr call = {0};
        Value result;
        call.kind = EXPR_CALL;
        call.token = entry->name;
        if (entry->declared_type == TYPE_VOID) {
            call_function(&interpreter, &call, NULL);
        } else if (call_function(&interpreter, &call, &result)) {
            if (program_exit_status != NULL) *program_exit_status = result.integer;
        }
    }

    {
        int succeeded = !interpreter.had_error;

        /* Free whatever is still alive: values held by top-level variables,
           lists that reference themselves, and anything left by an error. */
        while (interpreter.live_lists != NULL) {
            ListValue *list = interpreter.live_lists;

            interpreter.live_lists = list->next;
            free(list->elements);
            free(list);
        }
        while (interpreter.live_strings != NULL) {
            StringObject *object = interpreter.live_strings;

            interpreter.live_strings = object->next;
            free(object->characters);
            free(object);
        }
        free(interpreter.release_stack);
        free(interpreter.variables);
        parser_free(&parser);
        return succeeded;
    }
}

