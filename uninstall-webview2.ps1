# Uninstalls Microsoft Edge WebView2 Runtime (Evergreen) if present.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

# Registry keys used by the runtime (same as installer detection).
$RuntimeKeys = @(
    'HKLM:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
    'HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
    'HKCU:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'
)

function Read-UserKey {
    if ($env:NO_PAUSE) { return }
    try { $rawUi = $Host.UI.RawUI } catch { return }
    if (-not $rawUi) { return }
    if ([Console]::IsInputRedirected) { return }
    Write-Host 'Press any key to close this window...' -ForegroundColor DarkGray
    [Console]::ReadKey($true) | Out-Null
}

function Exit-WithPause([int]$code) {
    Read-UserKey
    exit $code
}

function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

function Restart-AsAdmin {
    if (Test-Admin) { return }

    $psExe = (Get-Process -Id $PID).Path
    if (-not $psExe) { $psExe = 'powershell.exe' }

    $scriptPath = if ($PSCommandPath) { $PSCommandPath } else { $MyInvocation.MyCommand.Path }
    # ...existing code...

    Start-Process -FilePath $psExe -ArgumentList $argList -Verb RunAs -WorkingDirectory (Get-Location)
    # Exit silently here; the elevated instance will handle user prompts.
    exit 0
}

function Get-WebView2Version {
    foreach ($key in $RuntimeKeys) {
        if (Test-Path $key) {
            $pv = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).pv
            if ($pv) { return $pv }
        }
    }
    return $null
}

function Find-SetupExe {
    param(
        [string]$Version
    )

    $pf86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    $localAppData = [Environment]::GetEnvironmentVariable('LOCALAPPDATA')

    $paths = @()
    if ($pf86) {
        $paths += Join-Path -Path $pf86 -ChildPath ("Microsoft\EdgeWebView\Application\{0}\Installer\setup.exe" -f $Version)
    }
    if ($localAppData) {
        $paths += Join-Path -Path $localAppData -ChildPath ("Microsoft\EdgeWebView\Application\{0}\Installer\setup.exe" -f $Version)
    }

    foreach ($p in $paths) {
        if (Test-Path $p) { return $p }
    }

    # Fallback: search a couple levels under expected roots
    $roots = @()
    if ($pf86) { $roots += Join-Path -Path $pf86 -ChildPath 'Microsoft\EdgeWebView\Application' }
    if ($localAppData) { $roots += Join-Path -Path $localAppData -ChildPath 'Microsoft\EdgeWebView\Application' }

    foreach ($r in $roots) {
        if (-not (Test-Path $r)) { continue }
        $candidate = Get-ChildItem -Path $r -Filter setup.exe -Recurse -Depth 3 -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($candidate) { return $candidate.FullName }
    }
    return $null
}

function Get-UninstallString {
    foreach ($key in $RuntimeKeys) {
        if (-not (Test-Path $key)) { continue }
        $prop = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
        if ($prop.uninstall) { return $prop.uninstall }
        if ($prop.UninstallString) { return $prop.UninstallString }
    }
    return $null
}

function Parse-UninstallString {
    param([string]$CommandLine)

    if (-not $CommandLine) { return $null }

    $path = $null
    $args = $null
    if ($CommandLine -match '^\s*\"(?<path>[^\"]+)\"\s*(?<args>.*)$') {
        $path = $Matches.path
        $args = $Matches.args
    }
    elseif ($CommandLine -match '^\s*(?<path>\S+)\s*(?<args>.*)$') {
        $path = $Matches.path
        $args = $Matches.args
    }
    if (-not $path) { return $null }

    $argArray = @()
    if ($args) {
        # Use PowerShell tokenizer to split arguments reliably.
        $tokens = [System.Management.Automation.PSParser]::Tokenize($args, [ref]@())
        foreach ($t in $tokens) { if ($t.Type -ne 'NewLine') { $argArray += $t.Content } }
    }

    return [PSCustomObject]@{
        Path = $path
        Args = $argArray
    }
}

