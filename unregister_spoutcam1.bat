@echo off
cd /d "%~dp0"

echo Unregistering SpoutCam1...
echo ==========================

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_64.ax" (
    echo Unregistering SpoutCam1 (64-bit)...
    regsvr32.exe /u "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_64.ax"
)

if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_32.ax" (
    echo Unregistering SpoutCam1 (32-bit)...
    regsvr32.exe /u "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_32.ax"
)

echo SpoutCam1 unregistration complete!
pause