# Newt

Newt 0.001 is an experimental systems-language project implemented in C. Newt runs `.nt` programs through a tree-walking interpreter and can emit native assembly for a restricted systems subset. It supports C-style typed declarations and functions, lexical scope, and runtime type checks.

Newt supports `let`, `mut`, `const`, semicolon-terminated simple statements, `{ }` blocks, and `;#` line comments. Programs with `start:` initialize top-level declarations and automatically call the marked `main`. Programs without `start:` execute top-level statements in order. Untyped `let`/`mut` declarations and `fn` functions are also supported.

Strings and lists are reference counted. Typed numbers use distinct host C `int`, `float`, and `double` payloads; untyped numbers use double-based arithmetic. Runtime values are tagged interpreter objects.

A complete program using the syntax implemented today:

```newt
int consume(int fuel, const int amount) {
    fuel = fuel - amount;
    return fuel;
}

start:
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

```

## Program entry and exit

```newt
start:
void main(void) {
    print "hello world";
}
```

`start:` marks the following `void main(void)` or `int main(void)` as the entry function. Empty `()` also means no parameters. Only one entry marker is allowed, at top level. In a program with `start:`, top-level code consists of variable and function declarations; write executable statements inside functions. Do not add a top-level `main();` call.

Newt registers functions and initializes globals in source order, then automatically calls `main`. `main` can use globals and functions declared later in the file because the automatic call happens after this initialization pass. Ordinary initializer expressions keep their existing declaration-order lookup rules. Command-line arguments are available through `arg_count()` and `arg()`.

A `void main` exits with status zero after returning or reaching its closing brace. An `int main` must return an integer; Newt passes that integer to the host process as its exit status. Use 0 for success and a value from 1–255 for portable nonzero shell statuses. Values outside 0–255 follow the host's exit-status rules. Source loading, parsing, and runtime errors exit with status 1 and a diagnostic. `--tokens` and `--parse` never execute the entry function.

Files without `start:` retain top-level execution. A function named `main` alone is an ordinary function in those files and must be called explicitly. `start` is a reserved keyword.

See `examples/start.nt` for a hello-world program and `examples/start_exit.nt` for a program that exits with status 7.

## Typed declarations and functions

- `int fuel = 25;` and `mut int fuel = 25;` create mutable integer bindings.
- `let int fuel = 25;` and `const int fuel = 25;` create read-only bindings.
- Type names and `const` are reserved keywords; rename any old functions or variables using those names.
- Available declaration types are `int`, `float`, `double`, `bool`, `string`, and `list`. `void` is only a function return type or a sole no-parameter marker.
- Types precede names. Typed parameters are mutable local copies unless marked `const` or `let`. Lists remain shared reference-counted values, not deep copies.
- Variables require initializers. `f()` and `f(void)` both mean no parameters in Newt.
- The current build requires the host C model with signed 32-bit `int`, binary32 `float`, and binary64 `double`; compile-time assertions reject other models.
- An expected numeric type gives number literals their type: `float n = 21.5;` and `int half = 5 / 2;` work. Typed integer division truncates toward zero; overflow and division by zero are errors. Typed float operations round to binary32. Literal-only expressions next to a typed operand adopt its type.
- Already-typed values do not silently change numeric types. Use `int(value)`, `float(value)`, or `double(value)`. Conversion to `int` truncates toward zero and checks range; non-finite values and out-of-range float/int conversions are rejected. Finite floating-point underflow may round to zero.
- Arguments, assignments, and returned values must match declared types. A typed non-void function must return a matching value on the executed path. A `void` function accepts `return;` or falls through, and cannot be used as a value.
- Existing numeric built-ins accept numeric inputs and still return compatibility-mode numbers; convert their numeric results explicitly when storing them in typed bindings, for example `double root = double(sqrt(9));`. Untyped code is checked during execution.

See `examples/typed_systems.nt` for a runnable example. Use `--tokens` and `--parse` to inspect type keywords, declaration types, return types, and parameter mutability.