function Uninstall-WebView2 {
    param(
        [string]$Version
    )

    $cmdInfo = $null
    $uninstallString = Get-UninstallString
    if ($uninstallString) {
        $cmdInfo = Parse-UninstallString -CommandLine $uninstallString
    }

    if (-not $cmdInfo) {
        $setup = Find-SetupExe -Version $Version
        if (-not $setup) {
            throw "Cannot find WebView2 installer (setup.exe) for version $Version. Please uninstall manually."
        }
        $cmdInfo = [PSCustomObject]@{ Path = $setup; Args = @() }
    }

    # Build canonical argument sets (ignore any incomplete args from registry).
    $baseArgs = @('--uninstall', '--msedgewebview', '--force-uninstall', '--verbose-logging')
    $sysArgs = $baseArgs + @()
    if (Test-Admin) { $sysArgs += '--system-level' }

    # Attempt both system-level and user-level to cover mismatch.
    $attempts = @(
        [PSCustomObject]@{ Path = $cmdInfo.Path; Args = $sysArgs },
        [PSCustomObject]@{ Path = $cmdInfo.Path; Args = $baseArgs }
    )

    $workingDir = [IO.Path]::GetDirectoryName($cmdInfo.Path)

    function Invoke-Setup([string]$path, [string[]]$args) {
        Write-Host "Running: `"$path`" $($args -join ' ')" -ForegroundColor Cyan
        if ($args -and $args.Count -gt 0) {
            $p = Start-Process -FilePath $path -ArgumentList $args -Wait -PassThru -WorkingDirectory $workingDir
        }
        else {
            $p = Start-Process -FilePath $path -Wait -PassThru -WorkingDirectory $workingDir
        }
        return $p.ExitCode
    }

    $successCodes = @(0, 3010)
    $lastExit = $null
    foreach ($attempt in $attempts) {
        $lastExit = Invoke-Setup -path $attempt.Path -args $attempt.Args
        if ($successCodes -contains $lastExit) { return $lastExit }
    }

    # If all attempts failed, return last exit code.
    return $lastExit

}

try {
    Restart-AsAdmin

    # In the elevated instance, ensure pause prompts are enabled.
    Remove-Item Env:NO_PAUSE -ErrorAction SilentlyContinue

    $version = Get-WebView2Version
    if (-not $version) {
        Write-Host 'WebView2 Runtime is not installed.' -ForegroundColor Green
        Exit-WithPause 0
    }

    Write-Host "Detected WebView2 Runtime version: $version" -ForegroundColor Yellow

    $exitCode = Uninstall-WebView2 -Version $version
    $successCodes = @(0, 3010)
    if ($successCodes -notcontains $exitCode) {
        Write-Warning "Uninstall exited with code $exitCode. Attempting to clean up registry entries..."
        # 自动清理注册表项
        foreach ($key in $RuntimeKeys) {
            if (Test-Path $key) {
                try {
                    Remove-Item -Path $key -Recurse -Force -ErrorAction Stop
                    Write-Host "Removed registry key: $key" -ForegroundColor Yellow
                }
                catch {
                    Write-Warning "Failed to remove registry key: $key. $_"
                }
            }
        }
        if (-not (Get-WebView2Version)) {
            Write-Host 'WebView2 registry entries have been removed.' -ForegroundColor Green
            Exit-WithPause 0
        }
        else {
            throw "Uninstall failed and registry cleanup did not fully remove WebView2. Please check manually."
        }
    }

    if (Get-WebView2Version) {
        Write-Warning "Uninstall reported success but WebView2 still detected. Attempting to clean up registry entries..."
        foreach ($key in $RuntimeKeys) {
            if (Test-Path $key) {
                try {
                    Remove-Item -Path $key -Recurse -Force -ErrorAction Stop
                    Write-Host "Removed registry key: $key" -ForegroundColor Yellow
                }
                catch {
                    Write-Warning "Failed to remove registry key: $key. $_"
                }
            }
        }
        if (-not (Get-WebView2Version)) {
            Write-Host 'WebView2 registry entries have been removed.' -ForegroundColor Green
            Exit-WithPause 0
        }
        else {
            throw "Uninstall reported success but registry cleanup did not fully remove WebView2. Please check manually."
        }
    }

    $msg = 'WebView2 Runtime has been removed.'
    if ($exitCode -eq 3010) {
        $msg += ' A reboot is recommended to finalize removal.'
    }
    Write-Host $msg -ForegroundColor Green
    Exit-WithPause $exitCode
}
catch {
    Write-Error $_
    Exit-WithPause 1
}
