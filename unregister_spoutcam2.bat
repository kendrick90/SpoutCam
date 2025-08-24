@echo off
cd /d "%~dp0"

echo Unregistering SpoutCam2...
echo ==========================

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_64.ax" (
    echo Unregistering SpoutCam2 (64-bit)...
    regsvr32.exe /u "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_64.ax"
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_32.ax" (
    echo Unregistering SpoutCam2 (32-bit)...
    regsvr32.exe /u "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_32.ax"
)

echo SpoutCam2 unregistration complete!
pause