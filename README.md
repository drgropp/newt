# Newt

Newt is a small experimental scripting language built in C. It aims to be readable, direct, and easy to understand, with Ruby-style `end` blocks and no indentation rules.

This repository is an early v1 checkpoint. Newt currently supports:

- numbers, strings, and booleans
- immutable `val` and mutable `mut` variables
- assignment to `mut` variables
- `print`
- `input_number("prompt")`
- arithmetic with `+`, `-`, `*`, and `/`
- comparisons with `==`, `!=`, `<`, `<=`, `>`, and `>=`
- boolean `and`
- `if / else / end`
- `while / end`
- the built-in `sqrt(number)` function
- comments beginning with `#`

User-defined functions are not supported yet.

## Example

```newt
# A tiny status check
val name = "newt"
mut hp = 100
val food = 5

hp = hp - 10

if hp > 0 and food > 0
    print name
else
    print "needs help"
end

print sqrt(49)
```

`val` variables cannot be reassigned. Use `mut` when a value needs to change:

```newt
mut count = 1

while count <= 3
    print count
    count = count + 1
end
```

Read numeric input with a string prompt:

```newt
val side = input_number("Side length: ")
print sqrt(side * side)
```

## Build and run

Build Newt with GCC:

```sh
gcc -std=c11 -Wall -Wextra -pedantic -o newt.exe src/main.c
```

Run a program:

```sh
./newt.exe --run examples/sqrt_test.nt
```

Other modes:

```sh
./newt.exe --tokens examples/calc.nt
./newt.exe --parse examples/calc.nt
```

## Examples

- `examples/sqrt_test.nt` demonstrates `sqrt` with literals and variables.
- `examples/and_test.nt` demonstrates `and` with booleans and comparisons.
- `examples/quadratic_test.nt` uses `sqrt` in the quadratic formula.
- `examples/bool_test.nt`, `while_test.nt`, and `if_test.nt` cover existing language behavior.

## Project layout

```txt
newt/
  examples/
  src/
    main.c
  README.md
  SPEC.md
  build.bat
  Makefile
```

## Roadmap

Future work may include user-defined functions, scope improvements, modules, standard-library growth, and interop experiments. These are deliberately outside this small checkpoint.

## License

No license has been chosen yet.

Copyright 2026 drgropp. All rights reserved.
