@echo off
cd /d "%~dp0"
runner.exe
if errorlevel 1 echo Benchmark stopped or failed. Please send the generated result ZIP if present.
pause
