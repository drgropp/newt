/* Built-ins: files, script arguments, text, lists, and math. */

#include "builtins.h"

static int check_builtin_arity(Interpreter *interpreter, Expr *call, int expected) {
    char message[160];

    if (call->argument_count == expected) {
        return 1;
    }

    snprintf(message,
             sizeof(message),
             "%.*s expects %d argument%s, got %d",
             call->token.length,
             call->token.start,
             expected,
             expected == 1 ? "" : "s",
             call->argument_count);
    runtime_error(interpreter, call->token, message);
    return 0;
}

static int copy_file_path(Interpreter *interpreter,
                          Token call_token,
                          Value path_value,
                          const char *subject,
                          char path[1024]) {
    int path_length;

    if (path_value.type != VALUE_STRING) {
        runtime_type_error(interpreter,
                           call_token,
                           subject,
                           "a string",
                           path_value);
        return 0;
    }

    path_length = path_value.string.length - 2;
    if (path_length < 0) {
        path_length = 0;
    }
    if (path_length >= 1024) {
        runtime_error(interpreter, call_token, "file path is too long");
        return 0;
    }

    memcpy(path, path_value.string.start + 1, (size_t)path_length);
    path[path_length] = '\0';
    return 1;
}

int call_file_read(Interpreter *interpreter, Expr *call, Value *value) {
    Expr *path_expression;
    Value path_value;
    char path[1024];
    char message[1200];
    char *characters;
    FILE *file;
    long file_size;
    size_t bytes_read;

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }

    path_expression = interpreter->parser->call_arguments[call->argument_start];
    if (!eval_expression(interpreter, path_expression, &path_value)) {
        return 0;
    }
    if (!copy_file_path(interpreter,
                        call->token,
                        path_value,
                        "file_read path",
                        path)) {
        return 0;
    }
    value_release(interpreter, path_value);

    file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(message,
                 sizeof(message),
                 "file_read could not open file '%s': %s",
                 path,
                 strerror(errno));
        runtime_error(interpreter, call->token, message);
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        runtime_error(interpreter, call->token, "file_read could not seek in file");
        return 0;
    }

    file_size = ftell(file);
    if (file_size < 0 || file_size > INT_MAX - 2) {
        fclose(file);
        runtime_error(interpreter, call->token, "file_read file is too large");
        return 0;
    }
    rewind(file);

    /* Read the whole file into one heap buffer. make_runtime_string owns it. */
    characters = malloc((size_t)file_size + 3);
    if (characters == NULL) {
        fclose(file);
        runtime_error(interpreter, call->token, "not enough memory to read file");
        return 0;
    }

    characters[0] = '"';
    bytes_read = fread(characters + 1, 1, (size_t)file_size, file);
    if (bytes_read != (size_t)file_size && ferror(file)) {
        free(characters);
        fclose(file);
        runtime_error(interpreter, call->token, "file_read could not read file");
        return 0;
    }
    if (fclose(file) != 0) {
        free(characters);
        runtime_error(interpreter, call->token, "file_read could not close file");
        return 0;
    }

    characters[bytes_read + 1] = '"';
    characters[bytes_read + 2] = '\0';
    return make_runtime_string(interpreter,
                               call->token,
                               characters,
                               (int)bytes_read,
                               value);
}

