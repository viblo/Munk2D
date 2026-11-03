@echo off
setlocal
cd /d "%~dp0"
echo Munk2D native Windows AVX2 comparison
echo.
echo Plug into AC power and close CPU-heavy applications.
echo F = full test (26 scenes, 7 paired rounds; roughly 10-30 minutes)
echo Q = quick screen (8 scenes, 3 paired rounds; roughly 2-5 minutes)
echo Full is recommended for judging a small performance difference.
echo.
if /I "%~1"=="quick" goto quick
if /I "%~1"=="full" goto full
choice /C FQ /N /T 20 /D F /M "Choose F or Q (defaults to full after 20 seconds): "
if errorlevel 2 goto quick
:full
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run.ps1" -Mode Full
goto done
:quick
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run.ps1" -Mode Quick
:done
set CODE=%ERRORLEVEL%
echo.
echo The script printed the results ZIP to send back. See the results folder.
pause
exit /B %CODE%
