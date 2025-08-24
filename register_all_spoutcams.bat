@echo off
echo Register All SpoutCam Instances
echo ===============================
echo.
echo This script will register all 4 SpoutCam instances.
echo You need to run this as Administrator.
echo.
echo Press any key to continue or Ctrl+C to cancel...
pause >nul

cd /d "%~dp0"

REM Check if we have admin privileges
net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: This script must be run as Administrator!
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

echo.
echo Registering SpoutCam instances...
echo.

REM Register 64-bit versions
if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_64.ax" (
    echo Registering SpoutCam1 (64-bit)...
    regsvr32.exe /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_64.ax"
    if errorlevel 1 echo   ERROR: Failed to register SpoutCam1_64.ax
    echo   SpoutCam1_64.ax registered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_64.ax" (
    echo Registering SpoutCam2 (64-bit)...
    regsvr32.exe /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_64.ax"
    if errorlevel 1 echo   ERROR: Failed to register SpoutCam2_64.ax
    echo   SpoutCam2_64.ax registered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_64.ax" (
    echo Registering SpoutCam3 (64-bit)...
    regsvr32.exe /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_64.ax"
    if errorlevel 1 echo   ERROR: Failed to register SpoutCam3_64.ax
    echo   SpoutCam3_64.ax registered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_64.ax" (
    echo Registering SpoutCam4 (64-bit)...
    regsvr32.exe /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_64.ax"
    if errorlevel 1 echo   ERROR: Failed to register SpoutCam4_64.ax
    echo   SpoutCam4_64.ax registered
)

echo.
echo Registering 32-bit versions (if available)...

REM Register 32-bit versions
if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_32.ax" (
    echo Registering SpoutCam1 (32-bit)...
    regsvr32.exe /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_32.ax"
    if errorlevel 1 echo   ERROR: Failed to register SpoutCam1_32.ax
    echo   SpoutCam1_32.ax registered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_32.ax" (
    echo Registering SpoutCam2 (32-bit)...
    regsvr32.exe /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_32.ax"
    if errorlevel 1 echo   ERROR: Failed to register SpoutCam2_32.ax
    echo   SpoutCam2_32.ax registered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_32.ax" (
    echo Registering SpoutCam3 (32-bit)...
    regsvr32.exe /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_32.ax"
    if errorlevel 1 echo   ERROR: Failed to register SpoutCam3_32.ax
    echo   SpoutCam3_32.ax registered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_32.ax" (
    echo Registering SpoutCam4 (32-bit)...
    regsvr32.exe /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_32.ax"
    if errorlevel 1 echo   ERROR: Failed to register SpoutCam4_32.ax
    echo   SpoutCam4_32.ax registered
)

echo.
echo Registration complete!
echo.
echo You should now see SpoutCam1, SpoutCam2, SpoutCam3, and SpoutCam4 
echo in your video source list in applications like OBS, etc.
echo.
echo Use SpoutCamSettings.exe to configure each camera's settings.
echo.
pause