int call_file_write(Interpreter *interpreter,
                           Expr *call,
                           const char *mode,
                           const char *name,
                           Value *value) {
    Expr *path_expression;
    Expr *text_expression;
    Value path_value;
    Value text_value;
    char path[1024];
    char message[1200];
    char path_subject[64];
    char text_subject[64];
    FILE *file;
    int text_length;
    size_t bytes_written;

    if (!check_builtin_arity(interpreter, call, 2)) {
        return 0;
    }

    path_expression = interpreter->parser->call_arguments[call->argument_start];
    text_expression = interpreter->parser->call_arguments[call->argument_start + 1];
    if (!eval_expression(interpreter, path_expression, &path_value)) {
        return 0;
    }

    snprintf(path_subject, sizeof(path_subject), "%s path", name);
    if (!copy_file_path(interpreter,
                        call->token,
                        path_value,
                        path_subject,
                        path)) {
        return 0;
    }
    value_release(interpreter, path_value);
    if (!eval_expression(interpreter, text_expression, &text_value)) {
        return 0;
    }
    if (text_value.type != VALUE_STRING) {
        snprintf(text_subject, sizeof(text_subject), "%s text", name);
        runtime_type_error(interpreter,
                           call->token,
                           text_subject,
                           "a string",
                           text_value);
        return 0;
    }

    file = fopen(path, mode);
    if (file == NULL) {
        snprintf(message,
                 sizeof(message),
                 "%s could not open file '%s': %s",
                 name,
                 path,
                 strerror(errno));
        runtime_error(interpreter, call->token, message);
        return 0;
    }

    text_length = text_value.string.length - 2;
    if (text_length < 0) {
        text_length = 0;
    }
    bytes_written = fwrite(text_value.string.start + 1,
                           1,
                           (size_t)text_length,
                           file);
    value_release(interpreter, text_value);
    if (bytes_written != (size_t)text_length) {
        fclose(file);
        runtime_error(interpreter, call->token, "could not write complete file text");
        return 0;
    }
    if (fclose(file) != 0) {
        runtime_error(interpreter, call->token, "could not close output file");
        return 0;
    }

    if (value != NULL) {
        *value = make_bool_value(1);
    }
    return 1;
}

int call_arg_count(Interpreter *interpreter, Expr *call, Value *value) {
    if (!check_builtin_arity(interpreter, call, 0)) {
        return 0;
    }

    if (value != NULL) {
        *value = make_number_value((double)interpreter->script_argument_count);
    }
    return 1;
}

int call_arg(Interpreter *interpreter, Expr *call, Value *value) {
    Expr *index_expression;
    Value index_value;
    double index;
    const char *argument;
    size_t argument_length;
    char *characters;
    char message[160];

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }

    index_expression = interpreter->parser->call_arguments[call->argument_start];
    if (!eval_expression(interpreter, index_expression, &index_value)) {
        return 0;
    }
    if (index_value.type != VALUE_NUMBER) {
        runtime_type_error(interpreter,
                           call->token,
                           "arg index",
                           "a number",
                           index_value);
        return 0;
    }

    index = index_value.number;
    if (!isfinite(index) || floor(index) != index) {
        runtime_error(interpreter, call->token, "arg index must be a whole number");
        return 0;
    }
    if (index < 0 || index >= interpreter->script_argument_count) {
        snprintf(message,
                 sizeof(message),
                 "arg index %.0f is out of range for %d script argument%s",
                 index,
                 interpreter->script_argument_count,
                 interpreter->script_argument_count == 1 ? "" : "s");
        runtime_error(interpreter, call->token, message);
        return 0;
    }

    if (value == NULL) {
        return 1;
    }

    argument = interpreter->script_arguments[(int)index];
    argument_length = strlen(argument);
    if (argument_length > INT_MAX - 2) {
        runtime_error(interpreter, call->token, "script argument is too long");
        return 0;
    }

    characters = malloc(argument_length + 3);
    if (characters == NULL) {
        runtime_error(interpreter, call->token, "not enough memory for script argument");
        return 0;
    }
    characters[0] = '"';
    memcpy(characters + 1, argument, argument_length);
    characters[argument_length + 1] = '"';
    characters[argument_length + 2] = '\0';
    return make_runtime_string(interpreter,
                               call->token,
                               characters,
                               (int)argument_length,
                               value);
}

