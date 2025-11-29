@echo off
setlocal
set CFG=%1
if "%CFG%"=="" set CFG=Release
set SCRIPT_DIR=%~dp0

if exist "%SCRIPT_DIR%%CFG%\WebServerLab2.exe" (
  call "%SCRIPT_DIR%%CFG%\WebServerLab2.exe" 8080 "%SCRIPT_DIR%..\www"
) else if exist "%SCRIPT_DIR%WebServerLab2.exe" (
  call "%SCRIPT_DIR%WebServerLab2.exe" 8080 "%SCRIPT_DIR%..\www"
) else (
  echo WebServerLab2.exe not found in "%SCRIPT_DIR%%CFG%" or "%SCRIPT_DIR%". Build the project first.
  exit /b 1
)
endlocal