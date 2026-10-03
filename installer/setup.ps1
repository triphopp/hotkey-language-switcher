param(
    [switch]$Uninstall,
    [string]$User = "$env:USERDOMAIN\$env:USERNAME"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$AppName = "HotkeyLanguageSwitcher"
$ExeName = "$AppName.exe"
$TaskName = $AppName
$SourceExe = Join-Path $PSScriptRoot $ExeName
# Program Files is admin-only writable, so the elevated task cannot be hijacked
# by a user-level process replacing the binary.
$InstallDir = Join-Path $env:ProgramFiles $AppName
$InstallExe = Join-Path $InstallDir $ExeName
$StartupPath = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\Startup\$ExeName"
$RunKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"

function Test-Admin {
    $Identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal]$Identity).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Stop-Switcher {
    Get-Process $AppName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300
}

function Remove-LegacyStartup {
    if (Get-ItemProperty -Path $RunKey -Name $AppName -ErrorAction SilentlyContinue) {
        Remove-ItemProperty -Path $RunKey -Name $AppName
        Write-Host "[OK] Removed legacy Registry Run entry."
    }
    if (Test-Path $StartupPath) {
        Remove-Item -Path $StartupPath -Force
        Write-Host "[OK] Removed legacy Startup folder copy."
    }
}

if (-not (Test-Admin)) {
    # Pass the real user through so the task is registered for them, not for
    # whichever admin account approves the UAC prompt.
    $ArgList = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`"", "-User", "`"$User`"")
    if ($Uninstall) { $ArgList += "-Uninstall" }
    try {
        $Proc = Start-Process powershell -Verb RunAs -ArgumentList $ArgList -Wait -PassThru
    } catch {
        Write-Host "[ERROR] Administrator permission is required."
        exit 1
    }
    exit $Proc.ExitCode
}

try {
    Stop-Switcher

    if ($Uninstall) {
        if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
            Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
            Write-Host "[OK] Removed scheduled task."
        } else {
            Write-Host "[INFO] Scheduled task not found."
        }
        if (Test-Path $InstallDir) {
            Remove-Item -Path $InstallDir -Recurse -Force
            Write-Host "[OK] Removed $InstallDir"
        }
        Remove-LegacyStartup
        Write-Host "[DONE] Uninstalled."
        Start-Sleep -Seconds 3
        exit 0
    }

    if (-not (Test-Path $SourceExe)) {
        throw "$ExeName not found next to this script."
    }

    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    Copy-Item -Path $SourceExe -Destination $InstallExe -Force
    Write-Host "[OK] Installed binary: $InstallExe"

    Remove-LegacyStartup

    $Action = New-ScheduledTaskAction -Execute $InstallExe
    $Trigger = New-ScheduledTaskTrigger -AtLogOn -User $User
    # Highest privileges lets the hook see keys typed into elevated windows.
    $Principal = New-ScheduledTaskPrincipal -UserId $User -LogonType Interactive -RunLevel Highest
    # Defaults would kill the task after 72h and skip/stop it on battery power.
    $Settings = New-ScheduledTaskSettingsSet `
        -AllowStartIfOnBatteries `
        -DontStopIfGoingOnBatteries `
        -ExecutionTimeLimit ([TimeSpan]::Zero) `
        -MultipleInstances IgnoreNew `
        -RestartCount 10 `
        -RestartInterval (New-TimeSpan -Minutes 1) `
        -Priority 4

    Register-ScheduledTask -TaskName $TaskName -Action $Action -Trigger $Trigger `
        -Principal $Principal -Settings $Settings -Force | Out-Null
    Write-Host "[OK] Registered scheduled task '$TaskName' (at logon, highest privileges) for $User"

    Start-ScheduledTask -TaskName $TaskName
    Start-Sleep -Seconds 1
    if (Get-Process $AppName -ErrorAction SilentlyContinue) {
        Write-Host "[OK] Running."
    } else {
        Write-Host "[WARN] Task started but process not detected yet."
    }

    Write-Host "[DONE] Installed. This window will close in 3 seconds."
    Start-Sleep -Seconds 3
    exit 0
} catch {
    Write-Host "[ERROR] $($_.Exception.Message)"
    Read-Host "Press Enter to close"
    exit 1
}