int call_text(Interpreter *interpreter, Expr *call, Value *value) {
    Expr *argument_expression;
    Value argument_value;
    char content[64];
    int content_length;
    char *characters;

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }

    argument_expression = interpreter->parser->call_arguments[call->argument_start];
    if (!eval_expression(interpreter, argument_expression, &argument_value)) {
        return 0;
    }

    if (value == NULL) {
        value_release(interpreter, argument_value);
        return 1;
    }
    if (argument_value.type == VALUE_STRING) {
        /* Hand the argument's reference straight to the caller. */
        *value = argument_value;
        return 1;
    }
    if (argument_value.type == VALUE_BOOL) {
        content_length = snprintf(content,
                                  sizeof(content),
                                  "%s",
                                  argument_value.boolean ? "true" : "false");
    } else if (argument_value.type == VALUE_LIST) {
        runtime_type_error(interpreter,
                           call->token,
                           "text argument",
                           "a number, string, or boolean",
                           argument_value);
        return 0;
    } else if (isfinite(argument_value.number) &&
               argument_value.number >= (double)LLONG_MIN &&
               argument_value.number < 9223372036854775808.0 &&
               floor(argument_value.number) == argument_value.number) {
        content_length = snprintf(content,
                                  sizeof(content),
                                  "%lld",
                                  (long long)argument_value.number);
    } else {
        content_length = snprintf(content,
                                  sizeof(content),
                                  "%g",
                                  argument_value.number);
    }

    if (content_length < 0 || content_length >= (int)sizeof(content)) {
        runtime_error(interpreter, call->token, "could not convert value to text");
        return 0;
    }

    characters = malloc((size_t)content_length + 3);
    if (characters == NULL) {
        runtime_error(interpreter, call->token, "not enough memory to convert value to text");
        return 0;
    }
    characters[0] = '"';
    memcpy(characters + 1, content, (size_t)content_length);
    characters[content_length + 1] = '"';
    characters[content_length + 2] = '\0';
    return make_runtime_string(interpreter,
                               call->token,
                               characters,
                               content_length,
                               value);
}

static int string_content_length(Value string_value) {
    int length = string_value.string.length - 2;

    return length < 0 ? 0 : length;
}

static int make_string_from_content(Interpreter *interpreter,
                                    Token source,
                                    const char *content,
                                    int content_length,
                                    Value *value) {
    char *characters;

    if (content_length < 0 || content_length > INT_MAX - 2) {
        runtime_error(interpreter, source, "string result is too long");
        return 0;
    }

    characters = malloc((size_t)content_length + 3);
    if (characters == NULL) {
        runtime_error(interpreter, source, "not enough memory for string result");
        return 0;
    }
    characters[0] = '"';
    memcpy(characters + 1, content, (size_t)content_length);
    characters[content_length + 1] = '"';
    characters[content_length + 2] = '\0';
    return make_runtime_string(interpreter,
                               source,
                               characters,
                               content_length,
                               value);
}

int call_len(Interpreter *interpreter, Expr *call, Value *value) {
    Value argument;

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }
    if (!eval_expression(interpreter,
                         interpreter->parser->call_arguments[call->argument_start],
                         &argument)) {
        return 0;
    }
    if (argument.type != VALUE_STRING) {
        runtime_type_error(interpreter,
                           call->token,
                           "len argument",
                           "a string",
                           argument);
        return 0;
    }

    if (value != NULL) {
        *value = make_number_value((double)string_content_length(argument));
    }
    value_release(interpreter, argument);
    return 1;
}

int call_length(Interpreter *interpreter, Expr *call, Value *value) {
    Value argument;

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }
    if (!eval_expression(interpreter,
                         interpreter->parser->call_arguments[call->argument_start],
                         &argument)) {
        return 0;
    }
    if (argument.type != VALUE_LIST) {
        runtime_type_error(interpreter,
                           call->token,
                           "length argument",
                           "a list",
                           argument);
        return 0;
    }

    if (value != NULL) {
        *value = make_number_value((double)argument.list->count);
    }
    value_release(interpreter, argument);
    return 1;
}

