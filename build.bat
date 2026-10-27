@echo off
gcc -std=c11 -Wall -Wextra -pedantic -o newt.exe src/main.c src/lexer.c src/parser.c src/interpreter.c src/builtins.c src/numeric.c -lm
if errorlevel 1 exit /b 1
