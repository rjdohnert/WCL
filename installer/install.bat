@echo off
setlocal
cd /d "%~dp0"

echo ============================================================
echo      Windows Command Language (WCL) Installer
echo ============================================================
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
if %ERRORLEVEL% neq 0 (
    echo.
    echo Installation did not complete successfully.
    pause
)
