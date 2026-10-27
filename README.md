# Newt

Newt 0.001 is an experimental systems-language project implemented in C. It is a long-term learning project for OS development, CLI/TUI tools, and mathematics, mainly inspired by C, assembly, and Rust, with selected ideas from Zig and Go.

The current implementation is a tree-walking interpreter. It supports C-style typed declarations and functions, `let`, `mut`, `const`, semicolon-terminated simple statements, `{ }` blocks, and `;#` line comments. It executes top-level statements; no special entry function is implemented yet.

## Direction and current milestone

The goal is predictable native code, explicit memory control, and interoperability with C, Rust, and assembly. Current strings and lists are reference counted. Typed numbers have distinct host C `int`, `float`, and `double` payloads; compatibility-mode numbers remain double-based. Runtime values are still tagged interpreter objects, not packed native data layouts.

Changes are intentionally small and independently testable:

1. **Implemented:** brace blocks replace `end`, retaining lexical scope and current execution behavior.
2. **Implemented:** C-style typed declarations and function signatures, including `let`, `mut`, and `const`.
3. **Implemented in the interpreter:** distinct 32-bit `int`, binary32 `float`, and binary64 `double` behavior, with checked arithmetic and explicit conversions.
4. **Planned:** `start:` and defined entry-function and return behavior.
5. **Planned:** a minimal compiler that emits assembly for one initial target.
6. **Planned:** pointers, fixed-layout data, external calls, and target-specific assembly in `{{ }}` blocks with explicit inputs, outputs, and clobbers.

`start:`, pointers, and assembly blocks are **not supported yet**. Type enforcement currently happens during interpretation, not in a whole-program static checker. Existing untyped `let`/`mut` declarations and `fn` functions remain supported during the transition. Direct execution from an OS terminal is a long-term goal, not a current feature. Advanced collections, modules, networking, and other optional features remain outside this milestone.

A complete program using the syntax implemented today:

```newt
int consume(int fuel, const int amount) {
    fuel = fuel - amount;
    return fuel;
}

void main(void) {
    const int capacity = 25;
    mut int fuel = capacity;
    let float temperature = 21.5;
    double precision = 0.000001;

    fuel = consume(fuel, 1);
    print fuel;
    print temperature;
    print precision;
}

;# Entry-point selection is not implemented yet.
main();
```

## Typed declarations and functions

- `int fuel = 25;` and `mut int fuel = 25;` create mutable integer bindings.
- `let int fuel = 25;` and `const int fuel = 25;` create read-only bindings. Pointer-level `const` qualification is a future feature; no pointer types exist yet.
- Type names and `const` are reserved keywords; rename any old functions or variables using those names.
- Available declaration types are `int`, `float`, `double`, `bool`, `string`, and `list`. `void` is only a function return type or a sole no-parameter marker.
- Types precede names. Typed parameters are mutable local copies unless marked `const` or `let`. Lists remain shared reference-counted values, not deep copies.
- Variables require initializers. Multiple declarations in one statement and function prototypes are not implemented. `f()` and `f(void)` both mean no parameters in Newt.
- The current build requires the host C model with signed 32-bit `int`, binary32 `float`, and binary64 `double`; compile-time assertions reject other models. This is an interpreter target restriction, not a promise of a portable C ABI yet.
- An expected numeric type gives number literals their type: `float n = 21.5;` and `int half = 5 / 2;` work. Typed integer division truncates toward zero; overflow and division by zero are errors. Typed float operations round to binary32. Literal-only expressions next to a typed operand adopt its type.
- Already-typed values do not silently change numeric types. Use `int(value)`, `float(value)`, or `double(value)`. Conversion to `int` truncates toward zero and checks range; non-finite values and out-of-range float/int conversions are rejected. Finite floating-point underflow may round to zero.
- Arguments, assignments, and returned values must match declared types. A typed non-void function must return a matching value on the executed path. A `void` function accepts `return;` or falls through, and cannot be used as a value.
- Existing numeric built-ins accept numeric inputs and still return compatibility-mode numbers; convert their numeric results explicitly when storing them in typed bindings, for example `double root = double(sqrt(9));`. Existing dynamic code remains valid, but is not statically checked.

