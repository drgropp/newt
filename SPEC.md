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

Newt currently has three value types:

- numbers, stored as C `double` values;
- strings, delimited by double quotes;
- booleans, written as `true` or `false`.

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

Reassigning a `val`, assigning an undefined variable, or defining the same variable twice in one scope is a runtime error. Function parameters behave as immutable local variables. Function-local variables are discarded when the call returns. `if` and `while` bodies do not introduce separate variable scopes in this preview.

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

1. unary `not` and unary `-`;
2. `*` and `/`;
3. `+` and `-`;
4. `<`, `<=`, `>`, and `>=`;
5. `==` and `!=`;
6. `and`;
7. `or`.

Arithmetic and ordered comparisons require numbers. Division by zero is a runtime error. Unary minus requires a number. `and`, `or`, and `not` require booleans; `and` and `or` short-circuit.

`+` adds two numbers or concatenates two strings:

```newt
print 20 + 22
print "hello " + "world"
```

Newt does not implicitly convert mixed operands. For example, `"count: " + 2` is a runtime error; use `"count: " + text(2)` instead.

Equality supports all current value types. Values of different types compare unequal. Ordered comparison is numeric only.

Conditions accept all current values. `false`, zero, and an empty string are falsey; `true`, nonzero numbers, and nonempty strings are truthy.

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

`break` immediately exits the nearest currently executing `while`. Statements after the `break` in that loop body are skipped. With nested loops, only the innermost loop exits; execution then continues after that inner loop.

A function call is a control-flow boundary. A `break` executed by a function may exit a loop running inside that same function call, but it cannot exit a loop in the caller. Using `break` outside a loop in the current function call is a runtime error. `continue` is not supported.

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

Invalid numeric input, a non-number passed to `sqrt`, or a negative square-root argument is a runtime error.

### Text conversion

- `text(value)` returns a string representation of a number or boolean and returns strings unchanged.

Because Newt has no `none` value, there is currently no `text(none)` behavior. `text` is explicit conversion only; operators do not perform implicit conversion.

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
- 1,024 runtime-created strings in one program run;
- file paths shorter than 1,024 bytes for file built-ins.

Programs that exceed a checked limit report an error. These limits are implementation constraints, not language goals.

## Known limitations and non-features

Newt is a small tree-walk interpreter intended for beginner-sized local scripts. The preview release does not include:

- a `none` value;
- arrays or other collection types;
- modules, imports, packages, or namespaces;
- classes, objects, or methods;
- a bytecode engine, virtual machine, compiler backend, or native-code compiler;
- general OS or process APIs;
- networking, HTTP, or JSON support;
- automatic numeric/string conversion;
- user-defined error handling;
- a stable package or embedding API.

File I/O is intentionally basic and is not sandboxed. Source capacity limits are global to the parsed file, and diagnostics stop after the first lexer or parser error. Newt is experimental; this specification records the preview behavior so development can safely pause and resume later.
