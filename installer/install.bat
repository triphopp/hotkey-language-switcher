@echo off
:: Installs or upgrades HotkeyLanguageSwitcher as a logon scheduled task.
:: Asks for Administrator permission (UAC) once.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1"
exit /b %errorlevel%
