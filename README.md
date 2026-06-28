# Newt

**Newt** is a small experimental scripting language built in C.

The goal is to create a lightweight, readable language for:

* tiny tools
* CLI/TUI apps
* math scripts
* game tooling
* future interop with C, Python, Julia, TypeScript etc.

Newt is currently very early. Right now it is focused on the language frontend: reading source files, tokenizing them, and parsing simple program structure.

Newt is designed to feel simple, direct, and low-clutter.

It takes inspiration from:

* Python’s readability
* Ruby-style `end` blocks
* Go-like simplicity
* Lua-style embeddability
* C-friendly implementation

Newt avoids Python-style indentation rules and C-style brace noise. Blocks are closed with `end`.

```newt
val name = "newt"
mut hp = 100

if hp >= 50
    print name
else
    print "low hp"
end
```

## Current Status

Newt currently supports:

* reading `.nt` source files
* printing source files
* lexer/token printer
* line and column tracking
* lexer error reporting
* parser skeleton
* basic parser error reporting
* simple expression precedence
* `val` declarations
* `mut` declarations
* assignment statements
* `if / else / end` parsing
* basic numeric execution with `--run`
* variable storage and lookup
* numeric `print` output

Current example:

```newt
val x = 2 + 3 * 4
print x

mut hp = 100
hp = hp - 10
print hp
```

Run mode:

```sh
./newt.exe --run examples/run_test.nt
```

Output:

```txt
14
90
```

Calculator example:

```newt
val a = 20
val b = 5

print a + b
print a - b
print a * b
print a / b
```

Run it:

```sh
./newt.exe --run examples/calculator.nt
```

Expected output:

```txt
25
15
100
4
```

Token mode:

```sh
./newt.exe --tokens examples/calc.nt
```

Parse mode:

```sh
./newt.exe --parse examples/calc.nt
```


## Planned Syntax

Newt uses `val` for immutable values and `mut` for mutable variables.

```newt
val pi = 3.14159
mut score = 0

score = score + 10
```

Functions will use `fn` and `end`.

```newt
fn add(a, b)
    return a + b
end

val result = add(2, 3)
print result
```

Modules will eventually use `use`.

```newt
use math
use fs
use tui
use math as m
```

Possible future Newt code:

```newt
use math as m
use tui

tui.title("Newt Calculator")

val a = tui.ask_number("a")
val b = tui.ask_number("b")

print "sum:", a + b
print "hypotenuse:", m.sqrt(a * a + b * b)
```

## Build

Newt is currently built with GCC.

```sh
gcc -std=c11 -Wall -Wextra -pedantic -o newt.exe src/main.c
```

Run a file:

```sh
./newt.exe examples/calc.nt
```

Print tokens:

```sh
./newt.exe --tokens examples/calc.nt
```

Print parse tree:

```sh
./newt.exe --parse examples/calc.nt
```

## Project Layout

```txt
newt/
  examples/
    calc.nt
    lexer_test.nt
    bad_lexer.nt
    bad_unknown.nt
    bad_parse.nt
  src/
    main.c
  README.md
  SPEC.md
  build.bat
  Makefile
```

## Roadmap


### Phase 1 — Parser

Parse tokens into program structure and report syntax errors.

### Phase 2 — AST

Store parsed programs in real AST data structures.

### Phase 3 — Interpreter

Execute simple Newt programs.

### Phase 4 — Variables, Scope, and Blocks

Support assignment, `if`, `else`, `while`, and block scoping.

### Phase 5 — Functions

Support function declarations, calls, parameters, and return values.

### Phase 6 — Standard Library Seeds

Add early built-ins for math, filesystem tools, terminal UI helpers, and future interop experiments.

## Philosophy

Newt should be small, readable, and practical.

The language should be easy to write, easy to embed, and easy to understand.

Newt is not ready for real-world use yet. It is currently an experimental language frontend under active development.

## License

No license has been chosen yet.

Copyright © 2026 drgropp. All rights reserved.
