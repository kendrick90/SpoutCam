@echo off
cd /d "%~dp0"

echo Building All 4 SpoutCam Instances...
echo ===================================

REM Create output directory
if not exist "binaries\MULTIPLE_SPOUTCAM" mkdir "binaries\MULTIPLE_SPOUTCAM"

REM Backup original files
copy "source\cam.h" "source\cam.h.orig" >nul 2>&1
copy "source\dll.cpp" "source\dll.cpp.orig" >nul 2>&1

REM Build SpoutCam1
echo.
echo [1/4] Building SpoutCam1...
powershell -Command "(Get-Content 'source\cam.h.orig') -replace '#define SPOUTCAMNAME \"SpoutCam\"', '#define SPOUTCAMNAME \"SpoutCam1\"' | Set-Content 'source\cam.h'"
powershell -Command "(Get-Content 'source\dll.cpp.orig') -replace '0xe9, 0x33\\);', '0xe9, 0x33);' | Set-Content 'source\dll.cpp'"

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SpoutCamDX.sln -p:Configuration=Release -p:Platform=x64 -verbosity:minimal
if exist "x64\Release\SpoutCam.ax" copy "x64\Release\SpoutCam.ax" "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_64.ax" >nul

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SpoutCamDX.sln -p:Configuration=Release -p:Platform=x86 -verbosity:minimal
if exist "Win32\Release\SpoutCam.ax" copy "Win32\Release\SpoutCam.ax" "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_32.ax" >nul

echo SpoutCam1 complete!

REM Build SpoutCam2
echo.
echo [2/4] Building SpoutCam2...
powershell -Command "(Get-Content 'source\cam.h.orig') -replace '#define SPOUTCAMNAME \"SpoutCam\"', '#define SPOUTCAMNAME \"SpoutCam2\"' | Set-Content 'source\cam.h'"
powershell -Command "(Get-Content 'source\dll.cpp.orig') -replace '0xe9, 0x33\\);', '0xe9, 0x34);' | Set-Content 'source\dll.cpp'"

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SpoutCamDX.sln -p:Configuration=Release -p:Platform=x64 -verbosity:minimal
if exist "x64\Release\SpoutCam.ax" copy "x64\Release\SpoutCam.ax" "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_64.ax" >nul

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SpoutCamDX.sln -p:Configuration=Release -p:Platform=x86 -verbosity:minimal
if exist "Win32\Release\SpoutCam.ax" copy "Win32\Release\SpoutCam.ax" "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_32.ax" >nul

echo SpoutCam2 complete!

REM Build SpoutCam3
echo.
echo [3/4] Building SpoutCam3...
powershell -Command "(Get-Content 'source\cam.h.orig') -replace '#define SPOUTCAMNAME \"SpoutCam\"', '#define SPOUTCAMNAME \"SpoutCam3\"' | Set-Content 'source\cam.h'"
powershell -Command "(Get-Content 'source\dll.cpp.orig') -replace '0xe9, 0x33\\);', '0xe9, 0x35);' | Set-Content 'source\dll.cpp'"

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SpoutCamDX.sln -p:Configuration=Release -p:Platform=x64 -verbosity:minimal
if exist "x64\Release\SpoutCam.ax" copy "x64\Release\SpoutCam.ax" "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_64.ax" >nul

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SpoutCamDX.sln -p:Configuration=Release -p:Platform=x86 -verbosity:minimal
if exist "Win32\Release\SpoutCam.ax" copy "Win32\Release\SpoutCam.ax" "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_32.ax" >nul

echo SpoutCam3 complete!

REM Build SpoutCam4
echo.
echo [4/4] Building SpoutCam4...
powershell -Command "(Get-Content 'source\cam.h.orig') -replace '#define SPOUTCAMNAME \"SpoutCam\"', '#define SPOUTCAMNAME \"SpoutCam4\"' | Set-Content 'source\cam.h'"
powershell -Command "(Get-Content 'source\dll.cpp.orig') -replace '0xe9, 0x33\\);', '0xe9, 0x36);' | Set-Content 'source\dll.cpp'"

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SpoutCamDX.sln -p:Configuration=Release -p:Platform=x64 -verbosity:minimal
if exist "x64\Release\SpoutCam.ax" copy "x64\Release\SpoutCam.ax" "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_64.ax" >nul

"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" SpoutCamDX.sln -p:Configuration=Release -p:Platform=x86 -verbosity:minimal
if exist "Win32\Release\SpoutCam.ax" copy "Win32\Release\SpoutCam.ax" "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_32.ax" >nul

echo SpoutCam4 complete!

REM Restore original files
echo.
echo Restoring original source files...
copy "source\cam.h.orig" "source\cam.h" >nul
copy "source\dll.cpp.orig" "source\dll.cpp" >nul
del "source\cam.h.orig"
del "source\dll.cpp.orig"

echo.
echo ===================================
echo All 4 SpoutCam instances built successfully!
echo.
echo Output files:
dir "binaries\MULTIPLE_SPOUTCAM\*.ax" /b

echo.
echo Ready to register with: register_all_spoutcams.bat