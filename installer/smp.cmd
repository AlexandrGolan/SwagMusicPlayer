@echo off
rem Launcher installed by the Swag Music Player setup.
rem Without arguments SMP shows a folder chooser, so the shortcut just starts
rem the player. Passing a path still works for people who edit the shortcut.

setlocal
"%~dp0smp.exe" %*
if errorlevel 1 pause
endlocal