See `examples/typed_systems.nt` for a runnable example. Use `--tokens` and `--parse` to inspect type keywords, declaration types, return types, and parameter mutability.

Simple statements need `;`, including the final statement in a file. A closing `}` needs no semicolon. Opening braces may be on the header line or the following line. `;#` begins a line comment; braces and comment markers inside strings remain text. Existing `end`-based programs must be migrated. Double braces are reserved for future assembly design and are not executable assembly today.

## Build

Newt currently builds with GCC:

```sh
gcc -std=c11 -Wall -Wextra -pedantic -o newt.exe src/main.c src/lexer.c src/parser.c src/interpreter.c src/builtins.c -lm
```

Or use the small Makefile:

```sh
make
```

On Windows, GCC must be installed and available on `PATH`.

Check the version:

```sh
./newt.exe --version
```

Expected output:

```txt
newt 0.001
```

## Run a file

Pass a `.nt` source file directly to execute it:

```sh
./newt.exe examples/calculator.nt
```

The explicit `--run` form is still supported:

```sh
./newt.exe --run examples/calculator.nt
```

File-loading, lexer, parser, runtime, and command-line errors are written to standard error and exit with a nonzero status. Normal program output, token output, parse trees, help, and version information are written to standard output. Source diagnostics include the file, line, and column when that location is available.

Pass script arguments after the source file. Newt exposes them through `arg_count()` and zero-based `arg(index)`:

```sh
./newt.exe examples/args.nt hello world
```

Newt also has development modes for inspecting tokens and parse trees:

```sh
./newt.exe --tokens examples/calculator.nt
./newt.exe --parse examples/calculator.nt
```

## Run the examples

Run one example directly:

```sh
./newt.exe examples/args.nt hello world
./newt.exe examples/read_file.nt
./newt.exe examples/write_file.nt
./newt.exe examples/append_file.nt
./newt.exe examples/ghostlog.nt
./newt.exe examples/escape_sequences.nt
./newt.exe examples/text_helpers.nt
./newt.exe examples/math_helpers.nt
./newt.exe --run examples/args_test.nt hello newt
./newt.exe --run examples/ghostlog_args.nt GardenGame "alchemy station"
./newt.exe --run examples/file_read_test.nt
./newt.exe --run examples/file_write_test.nt
./newt.exe --run examples/file_append_test.nt
./newt.exe --run examples/sqrt_test.nt
./newt.exe --run examples/and_test.nt
./newt.exe --run examples/or_test.nt
./newt.exe --run examples/not_test.nt
./newt.exe --run examples/unary_minus_test.nt
./newt.exe --run examples/bool_test.nt
./newt.exe --run examples/while_test.nt
./newt.exe --run examples/continue.nt
./newt.exe --run examples/for_loop.nt
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

The script stops immediately if the build or any example fails. On platforms with Make, the portable utility-example check is:

```sh
make test
```

Run the focused stdout, stderr, and exit-code regression suite on Windows with:

```bat
regression_tests.bat
```

See [TESTING.md](TESTING.md) for Windows and Unix-like testing instructions.

## Supported language features

Newt currently supports:

- numbers, strings, booleans, and list literals
- zero-based list indexing with `items[index]`
- mutable-list indexed assignment with `items[index] = value`
- list growth with `append(list, value)`
- list size with `length(list)`
- immutable `let` variables
- mutable `mut` variables and assignment
- `print`
- numeric input with `input_number("prompt")`
- arithmetic with `+`, `-`, `*`, and `/`
- string concatenation with `+`
- string escapes `\n`, `\t`, `\r`, `\\`, and `\"`
- string helpers with `len`, `contains`, `upper`, `lower`, and `trim`
- comparisons with `==`, `!=`, `<`, `<=`, `>`, and `>=`
- boolean `&&`
- boolean `||`
- boolean `!`
- unary minus and negative numbers
- `if / else if / else` conditions with brace blocks
- `while` loops with brace blocks
- `for name in list` loops with brace blocks
- `break` to exit the nearest enclosing loop
- `continue` to begin the next iteration of the nearest enclosing loop
- user-defined functions with parameters and return values
- the built-in `sqrt(number)` function
- standard math helpers with `abs`, `floor`, `ceil`, `min`, `max`, and `pow`
- scientific math with `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `log`, `log10`, `exp`, and `round`
- immutable `pi` and `e` constants
- explicit conversion with `text(value)` for numbers, booleans, and strings
- local file I/O with `file_read`, `file_write`, and `file_append`
- script arguments with `arg_count()` and `arg(index)`
- line comments beginning with `;#`

