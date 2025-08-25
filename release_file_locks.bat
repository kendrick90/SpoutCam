@echo off
REM SpoutCam File Lock Release Script (Batch wrapper)
REM This provides an easy double-click option

echo === SpoutCam File Lock Release ===
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorlevel% NEQ 0 (
    echo WARNING: Not running as Administrator
    echo Some lock release methods may not work
    echo.
    set /p choice="Continue anyway? (y/N): "
    if /i NOT "%choice%"=="y" goto :end
)

REM Run the PowerShell script
echo Running file lock release...
powershell.exe -ExecutionPolicy Bypass -File "%~dp0release_file_locks.ps1" -Force

echo.
echo Done! You can now run build.bat
echo.

:end
pause