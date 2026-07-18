@echo off

echo Building Newt...
gcc -std=c11 -Wall -Wextra -pedantic -o newt.exe src/main.c
if errorlevel 1 exit /b 1

echo.
echo Running examples...

echo [sqrt_test.nt]
.\newt.exe --run examples/sqrt_test.nt
if errorlevel 1 exit /b 1

echo [and_test.nt]
.\newt.exe --run examples/and_test.nt
if errorlevel 1 exit /b 1

echo [or_test.nt]
.\newt.exe --run examples/or_test.nt
if errorlevel 1 exit /b 1

echo [not_test.nt]
.\newt.exe --run examples/not_test.nt
if errorlevel 1 exit /b 1

echo [unary_minus_test.nt]
.\newt.exe --run examples/unary_minus_test.nt
if errorlevel 1 exit /b 1

echo [bool_test.nt]
.\newt.exe --run examples/bool_test.nt
if errorlevel 1 exit /b 1

echo [while_test.nt]
.\newt.exe --run examples/while_test.nt
if errorlevel 1 exit /b 1

echo [if_test.nt]
.\newt.exe --run examples/if_test.nt
if errorlevel 1 exit /b 1

echo [else_if_test.nt]
.\newt.exe --run examples/else_if_test.nt
if errorlevel 1 exit /b 1

echo [function_test.nt]
.\newt.exe --run examples/function_test.nt
if errorlevel 1 exit /b 1

echo [function_params_test.nt]
.\newt.exe --run examples/function_params_test.nt
if errorlevel 1 exit /b 1

echo [return_test.nt]
.\newt.exe --run examples/return_test.nt
if errorlevel 1 exit /b 1

echo [calculator.nt]
.\newt.exe --run examples/calculator.nt
if errorlevel 1 exit /b 1

echo.
echo All Newt examples passed.