## Language examples

See [docs/language.md](docs/language.md) for a short language guide.

Values and arithmetic:

```newt
let width = 8;
let height = 4;
print width * height;

mut score = 10;
score = score + 5;
print score;
```

Strings, booleans, and comparisons:

```newt
let player = "Ada";
let hp = 10;
let food = 5;
let message = "Player: " + player;

if hp > 0 && food > 0 {
    print message;
} else {
    print "needs help";
}
```

String literals decode `\n` as newline, `\t` as tab, `\r` as carriage return, `\\` as one backslash, and `\"` as one double quote:

```newt
print "Name:\tNewt\nStatus:\tready";
print "She said \"hello\".";
print "C:\\notes";
```

Any other backslash sequence is a lexer error. A backslash at the end of an unterminated string is reported as an incomplete escape sequence, with the diagnostic located at that backslash.

Boolean `||`, boolean `!`, and negative numbers:

```newt
let tired = false;
let hungry = true;

if hungry || ! tired {
    print "needs care";
}

let change = -(2 + 3);
print change;
```

Multiple conditional branches with `else if`:

```newt
let crop = 2;

if crop == 1 {
    print "carrot";
} else if crop == 2 {
    print "strawberry";
} else {
    print "unknown crop";
}
```

Loops:

```newt
mut count = 1;

while count <= 3 {
    print count;
    if count == 2 {
        break;
    }
    count = count + 1;
}
```

The condition is evaluated before every iteration. If it is initially false, the loop body does not run.

`break` exits only the nearest enclosing loop. In nested loops, an inner `break` leaves the inner loop and execution continues in the outer loop. A function call is a control-flow boundary: a `break` inside a function must be inside a loop executing in that same function and cannot exit a loop belonging to its caller. Using `break` without such a loop is a runtime error.

`continue` skips the rest of the current iteration of the nearest enclosing loop. A `while` reevaluates its condition, and a `for` advances to the next element. Like `break`, it cannot target a loop in a calling function, and using it outside a loop in the current function call is a runtime error.

Use `for ... in` to visit list elements from first to last. Declare a list with `mut` when its elements or length need to change:

```newt
mut names = ["newt", "ghostnote"];

names[1] = "moss";
append(names, "river");

print names[0];
print length(names);

for name in names {
    print name;
}
```

List indexes are zero-based integers. Indexed assignment replaces an existing element and uses the same list, integer, and range validation as indexed reads. `append(names, value)` adds one element at the end and returns `true`. Both mutations require an access path rooted in a `mut` binding; attempting them through a `let` binding is a runtime error. `length(names)` returns the number of elements without changing the list.

The iterable expression and its starting length are evaluated once before iteration starts. Mutations completed before the loop are visible; appending during a loop does not extend that loop. An empty list runs the body zero times. The loop variable is an immutable binding local to one iteration: it may shadow an outer variable, and it is discarded before the next iteration and after the loop. `break` and `continue` target the nearest enclosing `for` or `while`, including when the loop kinds are nested. A non-list iterable is a runtime error.

Numeric input and square roots:

```newt
let area = input_number("Area: ");
print sqrt(area);
```

Functions with parameters and return values:

```newt
fn add(a, b) {
    return a + b;
}

let result = add(2, 3);
print result;
```

