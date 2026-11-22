@echo off
cd /d "%~dp0"
if not exist shopease-server.exe (
    echo shopease-server.exe is missing. Run build.bat first.
    pause
    exit /b 1
)
shopease-server.exe
pause