int call_append(Interpreter *interpreter, Expr *call, Value *value) {
    Expr *target;
    Expr *root;
    Variable *variable;
    Value list_value;
    Value appended_value;
    Value *grown_elements;
    int new_capacity;

    if (!check_builtin_arity(interpreter, call, 2)) {
        return 0;
    }
    target = interpreter->parser->call_arguments[call->argument_start];
    root = target;
    while (root->kind == EXPR_INDEX) {
        root = root->left;
    }
    if (root->kind != EXPR_IDENT) {
        runtime_error(interpreter,
                      target->token,
                      "append target must be a list binding or indexed list element");
        return 0;
    }
    variable = find_variable(interpreter, root->token);
    if (variable == NULL) {
        runtime_name_error(interpreter, root->token, "undefined variable");
        return 0;
    }
    if (!variable->is_mutable) {
        runtime_name_error(interpreter, root->token, "cannot mutate immutable val");
        return 0;
    }
    if (!eval_expression(interpreter, target, &list_value)) {
        return 0;
    }
    if (list_value.type != VALUE_LIST) {
        runtime_type_error(interpreter,
                           target->token,
                           "append target",
                           "a list",
                           list_value);
        return 0;
    }
    if (!eval_expression(interpreter,
                         interpreter->parser->call_arguments[call->argument_start + 1],
                         &appended_value)) {
        return 0;
    }

    if (list_value.list->count == list_value.list->capacity) {
        if (list_value.list->count == INT_MAX) {
            runtime_error(interpreter, call->token, "list is too large to append");
            return 0;
        }
        if (list_value.list->capacity < 8) {
            new_capacity = 8;
        } else if (list_value.list->capacity > INT_MAX / 2) {
            new_capacity = INT_MAX;
        } else {
            new_capacity = list_value.list->capacity * 2;
        }
        if ((size_t)new_capacity > (size_t)-1 / sizeof(Value)) {
            runtime_error(interpreter, call->token, "list is too large to append");
            return 0;
        }
        grown_elements = realloc(list_value.list->elements,
                                 (size_t)new_capacity * sizeof(Value));
        if (grown_elements == NULL) {
            runtime_error(interpreter, call->token, "not enough memory to append to list");
            return 0;
        }
        list_value.list->elements = grown_elements;
        list_value.list->capacity = new_capacity;
    }

    /* The appended value's reference moves into the list. */
    list_value.list->elements[list_value.list->count] = appended_value;
    list_value.list->count++;
    value_release(interpreter, list_value);
    if (value != NULL) {
        *value = make_bool_value(1);
    }
    return 1;
}

int call_contains(Interpreter *interpreter, Expr *call, Value *value) {
    Value text_value;
    Value search_value;
    int text_length;
    int search_length;
    int found = 0;
    int i;

    if (!check_builtin_arity(interpreter, call, 2)) {
        return 0;
    }
    if (!eval_expression(interpreter,
                         interpreter->parser->call_arguments[call->argument_start],
                         &text_value)) {
        return 0;
    }
    if (text_value.type != VALUE_STRING) {
        runtime_type_error(interpreter,
                           call->token,
                           "contains text",
                           "a string",
                           text_value);
        return 0;
    }
    if (!eval_expression(interpreter,
                         interpreter->parser->call_arguments[call->argument_start + 1],
                         &search_value)) {
        return 0;
    }
    if (search_value.type != VALUE_STRING) {
        runtime_type_error(interpreter,
                           call->token,
                           "contains search text",
                           "a string",
                           search_value);
        return 0;
    }

    text_length = string_content_length(text_value);
    search_length = string_content_length(search_value);
    if (search_length == 0) {
        found = 1;
    } else if (search_length <= text_length) {
        for (i = 0; i <= text_length - search_length; i++) {
            if (memcmp(text_value.string.start + 1 + i,
                       search_value.string.start + 1,
                       (size_t)search_length) == 0) {
                found = 1;
                break;
            }
        }
    }

    value_release(interpreter, text_value);
    value_release(interpreter, search_value);
    if (value != NULL) {
        *value = make_bool_value(found);
    }
    return 1;
}

