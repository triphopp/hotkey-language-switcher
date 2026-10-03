param(
    [switch]$SkipGitChecks
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $Root "build"
$Src = Join-Path $Root "src\hotkey.c"
$UnitTestSrc = Join-Path $Root "tests\test_hotkey_core.c"
$AppExe = Join-Path $BuildDir "HotkeyLanguageSwitcher.exe"
$UnitTestExe = Join-Path $BuildDir "test_hotkey_core.exe"
$InstallerExe = Join-Path $Root "installer\HotkeyLanguageSwitcher.exe"

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Command
    )

    Write-Host "[TEST] $Name"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

function Assert-Contains {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Needle
    )

    if (-not $Text.Contains($Needle)) {
        throw "$Name is missing required text: $Needle"
    }
}

$Gcc = (Get-Command gcc -ErrorAction Stop).Source
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Invoke-Native "build app with warnings as errors" {
    & $Gcc $Src -o $AppExe -mwindows -O2 -Wall -Wextra -Werror
}

Invoke-Native "build hotkey core unit tests" {
    & $Gcc $UnitTestSrc -o $UnitTestExe -O2 -Wall -Wextra -Werror
}

Invoke-Native "run hotkey core unit tests" {
    & $UnitTestExe
}

Write-Host "[TEST] sync installer executable"
Copy-Item -Path $AppExe -Destination $InstallerExe -Force

Write-Host "[TEST] verify build and installer executables match"
$BuildHash = (Get-FileHash -Algorithm SHA256 $AppExe).Hash
$InstallerHash = (Get-FileHash -Algorithm SHA256 $InstallerExe).Hash
if ($BuildHash -ne $InstallerHash) {
    throw "Installer executable hash mismatch: build=$BuildHash installer=$InstallerHash"
}

Write-Host "[TEST] verify scheduled task setup and legacy startup cleanup"
$InstallScript = Get-Content -Path (Join-Path $Root "installer\install.bat") -Raw
$ResetScript = Get-Content -Path (Join-Path $Root "installer\reset.bat") -Raw
$SetupScript = Get-Content -Path (Join-Path $Root "installer\setup.ps1") -Raw

Assert-Contains "install.bat" $InstallScript 'setup.ps1"'
Assert-Contains "reset.bat" $ResetScript 'setup.ps1" -Uninstall'
Assert-Contains "setup.ps1" $SetupScript 'New-ScheduledTaskTrigger -AtLogOn -User $User'
Assert-Contains "setup.ps1" $SetupScript '-RunLevel Highest'
Assert-Contains "setup.ps1" $SetupScript '-AllowStartIfOnBatteries'
Assert-Contains "setup.ps1" $SetupScript '-DontStopIfGoingOnBatteries'
Assert-Contains "setup.ps1" $SetupScript '-ExecutionTimeLimit ([TimeSpan]::Zero)'
Assert-Contains "setup.ps1" $SetupScript 'Join-Path $env:ProgramFiles $AppName'
Assert-Contains "setup.ps1" $SetupScript 'Remove-ItemProperty -Path $RunKey -Name $AppName'
Assert-Contains "setup.ps1" $SetupScript 'Remove-Item -Path $StartupPath -Force'
Assert-Contains "setup.ps1" $SetupScript 'Unregister-ScheduledTask -TaskName $TaskName'

foreach ($Legacy in @("startup.bat", "config.bat")) {
    if (Test-Path (Join-Path $Root "installer\$Legacy")) {
        throw "Legacy installer must not ship (creates duplicate startup path): $Legacy"
    }
}

if (-not $SkipGitChecks) {
    $Git = Get-Command git -ErrorAction SilentlyContinue
    if ($Git) {
        Invoke-Native "git diff whitespace check" {
            & $Git.Source -C $Root diff --check
        }
    } else {
        Write-Host "[SKIP] git diff whitespace check (git not found)"
    }
}

Write-Host "[PASS] full production test suite"
