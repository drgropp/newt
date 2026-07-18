# Newt language guide

Newt is a small scripting language with line-oriented statements and blocks closed by `end`.

## Variables

Use `val` for a value that cannot be reassigned. Use `mut` when the value needs to change.

```newt
val name = "Ada"
mut score = 10
score = score + 5
```

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
    count = count + 1
end
```

## Booleans

Boolean values are `true` and `false`. Conditions can use `and`, `or`, and `not`.

```newt
val hungry = true
val sleeping = false

if hungry and not sleeping
    print "snack time"
end
```

## Math

Newt supports `+`, `-`, `*`, `/`, comparisons, negative numbers, and grouped expressions.

```newt
val total = (4 + 6) * 2
print -total
print total >= 10
```

Use `sqrt` for the square root of a non-negative number.

```newt
print sqrt(25)
```

## Functions

Declare a function with `fn`, a name, optional parameter names, and `end`. Pass argument values in the function call. Use `return` to stop the function and send a value back.

```newt
fn add(a, b)
    return a + b
end

print add(2, 3)
```

Functions may still have no parameters. Calls whose return values are not needed can remain standalone statements. A function currently supports up to 16 parameters.

## Comments

A comment begins with `#` and continues to the end of the line.

```newt
# Calculate the area of a square.
val side = 4
print side * side
```