int call_change_case(Interpreter *interpreter,
                            Expr *call,
                            const char *name,
                            int to_upper,
                            Value *value) {
    Value argument;
    int content_length;
    char *characters;
    char subject[64];
    int i;

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }
    if (!eval_expression(interpreter,
                         interpreter->parser->call_arguments[call->argument_start],
                         &argument)) {
        return 0;
    }
    if (argument.type != VALUE_STRING) {
        snprintf(subject, sizeof(subject), "%s argument", name);
        runtime_type_error(interpreter,
                           call->token,
                           subject,
                           "a string",
                           argument);
        return 0;
    }
    if (value == NULL) {
        value_release(interpreter, argument);
        return 1;
    }

    content_length = string_content_length(argument);
    if (content_length > INT_MAX - 2) {
        runtime_error(interpreter, call->token, "string result is too long");
        return 0;
    }
    characters = malloc((size_t)content_length + 3);
    if (characters == NULL) {
        runtime_error(interpreter, call->token, "not enough memory for string result");
        return 0;
    }
    characters[0] = '"';
    for (i = 0; i < content_length; i++) {
        unsigned char ch = (unsigned char)argument.string.start[i + 1];

        if (to_upper && ch >= 'a' && ch <= 'z') {
            ch = (unsigned char)(ch - 'a' + 'A');
        } else if (!to_upper && ch >= 'A' && ch <= 'Z') {
            ch = (unsigned char)(ch - 'A' + 'a');
        }
        characters[i + 1] = (char)ch;
    }
    characters[content_length + 1] = '"';
    characters[content_length + 2] = '\0';
    value_release(interpreter, argument);
    return make_runtime_string(interpreter,
                               call->token,
                               characters,
                               content_length,
                               value);
}

static int ascii_is_whitespace(unsigned char ch) {
    return ch == ' ' ||
           ch == '\t' ||
           ch == '\n' ||
           ch == '\r' ||
           ch == '\f' ||
           ch == '\v';
}

int call_trim(Interpreter *interpreter, Expr *call, Value *value) {
    Value argument;
    int content_length;
    int start = 0;
    int end;

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }
    if (!eval_expression(interpreter,
                         interpreter->parser->call_arguments[call->argument_start],
                         &argument)) {
        return 0;
    }
    if (argument.type != VALUE_STRING) {
        runtime_type_error(interpreter,
                           call->token,
                           "trim argument",
                           "a string",
                           argument);
        return 0;
    }
    if (value == NULL) {
        value_release(interpreter, argument);
        return 1;
    }

    content_length = string_content_length(argument);
    end = content_length;
    while (start < end &&
           ascii_is_whitespace((unsigned char)argument.string.start[start + 1])) {
        start++;
    }
    while (end > start &&
           ascii_is_whitespace((unsigned char)argument.string.start[end])) {
        end--;
    }

    if (start == 0 && end == content_length) {
        *value = argument;
        return 1;
    }
    {
        /* Copy the trimmed text before dropping the argument. */
        int succeeded = make_string_from_content(interpreter,
                                                 call->token,
                                                 argument.string.start + 1 + start,
                                                 end - start,
                                                 value);

        value_release(interpreter, argument);
        return succeeded;
    }
}

static int get_number_argument(Interpreter *interpreter,
                               Expr *call,
                               int index,
                               const char *subject,
                               double *number) {
    Value argument;

    if (!eval_expression(interpreter,
                         interpreter->parser->call_arguments[call->argument_start + index],
                         &argument)) {
        return 0;
    }
    if (argument.type != VALUE_NUMBER) {
        runtime_type_error(interpreter,
                           call->token,
                           subject,
                           "a number",
                           argument);
        return 0;
    }

    *number = argument.number;
    return 1;
}

int call_unary_math(Interpreter *interpreter,
                           Expr *call,
                           const char *name,
                           double (*operation)(double),
                           Value *value) {
    char subject[64];
    double argument;

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }
    snprintf(subject, sizeof(subject), "%s argument", name);
    if (!get_number_argument(interpreter, call, 0, subject, &argument)) {
        return 0;
    }

    if (value != NULL) {
        *value = make_number_value(operation(argument));
    }
    return 1;
}

