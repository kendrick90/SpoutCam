cd /d "%~dp0"

echo Unregistering All SpoutCam Instances...
echo ======================================

call SpoutCam1_unregister.bat
call SpoutCam2_unregister.bat
call SpoutCam3_unregister.bat
call SpoutCam4_unregister.bat

echo.
echo All SpoutCam instances unregistered!
pause