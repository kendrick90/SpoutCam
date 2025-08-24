@echo off

REM Check if already running as admin
net session >nul 2>&1
if not errorlevel 1 (
    REM Already admin, run the main script
    call spoutcam_manager.bat
    exit /b
)

REM Not admin, request elevation
echo Requesting Administrator privileges...
powershell -Command "Start-Process '%~dp0spoutcam_manager.bat' -Verb RunAs"
exit /b