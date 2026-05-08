@echo off
setlocal

set "APP_NAME=HotkeyLanguageSwitcher"
set "EXE_NAME=HotkeyLanguageSwitcher.exe"
set "APP_PATH=%~dp0%EXE_NAME%"

if not exist "%APP_PATH%" (
    echo [ERROR] File not found: %APP_PATH%
    echo        Please place %EXE_NAME% in the same folder as this script.
    timeout /t 5 >nul
    exit /b 1
)

reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" ^
    /v "%APP_NAME%" ^
    /t REG_SZ ^
    /d "\"%APP_PATH%\"" ^
    /f >nul 2>&1

if %errorlevel%==0 (
    echo [OK] Added to startup: %APP_PATH%
    echo [*]  The program will now start automatically on login.
) else (
    echo [ERROR] Failed to add to startup.
)

timeout /t 3 >nul
