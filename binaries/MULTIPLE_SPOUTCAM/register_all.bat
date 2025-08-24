cd /d "%~dp0"

echo Registering All SpoutCam Instances...
echo ====================================

call SpoutCam1_register.bat
call SpoutCam2_register.bat
call SpoutCam3_register.bat
call SpoutCam4_register.bat

echo.
echo All SpoutCam instances registered!
pause