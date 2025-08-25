@echo off
echo Setting up Visual Studio environment...

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

echo Compiling registration test...

cl.exe /EHsc test_registration.cpp /Fe:test_registration.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Running test...
    echo.
    test_registration.exe
) else (
    echo Compilation failed!
    pause
)

echo.
echo Cleaning up...
del test_registration.obj 2>nul
del test_registration.exe 2>nul

pause