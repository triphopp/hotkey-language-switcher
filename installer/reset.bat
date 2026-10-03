@echo off
:: Removes the scheduled task, installed binary and any legacy startup entries.
:: Asks for Administrator permission (UAC) once.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" -Uninstall
exit /b %errorlevel%
