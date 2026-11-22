@echo off
setlocal

set "MYSQL_DIR=C:\Program Files\MySQL\MySQL Server 8.0"
set "GXX=C:\msys64\ucrt64\bin\g++.exe"
set "PATH=C:\msys64\ucrt64\bin;%PATH%"

if not exist "%GXX%" (
    echo g++ not found at "%GXX%".
    echo Install MSYS2 with the ucrt64 toolchain or edit GXX in build.bat.
    exit /b 1
)

"%GXX%" -std=c++17 -O2 -Wall -Wextra ^
    src\json.cpp src\crypto.cpp src\config.cpp src\db.cpp ^
    src\store.cpp src\http.cpp src\api.cpp src\main.cpp ^
    -I"%MYSQL_DIR%\include" ^
    -L"%MYSQL_DIR%\lib" -lmysql -lws2_32 ^
    -o shopease-server.exe

if errorlevel 1 (
    echo.
    echo Build failed.
    exit /b 1
)

copy /Y "%MYSQL_DIR%\lib\libmysql.dll" . >nul
echo.
echo Build successful: backend\shopease-server.exe
