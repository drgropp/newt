@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tests\run_tests.ps1"
exit /b %ERRORLEVEL%
