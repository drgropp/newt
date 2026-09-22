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
        Stderr = "tests/cases/bad_pi_assignment.nt:1:1: runtime error: cannot assign to immutable val 'pi'`n"
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
        Name = "indexed assignment rejects immutable val"
        Arguments = "tests/cases/bad_list_index_assignment_immutable.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_index_assignment_immutable.nt:2:1: runtime error: cannot mutate immutable val 'items'`n"
    },
    @{
        Name = "append rejects immutable val"
        Arguments = "tests/cases/bad_list_append_immutable.nt"
        ExitCode = 1
        Stdout = ""
        Stderr = "tests/cases/bad_list_append_immutable.nt:2:8: runtime error: cannot mutate immutable val 'items'`n"
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
        Stdout = "1:1 PRINT print`n1:7 NUMBER 1`n1:9 EQUAL_EQUAL ==`n1:12 NUMBER 2`n1:13 NEWLINE`n2:1 PRINT print`n2:7 NUMBER 3`n2:9 BANG_EQUAL !=`n2:12 NUMBER 4`n2:13 NEWLINE`n3:1 EOF`n"
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
        Stderr = "tests/cases/bad_parse.nt:1:5: parse error: expected identifier after val`n"
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
