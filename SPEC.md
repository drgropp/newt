# Newt language specification (preview)

This document describes the language implemented by Newt 0.1.0. It is a snapshot of current behavior for the preview release, not a promise that future versions will remain source-compatible.

## Source files and execution

Newt source files conventionally use the `.nt` extension. Statements are line-oriented. Blank lines are allowed, and `#` starts a comment that continues to the end of the line.

Run a source file directly or with `--run`:

```sh
newt example.nt first-argument second-argument
newt --run example.nt first-argument second-argument
```

Development modes print tokens or a parse tree without running the program:

```sh
newt --tokens example.nt
newt --parse example.nt
```

Successful commands exit with status 0. File-loading, lexer, parser, runtime, and CLI failures are written to standard error and exit nonzero. Program output, token output, parse trees, help, and version information are written to standard output.

## Values

Newt currently has four value types:

- numbers, stored as C `double` values;
- strings, delimited by double quotes;
- booleans, written as `true` or `false`;
- lists, written as comma-separated expressions inside `[` and `]`.

List literals may be empty. Their element expressions are evaluated from left to right when the literal is evaluated. Lists are shared values: copying a list value to another binding preserves the same list storage, so a mutation made through an allowed `mut` access path is visible through aliases.

Newt does not currently have a `none` or null value. A function call used as a value returns the number `0` if the function completes without an explicit `return`.

Strings support exactly these escape sequences:

- `\n` produces a newline;
- `\t` produces a horizontal tab;
- `\r` produces a carriage return;
- `\\` produces one backslash;
- `\"` produces one double quote.

Escape pairs remain in their original spelling in lexer tokens and parse-tree output. They are decoded once when the string literal is evaluated. An unsupported escape is a lexer error at the backslash. A backslash followed by the end of the file or a source line ending is an incomplete escape sequence and is also reported at the backslash. Raw source line endings are not allowed inside a string.

## Variables

`val` declares an immutable variable. `mut` declares a mutable variable.

```newt
val name = "Ada"
mut score = 10
score = score + 5
```

Reassigning a `val`, mutating a list through a `val` access path, assigning an undefined variable, or defining the same variable twice in one scope is a runtime error. Function parameters behave as immutable local variables, including when their values are lists. Function-local variables are discarded when the call returns. Each executed `if`, `else if`, or `else` branch creates a child scope. Each `while` iteration also creates a child scope. Variables declared in those blocks are discarded on exit, including exit through `break`, `continue`, or `return`; assignments to an outer `mut` still update the outer binding. Each `for` iteration keeps its existing child scope for its immutable loop variable and declarations in that iteration. A block-local variable may shadow an outer variable without replacing it.

## Output

`print` evaluates one expression and writes its value to standard output.

```newt
print "hello"
print 2 + 3
print true
```

`print` normally adds a newline. When a string already ends in a newline, it does not add another one.

## Expressions and operators

Parentheses may group expressions. From highest to lowest precedence, operators are:

1. postfix list indexing with `[index]`;
2. unary `not` and unary `-`;
3. `*` and `/`;
4. `+` and `-`;
5. `<`, `<=`, `>`, and `>=`;
6. `==` and `!=`;
7. `and`;
8. `or`.

Arithmetic and ordered comparisons require numbers. Division by zero is a runtime error. Unary minus requires a number. `and`, `or`, and `not` require booleans; `and` and `or` short-circuit.

`+` adds two numbers or concatenates two strings:

```newt
print 20 + 22
print "hello " + "world"
```

Newt does not implicitly convert mixed operands. For example, `"count: " + 2` is a runtime error; use `"count: " + text(2)` instead.

Equality supports numbers, strings, and booleans. Values of different types compare unequal. List comparison is not supported. Ordered comparison is numeric only.

Conditions accept all current values. `false`, zero, an empty string, and an empty list are falsey; `true`, nonzero numbers, nonempty strings, and nonempty lists are truthy.

List indexing is zero-based. `items[0]` returns the first element, and postfix indexes may be chained for nested lists. The target must be a list, and the index must be a finite whole number within the list's bounds. Indexing a non-list, using a non-integer index, or using an index outside `0` through `length(list) - 1` is a runtime error.

Indexed assignment uses the same target, integer, and bounds validation as indexed reads. It replaces one existing element and does not change the list's length. The assignment target may be nested, as in `rows[1][0] = value`, but its access path must begin with a `mut` binding.

`length(list)` returns the number of elements as a number. Its argument must be a list. Reading an element or its length does not mutate the list.

## Conditional execution

An `if` block may contain any number of `else if` branches and an optional final `else`. The first truthy branch runs. `end` closes the complete conditional.

```newt
if temperature < 0
    print "freezing"
else if temperature < 20
    print "cool"
else
    print "warm"
end
```

## Loops

`while` repeats its body while its condition is truthy. `end` closes the loop.

```newt
mut count = 0
while count < 3
    print count
    count = count + 1
end
```

`for` evaluates its iterable expression once, requires the result to be a list, captures its starting length, and executes its body once for each of those elements from first to last. Mutations completed before the loop are visible. Appending during the loop does not extend the current loop. An empty list executes the body zero times. A non-list result is a runtime error.

```newt
val names = ["newt", "ghostnote", "stray signal"]
for name in names
    print name
end
```

The loop variable is an immutable, iteration-local binding containing the current element. It may shadow a variable in an outer scope. The binding and any other variables declared during that iteration are discarded before the next iteration and are not visible after the loop.

`break` immediately exits the nearest currently executing `for` or `while`. Statements after the `break` in that loop body are skipped. With nested loops, only the innermost loop exits; execution then continues after that inner loop.

