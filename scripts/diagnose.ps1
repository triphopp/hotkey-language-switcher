Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$ExeName = "HotkeyLanguageSwitcher.exe"
$AppName = "HotkeyLanguageSwitcher"
$TaskName = $AppName
$InstallerExe = Join-Path $Root "installer\$ExeName"
$InstallExe = Join-Path $env:ProgramFiles "$AppName\$ExeName"
$StartupPath = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\Startup\$ExeName"
$RunKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$Issues = New-Object System.Collections.Generic.List[string]

Write-Host "[DIAG] process state"
$Processes = @(Get-Process $AppName -ErrorAction SilentlyContinue)
if ($Processes.Count -eq 0) {
    Write-Host "  no running process"
    $Issues.Add("$AppName is not running.")
} else {
    $Processes | Select-Object Id, ProcessName, StartTime | Format-Table -AutoSize
}

if ($Processes.Count -gt 1) {
    $Issues.Add("More than one $AppName process is running.")
}

Write-Host "[DIAG] scheduled task"
$Task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if (-not $Task) {
    Write-Host "  task: absent"
    $Issues.Add("Scheduled task '$TaskName' is missing. Run installer\install.bat.")
} else {
    $Exec = $Task.Actions[0].Execute
    $Settings = $Task.Settings
    Write-Host "  task: $($Task.State)"
    Write-Host "  execute: $Exec"
    Write-Host "  run level: $($Task.Principal.RunLevel)"
    Write-Host "  time limit: $($Settings.ExecutionTimeLimit)"

    if ($Exec -ne $InstallExe) {
        $Issues.Add("Task runs '$Exec' instead of '$InstallExe'.")
    }
    if (@($Task.Triggers | Where-Object { $_.CimClass.CimClassName -eq "MSFT_TaskLogonTrigger" }).Count -eq 0) {
        $Issues.Add("Task has no logon trigger.")
    }
    if ($Task.Principal.RunLevel -ne "Highest") {
        $Issues.Add("Task does not run with highest privileges.")
    }
    if ($Settings.ExecutionTimeLimit -ne "PT0S") {
        $Issues.Add("Task has an execution time limit ($($Settings.ExecutionTimeLimit)); it will be killed.")
    }
    if ($Settings.DisallowStartIfOnBatteries -or $Settings.StopIfGoingOnBatteries) {
        $Issues.Add("Task is blocked or stopped on battery power.")
    }
}

Write-Host "[DIAG] installed binary"
if (Test-Path $InstallExe) {
    Write-Host "  $InstallExe"
    if (Test-Path $InstallerExe) {
        $Expected = (Get-FileHash -Algorithm SHA256 $InstallerExe).Hash
        $Actual = (Get-FileHash -Algorithm SHA256 $InstallExe).Hash
        if ($Expected -ne $Actual) {
            $Issues.Add("Installed binary hash differs from installer executable. Run installer\install.bat.")
        }
    }
} else {
    Write-Host "  absent"
    $Issues.Add("Installed binary is missing: $InstallExe")
}

Write-Host "[DIAG] legacy startup entries"
if (Get-ItemProperty -Path $RunKey -Name $AppName -ErrorAction SilentlyContinue) {
    $Issues.Add("Legacy Registry Run entry still exists; it duplicates the scheduled task.")
}
if (Test-Path $StartupPath) {
    $Issues.Add("Legacy Startup folder copy still exists; it duplicates the scheduled task.")
}

if ($Issues.Count -gt 0) {
    Write-Host "[FAIL] runtime diagnostics found issues"
    foreach ($Issue in $Issues) {
        Write-Host "  - $Issue"
    }
    exit 1
}

Write-Host "[PASS] runtime diagnostics"
