@echo off
cd /d "%~dp0"
if "%1"=="" (
    START C:\Windows\System32\rundll32.exe SpoutCam32.ax,Configure
) else (
    START C:\Windows\System32\rundll32.exe SpoutCam32.ax,ConfigureSpoutCameraFromFile
)
