# Testing Newt

Newt has two test layers:

- `test_newt.bat` rebuilds the interpreter and runs the established example smoke suite.
- `regression_tests.bat` runs focused behavioral tests that check exact standard output, exact standard error, and process exit codes for positive and negative programs.

The regression cases live in `tests/cases`, and the data-driven runner is `tests/run_tests.ps1`.

The regression harness treats standard output, standard error, and process status as separate parts of Newt's command-line contract. Successful program and development-mode output belongs on standard output. File-loading, lexer, parser, runtime, and command-line failures belong on standard error and must return a nonzero exit code.

## Windows

Requirements:

- GCC available as `gcc` on `PATH`;
- Windows PowerShell (included with supported Windows versions);
- `make` only if you want to use the Makefile.

From the repository root, build Newt:

```bat
make
```

If `make` is unavailable, use:

```bat
build.bat
```

Run the established examples:

```bat
test_newt.bat
```

Run the focused regression harness:

```bat
regression_tests.bat
```

The batch wrapper uses `-ExecutionPolicy Bypass` for only that PowerShell process, so it does not change the machine's execution-policy setting.

Before committing, run the whitespace check if Git is installed:

```bat
git diff --check
```

## Unix-like systems

The Makefile builds an executable named `newt` when `OS` is not `Windows_NT`:

```sh
make
```

Run the existing portable example smoke tests with:

```sh
make test
```

The focused regression runner is cross-platform PowerShell. If PowerShell 7 (`pwsh`) is installed, run:

```sh
pwsh -NoProfile -File ./tests/run_tests.ps1
```

The Unix regression command is supported through PowerShell 7; a separate POSIX-shell regression runner is not currently included. Finish with:

```sh
git diff --check
```

## What the regression harness covers

The focused suite currently checks:

- an existing positive example;
- string concatenation;
- each supported string escape: `\n`, `\t`, `\r`, `\\`, and `\"`;
- multiple escapes in one string;
- invalid and incomplete escape diagnostics with source columns;
- `text(value)` for numbers, booleans, and strings;
- string length, case conversion, trimming, substring search, and type failures;
- standard and scientific math helpers, constants, numeric type failures, and domain/range failures;
- lexical block scope, nested shadowing, outer mutation, and cleanup through `break`, `continue`, and `return`;
- `for` parse-tree output and first-to-last list iteration;
- empty-list iteration;
- `break` and `continue` in `for` loops;
- nested `for`/`while` execution;
- loop-variable shadowing and iteration-local lifetime;
- runtime errors for non-list `for` iterables;
- postfix and nested list indexing;
- `length` for empty and nonempty lists;
- direct and nested indexed assignment;
- `append` on an empty list and shared-list alias behavior;
- indexing and `for` iteration after list mutation;
- runtime errors for mutation through immutable `val` bindings;
- shared integer and range validation for indexed assignment;
- runtime errors for out-of-range, fractional, and non-number indexes;
- runtime errors for indexing a non-list or passing a non-list to `length`;
- `break` exiting the nearest loop, including nested loops;
- `break` in parse-tree output;
- runtime errors for `break` outside a loop or across a function boundary;
- `continue` skipping the rest of an iteration and affecting only the nearest loop;
- `continue` in parse-tree output;
- runtime errors for `continue` outside a loop or across a function boundary;
- columns after two-character lexer operators;
- lexer failure in `--tokens` mode;
- parser failure in `--parse` mode;
- a runtime type error;
- missing CLI input;
- source-file loading failure.

Each case has an expected exit code and expected stdout/stderr. Newlines are normalized from CRLF to LF before comparison so the same expectations work on Windows and Unix.

## Line endings

The repository's `.gitattributes` file makes line endings explicit. C source and headers, Markdown, Newt programs, PowerShell scripts, and the Makefile use LF. Windows batch files use CRLF so they retain their native command-processor format. Git stores text consistently and checks out each file using the ending selected for its file type, avoiding platform-dependent whole-file diffs.

After changing attributes or moving files between types, use `git diff --check` to catch whitespace problems. A future intentional repository-wide renormalization should be reviewed as a separate mechanical change rather than mixed into a language feature.

## Adding a regression case

Add a small `.nt` file under `tests/cases`, then add one entry to `$Tests` in `tests/run_tests.ps1`. Specify:

- a descriptive `Name`;
- the command-line `Arguments` passed to Newt;
- the expected `ExitCode`;
- exact `Stdout`;
- exact `Stderr`.

Keep cases deterministic and avoid depending on machine-specific absolute paths, locale-specific text, or existing user files.
