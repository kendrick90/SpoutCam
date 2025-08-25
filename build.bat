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

echo Building both x64 and x86

:BUILD_BOTH
call :BuildArch x64
if errorlevel 1 goto FAILED
call :BuildArch x86
if errorlevel 1 goto FAILED
goto END

:BUILD_X64
call :BuildArch x64
if errorlevel 1 goto FAILED
goto END

:BUILD_X86
call :BuildArch x86
if errorlevel 1 goto FAILED
goto END

:BuildArch
set "ARCH=%1"
if "%ARCH%"=="x64" (
    set "BINARIES_AX=binaries\SPOUTCAM\SpoutCam64\SpoutCam64.ax"
    set "DIR_ARCH=64"
    set "PLATFORM=x64"
) else (
    set "BINARIES_AX=binaries\SPOUTCAM\SpoutCam32\SpoutCam32.ax"
    set "DIR_ARCH=32"
    set "PLATFORM=x86"
)

echo.
echo ===============================================
echo Building %ARCH% version
echo ===============================================

echo Step 1: Unregistering existing SpoutCam filters
echo - Clearing registrations to prevent file locks

call :UnregisterSpoutCam

echo.
echo Step 2: Building %ARCH% solution
echo - MSBuild Command: "%MSBUILD_PATH%" SpoutCamDX.sln -p:Configuration=Release -p:Platform=%PLATFORM% -verbosity:minimal
echo - Expected output: %BINARIES_AX%
echo - Checking if output directory exists: binaries\SPOUTCAM\SpoutCam%DIR_ARCH%\
if not exist "binaries\SPOUTCAM\SpoutCam%DIR_ARCH%\" (
    echo - Creating output directory: binaries\SPOUTCAM\SpoutCam%DIR_ARCH%\
    mkdir "binaries\SPOUTCAM\SpoutCam%DIR_ARCH%\"
)

echo - Starting build
"%MSBUILD_PATH%" SpoutCamDX.sln -p:Configuration=Release -p:Platform=%PLATFORM% -verbosity:minimal

echo - DEBUG: MSBuild completed, checking error level
set BUILD_RESULT=%ERRORLEVEL%
echo - DEBUG: Build result is: %BUILD_RESULT%

echo - DEBUG: About to check if build failed
if "%BUILD_RESULT%" neq "0" (
    echo.
    echo *** %ARCH% BUILD FAILED Error Level: %BUILD_RESULT% ***
    exit /b 1
)
echo - DEBUG: Build check passed, continuing

echo.
echo Step 3: Verifying build output
echo - DEBUG: Checking for output file: %BINARIES_AX%
echo - DEBUG: ARCH variable is: %ARCH%
echo - DEBUG: DIR_ARCH variable is: %DIR_ARCH%

if exist "%BINARIES_AX%" (
    echo - SUCCESS: File found at %BINARIES_AX%
    echo - DEBUG: About to call ShowFileInfo function
    for %%i in ("%BINARIES_AX%") do (
        echo - File size: %%~zi bytes, Modified: %%~ti
    )
    echo - DEBUG: ShowFileInfo completed
) else (
    echo - ERROR: Expected output file NOT FOUND: %BINARIES_AX%
    echo - Listing contents of output directory:
    if exist "binaries\SPOUTCAM\SpoutCam%DIR_ARCH%\" (
        dir "binaries\SPOUTCAM\SpoutCam%DIR_ARCH%\" /B
    ) else (
        echo - Output directory does not exist!
    )
)

echo.
echo Step 4: Verifying SpoutCamSettings.exe build
if exist "binaries\SPOUTCAM\SpoutCamSettings.exe" (
    echo - SUCCESS: SpoutCamSettings.exe built successfully
    for %%i in ("binaries\SPOUTCAM\SpoutCamSettings.exe") do (
        echo - File size: %%~zi bytes, Modified: %%~ti
    )
) else (
    echo - WARNING: SpoutCamSettings.exe not found - check post-build step
)

echo.
echo Step 5: Build completed successfully!
echo - Built: %BINARIES_AX%
echo - Settings: binaries\SPOUTCAM\SpoutCamSettings.exe
echo.
echo NOTE: SpoutCam is NOT automatically registered after build.
echo To register SpoutCam for use:
echo   1. Run SpoutCamProperties.cmd, or
echo   2. Use the registration batch files in the binaries folder
echo   3. Register individual cameras through the properties dialog

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
echo Build process completed! Press any key to exit
pause >nul
exit /b 0

:FAILED
echo.
echo ===============================================
echo BUILD FAILED!
echo ===============================================
echo.
echo One or more builds failed. Please check the error messages above.
echo.
echo Build process failed! Press any key to exit
pause >nul
exit /b 1

:ShowFileInfo
for %%i in (%1) do (
    echo - File size: %%~zi bytes, Modified: %%~ti
)
goto :eof

:UnregisterSpoutCam
echo - Checking if SpoutCam filters are registered

:: Check if any SpoutCam filters are registered
powershell -Command "if (Get-Process | Where-Object {$_.ProcessName -match 'regsvr32'}) { Stop-Process -Name regsvr32 -Force -ErrorAction SilentlyContinue }" 2>nul

