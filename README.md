# Newt

Newt is a small experimental scripting language implemented in C. It is designed to be readable, direct, and easy to learn, with simple statements and `end`-delimited blocks.

This repository is a small v1 checkpoint: the core language can run useful beginner-sized scripts, while larger features such as arrays, imports, modules, file I/O, and networking are intentionally left for later.

## Build

Newt currently builds with GCC:

```sh
gcc -std=c11 -Wall -Wextra -pedantic -o newt.exe src/main.c
```

On Windows, GCC must be installed and available on `PATH`.

Check the version:

```sh
./newt.exe --version
```

Expected output:

```txt
newt 0.1.0
```

## Run a file

Use `--run` to execute a `.nt` source file:

```sh
./newt.exe --run examples/calculator.nt
```

Runtime errors include the source file, line, and column. A failed run exits with a nonzero status.

Newt also has development modes for inspecting tokens and parse trees:

```sh
./newt.exe --tokens examples/calculator.nt
./newt.exe --parse examples/calculator.nt
```

## Run the examples

Run one example directly:

```sh
./newt.exe --run examples/sqrt_test.nt
./newt.exe --run examples/and_test.nt
./newt.exe --run examples/or_test.nt
./newt.exe --run examples/not_test.nt
./newt.exe --run examples/unary_minus_test.nt
./newt.exe --run examples/bool_test.nt
./newt.exe --run examples/while_test.nt
./newt.exe --run examples/if_test.nt
./newt.exe --run examples/else_if_test.nt
./newt.exe --run examples/function_test.nt
./newt.exe --run examples/function_params_test.nt
./newt.exe --run examples/return_test.nt
./newt.exe --run examples/calculator.nt
```

On Windows, rebuild and run the main examples together with:

```bat
test_newt.bat
```

The script stops immediately if the build or any example fails.

## Supported language features

Newt currently supports:

- numbers, strings, and booleans
- immutable `val` variables
- mutable `mut` variables and assignment
- `print`
- numeric input with `input_number("prompt")`
- arithmetic with `+`, `-`, `*`, and `/`
- comparisons with `==`, `!=`, `<`, `<=`, `>`, and `>=`
- boolean `and`
- boolean `or`
- boolean `not`
- unary minus and negative numbers
- `if / else if / else / end` conditions
- `while / end` loops
- user-defined functions with parameters and return values
- the built-in `sqrt(number)` function
- line comments beginning with `#`

## Language examples

See [docs/language.md](docs/language.md) for a short language guide.

Values and arithmetic:

```newt
val width = 8
val height = 4
print width * height

mut score = 10
score = score + 5
print score
```

Strings, booleans, and comparisons:

```newt
val player = "Ada"
val hp = 10
val food = 5

if hp > 0 and food > 0
    print player
else
    print "needs help"
end
```

Boolean `or`, boolean `not`, and negative numbers:

```newt
val tired = false
val hungry = true

if hungry or not tired
    print "needs care"
end

val change = -(2 + 3)
print change
```

Multiple conditional branches with `else if`:

```newt
val crop = 2

if crop == 1
    print "carrot"
else if crop == 2
    print "strawberry"
else
    print "unknown crop"
end
```

Loops:

```newt
mut count = 1

while count <= 3
    print count
    count = count + 1
end
```

Numeric input and square roots:

```newt
val area = input_number("Area: ")
print sqrt(area)
```

Functions with parameters and return values:

```newt
fn add(a, b)
    return a + b
end

val result = add(2, 3)
print result
```

Functions can also have no parameters, and a call may be used as a standalone statement when its return value is not needed. A function currently supports up to 16 parameters. Because Newt does not have a `none` value yet, a value-producing call to a function without an explicit `return` safely produces the number `0`.

Arrays, imports/modules, and file I/O are not part of Newt yet.

## Main examples

- `sqrt_test.nt` demonstrates `sqrt` with a literal and a variable.
- `and_test.nt` combines boolean values and numeric comparisons.
- `or_test.nt` demonstrates `or` with boolean values and comparisons.
- `not_test.nt` demonstrates negating boolean values with `not`.
- `unary_minus_test.nt` demonstrates negative numbers and negated expressions.
- `bool_test.nt` demonstrates boolean values and conditional branches.
- `while_test.nt` demonstrates a loop and mutable assignment.
- `if_test.nt` demonstrates comparisons with `if` and `else`.
- `else_if_test.nt` demonstrates ordered `else if` branches and an `else` fallback.
- `function_test.nt` demonstrates declaring and calling basic functions.
- `function_params_test.nt` demonstrates parameters, arguments, and returned values.
- `return_test.nt` demonstrates string and boolean return values.
- `wrong_argument_count_test.nt` demonstrates the runtime error for an incorrect argument count.
- `calculator.nt` demonstrates the four arithmetic operators.
- `quadratic_test.nt` uses arithmetic and `sqrt` to solve a quadratic equation.

## Project layout

```txt
newt/
  docs/
    language.md
  examples/
  src/
    main.c
  README.md
  test_newt.bat
  build.bat
  SPEC.md
  Makefile
```

## Status

Newt is experimental, and still in an early stage. Newt is currently being worked on and will continue to have updates.

## License

No license has been chosen yet.

Copyright 2026 drgropp. All rights reserved.
