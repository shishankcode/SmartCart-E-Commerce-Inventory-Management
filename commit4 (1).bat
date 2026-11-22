@echo off
setlocal

set "MYSQL_DIR=C:\Program Files\MySQL\MySQL Server 8.0"

if not exist "%MYSQL_DIR%\bin\mysql.exe" (
    echo MySQL client not found at "%MYSQL_DIR%\bin\mysql.exe".
    echo Install MySQL Server 8.0 or edit MYSQL_DIR in setup.bat.
    pause
    exit /b 1
)

cd /d "%~dp0"

echo Creating the "shopease" database and the "shopease_user" account.
echo Enter your MySQL root password when prompted.
echo.

"%MYSQL_DIR%\bin\mysql.exe" -u root -p < sql\create_database.sql

if errorlevel 1 (
    echo.
    echo Setup failed. Make sure the MySQL80 service is running.
    pause
    exit /b 1
)

echo.
echo Database setup complete. Next: run.bat
pause
