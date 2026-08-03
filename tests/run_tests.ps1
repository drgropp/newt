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
        Name = "text conversions"
        Arguments = "tests/cases/text_values.nt"
        ExitCode = 0
        Stdout = "42`n3.5`ntrue`nfalse`nnewt`nvalue=7`n"
        Stderr = ""
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