Simple statements need `;`, including the final statement in a file. A closing `}` needs no semicolon. Opening braces may be on the header line or the following line. `;#` begins a line comment; braces and comment markers inside strings remain text.

## Native compiler

```sh
./newt --emit-asm examples/native_integer.nt -o /tmp/native_integer.s --target x86_64-linux
gcc -no-pie /tmp/native_integer.s -o /tmp/native_integer
/tmp/native_integer
```

The compiler emits GNU assembly in Intel syntax for **x86-64 Linux / System V**. GCC's assembler and linker produce the executable; linking uses the host C library for printing and error reporting. Cross-architecture and bare-metal output are not supported. The compiler can emit this text on other hosts, but execution requires the selected target and toolchain.

The supported subset includes `start:`, typed `int`/`bool`/`void` functions, up to six scalar/pointer parameters, local bindings, assignment, checked integer arithmetic, comparisons, logical operators, `if`, `while`, `break`, `continue`, returns, and printing integers, booleans, or string literals. Native-only systems features include pointers, structs, external calls, and assembly blocks, described below. Globals must be moved into functions. Floats, doubles, lists, dynamic declarations, and other interpreter-only constructs produce compilation errors. Arithmetic needs an explicit integer context or typed operand: use `int half = 5 / 2;` rather than an untyped arithmetic expression. Comparisons containing arithmetic also require typed operands; untyped comparisons such as `5 / 2 == 2` are rejected rather than changing double-based interpreter semantics. Native loops and calls use the host stack and do not impose the interpreter's iteration/call-depth limits.

All compilation checks complete before an output file is opened. Runtime overflow, division by zero, and missing returns print native diagnostics and exit with status 1. `--emit-asm` never invokes the interpreter as a fallback.

## Native pointers, structs, and C calls

```newt
struct Counter {
    int value;
};

int bump(int* value) {
    *value = *value + 1;
    return *value;
}

start:
int main(void) {
    struct Counter counter = {40};
    print bump(&counter.value);
    print sizeof(struct Counter);
    return 0;
}
```

These operations require `--emit-asm`; they are not executed by the interpreter. Native pointers have one level and can point to `int`, `void`, or a struct. `&object` takes an address, `*pointer` dereferences it, and `pointer[index]` indexes memory. Pointer addition/subtraction by an integer scales by the pointed-to object size; pointer equality and null literal `0` are supported. `void*` can cross compatible pointer interfaces, but cannot be dereferenced or used for scaled arithmetic. Pointer accesses have no automatic bounds, lifetime, or null checks.

`let int* p = &value;` locks the pointer binding while allowing writes through it. `const int* p = &value;` allows pointer reassignment but forbids writes through that pointer. Adding a const qualification is allowed; dropping it is rejected. `const` is a restriction on the access path, not a guarantee that another mutable alias cannot change the object.

Structs are declared at top level. Their fields support `int`, pointers, and nested structs, with C-compatible layout for this target: int fields have size/alignment 4, pointers have size/alignment 8, and structs include alignment padding. Local aggregate initialization requires one value per field; padding is zeroed. Use `object.field`, `pointer->field`, and `sizeof(type)`. Passing or returning a struct by value, aggregate copying, multiple pointer levels, and global objects are outside the native subset.

External functions use an explicit declaration:

```newt
extern int add_to(int* value, int amount);

start:
int main(void) {
    int value = 40;
    print add_to(&value, 2);
    return 0;
}
```

```sh
./newt --emit-asm examples/native_interop.nt -o /tmp/native_interop.s
gcc -no-pie /tmp/native_interop.s examples/native_bridge.c -o /tmp/native_interop
/tmp/native_interop
```

External calls support up to six `int` or pointer arguments, with `int`, pointer, or `void` results. Declarations must match the separately linked C implementation and System V ABI. External booleans and structs by value are rejected. Interpreter built-ins are not automatically external native functions.

