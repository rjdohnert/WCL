#Requires -Version 5.1
<#
.SYNOPSIS
    Installs Windows Command Language (WCL) to C:\Program Files\wcl\bin,
    adds it to the system PATH, and registers a Windows Terminal profile.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

# Determine the absolute path of this script and the installer payload directory
$scriptPath = $PSCommandPath
if (-not $scriptPath) {
    $scriptPath = $MyInvocation.MyCommand.Definition
}
if (-not $scriptPath -or -not (Test-Path -LiteralPath $scriptPath)) {
    $scriptPath = (Resolve-Path (Join-Path $PSScriptRoot "install.ps1")).Path
}
$scriptPath = [System.IO.Path]::GetFullPath($scriptPath)
$installerDir = Split-Path -Parent $scriptPath

# 1. Check for Administrative Privileges and Elevate if Necessary
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]$identity
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "Administrative privileges required. Requesting elevation..." -ForegroundColor Cyan
    $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`""
    try {
        $proc = Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments -PassThru -Wait
        exit $proc.ExitCode
    } catch {
        Write-Warning "Elevation request failed: $_"
        Write-Host "Please right-click install.bat and select 'Run as Administrator'." -ForegroundColor Red
        if ([Environment]::UserInteractive -and -not [Console]::IsInputRedirected) {
            Write-Host "Press Enter to exit..."
            $null = Read-Host
        }
        exit 1
    }
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "     Windows Command Language (WCL) Installer" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

$targetRoot = "C:\Program Files\wcl"
$targetBin  = Join-Path $targetRoot "bin"
$targetDocs = Join-Path $targetRoot "docs"

# 2. Verify installer payload files
$sourceExe   = Join-Path $installerDir "wcl.exe"
$sourceGuide = Join-Path $installerDir "USER_GUIDE.md"
$sourceRef   = Join-Path $installerDir "command-reference.md"

if (-not (Test-Path -LiteralPath $sourceExe)) {
    Write-Error "wcl.exe not found in installer directory: $installerDir"
    if ([Environment]::UserInteractive) {
        Write-Host "Press Enter to exit..."
        $null = Read-Host
    }
    exit 1
}

# 3. Create destination directories
Write-Host "[1/4] Creating installation directories..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path $targetBin -Force | Out-Null
New-Item -ItemType Directory -Path $targetDocs -Force | Out-Null
Write-Host "      Installed to: $targetRoot" -ForegroundColor Green

# 4. Copy files
Write-Host "[2/4] Copying binaries and documentation..." -ForegroundColor Yellow
Copy-Item -LiteralPath $sourceExe -Destination (Join-Path $targetBin "wcl.exe") -Force
if (Test-Path -LiteralPath $sourceGuide) {
    Copy-Item -LiteralPath $sourceGuide -Destination (Join-Path $targetDocs "USER_GUIDE.md") -Force
    Copy-Item -LiteralPath $sourceGuide -Destination (Join-Path $targetRoot "USER_GUIDE.md") -Force
}
if (Test-Path -LiteralPath $sourceRef) {
    Copy-Item -LiteralPath $sourceRef -Destination (Join-Path $targetDocs "command-reference.md") -Force
    Copy-Item -LiteralPath $sourceRef -Destination (Join-Path $targetRoot "command-reference.md") -Force
}
Write-Host "      wcl.exe -> $targetBin\wcl.exe" -ForegroundColor Green
Write-Host "      Docs    -> $targetDocs" -ForegroundColor Green

# 5. Add to System PATH
Write-Host "[3/4] Configuring System PATH environment variable..." -ForegroundColor Yellow
$currentPath = [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::Machine)
$pathEntries = $currentPath -split ";" | Where-Object { $_ -ne "" }
if ($pathEntries -notcontains $targetBin) {
    $newPath = ($pathEntries + $targetBin) -join ";"
    [Environment]::SetEnvironmentVariable("Path", $newPath, [EnvironmentVariableTarget]::Machine)
    Write-Host "      Added '$targetBin' to Machine PATH." -ForegroundColor Green
} else {
    Write-Host "      '$targetBin' is already in Machine PATH." -ForegroundColor Gray
}

# Also update current process path so it is immediately usable
if (($env:Path -split ";") -notcontains $targetBin) {
    $env:Path = "$env:Path;$targetBin"
}

# 6. Install Windows Terminal Profile
Write-Host "[4/4] Installing Windows Terminal Profile..." -ForegroundColor Yellow

$terminalFragmentJson = @'
{
  "$schema": "https://aka.ms/terminal-profiles-schema",
  "profiles": [
    {
      "guid": "{58b75f80-7b24-4f51-b844-325bce8c1301}",
      "name": "WCL (OpenVMS DCL)",
      "commandline": "\"C:\\Program Files\\wcl\\bin\\wcl.exe\"",
      "startingDirectory": "%USERPROFILE%",
      "hidden": false
    }
  ]
}
'@

$fragmentDirs = @(
    Join-Path $env:ProgramData "Microsoft\Windows Terminal\Fragments\wcl"
)

if ($env:LOCALAPPDATA) {
    $fragmentDirs += Join-Path $env:LOCALAPPDATA "Microsoft\Windows Terminal\Fragments\wcl"
}

$fragmentInstalled = $false
foreach ($fdir in $fragmentDirs) {
    try {
        if (-not (Test-Path -LiteralPath $fdir)) {
            New-Item -ItemType Directory -Path $fdir -Force | Out-Null
        }
        $fpath = Join-Path $fdir "wcl.json"
        [System.IO.File]::WriteAllText($fpath, $terminalFragmentJson, [System.Text.Encoding]::UTF8)
        Write-Host "      Registered Terminal fragment: $fpath" -ForegroundColor Green
        $fragmentInstalled = $true
    } catch {
        Write-Warning "Could not write fragment to ${fdir}: $_"
    }
}

# Optional direct settings.json insertion for older Windows Terminal versions without fragment support
$settingsJsonPaths = @(
    "$env:LOCALAPPDATA\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json",
    "$env:LOCALAPPDATA\Microsoft\Windows Terminal\settings.json"
)

foreach ($settingsPath in $settingsJsonPaths) {
    if (Test-Path -LiteralPath $settingsPath) {
        try {
            $rawJson = [System.IO.File]::ReadAllText($settingsPath, [System.Text.Encoding]::UTF8)
            if ($rawJson -notmatch '58b75f80-7b24-4f51-b844-325bce8c1301' -and $rawJson -notmatch 'wcl\.exe') {
                $settingsObj = $rawJson | ConvertFrom-Json
                $newProfile = [PSCustomObject]@{
                    guid              = "{58b75f80-7b24-4f51-b844-325bce8c1301}"
                    name              = "WCL (OpenVMS DCL)"
                    commandline       = "`"C:\Program Files\wcl\bin\wcl.exe`""
                    startingDirectory = "%USERPROFILE%"
                    hidden            = $false
                }

                if ($settingsObj.profiles.list) {
                    $settingsObj.profiles.list += $newProfile
                } elseif ($settingsObj.profiles -is [System.Array]) {
                    $settingsObj.profiles += $newProfile
                }

                $updatedJson = $settingsObj | ConvertTo-Json -Depth 32
                [System.IO.File]::WriteAllText($settingsPath, $updatedJson, [System.Text.Encoding]::UTF8)
                Write-Host "      Updated settings.json at: $settingsPath" -ForegroundColor Green
            }
        } catch {
            # Non-fatal if fragment already handled it
        }
    }
}

