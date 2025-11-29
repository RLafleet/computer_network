@echo off
setlocal
set CFG=%1
if "%CFG%"=="" set CFG=Release
set SCRIPT_DIR=%~dp0

if exist "%SCRIPT_DIR%%CFG%\HighLoadServerC.exe" (
  call "%SCRIPT_DIR%%CFG%\HighLoadServerC.exe" 5050 "server from max"
) else if exist "%SCRIPT_DIR%HighLoadServerC.exe" (
  call "%SCRIPT_DIR%HighLoadServerC.exe" 5050 "server from max"
) else (
  echo HighLoadServerC.exe not found in "%SCRIPT_DIR%%CFG%" or "%SCRIPT_DIR%". Build the project first.
  exit /b 1
)
endlocal