## Native assembly blocks

```newt
start:
int main(void) {
    int value = 41;
    int result = 0;
    asm(edi = value) {{
        mov eax, edi
        add eax, 1
    }} -> result;
    print result;
    return 0;
}
```

`{{ ... }}` contains actual GNU assembler instructions in Intel syntax for the selected x86-64 Linux target. A bare block such as `{{ nop }}` is valid. The optional `asm(register = expression, ...)` prefix supplies up to six inputs, evaluated left to right; `-> variable;` stores the final `eax` (int/bool) or `rax` (pointer) into an existing mutable binding. Bool outputs are normalized to zero or one. A 32-bit input register cannot receive a pointer, and aliases such as `rax`/`eax` cannot both be bound. Supported input registers are `rax`, `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`, `rbx`, and `r12`–`r15`, including their 32-bit aliases.

The backend preserves `rbx` and `r12`–`r15` around each assembly block and assumes caller-saved registers, flags, and memory may change. Raw code must preserve ABI state not managed by the wrapper: `rsp`/`rbp`, stack alignment, the direction flag's clear state, and floating-point control state. It must leave the assembler in its original text section and Intel/64-bit syntax and fall through to the closing delimiter. It must not return or jump out of its containing function. These are programmer obligations, not a sandbox. Integer overflow checks do not apply inside raw assembly.

Inside the block, the assembler owns comment and punctuation rules: `#` comments and quoted text are preserved. This is target-specific assembly, not a translation layer for ARM or other CPUs; unsupported targets are rejected. The GNU assembler reports invalid instructions when assembling the emitted file.

`examples/native_systems.nt` combines a struct, a pointer function, and an assembly output. Compile and link it with the same commands as the integer example.

## Build

Newt currently builds with GCC:

```sh
gcc -std=c11 -Wall -Wextra -pedantic -o newt.exe src/main.c src/lexer.c src/parser.c src/interpreter.c src/builtins.c src/numeric.c src/compiler.c -lm
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

On x86-64 Linux with Python 3 and GCC, run the complete interpreter/native/reference/stress suite with `make test-all`. Run `python3 tests/run_regressions.py` for the shared interpreter cases without PowerShell.

See [TESTING.md](TESTING.md) for Windows and Unix-like testing instructions.

## Supported language features

Newt currently supports:

- numbers, strings, booleans, and list literals
- zero-based list indexing with `items[index]`
- mutable-list indexed assignment with `items[index] = value`
- list growth with `append(list, value)`
- list size with `length(list)`
- `start:` with automatic entry into typed `main` and host exit-status propagation
- C-style typed declarations and function signatures
- distinct `int`, `float`, and `double` numeric behavior and explicit conversions
- immutable `let` and `const` variables
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

List indexes are zero-based integers. Indexed assignment replaces an existing element and uses the same list, integer, and range validation as indexed reads. `append(names, value)` adds one element at the end and returns `true`. Both mutations require an access path rooted in a mutable binding, such as `mut names` or `list names`; attempting them through a `let` or `const` binding is a runtime error. `length(names)` returns the number of elements without changing the list.

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

Functions can also have no parameters, and a call may be used as a standalone statement when its return value is not needed. A function supports up to 16 parameters. Untyped `fn` functions produce the number `0` when they finish without an explicit `return`. Typed non-void functions must return a matching value on the executed path; `void` functions do not produce a value.

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

## Main examples

- `start.nt` demonstrates automatic program entry.
- `start_exit.nt` demonstrates returning a process exit status.
- `typed_systems.nt` demonstrates typed declarations, C-style functions, qualifiers, and explicit numeric conversion.
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
    numeric.c/.h    typed numeric arithmetic and checked conversions
    compiler.c/.h   native assembly generation
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

Newt is experimental. Type checks happen during interpretation, and diagnostics include source locations when available.

## License

No license has been chosen yet.

Copyright 2026 drgropp. All rights reserved.

