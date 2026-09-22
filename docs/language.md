# Newt language guide

Newt is a small scripting language with line-oriented statements and blocks closed by `end`.

## Variables

Use `val` for a value that cannot be reassigned. Use `mut` when the value needs to change.

```newt
val name = "Ada"
mut score = 10
score = score + 5
```

Variables declared in an `if`/`else` branch or a `while` body are local to that block. A block may shadow an outer name, while assignment still finds and updates an outer `mut` when no local binding has that name.

## Print and input

`print` writes a value followed by a newline. `input_number` displays a prompt and reads a number.

```newt
print "Welcome"
val age = input_number("Age: ")
print age
```

## Conditions

Use `if`, optional `else if` and `else` branches, and `end` to choose which statements run. Newt runs only the first matching branch.

```newt
if age >= 18
    print "adult"
else if age >= 13
    print "teen"
else
    print "child"
end
```

## Loops

Use `while` and `end` to repeat statements while a condition is true.

```newt
mut count = 1

while count <= 3
    print count
    if count == 2
        break
    end
    count = count + 1
end
```

Use `for ... in` to visit every element of a list from first to last. Use a `mut` binding for a list that needs to change:

```newt
mut names = ["newt", "ghostnote", "stray signal"]

names[1] = "moss"
append(names, "river")

print names[0]
print length(names)

for name in names
    print name
end
```

Indexes start at zero, so `names[0]` is the first element. Assigning to an existing index replaces that element, and `append(names, value)` adds one element at the end. Both operations require a `mut` binding; mutation through `val` is a runtime error. `length(names)` returns the number of elements. Nested indexing and indexed assignment such as `rows[1][0]` are also supported. Indexes must be whole numbers within the list's bounds, and only lists can be indexed or passed to `length`.

The expression after `in` is evaluated once and must produce a list. Its length is captured when the loop starts, so mutations made before the loop are visible while appending during the loop does not extend that loop. An empty list skips the body. The loop variable contains the current element and is immutable. It exists only for that iteration, may shadow an outer variable, and is unavailable after the loop.

`break` exits the nearest enclosing `for` or `while`. In nested loops, it exits only the innermost loop. A function cannot use `break` to exit a loop in its caller; the function must be running its own loop. Using `break` outside a loop in the current function call is a runtime error.

`continue` skips the rest of the current iteration of the nearest enclosing loop. A `while` checks its condition again, while a `for` advances to its next element. In nested loops, it affects only the innermost loop. A function cannot use `continue` on a caller's loop, and using it outside a loop in the current function call is a runtime error.

List literals use square brackets and commas. Lists can be stored, indexed, changed through mutable bindings, measured with `length`, and iterated. `append` returns `true` after a successful mutation.

## Booleans

Boolean values are `true` and `false`. Conditions can use `and`, `or`, and `not`.

```newt
val hungry = true
val sleeping = false

if hungry and not sleeping
    print "snack time"
end
```

## Strings and escapes

Strings are enclosed in double quotes. Use `\n` for newline, `\t` for tab, `\r` for carriage return, `\\` for a literal backslash, and `\"` for a literal double quote.

```newt
print "Name:\tNewt\nStatus:\tready"
print "She said \"hello\"."
print "C:\\notes"
```

Only those five escapes are valid. An unsupported escape or a backslash left at the end of an unterminated string is a lexer error reported at the backslash.

Use `len` to count string bytes, `contains` for case-sensitive substring search, `upper` and `lower` for ASCII case conversion, and `trim` to remove surrounding ASCII whitespace.

```newt
val command = trim("  Build Project  ")
print upper(command)
print len(command)
print contains(lower(command), "build")
```

`contains(text, "")` is always true. Newt strings are byte strings, so `len` counts UTF-8 bytes rather than Unicode characters, and case conversion leaves non-ASCII bytes unchanged.

## Math

Newt supports `+`, `-`, `*`, `/`, comparisons, negative numbers, and grouped expressions. `+` also concatenates two strings.

```newt
val total = (4 + 6) * 2
print -total
print total >= 10
print "total: " + text(total)
```

`text(value)` explicitly converts a number or boolean to a string and leaves a string unchanged. Newt does not implicitly combine strings and numbers.

Use `sqrt` for the square root of a non-negative number.

```newt
print sqrt(25)
print abs(-12.5)
print floor(3.9)
print ceil(3.1)
print min(4, -2)
print max(4, -2)
print pow(2, 8)
print sin(pi / 2)
print atan2(1, -1)
print log(e)
print log10(1000)
print exp(1)
print round(2.5)
```

`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, and `atan2` use radians. `pi` and `e` are predefined immutable numbers. Inverse sine and cosine accept only -1 through 1, logarithms require positive inputs, and scientific helpers report non-finite results as runtime errors.

## Functions

Declare a function with `fn`, a name, optional parameter names, and `end`. Pass argument values in the function call. Use `return` to stop the function and send a value back.

```newt
fn add(a, b)
    return a + b
end

print add(2, 3)
```

Functions may still have no parameters. Calls whose return values are not needed can remain standalone statements. A function currently supports up to 16 parameters. Newt does not have a `none` value yet, so a function without an explicit `return` produces the number `0` when its call is used as a value.

## Comments

A comment begins with `#` and continues to the end of the line.

```newt
# Calculate the area of a square.
val side = 4
print side * side
```

## File I/O

Use `file_read` to read a complete text file. Use `file_write` to create or overwrite a file, and `file_append` to add text to its end.

```newt
val note = file_read("notes.txt")
print note

file_write("notes.txt", "first line")
file_append("notes.txt", "\nsecond line")
```

All file paths and written text must be strings. `file_write` and `file_append` return `true` when successful.

File I/O is an early feature. Newt reads a whole file into memory, does not create parent directories, and uses the operating system's normal path and permission rules.

## Command-line arguments

Arguments written after the script path are available through `arg_count()` and zero-based `arg(index)`.

```sh
./newt.exe examples/args.nt hello newt
```

```newt
print arg_count()
print arg(0)
print arg(1)
```

This prints `2`, `hello`, and `newt`. An index outside the available arguments is a runtime error.