:: Check registry for SpoutCam entries
powershell -Command "$found = $false; Get-ChildItem 'HKLM:\SOFTWARE\Classes\CLSID' -ErrorAction SilentlyContinue | ForEach-Object { $default = Get-ItemProperty $_.PSPath -Name '(default)' -ErrorAction SilentlyContinue; if ($default.'(default)' -match 'SpoutCam') { Write-Host 'Found CLSID:' $_.PSChildName '=' $default.'(default)'; $found = $true } }; Get-ChildItem 'HKLM:\SOFTWARE\Classes\CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance' -ErrorAction SilentlyContinue | ForEach-Object { $friendlyName = Get-ItemProperty $_.PSPath -Name 'FriendlyName' -ErrorAction SilentlyContinue; if ($friendlyName.FriendlyName -match 'SpoutCam') { Write-Host 'Found DirectShow filter:' $friendlyName.FriendlyName; $found = $true } }; if ($found) { Write-Host '- SpoutCam filters ARE registered - unregistration required'; Write-Host 'CAMERAS_FOUND' } else { Write-Host '- No SpoutCam filters found in registry'; Write-Host 'NO_CAMERAS' }" 2>nul | findstr "CAMERAS_FOUND" >nul

if %ERRORLEVEL% equ 0 (
    echo - Attempting unregistration with ADMIN privileges UAC prompt will appear
) else (
    echo - No unregistration needed - skipping to build
    goto :SkipUnregistration
)

:: Use the same cleanup method as the UI cleanup button (works reliably)
if "%ARCH%"=="x64" (
    set "SETTINGS_EXE=binaries\SPOUTCAM\SpoutCamSettings64.exe"
) else (
    set "SETTINGS_EXE=binaries\SPOUTCAM\SpoutCamSettings32.exe"
)

if exist "%SETTINGS_EXE%" (
    echo   Using SpoutCamSettings cleanup (same method as UI cleanup button)
    echo   (UAC prompt will appear - please click Yes to continue)
    powershell -Command "Start-Process \"%SETTINGS_EXE%\" -ArgumentList '--cleanup-all' -Verb RunAs -Wait" 2>nul
    echo   - SpoutCamSettings cleanup completed
) else (
    echo   WARNING: %SETTINGS_EXE% not found, using fallback method
    :: Fallback: Call SpoutCamSettings directly if available
    if exist "binaries\SPOUTCAM\SpoutCamSettings.exe" (
        echo   Using fallback SpoutCamSettings.exe
        powershell -Command "Start-Process 'binaries\SPOUTCAM\SpoutCamSettings.exe' -ArgumentList '--cleanup-all' -Verb RunAs -Wait" 2>nul
        echo   - Fallback cleanup completed
    ) else (
        echo   ERROR: No SpoutCamSettings executable found for cleanup
        echo   Please manually run cleanup from SpoutCamSettings UI
    )
)

echo - Killing any remaining SpoutCamSettings processes
powershell -Command "Get-Process -Name '*SpoutCam*' -ErrorAction SilentlyContinue | ForEach-Object { Write-Host 'Terminating process:' $_.Name 'PID:' $_.Id; Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }" 2>nul
timeout /t 2 /nobreak >nul
echo - Verifying unregistration was successful
powershell -Command "$found = $false; Get-ChildItem 'HKLM:\SOFTWARE\Classes\CLSID' -ErrorAction SilentlyContinue | ForEach-Object { $default = Get-ItemProperty $_.PSPath -Name '(default)' -ErrorAction SilentlyContinue; if ($default.'(default)' -match 'SpoutCam') { $found = $true } }; Get-ChildItem 'HKLM:\SOFTWARE\Classes\CLSID\{083863F1-70DE-11D0-BD40-00A0C911CE86}\Instance' -ErrorAction SilentlyContinue | ForEach-Object { $friendlyName = Get-ItemProperty $_.PSPath -Name 'FriendlyName' -ErrorAction SilentlyContinue; if ($friendlyName.FriendlyName -match 'SpoutCam') { $found = $true } }; if ($found) { Write-Host '- ERROR: SpoutCam filters are STILL registered!'; Write-Host '- Build cannot continue safely - please manually unregister using SpoutCamProperties'; exit 1 } else { Write-Host '- SUCCESS: All SpoutCam filters have been unregistered'; exit 0 }" 2>nul

if %ERRORLEVEL% neq 0 (
    echo.
    echo *** BUILD STOPPED ***
    echo SpoutCam filters are still registered and may cause file locks.
    echo Please manually unregister using SpoutCamProperties.cmd, then run build again.
    echo.
    pause
    exit /b 1
)

:SkipUnregistration

echo - Testing if target file is ready for build
if exist "%BINARIES_AX%" (
    copy "%BINARIES_AX%" "%BINARIES_AX%.test" >nul 2>&1
    if exist "%BINARIES_AX%.test" (
        del "%BINARIES_AX%.test" >nul 2>&1
        echo - File is ready for build
    ) else (
        echo - WARNING: File may still be locked
    )
) else (
    echo - No existing file found (normal for fresh builds)
)

echo - Unregistration verification completed - build can proceed safely
goto :eof