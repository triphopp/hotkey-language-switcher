@echo off
set "APP_NAME=HotkeyLanguageSwitcher"
set "EXE_NAME=HotkeyLanguageSwitcher.exe"
set "STARTUP_PATH=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\%EXE_NAME%"

taskkill /F /IM "%EXE_NAME%" >nul 2>&1

reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "HotkeyLanguageSwitcher" >nul 2>&1
if %errorlevel%==0 (
    reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "HotkeyLanguageSwitcher" /f >nul
    echo [OK] HotkeyLanguageSwitcher removed from startup.
) else (
    echo [INFO] HotkeyLanguageSwitcher is not in startup.
)

if exist "%STARTUP_PATH%" (
    del /F /Q "%STARTUP_PATH%" >nul
    echo [OK] Removed Startup folder copy.
) else (
    echo [INFO] No Startup folder copy found.
)
timeout /t 10 >nul
