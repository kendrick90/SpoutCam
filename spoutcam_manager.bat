@echo off
title SpoutCam Manager
color 0F
cd /d "%~dp0"

REM Check for admin privileges
net session >nul 2>&1
if errorlevel 1 (
    echo.
    echo ========================================
    echo           ADMIN REQUIRED
    echo ========================================
    echo.
    echo This script requires Administrator privileges
    echo to register/unregister DirectShow filters.
    echo.
    echo Please:
    echo 1. Right-click on spoutcam_manager.bat
    echo 2. Select "Run as administrator"
    echo.
    echo Press any key to exit...
    pause >nul
    exit /b 1
)

:main_menu
cls
echo.
echo ========================================
echo           SPOUTCAM MANAGER
echo ========================================
echo.
echo Choose which SpoutCam to manage:
echo.
echo [1] SpoutCam1
echo [2] SpoutCam2  
echo [3] SpoutCam3
echo [4] SpoutCam4
echo.
echo [A] Register ALL cameras
echo [U] Unregister ALL cameras
echo [B] Build all 4 cameras
echo.
echo [Q] Quit
echo.
set /p choice="Enter your choice: "

if /i "%choice%"=="1" goto spoutcam1
if /i "%choice%"=="2" goto spoutcam2
if /i "%choice%"=="3" goto spoutcam3
if /i "%choice%"=="4" goto spoutcam4
if /i "%choice%"=="A" goto register_all
if /i "%choice%"=="U" goto unregister_all
if /i "%choice%"=="B" goto build_all
if /i "%choice%"=="Q" exit /b
echo Invalid choice. Press any key to try again...
pause >nul
goto main_menu

:spoutcam1
cls
echo.
echo ========================================
echo         SPOUTCAM1 MANAGEMENT
echo ========================================
echo.
echo [R] Register SpoutCam1
echo [U] Unregister SpoutCam1
echo [P] Properties/Settings
echo [S] Status (check if registered)
echo.
echo [M] Back to Main Menu
echo.
set /p subchoice="Enter your choice: "

if /i "%subchoice%"=="R" (
    echo.
    echo Registering SpoutCam1...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam1_register.bat
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam1
)
if /i "%subchoice%"=="U" (
    echo.
    echo Unregistering SpoutCam1...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam1_unregister.bat
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam1
)
if /i "%subchoice%"=="P" (
    echo.
    echo Opening SpoutCam1 Properties...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam1Properties.cmd
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam1
)
if /i "%subchoice%"=="S" (
    echo.
    echo Checking SpoutCam1 status...
    if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam1_64.ax" (
        echo ✓ SpoutCam1_64.ax file exists
        reg query "HKCR\CLSID\{8E14549A-DB61-4309-AFA1-3578E927E933}" >nul 2>&1
        if errorlevel 1 (
            echo ✗ SpoutCam1 not registered in system
        ) else (
            echo ✓ SpoutCam1 registered in system
        )
    ) else (
        echo ✗ SpoutCam1_64.ax file not found
    )
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam1
)
if /i "%subchoice%"=="M" goto main_menu
echo Invalid choice. Press any key to try again...
pause >nul
goto spoutcam1

:spoutcam2
cls
echo.
echo ========================================
echo         SPOUTCAM2 MANAGEMENT
echo ========================================
echo.
echo [R] Register SpoutCam2
echo [U] Unregister SpoutCam2
echo [P] Properties/Settings
echo [S] Status (check if registered)
echo.
echo [M] Back to Main Menu
echo.
set /p subchoice="Enter your choice: "

if /i "%subchoice%"=="R" (
    echo.
    echo Registering SpoutCam2...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam2_register.bat
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam2
)
if /i "%subchoice%"=="U" (
    echo.
    echo Unregistering SpoutCam2...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam2_unregister.bat
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam2
)
if /i "%subchoice%"=="P" (
    echo.
    echo Opening SpoutCam2 Properties...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam2Properties.cmd
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam2
)
if /i "%subchoice%"=="S" (
    echo.
    echo Checking SpoutCam2 status...
    if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam2_64.ax" (
        echo ✓ SpoutCam2_64.ax file exists
        reg query "HKCR\CLSID\{8E14549A-DB61-4309-AFA1-3578E927E934}" >nul 2>&1
        if errorlevel 1 (
            echo ✗ SpoutCam2 not registered in system
        ) else (
            echo ✓ SpoutCam2 registered in system
        )
    ) else (
        echo ✗ SpoutCam2_64.ax file not found
    )
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam2
)
if /i "%subchoice%"=="M" goto main_menu
echo Invalid choice. Press any key to try again...
pause >nul
goto spoutcam2

