@echo off
setlocal

echo.
echo ===============================================
echo SpoutCam Build Script
echo ===============================================
echo.

:: Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

:: Define paths
set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

:: Ask which architecture to build
echo Select build configuration:
echo 1. Build x64 only
echo 2. Build x86 only  
echo 3. Build both x64 and x86
set /p CHOICE="Enter choice (1-3): "

if "%CHOICE%"=="1" goto BUILD_X64
if "%CHOICE%"=="2" goto BUILD_X86
if "%CHOICE%"=="3" goto BUILD_BOTH
echo Invalid choice. Building x64 by default.
goto BUILD_X64

:BUILD_BOTH
call :BuildArch x64
call :BuildArch x86
goto END

:BUILD_X64
call :BuildArch x64
goto END

:BUILD_X86
call :BuildArch x86
goto END

:BuildArch
set "ARCH=%1"
if "%ARCH%"=="x64" (
    set "TARGET_AX=x64\Release\SpoutCam64.ax"
    set "SOURCE_AX=x64\Release\SpoutCam.ax"
    set "BINARIES_AX=binaries\SPOUTCAM\SpoutCam64\SpoutCam64.ax"
    set "PLATFORM=x64"
) else (
    set "TARGET_AX=Win32\Release\SpoutCam32.ax"
    set "SOURCE_AX=Win32\Release\SpoutCam.ax"
    set "BINARIES_AX=binaries\SPOUTCAM\SpoutCam32\SpoutCam32.ax"
    set "PLATFORM=x86"
)

echo.
echo ===============================================
echo Building %ARCH% version
echo ===============================================

echo Step 1: Unregistering ALL existing SpoutCam filters...
echo - Unregistering from binaries folder...
if exist "%BINARIES_AX%" (
    powershell -Command "Start-Process regsvr32 -ArgumentList '/s /u \"%BINARIES_AX%\"' -Verb RunAs -Wait" 2>nul
    echo - Unregistered %BINARIES_AX%
)
echo - Unregistering from build folders...
if exist "%TARGET_AX%" (
    powershell -Command "Start-Process regsvr32 -ArgumentList '/s /u \"%TARGET_AX%\"' -Verb RunAs -Wait" 2>nul
    echo - Unregistered %TARGET_AX%
)
if exist "%SOURCE_AX%" (
    powershell -Command "Start-Process regsvr32 -ArgumentList '/s /u \"%SOURCE_AX%\"' -Verb RunAs -Wait" 2>nul
    echo - Unregistered %SOURCE_AX%
)

echo.
echo Step 2: Building %ARCH% solution...
"%MSBUILD_PATH%" SpoutCamDX.sln -p:Configuration=Release -p:Platform=%PLATFORM% -verbosity:minimal

if %ERRORLEVEL% neq 0 (
    echo.
    echo *** %ARCH% BUILD FAILED ***
    goto :eof
)

echo.
echo Step 3: Copying %ARCH% files (should work now)...
if exist "%SOURCE_AX%" (
    copy /y "%SOURCE_AX%" "%TARGET_AX%" >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        echo - Successfully copied to %TARGET_AX%
    ) else (
        echo - Warning: Copy failed, but build succeeded
    )
)

echo.
echo Step 4: Re-registering SpoutCam %ARCH% (optional)...
set /p REGISTER="Register SpoutCam %ARCH% now? (y/n): "
if /i "%REGISTER%"=="y" (
    if exist "%TARGET_AX%" (
        regsvr32 /s "%TARGET_AX%"
        echo - SpoutCam %ARCH% registered successfully
    ) else (
        echo - Warning: %TARGET_AX% not found
    )
) else (
    echo - Skipped registration
)

echo %ARCH% build completed successfully!
goto :eof

:END
echo.
echo ===============================================
echo All builds completed successfully!
echo ===============================================
echo.
echo To test: Use SpoutCamProperties.cmd or register individual cameras
echo through the properties dialog.
echo.
pause