Functions can also have no parameters, and a call may be used as a standalone statement when its return value is not needed. A function currently supports up to 16 parameters. Because Newt does not have a `none` value yet, a value-producing call to a function without an explicit `return` safely produces the number `0`.

Convert an existing value to text explicitly when building a string:

```newt
let score = 42;
print "Score: " + text(score);
```

Newt does not implicitly combine strings and numbers.

Clean and inspect command-line text with the string helpers:

```newt
let command = trim("  Build Project  ");
print upper(command);
print len(command);
print contains(lower(command), "build");
```

Basic file I/O:

```newt
let note = file_read("notes.txt");
print note;

file_write("notes.txt", "first line");
file_append("notes.txt", "\nsecond line");
```

`file_read` returns the complete file text. `file_write` overwrites a file and `file_append` adds to it; both return `true` after a successful write.

File I/O is early and intentionally simple. Paths are local paths interpreted by the operating system, parent directories are not created automatically, and `file_read` loads the complete file into memory. Newt reports open, allocation, read, write, type, and argument-count failures instead of continuing with an invalid value.

Command-line arguments:

```sh
./newt.exe examples/args.nt GardenGame "alchemy station"
```

```newt
print arg_count();
print arg(0);
print arg(1);
```

`arg_count()` returns the number of values after the script path. `arg(0)` returns the first value, and an invalid index produces a runtime error.

Generate a small Markdown development log:

```newt
file_write("examples/devlog.md", "# Devlog\n\n");
file_append("examples/devlog.md", "Today I worked on Newt.\n");
```

Additional list operations, imports/modules, HTTP, and JSON are not part of Newt yet.

## Main examples

- `args.nt` demonstrates direct command-line arguments.
- `read_file.nt` demonstrates reading a complete text file.
- `write_file.nt` demonstrates overwriting a text file.
- `append_file.nt` demonstrates deterministic append behavior.
- `ghostlog.nt` generates a small Markdown development log.
- `escape_sequences.nt` demonstrates newline and tab escapes.
- `text_helpers.nt` demonstrates trimming, case conversion, length, and substring search.
- `math_helpers.nt` demonstrates powers, absolute values, rounding direction, minimums, and maximums.
- `projectile_lab.nt` samples a projectile trajectory using trigonometry and a loop.
- `args_test.nt` demonstrates counting and reading script arguments.
- `ghostlog_args.nt` demonstrates passing a quoted argument containing a space.
- `argument_report.nt` writes command-line arguments to a report using a function and loop.
- `file_read_test.nt` demonstrates reading a complete text file.
- `file_write_test.nt` demonstrates creating or overwriting a text file.
- `file_append_test.nt` demonstrates appending text to a file.
- `sqrt_test.nt` demonstrates `sqrt` with a literal and a variable.
- `and_test.nt` combines boolean values and numeric comparisons.
- `or_test.nt` demonstrates `||` with boolean values and comparisons.
- `not_test.nt` demonstrates negating boolean values with `!`.
- `unary_minus_test.nt` demonstrates negative numbers and negated expressions.
- `bool_test.nt` demonstrates boolean values and conditional branches.
- `while.nt` demonstrates condition-controlled repetition and mutable loop state.
- `while_test.nt` demonstrates a loop and mutable assignment.
- `continue.nt` demonstrates skipping one loop iteration.
- `for_loop.nt` demonstrates indexed assignment, `append`, `length`, and first-to-last list iteration.
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
  .gitattributes
  docs/
    language.md
  examples/
  src/
    main.c          command line: --run, --tokens, --parse
    common.h        shared includes and limits
    lexer.c/.h      source text -> tokens
    parser.c/.h     tokens -> statement and expression trees
    interpreter.c/.h  runs the trees
    builtins.c/.h   built-in functions (files, text, lists, math)
  README.md
  TESTING.md
  regression_tests.bat
  test_newt.bat
  tests/
  build.bat
  SPEC.md
  Makefile
```

## Status

Newt is experimental, and still in an early stage. Newt is currently being worked on and will continue to have updates.

## License

No license has been chosen yet.

Copyright 2026 drgropp. All rights reserved.