:spoutcam3
cls
echo.
echo ========================================
echo         SPOUTCAM3 MANAGEMENT
echo ========================================
echo.
echo [R] Register SpoutCam3
echo [U] Unregister SpoutCam3
echo [P] Properties/Settings
echo [S] Status (check if registered)
echo.
echo [M] Back to Main Menu
echo.
set /p subchoice="Enter your choice: "

if /i "%subchoice%"=="R" (
    echo.
    echo Registering SpoutCam3...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam3_register.bat
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam3
)
if /i "%subchoice%"=="U" (
    echo.
    echo Unregistering SpoutCam3...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam3_unregister.bat
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam3
)
if /i "%subchoice%"=="P" (
    echo.
    echo Opening SpoutCam3 Properties...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam3Properties.cmd
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam3
)
if /i "%subchoice%"=="S" (
    echo.
    echo Checking SpoutCam3 status...
    if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam3_64.ax" (
        echo ✓ SpoutCam3_64.ax file exists
        reg query "HKCR\CLSID\{8E14549A-DB61-4309-AFA1-3578E927E935}" >nul 2>&1
        if errorlevel 1 (
            echo ✗ SpoutCam3 not registered in system
        ) else (
            echo ✓ SpoutCam3 registered in system
        )
    ) else (
        echo ✗ SpoutCam3_64.ax file not found
    )
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam3
)
if /i "%subchoice%"=="M" goto main_menu
echo Invalid choice. Press any key to try again...
pause >nul
goto spoutcam3

:spoutcam4
cls
echo.
echo ========================================
echo         SPOUTCAM4 MANAGEMENT
echo ========================================
echo.
echo [R] Register SpoutCam4
echo [U] Unregister SpoutCam4
echo [P] Properties/Settings
echo [S] Status (check if registered)
echo.
echo [M] Back to Main Menu
echo.
set /p subchoice="Enter your choice: "

if /i "%subchoice%"=="R" (
    echo.
    echo Registering SpoutCam4...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam4_register.bat
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam4
)
if /i "%subchoice%"=="U" (
    echo.
    echo Unregistering SpoutCam4...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam4_unregister.bat
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam4
)
if /i "%subchoice%"=="P" (
    echo.
    echo Opening SpoutCam4 Properties...
    cd "binaries\MULTIPLE_SPOUTCAM"
    call SpoutCam4Properties.cmd
    cd ..\..
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam4
)
if /i "%subchoice%"=="S" (
    echo.
    echo Checking SpoutCam4 status...
    if exist "binaries\MULTIPLE_SPOUTCAM\SpoutCam4_64.ax" (
        echo ✓ SpoutCam4_64.ax file exists
        reg query "HKCR\CLSID\{8E14549A-DB61-4309-AFA1-3578E927E936}" >nul 2>&1
        if errorlevel 1 (
            echo ✗ SpoutCam4 not registered in system
        ) else (
            echo ✓ SpoutCam4 registered in system
        )
    ) else (
        echo ✗ SpoutCam4_64.ax file not found
    )
    echo.
    echo Press any key to continue...
    pause >nul
    goto spoutcam4
)
if /i "%subchoice%"=="M" goto main_menu
echo Invalid choice. Press any key to try again...
pause >nul
goto spoutcam4

:register_all
cls
echo.
echo ========================================
echo        REGISTER ALL CAMERAS
echo ========================================
echo.
echo This will register all 4 SpoutCam instances.
echo.
set /p confirm="Continue? (Y/N): "
if /i not "%confirm%"=="Y" goto main_menu

echo.
echo Registering all SpoutCam instances...
cd "binaries\MULTIPLE_SPOUTCAM"
call register_all.bat
cd ..\..
echo.
echo Press any key to return to main menu...
pause >nul
goto main_menu

:unregister_all
cls
echo.
echo ========================================
echo       UNREGISTER ALL CAMERAS
echo ========================================
echo.
echo This will unregister all 4 SpoutCam instances.
echo.
set /p confirm="Continue? (Y/N): "
if /i not "%confirm%"=="Y" goto main_menu

echo.
echo Unregistering all SpoutCam instances...
cd "binaries\MULTIPLE_SPOUTCAM"
call unregister_all.bat
cd ..\..
echo.
echo Press any key to return to main menu...
pause >nul
goto main_menu

:build_all
cls
echo.
echo ========================================
echo         BUILD ALL CAMERAS
echo ========================================
echo.
echo This will build all 4 SpoutCam instances.
echo This may take several minutes...
echo.
set /p confirm="Continue? (Y/N): "
if /i not "%confirm%"=="Y" goto main_menu

echo.
echo Building all SpoutCam instances...
call build_all_four.bat
echo.
echo Press any key to return to main menu...
pause >nul
goto main_menu