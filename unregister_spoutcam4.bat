@echo off
cd /d "%~dp0"

echo Unregistering SpoutCam4...
echo ==========================

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_64.ax" (
    echo Unregistering SpoutCam4 (64-bit)...
    regsvr32.exe /u "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_64.ax"
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_32.ax" (
    echo Unregistering SpoutCam4 (32-bit)...
    regsvr32.exe /u "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_32.ax"
)

echo SpoutCam4 unregistration complete!
pause