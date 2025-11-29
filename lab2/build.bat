@echo off
setlocal

echo Building Lab 2 HTTP Server...
echo.

REM Create build directory if it doesn't exist
if not exist "build" mkdir build

REM Generate build files with CMake
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo Failed to generate build files
    exit /b 1
)

REM Build the project
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo Failed to build project
    exit /b 1
)

echo.
echo Build completed successfully!
echo You can now run the server with: bin\run_server.bat

endlocal