`continue` skips the remaining statements in the current iteration of the nearest currently executing loop. A `while` reevaluates its condition; a `for` advances to its next element. With nested loops, only the innermost loop is affected.

A function call is a control-flow boundary. A `break` or `continue` executed by a function may affect a loop running inside that same function call, but it cannot affect a loop in the caller. Using either statement outside a loop in the current function call is a runtime error.

A single loop execution is limited to 100,000 iterations and reports a runtime error if it reaches the limit.

## Functions and return

Declare a function with `fn`, a name, a parenthesized parameter list, a body, and `end`.

```newt
fn add(a, b)
    return a + b
end

print add(2, 3)
```

Functions may have no parameters. Arguments are evaluated before entering the function. Calls require exactly the declared number of arguments. `return` evaluates one expression, exits the current function, and supplies that value to the caller. Using `return` outside a function is a runtime error.

Function declarations take effect when execution reaches them, so a function must be declared before a call that executes. Recursive calls are supported up to the call-depth limit.

## Built-in functions

### Numeric input and math

- `input_number(prompt)` writes a string-literal prompt, reads one number from standard input, and returns it.
- `sqrt(number)` returns the square root of a non-negative number.
- `abs(number)` returns the non-negative magnitude of a number.
- `floor(number)` returns the greatest integer not greater than the number.
- `ceil(number)` returns the least integer not less than the number.
- `min(first, second)` returns the smaller number.
- `max(first, second)` returns the larger number.
- `pow(base, exponent)` raises the base to the exponent.
- `sin(number)`, `cos(number)`, and `tan(number)` use radians.
- `asin(number)`, `acos(number)`, and `atan(number)` return radians.
- `atan2(y, x)` returns the quadrant-aware angle in radians.
- `log(number)` returns the natural logarithm.
- `log10(number)` returns the base-10 logarithm.
- `exp(number)` returns e raised to the number.
- `round(number)` rounds halfway values away from zero.

The predefined immutable values `pi` and `e` are available as ordinary numeric identifiers. They may be shadowed in a child scope but cannot be assigned.

Invalid numeric input, a non-number passed to a math helper, or a negative square-root argument is a runtime error. Inverse sine and cosine require values from -1 through 1, logarithms require positive values, and `atan2(0, 0)` is rejected as undefined. New scientific helpers and `pow` report domain errors and results outside the finite numeric range instead of returning a NaN or infinity.

### Text conversion

- `text(value)` returns a string representation of a number or boolean and returns strings unchanged.

Because Newt has no `none` value, there is currently no `text(none)` behavior. `text` is explicit conversion only; operators do not perform implicit conversion.

### String helpers

- `len(text)` returns the number of bytes in a string.
- `contains(text, search)` returns whether `search` occurs within `text`.
- `upper(text)` converts ASCII letters to uppercase.
- `lower(text)` converts ASCII letters to lowercase.
- `trim(text)` removes ASCII spaces, tabs, newlines, carriage returns, form feeds, and vertical tabs from both ends.

String helpers require string arguments. `contains` is case-sensitive and an empty search string is contained in every string. Newt strings are byte strings in this preview, so `len` counts UTF-8 bytes rather than Unicode characters. `upper` and `lower` leave non-ASCII bytes unchanged.

### Lists

- `length(list)` returns the number of elements.
- `append(list, value)` adds one value at the end and returns `true`.

`append` requires its target to be a list binding, or an indexed list element, whose access path begins with a `mut` binding. Mutation through a `val` binding is a runtime error.

### Command-line arguments

- `arg_count()` returns the number of script arguments after the source-file path.
- `arg(index)` returns the zero-based argument as a string.

The index must be a whole number in range. Newt does not parse argument strings into other value types automatically.

### File I/O

- `file_read(path)` reads a complete file and returns its contents as a string.
- `file_write(path, text)` creates or overwrites a file and returns `true` on success.
- `file_append(path, text)` appends to a file and returns `true` on success.

Paths and written content must be strings. Relative paths are resolved by the operating system from Newt's working directory. Parent directories are not created. `file_read` loads the entire file into memory. Open, seek, allocation, read, write, and close failures are runtime errors.

## Current implementation limits

The preview interpreter uses fixed-size internal storage. Current hard limits include:

- 16 parameters or arguments for a user-defined function;
- 256 nested function calls;
- 100,000 iterations per execution of one `while` statement;
- 256 variables and 256 functions in the active interpreter state;
- 256 parsed statements and 1,024 parsed expressions per source file;
- 1,024 parsed call arguments in total;
- 1,024 parsed list elements in total;
- 1,024 runtime-created strings in one program run;
- 1,024 runtime-created lists in one program run;
- file paths shorter than 1,024 bytes for file built-ins.

Programs that exceed a checked limit report an error. These limits are implementation constraints, not language goals.

## Known limitations and non-features

Newt is a small tree-walk interpreter intended for beginner-sized local scripts. The preview release does not include:

- a `none` value;
- list operations other than indexed assignment and `append`, and collection types other than lists;
- modules, imports, packages, or namespaces;
- classes, objects, or methods;
- a bytecode engine, virtual machine, compiler backend, or native-code compiler;
- general OS or process APIs;
- networking, HTTP, or JSON support;
- automatic numeric/string conversion;
- user-defined error handling;
- a stable package or embedding API.

File I/O is intentionally basic and is not sandboxed. Source capacity limits are global to the parsed file, and diagnostics stop after the first lexer or parser error. Newt is experimental; this specification records the preview behavior so development can safely pause and resume later.
