@echo off
setlocal EnableDelayedExpansion

set "EXE=HotkeyLanguageSwitcher.exe"
set "NAME=HotkeyLanguageSwitcher"
set "SRC=%~dp0%EXE%"
set "STARTUP=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\%EXE%"
set "REG=HKCU\Software\Microsoft\Windows\CurrentVersion\Run"
set "DEST="

echo.
echo  ============================================
echo   Hotkey Language Switcher - Install/Upgrade
echo  ============================================
echo.

if not exist "%SRC%" (
    echo [ERROR] %EXE% not found. Place it next to this script.
    pause & exit /b 1
)

taskkill /F /IM "%EXE%" >nul 2>&1

:: --- Case 1: already in Startup folder (via startup.bat) ---
if exist "%STARTUP%" (
    copy /Y "%SRC%" "%STARTUP%" >nul
    echo [OK] Upgraded existing installation in Startup folder.
    set "DEST=%STARTUP%"
    goto :launch
)

:: --- Case 2: registered via Registry Run key (via config.bat) ---
for /f "tokens=*" %%L in ('reg query "%REG%" /v "%NAME%" 2^>nul') do (
    set "LINE=%%L"
    if "!LINE:REG_SZ=!" neq "!LINE!" (
        for /f "tokens=1,2,*" %%A in ("!LINE!") do set "REGVAL=%%C"
    )
)
if defined REGVAL (
    set "REGVAL=!REGVAL:"=!"
    for %%F in ("!REGVAL!") do set "DEST=%%~dpF%EXE%"
    copy /Y "%SRC%" "!DEST!" >nul
    echo [OK] Upgraded existing installation at: !DEST!
    goto :launch
)

:: --- Case 3: fresh install ---
copy /Y "%SRC%" "%STARTUP%" >nul
echo [OK] Installed to Startup folder.
echo      Will auto-start on next login.
set "DEST=%STARTUP%"

:launch
echo.
start "" "!DEST!"
echo [*] Running. This window will close in 3 seconds.
timeout /t 3 >nul
