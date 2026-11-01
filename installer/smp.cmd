@echo off
rem Launcher installed by the Swag Music Player setup.
rem Opens the player on the folder passed as the first argument, or on the
rem Music folder. The window stays open when the player exits with an error so
rem the message is readable.

setlocal
set "FOLDER=%~1"
if "%FOLDER%"=="" set "FOLDER=%USERPROFILE%\Music"
if not exist "%FOLDER%" set "FOLDER=%USERPROFILE%"

"%~dp0smp.exe" "%FOLDER%"
if errorlevel 1 pause
endlocal