# 7. Generate Uninstaller
$uninstallerScript = @'
#Requires -Version 5.1
$ErrorActionPreference = "SilentlyContinue"

# Check elevation
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit 0
}

Write-Host "Uninstalling Windows Command Language (WCL)..." -ForegroundColor Cyan

# Remove from PATH
$targetBin = "C:\Program Files\wcl\bin"
$currentPath = [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::Machine)
$pathEntries = $currentPath -split ";" | Where-Object { $_ -ne "" -and $_ -ne $targetBin }
$newPath = $pathEntries -join ";"
[Environment]::SetEnvironmentVariable("Path", $newPath, [EnvironmentVariableTarget]::Machine)

# Remove Windows Terminal fragments
Remove-Item -Path "$env:ProgramData\Microsoft\Windows Terminal\Fragments\wcl" -Recurse -Force
Remove-Item -Path "$env:LOCALAPPDATA\Microsoft\Windows Terminal\Fragments\wcl" -Recurse -Force

# Remove installation directory
Remove-Item -Path "C:\Program Files\wcl" -Recurse -Force

Write-Host "WCL has been successfully uninstalled." -ForegroundColor Green
if ([Environment]::UserInteractive) {
    Write-Host "Press Enter to exit..."
    $null = Read-Host
}
'@

try {
    $uninstallerPath = Join-Path $targetRoot "uninstall.ps1"
    [System.IO.File]::WriteAllText($uninstallerPath, $uninstallerScript, [System.Text.Encoding]::UTF8)
} catch {}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Installation Complete! You can now run 'wcl' from any" -ForegroundColor Green
Write-Host "  terminal prompt or select 'WCL (OpenVMS DCL)' in" -ForegroundColor Green
Write-Host "  the Windows Terminal dropdown menu." -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if ([Environment]::UserInteractive -and -not [Console]::IsInputRedirected) {
    Write-Host "Press Enter to close..." -ForegroundColor Gray
    $null = Read-Host
}
