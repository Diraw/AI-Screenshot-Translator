# Checks for Microsoft Edge WebView2 Runtime; downloads and installs silently if missing.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

# Registry keys that store the runtime version.
$RuntimeKeys = @(
    'HKLM:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
    'HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
    'HKCU:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'
)

function Get-WebView2Version {
    foreach ($key in $RuntimeKeys) {
        if (Test-Path $key) {
            $pv = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).pv
            if ($pv) { return $pv }
        }
    }
    return $null
}

# Returns $true when running elevated.
function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

# Relaunches the script with elevation if not already admin.
function Restart-AsAdmin {
    if (Test-Admin) { return }

    $psExe = (Get-Process -Id $PID).Path
    if (-not $psExe) { $psExe = 'powershell.exe' }

    $scriptPath = if ($PSCommandPath) { $PSCommandPath } else { $MyInvocation.MyCommand.Path }
    $argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File', $scriptPath) + $MyInvocation.UnboundArguments

    Start-Process -FilePath $psExe -ArgumentList $argList -Verb RunAs -WorkingDirectory (Get-Location)
    exit
}

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

function Enable-Tls12 {
    $tls12 = [Net.SecurityProtocolType]::Tls12
    if (-not ([Net.ServicePointManager]::SecurityProtocol.HasFlag($tls12))) {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor $tls12
    }
}

function Install-WebView2 {
    Enable-Tls12

    $installerUrl  = 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' # Evergreen bootstrapper
    $installerPath = Join-Path $env:TEMP 'MicrosoftEdgeWebView2Setup.exe'

    Write-Host "Downloading WebView2 Runtime from $installerUrl ..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath -UseBasicParsing

    Write-Host 'Installing WebView2 Runtime silently...' -ForegroundColor Cyan
    $process = Start-Process -FilePath $installerPath -ArgumentList '/silent','/install' -Wait -PassThru

    Remove-Item $installerPath -ErrorAction SilentlyContinue

    return $process.ExitCode
}

try {
    Restart-AsAdmin
    if (-not (Test-Admin)) {
        throw 'Please run this script in an elevated PowerShell (Run as administrator).'
    }

    $currentVersion = Get-WebView2Version
    if ($currentVersion) {
        Write-Host "WebView2 Runtime is already installed. Version: $currentVersion" -ForegroundColor Green
        Exit-WithPause 0
    }

    Write-Host 'WebView2 Runtime not found. Beginning installation...' -ForegroundColor Yellow

    $exitCode = Install-WebView2

    $successCodes = @(0, 3010)
    if ($successCodes -notcontains $exitCode) {
        throw "WebView2 installer exited with code $exitCode."
    }

    $installedVersion = Get-WebView2Version
    if ($installedVersion) {
        $msg = "WebView2 Runtime installation complete. Version: $installedVersion"
        if ($exitCode -eq 3010) {
            $msg += ' (Installer requested a reboot to finalize.)'
        }
        Write-Host $msg -ForegroundColor Green
        Exit-WithPause $exitCode
    }

    throw 'WebView2 Runtime installation did not report a version. Please install manually.'
}
catch {
    Write-Error $_
    Exit-WithPause 1
}
