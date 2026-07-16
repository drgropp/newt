# Newt

Newt is a small experimental scripting language implemented in C. It is designed to be readable, direct, and easy to learn, with simple statements and `end`-delimited blocks.

This repository is a small v1 checkpoint: the core language can run useful beginner-sized scripts, while larger features such as user-defined functions, arrays, imports, modules, file I/O, and networking are intentionally left for later.

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
./newt.exe --run examples/calculator.nt
```

On Windows, rebuild and run the main examples together with:

```bat
test_newt.bat
```

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
- `if / else / end` conditions
- `while / end` loops
- the built-in `sqrt(number)` function
- line comments beginning with `#`

## Language examples

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

## Main examples

- `sqrt_test.nt` demonstrates `sqrt` with a literal and a variable.
- `and_test.nt` combines boolean values and numeric comparisons.
- `or_test.nt` demonstrates `or` with boolean values and comparisons.
- `not_test.nt` demonstrates negating boolean values with `not`.
- `unary_minus_test.nt` demonstrates negative numbers and negated expressions.
- `bool_test.nt` demonstrates boolean values and conditional branches.
- `while_test.nt` demonstrates a loop and mutable assignment.
- `if_test.nt` demonstrates comparisons with `if` and `else`.
- `calculator.nt` demonstrates the four arithmetic operators.
- `quadratic_test.nt` uses arithmetic and `sqrt` to solve a quadratic equation.

## Project layout

```txt
newt/
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