int call_scientific_unary(Interpreter *interpreter,
                                 Expr *call,
                                 const char *name,
                                 double (*operation)(double),
                                 ScientificMathDomain domain,
                                 Value *value) {
    char subject[64];
    char message[128];
    double argument;
    double result;

    if (!check_builtin_arity(interpreter, call, 1)) {
        return 0;
    }
    snprintf(subject, sizeof(subject), "%s argument", name);
    if (!get_number_argument(interpreter, call, 0, subject, &argument)) {
        return 0;
    }
    if (!isfinite(argument)) {
        snprintf(message, sizeof(message), "%s argument must be finite", name);
        runtime_error(interpreter, call->token, message);
        return 0;
    }
    if (domain == SCIENTIFIC_DOMAIN_UNIT &&
        (argument < -1.0 || argument > 1.0)) {
        snprintf(message,
                 sizeof(message),
                 "%s argument must be between -1 and 1",
                 name);
        runtime_error(interpreter, call->token, message);
        return 0;
    }
    if (domain == SCIENTIFIC_DOMAIN_POSITIVE && argument <= 0.0) {
        snprintf(message,
                 sizeof(message),
                 "%s argument must be greater than zero",
                 name);
        runtime_error(interpreter, call->token, message);
        return 0;
    }

    errno = 0;
    result = operation(argument);
    if (errno == EDOM || isnan(result)) {
        snprintf(message,
                 sizeof(message),
                 "%s argument is outside the numeric domain",
                 name);
        runtime_error(interpreter, call->token, message);
        return 0;
    }
    if (errno == ERANGE || !isfinite(result)) {
        snprintf(message, sizeof(message), "%s result is out of range", name);
        runtime_error(interpreter, call->token, message);
        return 0;
    }

    if (value != NULL) {
        *value = make_number_value(result);
    }
    return 1;
}

int call_atan2(Interpreter *interpreter, Expr *call, Value *value) {
    double y;
    double x;
    double result;

    if (!check_builtin_arity(interpreter, call, 2)) {
        return 0;
    }
    if (!get_number_argument(interpreter, call, 0, "atan2 y", &y) ||
        !get_number_argument(interpreter, call, 1, "atan2 x", &x)) {
        return 0;
    }
    if (!isfinite(y) || !isfinite(x)) {
        runtime_error(interpreter, call->token, "atan2 arguments must be finite");
        return 0;
    }
    if (y == 0.0 && x == 0.0) {
        runtime_error(interpreter, call->token, "atan2 arguments cannot both be zero");
        return 0;
    }

    errno = 0;
    result = atan2(y, x);
    if (errno == EDOM || isnan(result)) {
        runtime_error(interpreter, call->token, "atan2 arguments are outside the numeric domain");
        return 0;
    }
    if (errno == ERANGE || !isfinite(result)) {
        runtime_error(interpreter, call->token, "atan2 result is out of range");
        return 0;
    }

    if (value != NULL) {
        *value = make_number_value(result);
    }
    return 1;
}

int call_min_max(Interpreter *interpreter,
                        Expr *call,
                        const char *name,
                        int choose_max,
                        Value *value) {
    char first_subject[64];
    char second_subject[64];
    double first;
    double second;
    double result;

    if (!check_builtin_arity(interpreter, call, 2)) {
        return 0;
    }
    snprintf(first_subject, sizeof(first_subject), "%s first argument", name);
    snprintf(second_subject, sizeof(second_subject), "%s second argument", name);
    if (!get_number_argument(interpreter, call, 0, first_subject, &first) ||
        !get_number_argument(interpreter, call, 1, second_subject, &second)) {
        return 0;
    }

    result = choose_max
        ? (first > second ? first : second)
        : (first < second ? first : second);
    if (value != NULL) {
        *value = make_number_value(result);
    }
    return 1;
}

int call_pow(Interpreter *interpreter, Expr *call, Value *value) {
    double base;
    double exponent;
    double result;

    if (!check_builtin_arity(interpreter, call, 2)) {
        return 0;
    }
    if (!get_number_argument(interpreter, call, 0, "pow base", &base) ||
        !get_number_argument(interpreter, call, 1, "pow exponent", &exponent)) {
        return 0;
    }

    errno = 0;
    result = pow(base, exponent);
    if (errno == EDOM || isnan(result)) {
        runtime_error(interpreter, call->token, "pow arguments are outside the numeric domain");
        return 0;
    }
    if (errno == ERANGE || !isfinite(result)) {
        runtime_error(interpreter, call->token, "pow result is out of range");
        return 0;
    }

    if (value != NULL) {
        *value = make_number_value(result);
    }
    return 1;
}
