$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$Newt = Join-Path $Root "newt.exe"
if (-not (Test-Path -LiteralPath $Newt)) {
    $Newt = Join-Path $Root "newt"
}
if (-not (Test-Path -LiteralPath $Newt)) {
    Write-Error "Newt executable not found. Build it before running regression tests."
    exit 1
}

function Normalize-Newlines([string]$Text) {
    return $Text.Replace("`r`n", "`n")
}

$Tests = @(
    @{
        Name = "large typed doubles print safely"
        Arguments = "tests/cases/types_large_double.nt"
        ExitCode = 0
        Stdout = "1e+100`n[1e+100]`n"
        Stderr = ""
    },
    @{
        Name = "typed example"
        Arguments = "examples/typed_systems.nt"
        ExitCode = 0
        Stdout = "24`n21.5`n1e-06`n12`n"
        Stderr = ""
    },
    @{
        Name = "typed parse output"
        Arguments = "--parse examples/typed_systems.nt"
        ExitCode = 0
        Stdout = "PROGRAM`n  FN_DECL name=consume`n    RETURNS int`n    PARAMETERS`n      PARAM name=fuel type=int mutable=1`n      PARAM name=amount type=int mutable=0`n    BODY`n      ASSIGN name=fuel`n        BINARY -`n          IDENT fuel`n          IDENT amount`n      RETURN`n        IDENT fuel`n  FN_DECL name=main`n    RETURNS void`n    BODY`n      LET_DECL name=capacity`n        TYPE int`n        NUMBER 25`n      MUT_DECL name=fuel`n        TYPE int`n        IDENT capacity`n      LET_DECL name=temperature`n        TYPE float`n        NUMBER 21.5`n      MUT_DECL name=precise`n        TYPE double`n        NUMBER 0.000001`n      ASSIGN name=fuel`n        CALL name=consume arguments=2`n          IDENT fuel`n          NUMBER 1`n      PRINT`n        IDENT fuel`n      PRINT`n        IDENT temperature`n      PRINT`n        IDENT precise`n      PRINT`n        BINARY /`n          CALL name=double arguments=1`n            IDENT fuel`n          NUMBER 2.0`n      RETURN`n  CALL name=main arguments=0`nEOF`n"
        Stderr = ""
    },
    @{
        Name = "typed token output"
        Arguments = "--tokens examples/typed_systems.nt"
        ExitCode = 0
        Stdout = "1:1 SEMICOLON ;`n1:72 NEWLINE`n2:1 INT int`n2:5 IDENT consume`n2:12 LEFT_PAREN (`n2:13 INT int`n2:17 IDENT fuel`n2:21 COMMA ,`n2:23 CONST const`n2:29 INT int`n2:33 IDENT amount`n2:39 RIGHT_PAREN )`n2:41 LEFT_BRACE {`n2:42 NEWLINE`n3:5 IDENT fuel`n3:10 EQUAL =`n3:12 IDENT fuel`n3:17 MINUS -`n3:19 IDENT amount`n3:25 SEMICOLON ;`n3:26 NEWLINE`n4:5 RETURN return`n4:12 IDENT fuel`n4:16 SEMICOLON ;`n4:17 NEWLINE`n5:1 RIGHT_BRACE }`n5:2 NEWLINE`n6:1 NEWLINE`n7:1 VOID void`n7:6 IDENT main`n7:10 LEFT_PAREN (`n7:11 VOID void`n7:15 RIGHT_PAREN )`n7:17 LEFT_BRACE {`n7:18 NEWLINE`n8:5 CONST const`n8:11 INT int`n8:15 IDENT capacity`n8:24 EQUAL =`n8:26 NUMBER 25`n8:28 SEMICOLON ;`n8:29 NEWLINE`n9:5 MUT mut`n9:9 INT int`n9:13 IDENT fuel`n9:18 EQUAL =`n9:20 IDENT capacity`n9:28 SEMICOLON ;`n9:29 NEWLINE`n10:5 LET let`n10:9 FLOAT float`n10:15 IDENT temperature`n10:27 EQUAL =`n10:29 NUMBER 21.5`n10:33 SEMICOLON ;`n10:34 NEWLINE`n11:5 DOUBLE double`n11:12 IDENT precise`n11:20 EQUAL =`n11:22 NUMBER 0.000001`n11:30 SEMICOLON ;`n11:31 NEWLINE`n12:1 NEWLINE`n13:5 IDENT fuel`n13:10 EQUAL =`n13:12 IDENT consume`n13:19 LEFT_PAREN (`n13:20 IDENT fuel`n13:24 COMMA ,`n13:26 NUMBER 1`n13:27 RIGHT_PAREN )`n13:28 SEMICOLON ;`n13:29 NEWLINE`n14:5 PRINT print`n14:11 IDENT fuel`n14:15 SEMICOLON ;`n14:16 NEWLINE`n15:5 PRINT print`n15:11 IDENT temperature`n15:22 SEMICOLON ;`n15:23 NEWLINE`n16:5 PRINT print`n16:11 IDENT precise`n16:18 SEMICOLON ;`n16:19 NEWLINE`n17:5 PRINT print`n17:11 DOUBLE double`n17:17 LEFT_PAREN (`n17:18 IDENT fuel`n17:22 RIGHT_PAREN )`n17:24 SLASH /`n17:26 NUMBER 2.0`n17:29 SEMICOLON ;`n17:30 NEWLINE`n18:5 RETURN return`n18:11 SEMICOLON ;`n18:12 NEWLINE`n19:1 RIGHT_BRACE }`n19:2 NEWLINE`n20:1 NEWLINE`n21:1 SEMICOLON ;`n21:49 NEWLINE`n22:1 IDENT main`n22:5 LEFT_PAREN (`n22:6 RIGHT_PAREN )`n22:7 SEMICOLON ;`n22:8 NEWLINE`n23:1 EOF`n"
        Stderr = ""
    },
    @{
        Name = "bad_types_float_overflow"
        Arguments = "tests/cases/bad_types_float_overflow.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_float_overflow.nt:1:39: runtime error: numeric value must be finite`n"
    },
    @{
        Name = "bad_types_double_overflow"
        Arguments = "tests/cases/bad_types_double_overflow.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_double_overflow.nt:1:42: runtime error: numeric value must be finite`n"
    },
    @{
        Name = "bad_types_non_numeric_return"
        Arguments = "tests/cases/bad_types_non_numeric_return.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_non_numeric_return.nt:1:27: runtime error: expected bool, got untyped (use an explicit numeric conversion when needed)`n"
    },
    @{
        Name = "bad_types_typed_legacy"
        Arguments = "tests/cases/bad_types_typed_legacy.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_typed_legacy.nt:1:20: runtime error: expected int, got untyped (use an explicit numeric conversion when needed)`n"
    },
    @{
        Name = "bad_types_unreached_return"
        Arguments = "tests/cases/bad_types_unreached_return.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_unreached_return.nt:1:45: runtime error: typed function completed without returning a value`n"
    },
    @{
        Name = "typed parameter pool growth"
        Arguments = "tests/cases/types_parameter_growth.nt"
        ExitCode = 0
        Stdout = "5`n"
        Stderr = ""
    },
    @{
        Name = "types_declarations"
        Arguments = "tests/cases/types_declarations.nt"
        ExitCode = 0
        Stdout = "24`n100`n2`n"
        Stderr = ""
    },
    @{
        Name = "types_functions"
        Arguments = "tests/cases/types_functions.nt"
        ExitCode = 0
        Stdout = "hello`n5`n"
        Stderr = ""
    },
    @{
        Name = "types_float_double"
        Arguments = "tests/cases/types_float_double.nt"
        ExitCode = 0
        Stdout = "2.5`n2.5`n10`n"
        Stderr = ""
    },
    @{
        Name = "types_integer_division"
        Arguments = "tests/cases/types_integer_division.nt"
        ExitCode = 0
        Stdout = "2`n-2`n4`ntrue`n"
        Stderr = ""
    },
    @{
        Name = "types_rounding"
        Arguments = "tests/cases/types_rounding.nt"
        ExitCode = 0
        Stdout = "true`ntrue`ntrue`n"
        Stderr = ""
    },
    @{
        Name = "types_limits"
        Arguments = "tests/cases/types_limits.nt"
        ExitCode = 0
        Stdout = "-2147483648`n2147483647`n-2`n5`n5`n"
        Stderr = ""
    },
    @{
        Name = "types_non_numeric"
        Arguments = "tests/cases/types_non_numeric.nt"
        ExitCode = 0
        Stdout = "newt`ntrue`n3`n"
        Stderr = ""
    },
    @{
        Name = "types_recursion"
        Arguments = "tests/cases/types_recursion.nt"
        ExitCode = 0
        Stdout = "21`n"
        Stderr = ""
    },
    @{
        Name = "types_const_parameter"
        Arguments = "tests/cases/types_const_parameter.nt"
        ExitCode = 0
        Stdout = "9`n9`n"
        Stderr = ""
    },
    @{
        Name = "types_void_fallthrough"
        Arguments = "tests/cases/types_void_fallthrough.nt"
        ExitCode = 0
        Stdout = "done`n"
        Stderr = ""
    },
    @{
        Name = "types_legacy_bridge"
        Arguments = "tests/cases/types_legacy_bridge.nt"
        ExitCode = 0
        Stdout = "5`n3`n"
        Stderr = ""
    },
    @{
        Name = "types_parameter_copy"
        Arguments = "tests/cases/types_parameter_copy.nt"
        ExitCode = 0
        Stdout = "9`n2`n"
        Stderr = ""
    },
    @{
        Name = "bad_types_assignment"
        Arguments = "tests/cases/bad_types_assignment.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_assignment.nt:1:16: runtime error: expected int, got string (use an explicit numeric conversion when needed)`n"
    },
    @{
        Name = "bad_types_const"
        Arguments = "tests/cases/bad_types_const.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_const.nt:1:18: runtime error: cannot assign to immutable variable 'n'`n"
    },
    @{
        Name = "bad_types_let"
        Arguments = "tests/cases/bad_types_let.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_let.nt:1:16: runtime error: cannot assign to immutable variable 'n'`n"
    },
    @{
        Name = "bad_types_parameter"
        Arguments = "tests/cases/bad_types_parameter.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_parameter.nt:1:42: runtime error: expected int, got string (use an explicit numeric conversion when needed)`n"
    },
    @{
        Name = "bad_types_const_parameter"
        Arguments = "tests/cases/bad_types_const_parameter.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_const_parameter.nt:1:28: runtime error: cannot assign to immutable variable 'n'`n"
    },
    @{
        Name = "bad_types_return"
        Arguments = "tests/cases/bad_types_return.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_return.nt:1:26: runtime error: expected int, got string (use an explicit numeric conversion when needed)`n"
    },
    @{
        Name = "bad_types_missing_return"
        Arguments = "tests/cases/bad_types_missing_return.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_missing_return.nt:1:20: runtime error: typed function completed without returning a value`n"
    },
    @{
        Name = "bad_types_void_return"
        Arguments = "tests/cases/bad_types_void_return.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_void_return.nt:1:20: runtime error: void function cannot return a value`n"
    },
    @{
        Name = "bad_types_void_value"
        Arguments = "tests/cases/bad_types_void_value.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_void_value.nt:1:27: runtime error: void function cannot be used as a value`n"
    },
    @{
        Name = "bad_types_bare_return"
        Arguments = "tests/cases/bad_types_bare_return.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_bare_return.nt:1:19: runtime error: return requires a value in this function`n"
    },
    @{
        Name = "bad_types_mixed"
        Arguments = "tests/cases/bad_types_mixed.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_mixed.nt:1:34: runtime error: mixed numeric types require an explicit conversion`n"
    },
    @{
        Name = "bad_types_conversion"
        Arguments = "tests/cases/bad_types_conversion.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_conversion.nt:1:23: runtime error: expected int, got double (use an explicit numeric conversion when needed)`n"
    },
    @{
        Name = "bad_types_fraction"
        Arguments = "tests/cases/bad_types_fraction.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_fraction.nt:1:9: runtime error: value is not representable as int`n"
    },
    @{
        Name = "bad_types_overflow"
        Arguments = "tests/cases/bad_types_overflow.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_overflow.nt:1:27: runtime error: int arithmetic overflow`n"
    },
    @{
        Name = "bad_types_multiply"
        Arguments = "tests/cases/bad_types_multiply.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_multiply.nt:1:29: runtime error: int arithmetic overflow`n"
    },
    @{
        Name = "bad_types_divide_overflow"
        Arguments = "tests/cases/bad_types_divide_overflow.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_divide_overflow.nt:1:30: runtime error: int arithmetic overflow`n"
    },
    @{
        Name = "bad_types_negate"
        Arguments = "tests/cases/bad_types_negate.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_negate.nt:1:28: runtime error: int arithmetic overflow`n"
    },
    @{
        Name = "bad_types_divide_zero"
        Arguments = "tests/cases/bad_types_divide_zero.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_divide_zero.nt:1:20: runtime error: division by zero`n"
    },
    @{
        Name = "bad_types_cast_range"
        Arguments = "tests/cases/bad_types_cast_range.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_cast_range.nt:1:9: runtime error: value is not representable as int`n"
    },
    @{
        Name = "bad_types_cast_string"
        Arguments = "tests/cases/bad_types_cast_string.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_cast_string.nt:1:9: runtime error: numeric conversion requires a number`n"
    },
    @{
        Name = "bad_types_cast_arity"
        Arguments = "tests/cases/bad_types_cast_arity.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_cast_arity.nt:1:9: runtime error: numeric conversion expects exactly one argument`n"
    },
    @{
        Name = "bad_types_float_range"
        Arguments = "tests/cases/bad_types_float_range.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_float_range.nt:1:11: runtime error: value is not representable as float`n"
    },
    @{
        Name = "bad_types_void_variable"
        Arguments = "tests/cases/bad_types_void_variable.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_void_variable.nt:1:6: parse error: variables cannot have type void`n"
    },
    @{
        Name = "bad_types_void_parameter"
        Arguments = "tests/cases/bad_types_void_parameter.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_void_parameter.nt:1:16: parse error: void must be the only parameter marker`n"
    },
    @{
        Name = "bad_types_parameter_type"
        Arguments = "tests/cases/bad_types_parameter_type.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_parameter_type.nt:1:11: parse error: expected non-void parameter type`n"
    },
    @{
        Name = "bad_types_conflict"
        Arguments = "tests/cases/bad_types_conflict.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_conflict.nt:1:5: parse error: expected identifier after let`n"
    },
    @{
        Name = "bad_types_const_no_type"
        Arguments = "tests/cases/bad_types_const_no_type.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_const_no_type.nt:1:7: parse error: expected type after const`n"
    },
    @{
        Name = "bad_types_uninitialized"
        Arguments = "tests/cases/bad_types_uninitialized.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_types_uninitialized.nt:1:6: parse error: expected '=' after variable name`n"
    },
    @{
        Name = "brace tokens"
        Arguments = "--tokens tests/cases/brace_tokens.nt"
        ExitCode = 0
        Stdout = "1:1 FN fn`n1:4 IDENT empty`n1:9 LEFT_PAREN (`n1:10 RIGHT_PAREN )`n1:12 LEFT_BRACE {`n1:13 RIGHT_BRACE }`n1:14 EOF`n"
        Stderr = ""
    },
    @{
        Name = "brace_empty"
        Arguments = "tests/cases/brace_empty.nt"
        ExitCode = 0
        Stdout = "0`n"
        Stderr = ""
    },
    @{
        Name = "brace_newline"
        Arguments = "tests/cases/brace_newline.nt"
        ExitCode = 0
        Stdout = "10`n20`n30`n"
        Stderr = ""
    },
    @{
        Name = "brace_nested_else"
        Arguments = "tests/cases/brace_nested_else.nt"
        ExitCode = 0
        Stdout = "1`n3`n"
        Stderr = ""
    },
    @{
        Name = "brace_comments"
        Arguments = "tests/cases/brace_comments.nt"
        ExitCode = 0
        Stdout = "{ } {{ }} end`n"
        Stderr = ""
    },
    @{
        Name = "brace_end_identifier"
        Arguments = "tests/cases/brace_end_identifier.nt"
        ExitCode = 0
        Stdout = "4`n"
        Stderr = ""
    },
    @{
        Name = "bad_brace_open"
        Arguments = "tests/cases/bad_brace_open.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_brace_open.nt:2:1: parse error: expected '{' to open block`n"
    },
    @{
        Name = "bad_brace_close"
        Arguments = "tests/cases/bad_brace_close.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_brace_close.nt:2:1: parse error: expected '}' after function declaration`n"
    },
    @{
        Name = "bad_brace_old_end"
        Arguments = "tests/cases/bad_brace_old_end.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_brace_old_end.nt:2:4: parse error: expected '=' after assignment name`n"
    },
    @{
        Name = "bad_brace_double"
        Arguments = "tests/cases/bad_brace_double.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_brace_double.nt:1:13: parse error: expected statement`n"
    },
    @{
        Name = "bad_brace_extra"
        Arguments = "tests/cases/bad_brace_extra.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_brace_extra.nt:1:1: parse error: expected statement`n"
    },
    @{
        Name = "bad_brace_if_close"
        Arguments = "tests/cases/bad_brace_if_close.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_brace_if_close.nt:1:19: parse error: expected '}' after if branch`n"
    },
    @{
        Name = "bad_brace_else_close"
        Arguments = "tests/cases/bad_brace_else_close.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_brace_else_close.nt:1:19: parse error: expected '}' after else branch`n"
    },
    @{
        Name = "semicolon_inline"
        Arguments = "tests/cases/semicolon_inline.nt"
        ExitCode = 0
        Stdout = "24`n"
        Stderr = ""
    },
    @{
        Name = "semicolon_blocks"
        Arguments = "tests/cases/semicolon_blocks.nt"
        ExitCode = 0
        Stdout = "5`n2`n2`n"
        Stderr = ""
    },
    @{
        Name = "semicolon_comments"
        Arguments = "tests/cases/semicolon_comments.nt"
        ExitCode = 0
        Stdout = "25`n"
        Stderr = ""
    },
    @{
        Name = "semicolon_strings"
        Arguments = "tests/cases/semicolon_strings.nt"
        ExitCode = 0
        Stdout = "a;#b#c;`nescaped `";# is still text`n"
        Stderr = ""
    },
    @{
        Name = "semicolon_crlf"
        Arguments = "tests/cases/semicolon_crlf.nt"
        ExitCode = 0
        Stdout = "2`n"
        Stderr = ""
    },
    @{
        Name = "semicolon_empty"
        Arguments = "tests/cases/semicolon_empty.nt"
        ExitCode = 0
        Stdout = "1`n"
        Stderr = ""
    },
    @{
        Name = "bad_missing_semicolon"
        Arguments = "tests/cases/bad_missing_semicolon.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_missing_semicolon.nt:1:14: parse error: expected ';' after statement`n"
    },
    @{
        Name = "bad_missing_semicolon_eof"
        Arguments = "tests/cases/bad_missing_semicolon_eof.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_missing_semicolon_eof.nt:1:9: parse error: expected ';' after statement`n"
    },
    @{
        Name = "brace closer needs no semicolon"
        Arguments = "tests/cases/brace_closer_no_semicolon.nt"
        ExitCode = 0
        Stdout = "1`n"
        Stderr = ""
    },
    @{
        Name = "bad_missing_body_semicolon"
        Arguments = "tests/cases/bad_missing_body_semicolon.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_missing_body_semicolon.nt:2:12: parse error: expected ';' after statement`n"
    },
    @{
        Name = "bad_hash_comment"
        Arguments = "tests/cases/bad_hash_comment.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_hash_comment.nt:1:1: lexer error: unknown character '#'`n"
    },
    @{
        Name = "semicolon_comment_column"
        Arguments = "--tokens tests/cases/semicolon_comment_column.nt"
        ExitCode = 0
        Stdout = "1:1 SEMICOLON ;`n1:8 NEWLINE`n2:1 PRINT print`n2:7 NUMBER 1`n2:8 SEMICOLON ;`n2:19 NEWLINE`n3:1 PRINT print`n3:7 NUMBER 2`n3:8 SEMICOLON ;`n3:9 EOF`n"
        Stderr = ""
    },
    @{
        Name = "let declaration and mutable assignment"
        Arguments = "tests/cases/let_keyword.nt"
        ExitCode = 0
        Stdout = "10`n9`n"
        Stderr = ""
    },
    @{
        Name = "let token output"
        Arguments = "--tokens tests/cases/let_keyword.nt"
        ExitCode = 0
        Stdout = "1:1 LET let`n1:5 IDENT score`n1:11 EQUAL =`n1:13 NUMBER 10`n1:15 SEMICOLON ;`n1:16 NEWLINE`n2:1 PRINT print`n2:7 IDENT score`n2:12 SEMICOLON ;`n2:13 NEWLINE`n3:1 MUT mut`n3:5 IDENT health`n3:12 EQUAL =`n3:14 NUMBER 10`n3:16 SEMICOLON ;`n3:17 NEWLINE`n4:1 IDENT health`n4:8 EQUAL =`n4:10 NUMBER 9`n4:11 SEMICOLON ;`n4:12 NEWLINE`n5:1 PRINT print`n5:7 IDENT health`n5:13 SEMICOLON ;`n5:14 NEWLINE`n6:1 EOF`n"
        Stderr = ""
    },
    @{
        Name = "let parse tree"
        Arguments = "--parse tests/cases/let_keyword.nt"
        ExitCode = 0
        Stdout = "PROGRAM`n  LET_DECL name=score`n    NUMBER 10`n  PRINT`n    IDENT score`n  MUT_DECL name=health`n    NUMBER 10`n  ASSIGN name=health`n    NUMBER 9`n  PRINT`n    IDENT health`nEOF`n"
        Stderr = ""
    },
    @{
        Name = "let assignment rejected"
        Arguments = "tests/cases/bad_let_assignment.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_let_assignment.nt:2:1: runtime error: cannot assign to immutable variable 'score'`n"
    },
    @{
        Name = "old val declaration rejected"
        Arguments = "tests/cases/bad_old_val_declaration.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_old_val_declaration.nt:1:5: parse error: expected '=' after assignment name`n"
    },
    @{
        Name = "keyword boundaries and val identifier"
        Arguments = "tests/cases/let_identifiers.nt"
        ExitCode = 0
        Stdout = "6`n"
        Stderr = ""
    },
    @{
        Name = "existing hello example"
        Arguments = "examples/hello.nt"
        ExitCode = 0
        Stdout = "hello from newt`n"
        Stderr = ""
    },
    @{
        Name = "string concatenation"
        Arguments = "tests/cases/string_concat.nt"
        ExitCode = 0
        Stdout = "hello world`nnewt`nabc`n"
        Stderr = ""
    },
    @{
        Name = "newline escape"
        Arguments = "tests/cases/escape_newline.nt"
        ExitCode = 0
        Stdout = "first`nsecond`n"
        Stderr = ""
    },
    @{
        Name = "tab escape"
        Arguments = "tests/cases/escape_tab.nt"
        ExitCode = 0
        Stdout = "left`tright`n"
        Stderr = ""
    },
    @{
        Name = "carriage-return escape"
        Arguments = "tests/cases/escape_carriage_return.nt"
        ExitCode = 0
        Stdout = "left`rright`n"
        Stderr = ""
    },
    @{
        Name = "escaped quote"
        Arguments = "tests/cases/escape_quote.nt"
        ExitCode = 0
        Stdout = "She said `"Newt`".`n"
        Stderr = ""
    },
    @{
        Name = "escaped backslash"
        Arguments = "tests/cases/escape_backslash.nt"
        ExitCode = 0
        Stdout = "C:\Newt`n\n`n"
        Stderr = ""
    },
    @{
        Name = "multiple escapes"
        Arguments = "tests/cases/escape_multiple.nt"
        ExitCode = 0
        Stdout = "row1`n`t`"Newt`"\done`n"
        Stderr = ""
    },
    @{
        Name = "invalid escape"
        Arguments = "--tokens tests/cases/bad_escape.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_escape.nt:1:5: lexer error: invalid escape sequence '\q'`n"
    },
    @{
        Name = "trailing backslash escape"
        Arguments = "--tokens tests/cases/bad_escape_trailing.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_escape_trailing.nt:1:5: lexer error: incomplete escape sequence`n"
    },
    @{
        Name = "text conversions"
        Arguments = "tests/cases/text_values.nt"
        ExitCode = 0
        Stdout = "42`n3.5`ntrue`nfalse`nnewt`nvalue=7`n"
        Stderr = ""
    },
    @{
        Name = "string helpers"
        Arguments = "tests/cases/string_helpers.nt"
        ExitCode = 0
        Stdout = "0`n4`n3`nNEWT CLI 2`nnewt cli 2`n[Newt]`ntrue`nfalse`ntrue`n"
        Stderr = ""
    },
    @{
        Name = "string helper type failure"
        Arguments = "tests/cases/bad_string_helper.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_string_helper.nt:1:7: runtime error: len argument must be a string, got number`n"
    },
    @{
        Name = "math helpers"
        Arguments = "tests/cases/math_helpers.nt"
        ExitCode = 0
        Stdout = "3.5`n3`n4`n-2`n4`n256`n3`n"
        Stderr = ""
    },
    @{
        Name = "math helper type failure"
        Arguments = "tests/cases/bad_math_helper.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_math_helper.nt:1:7: runtime error: pow base must be a number, got string`n"
    },
    @{
        Name = "pow domain failure"
        Arguments = "tests/cases/bad_pow_domain.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_pow_domain.nt:1:7: runtime error: pow arguments are outside the numeric domain`n"
    },
    @{
        Name = "scientific math builtins and constants"
        Arguments = "tests/cases/scientific_math.nt"
        ExitCode = 0
        Stdout = "1`n1`n1`n1.5708`n0`n0.785398`n2.35619`n1`n3`n2.71828`n3`n-3`n3.14159`n2.71828`n3`n3.14159`n"
        Stderr = ""
    },
    @{
        Name = "inverse trig domain failure"
        Arguments = "tests/cases/bad_asin_domain.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_asin_domain.nt:1:7: runtime error: asin argument must be between -1 and 1`n"
    },
    @{
        Name = "logarithm domain failure"
        Arguments = "tests/cases/bad_log_domain.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_log_domain.nt:1:7: runtime error: log argument must be greater than zero`n"
    },
    @{
        Name = "exponential range failure"
        Arguments = "tests/cases/bad_exp_range.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_exp_range.nt:1:7: runtime error: exp result is out of range`n"
    },
    @{
        Name = "scientific math type failure"
        Arguments = "tests/cases/bad_scientific_type.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_scientific_type.nt:1:7: runtime error: sin argument must be a number, got string`n"
    },
    @{
        Name = "atan2 undefined origin failure"
        Arguments = "tests/cases/bad_atan2_origin.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_atan2_origin.nt:1:7: runtime error: atan2 arguments cannot both be zero`n"
    },
    @{
        Name = "pi is immutable"
        Arguments = "tests/cases/bad_pi_assignment.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_pi_assignment.nt:1:1: runtime error: cannot assign to immutable variable 'pi'`n"
    },
    @{
        Name = "while parse tree"
        Arguments = "--parse tests/cases/while_parse.nt"
        ExitCode = 0
        Stdout = "PROGRAM`n  MUT_DECL name=count`n    NUMBER 0`n  WHILE`n    CONDITION`n      BINARY <`n        IDENT count`n        NUMBER 2`n    BODY`n      PRINT`n        IDENT count`n      ASSIGN name=count`n        BINARY +`n          IDENT count`n          NUMBER 1`nEOF`n"
        Stderr = ""
    },
    @{
        Name = "while runtime"
        Arguments = "tests/cases/while_runtime.nt"
        ExitCode = 0
        Stdout = "0`n1`n2`n3`n0`n"
        Stderr = ""
    },
    @{
        Name = "if, else, nested shadowing, and outer mutation scope"
        Arguments = "tests/cases/block_scope.nt"
        ExitCode = 0
        Stdout = "20`n30`n20`n10`n5`n40`n"
        Stderr = ""
    },
    @{
        Name = "if local does not leak"
        Arguments = "tests/cases/bad_if_scope.nt"
        ExitCode = 1
        Stdout = "12`n"
        Stderr = "tests/cases/bad_if_scope.nt:5:7: runtime error: undefined variable 'hidden'`n"
    },
    @{
        Name = "else local does not leak"
        Arguments = "tests/cases/bad_else_scope.nt"
        ExitCode = 1
        Stdout = "13`n"
        Stderr = "tests/cases/bad_else_scope.nt:7:7: runtime error: undefined variable 'hidden'`n"
    },
    @{
        Name = "while local does not leak"
        Arguments = "tests/cases/bad_while_scope.nt"
        ExitCode = 1
        Stdout = "42`n"
        Stderr = "tests/cases/bad_while_scope.nt:7:7: runtime error: undefined variable 'local'`n"
    },
    @{
        Name = "scope cleanup through continue, break, return, and nested blocks"
        Arguments = "tests/cases/scope_control_flow.nt"
        ExitCode = 0
        Stdout = "1`n3`n3`n7`n"
        Stderr = ""
    },
    @{
        Name = "for parse tree"
        Arguments = "--parse tests/cases/for_parse.nt"
        ExitCode = 0
        Stdout = "PROGRAM`n  FOR name=name`n    ITERABLE`n      LIST elements=2`n        STRING `"newt`"`n        STRING `"ghostnote`"`n    BODY`n      PRINT`n        IDENT name`nEOF`n"
        Stderr = ""
    },
    @{
        Name = "for normal iteration and loop variable shadowing"
        Arguments = "tests/cases/for_iteration.nt"
        ExitCode = 0
        Stdout = "newt`nghostnote`nstray signal`nouter`n"
        Stderr = ""
    },
    @{
        Name = "for empty list"
        Arguments = "tests/cases/for_empty.nt"
        ExitCode = 0
        Stdout = "done`n"
        Stderr = ""
    },
    @{
        Name = "break exits for loop"
        Arguments = "tests/cases/for_break.nt"
        ExitCode = 0
        Stdout = "1`ndone`n"
        Stderr = ""
    },
    @{
        Name = "continue advances for loop"
        Arguments = "tests/cases/for_continue.nt"
        ExitCode = 0
        Stdout = "1`n3`n4`n"
        Stderr = ""
    },
    @{
        Name = "nested for and while loops"
        Arguments = "tests/cases/for_nested.nt"
        ExitCode = 0
        Stdout = "1:1`n1:2`n2:1`n2:2`n1a`n1b`n2a`n2b`n"
        Stderr = ""
    },
    @{
        Name = "for invalid iterable failure"
        Arguments = "tests/cases/bad_for_iterable.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_for_iterable.nt:1:13: runtime error: for iterable must be a list, got number`n"
    },
    @{
        Name = "for loop variable does not leak"
        Arguments = "tests/cases/bad_for_scope.nt"
        ExitCode = 1
        Stdout = "1`n"
        Stderr = "tests/cases/bad_for_scope.nt:5:7: runtime error: undefined variable 'item'`n"
    },
    @{
        Name = "list index parse tree"
        Arguments = "--parse tests/cases/list_index_parse.nt"
        ExitCode = 0
        Stdout = "PROGRAM`n  PRINT`n    INDEX`n      TARGET`n        IDENT items`n      POSITION`n        NUMBER 1`nEOF`n"
        Stderr = ""
    },
    @{
        Name = "list indexing"
        Arguments = "tests/cases/list_index.nt"
        ExitCode = 0
        Stdout = "zero`ntwo`nvalue`n3`n"
        Stderr = ""
    },
    @{
        Name = "list length"
        Arguments = "tests/cases/list_length.nt"
        ExitCode = 0
        Stdout = "0`n3`n2`n"
        Stderr = ""
    },
    @{
        Name = "indexed list assignment"
        Arguments = "tests/cases/list_index_assignment.nt"
        ExitCode = 0
        Stdout = "bow`n9`n"
        Stderr = ""
    },
    @{
        Name = "append to list and preserve aliases"
        Arguments = "tests/cases/list_append.nt"
        ExitCode = 0
        Stdout = "2`nshield`n"
        Stderr = ""
    },
    @{
        Name = "list mutation followed by indexing"
        Arguments = "tests/cases/list_mutation_index.nt"
        ExitCode = 0
        Stdout = "wand`nshield`n"
        Stderr = ""
    },
    @{
        Name = "list mutation followed by for iteration"
        Arguments = "tests/cases/list_mutation_for.nt"
        ExitCode = 0
        Stdout = "wand`nshield`nbow`n"
        Stderr = ""
    },
    @{
        Name = "indexed assignment rejects immutable variable"
        Arguments = "tests/cases/bad_list_index_assignment_immutable.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_index_assignment_immutable.nt:2:1: runtime error: cannot mutate immutable variable 'items'`n"
    },
    @{
        Name = "append rejects immutable variable"
        Arguments = "tests/cases/bad_list_append_immutable.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_append_immutable.nt:2:8: runtime error: cannot mutate immutable variable 'items'`n"
    },
    @{
        Name = "indexed assignment validates integer index"
        Arguments = "tests/cases/bad_list_assignment_index.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_assignment_index.nt:2:7: runtime error: list index must be an integer`n"
    },
    @{
        Name = "indexed assignment validates index range"
        Arguments = "tests/cases/bad_list_assignment_range.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_assignment_range.nt:2:7: runtime error: list index 2 out of range for list of length 2`n"
    },
    @{
        Name = "list index out of range"
        Arguments = "tests/cases/bad_list_index_range.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_index_range.nt:2:13: runtime error: list index 3 out of range for list of length 3`n"
    },
    @{
        Name = "list index must be an integer"
        Arguments = "tests/cases/bad_list_index_integer.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_index_integer.nt:2:13: runtime error: list index must be an integer`n"
    },
    @{
        Name = "list index type failure"
        Arguments = "tests/cases/bad_list_index_type.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_index_type.nt:2:13: runtime error: list index must be an integer, got string`n"
    },
    @{
        Name = "indexing a non-list"
        Arguments = "tests/cases/bad_list_index_non_list.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_index_non_list.nt:1:9: runtime error: indexed value must be a list, got number`n"
    },
    @{
        Name = "length argument type failure"
        Arguments = "tests/cases/bad_list_length.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_length.nt:1:7: runtime error: length argument must be a list, got string`n"
    },
    @{
        Name = "break exits nearest loop"
        Arguments = "tests/cases/break_nested.nt"
        ExitCode = 0
        Stdout = "1:2`n2:2`n3:2`ndone`n"
        Stderr = ""
    },
    @{
        Name = "break parse tree"
        Arguments = "--parse tests/cases/break_parse.nt"
        ExitCode = 0
        Stdout = "PROGRAM`n  WHILE`n    CONDITION`n      TRUE true`n    BODY`n      BREAK`nEOF`n"
        Stderr = ""
    },
    @{
        Name = "continue begins nearest loop iteration"
        Arguments = "tests/cases/continue_runtime.nt"
        ExitCode = 0
        Stdout = "1:3`nouter`n2:3`nouter`n"
        Stderr = ""
    },
    @{
        Name = "continue parse tree"
        Arguments = "--parse tests/cases/continue_parse.nt"
        ExitCode = 0
        Stdout = "PROGRAM`n  WHILE`n    CONDITION`n      TRUE true`n    BODY`n      CONTINUE`nEOF`n"
        Stderr = ""
    },
    @{
        Name = "lexer match columns"
        Arguments = "--tokens tests/cases/lexer_columns.nt"
        ExitCode = 0
        Stdout = "1:1 PRINT print`n1:7 NUMBER 1`n1:9 EQUAL_EQUAL ==`n1:12 NUMBER 2`n1:13 SEMICOLON ;`n1:14 NEWLINE`n2:1 PRINT print`n2:7 NUMBER 3`n2:9 BANG_EQUAL !=`n2:12 NUMBER 4`n2:13 SEMICOLON ;`n2:14 NEWLINE`n3:1 EOF`n"
        Stderr = ""
    },
    @{
        Name = "token mode lexer failure"
        Arguments = "--tokens tests/cases/bad_lexer.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_lexer.nt:1:1: lexer error: unknown character '@'`n"
    },
    @{
        Name = "parse mode parse failure"
        Arguments = "--parse tests/cases/bad_parse.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_parse.nt:1:5: parse error: expected identifier after let`n"
    },
    @{
        Name = "runtime type failure"
        Arguments = "tests/cases/bad_runtime.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_runtime.nt:1:17: runtime error: numeric operators require numbers`n"
    },
    @{
        Name = "break outside loop failure"
        Arguments = "tests/cases/bad_break.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_break.nt:1:1: runtime error: break can only be used inside a loop`n"
    },
    @{
        Name = "break cannot cross function boundary"
        Arguments = "tests/cases/bad_break_function.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_break_function.nt:2:5: runtime error: break can only be used inside a loop`n"
    },
    @{
        Name = "continue outside loop failure"
        Arguments = "tests/cases/bad_continue.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_continue.nt:1:1: runtime error: continue can only be used inside a loop`n"
    },
    @{
        Name = "continue cannot cross function boundary"
        Arguments = "tests/cases/bad_continue_function.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_continue_function.nt:2:5: runtime error: continue can only be used inside a loop`n"
    },
    @{
        Name = "functions read and assign top-level variables"
        Arguments = "tests/cases/lexical_globals.nt"
        ExitCode = 0
        Stdout = "count: 1`ncount: 2`n2`n3.14159`n"
        Stderr = ""
    },
    @{
        Name = "functions cannot see caller locals"
        Arguments = "tests/cases/bad_lexical_caller_local.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_lexical_caller_local.nt:2:11: runtime error: undefined variable 'secret'`n"
    },
    @{
        Name = "functions cannot see top-level block locals"
        Arguments = "tests/cases/bad_lexical_block_local.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_lexical_block_local.nt:2:11: runtime error: undefined variable 'hidden'`n"
    },
    @{
        Name = "parameters and locals shadow globals"
        Arguments = "tests/cases/lexical_shadowing.nt"
        ExitCode = 0
        Stdout = "parameter`nouter local`nglobal`n"
        Stderr = ""
    },
    @{
        Name = "missing input CLI failure"
        Arguments = ""
        ExitCode = 1
        Stdout = ""
        Stderr = "error: missing input file`nusage: newt <file.nt>`n"
    },
    @{
        Name = "file loading failure"
        Arguments = "tests/cases/does_not_exist.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "error: could not open tests/cases/does_not_exist.nt`n"
    },
    @{
        Name = "symbol logic operators"
        Arguments = "tests/cases/logic_symbols.nt"
        ExitCode = 0
        Stdout = "false`ntrue`nfalse`ntrue`n"
        Stderr = ""
    }
)

$Failures = 0
foreach ($Test in $Tests) {
    $StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $StartInfo.FileName = $Newt
    $StartInfo.Arguments = $Test.Arguments
    $StartInfo.WorkingDirectory = $Root
    $StartInfo.UseShellExecute = $false
    $StartInfo.RedirectStandardOutput = $true
    $StartInfo.RedirectStandardError = $true

    $Process = New-Object System.Diagnostics.Process
    $Process.StartInfo = $StartInfo
    [void]$Process.Start()
    $ActualStdout = Normalize-Newlines $Process.StandardOutput.ReadToEnd()
    $ActualStderr = Normalize-Newlines $Process.StandardError.ReadToEnd()
    $Process.WaitForExit()

    $Problems = @()
    if ($Process.ExitCode -ne $Test.ExitCode) {
        $Problems += "exit code: expected $($Test.ExitCode), got $($Process.ExitCode)"
    }
    if ($ActualStdout -cne $Test.Stdout) {
        $Problems += "stdout mismatch (expected '$($Test.Stdout -replace "`n", "\n")', got '$($ActualStdout -replace "`n", "\n")')"
    }
    if ($ActualStderr -cne $Test.Stderr) {
        $Problems += "stderr mismatch (expected '$($Test.Stderr -replace "`n", "\n")', got '$($ActualStderr -replace "`n", "\n")')"
    }

    if ($Problems.Count -eq 0) {
        Write-Host "PASS $($Test.Name)"
    } else {
        $Failures++
        Write-Host "FAIL $($Test.Name)"
        foreach ($Problem in $Problems) {
            Write-Host "  $Problem"
        }
    }
}

if ($Failures -ne 0) {
    Write-Host "$Failures regression test(s) failed."
    exit 1
}

Write-Host "All $($Tests.Count) regression tests passed."
