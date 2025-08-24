@echo off
cd /d "%~dp0"

echo Unregistering SpoutCam3...
echo ==========================

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_64.ax" (
    echo Unregistering SpoutCam3 (64-bit)...
    regsvr32.exe /u "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_64.ax"
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_32.ax" (
    echo Unregistering SpoutCam3 (32-bit)...
    regsvr32.exe /u "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_32.ax"
)

echo SpoutCam3 unregistration complete!
pause