@echo off
echo Unregister All SpoutCam Instances
echo =================================
echo.
echo This script will unregister all 4 SpoutCam instances.
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
echo Unregistering SpoutCam instances...
echo.

REM Unregister 64-bit versions
if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_64.ax" (
    echo Unregistering SpoutCam1 (64-bit)...
    regsvr32.exe /u /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_64.ax"
    echo   SpoutCam1_64.ax unregistered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_64.ax" (
    echo Unregistering SpoutCam2 (64-bit)...
    regsvr32.exe /u /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_64.ax"
    echo   SpoutCam2_64.ax unregistered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_64.ax" (
    echo Unregistering SpoutCam3 (64-bit)...
    regsvr32.exe /u /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_64.ax"
    echo   SpoutCam3_64.ax unregistered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_64.ax" (
    echo Unregistering SpoutCam4 (64-bit)...
    regsvr32.exe /u /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_64.ax"
    echo   SpoutCam4_64.ax unregistered
)

echo.
echo Unregistering 32-bit versions (if available)...

REM Unregister 32-bit versions
if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_32.ax" (
    echo Unregistering SpoutCam1 (32-bit)...
    regsvr32.exe /u /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_32.ax"
    echo   SpoutCam1_32.ax unregistered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_32.ax" (
    echo Unregistering SpoutCam2 (32-bit)...
    regsvr32.exe /u /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_32.ax"
    echo   SpoutCam2_32.ax unregistered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_32.ax" (
    echo Unregistering SpoutCam3 (32-bit)...
    regsvr32.exe /u /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_32.ax"
    echo   SpoutCam3_32.ax unregistered
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_32.ax" (
    echo Unregistering SpoutCam4 (32-bit)...
    regsvr32.exe /u /s "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_32.ax"
    echo   SpoutCam4_32.ax unregistered
)

echo.
echo Unregistration complete!
echo.
echo All SpoutCam instances have been removed from the system.
echo.
pause