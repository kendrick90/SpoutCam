@echo off
cd /d "%~dp0"
if "%1"=="" (
    START C:\Windows\System32\rundll32.exe SpoutCam64.ax,Configure
) else (
    echo Camera index: %1 > camera_index.tmp
    START C:\Windows\System32\rundll32.exe SpoutCam64.ax,ConfigureSpoutCameraFromFile
)
