Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$ExeName = "HotkeyLanguageSwitcher.exe"
$AppName = "HotkeyLanguageSwitcher"
$InstallerExe = Join-Path $Root "installer\$ExeName"
$StartupPath = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\Startup\$ExeName"
$RunKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$Issues = New-Object System.Collections.Generic.List[string]

function Get-NormalizedRunPath {
    param([string]$Value)

    if (-not $Value) {
        return $null
    }

    $Trimmed = $Value.Trim()
    if ($Trimmed.StartsWith('"')) {
        $EndQuote = $Trimmed.IndexOf('"', 1)
        if ($EndQuote -gt 1) {
            return $Trimmed.Substring(1, $EndQuote - 1)
        }
    }

    return ($Trimmed -split "\s+", 2)[0]
}

function Compare-InstalledHash {
    param(
        [string]$Label,
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        return
    }

    if (-not (Test-Path $InstallerExe)) {
        $Issues.Add("Cannot compare $Label hash because installer executable is missing: $InstallerExe")
        return
    }

    $Expected = (Get-FileHash -Algorithm SHA256 $InstallerExe).Hash
    $Actual = (Get-FileHash -Algorithm SHA256 $Path).Hash
    if ($Expected -ne $Actual) {
        $Issues.Add("$Label hash differs from installer executable: $Path")
    }
}

Write-Host "[DIAG] process state"
$Processes = @(Get-Process $AppName -ErrorAction SilentlyContinue)
if ($Processes.Count -eq 0) {
    Write-Host "  no running process"
} else {
    $Processes | Select-Object Id, ProcessName, Path, StartTime | Format-Table -AutoSize
}

if ($Processes.Count -gt 1) {
    $Issues.Add("More than one $AppName process is running.")
}

Write-Host "[DIAG] startup entries"
$RunValue = Get-ItemProperty -Path $RunKey -Name $AppName -ErrorAction SilentlyContinue
$RunCommand = $null
$RunPath = $null
if ($RunValue) {
    $RunCommand = $RunValue.$AppName
    $RunPath = Get-NormalizedRunPath $RunCommand
    Write-Host "  Registry Run: $RunCommand"
} else {
    Write-Host "  Registry Run: absent"
}

if (Test-Path $StartupPath) {
    Write-Host "  Startup folder: $StartupPath"
} else {
    Write-Host "  Startup folder: absent"
}

if ($RunCommand -and (Test-Path $StartupPath)) {
    $Issues.Add("Both Registry Run and Startup folder entries exist. Keep only one startup path.")
}

if ($RunPath -and -not (Test-Path $RunPath)) {
    $Issues.Add("Registry Run path does not exist: $RunPath")
}

Compare-InstalledHash "Startup folder executable" $StartupPath
if ($RunPath) {
    Compare-InstalledHash "Registry Run executable" $RunPath
}

if ($Issues.Count -gt 0) {
    Write-Host "[FAIL] runtime diagnostics found issues"
    foreach ($Issue in $Issues) {
        Write-Host "  - $Issue"
    }
    exit 1
}

Write-Host "[PASS] runtime diagnostics"
