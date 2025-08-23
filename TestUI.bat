@echo off
echo Testing SpoutCam Properties Dialog
echo.
echo This will attempt to show the SpoutCam configuration dialog
echo.
cd /d "%~dp0\x64\Release"

REM Try to show properties using rundll32
echo Attempting to show properties dialog...
rundll32.exe SpoutCam64.ax,Configure

echo.
echo If dialog didn't appear, try these methods:
echo 1. Open OBS Studio and add Video Capture Device
echo 2. Select SpoutCam and click "Configure Video"
echo 3. Use GraphStudioNext (DirectShow editor)
echo.
pause