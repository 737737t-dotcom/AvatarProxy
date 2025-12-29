@echo off
cd /d "%~dp0"

if not exist "build" mkdir build

cd build
cmake ..
make

if %errorlevel% equ 0 (
    avatar_proxy.exe
) else (
    echo Build failed!
    pause
    exit /b 1
)
