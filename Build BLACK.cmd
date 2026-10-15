@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\setup.ps1"
if errorlevel 1 (
    echo.
    echo Build failed. Read the error above and README.md for prerequisites.
    pause
    exit /b 1
)
echo.
echo Done. Open BLACK Launcher.exe to set up your disc, change settings and play.
pause
