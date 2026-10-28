# One-click setup + build for Lost World Native.
# Installs Python 3 and the Visual Studio 2022 Build Tools (only the parts the
# build needs) if they're missing, then runs build.bat. Use update.bat later to
# get new versions (no Git needed).
# Run it with setup-and-build.bat (double-click), not directly.
# -PrereqsOnly: only install what's missing, don't build (used by update.bat).
param([switch]$PrereqsOnly)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot

function Step($msg) { Write-Host "`n=== $msg ===" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "  OK: $msg" -ForegroundColor Green }
function Fail($msg) { Write-Host "`nERROR: $msg" -ForegroundColor Red; exit 1 }

# Picks up PATH changes made by installers without reopening the window
function Refresh-Path {
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user    = [Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = "$machine;$user"
}

# True only for a real Python 3 (not the Microsoft Store placeholder)
function Test-Python {
    try {
        $v = & python -c "import sys; print(sys.version_info[0])" 2>$null
        return ($LASTEXITCODE -eq 0 -and "$v".Trim() -eq '3')
    } catch { return $false }
}

# --- 0. Basic checks -------------------------------------------------------
Step 'Checking files'
if (-not (Test-Path 'build.bat')) { Fail 'build.bat not found. Put this script in the LostWorldNative folder.' }
if (-not (Test-Path 'data\crom.bin') -and -not (Test-Path 'lostwsga.zip')) {
    Fail 'lostwsga.zip not found next to build.bat.'
}
Ok 'build.bat and ROM found'

$haveWinget = [bool](Get-Command winget -ErrorAction SilentlyContinue)

# --- 1. Python -------------------------------------------------------------
Step 'Python 3'
if (Test-Python) {
    Ok (& python --version)
} else {
    if (-not $haveWinget) { Fail 'Python 3 is missing and winget is not available. Install Python from https://www.python.org/downloads/ (tick "Add python.exe to PATH") and run this again.' }
    Write-Host '  Installing Python 3.12 with winget...'
    winget install --id Python.Python.3.12 -e --accept-source-agreements --accept-package-agreements
    Refresh-Path
    if (-not (Test-Python)) {
        Fail 'Python was installed but is not usable yet. Close this window and run it again. If it still fails, turn off the "python.exe" App execution alias in Windows Settings > Apps > Advanced app settings.'
    }
    Ok (& python --version)
}

# --- 2. Visual Studio 2022 Build Tools ------------------------------------
Step 'Visual Studio 2022 C++ build tools'
$components = @(
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    'Microsoft.VisualStudio.Component.VC.CMake.Project',
    'Microsoft.VisualStudio.Component.Windows11SDK.22621'
)
$vsInstallerDir = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
$vswhere = Join-Path $vsInstallerDir 'vswhere.exe'

function Find-VS([string[]]$req) {
    if (-not (Test-Path $vswhere)) { return $null }
    $vsArgs = @('-latest', '-products', '*', '-version', '[17.0,18.0)', '-property', 'installationPath')
    if ($req) { $vsArgs += '-requires'; $vsArgs += $req }
    $p = & $vswhere @args
    if ($p) { return "$p".Trim() } else { return $null }
}

$vsReady = Find-VS $components
if ($vsReady) {
    Ok "found at $vsReady"
} else {
    $existing = Find-VS @()
    if ($existing) {
        # VS 2022 is installed but missing pieces: add them
        Write-Host "  Visual Studio found at $existing but missing components. Adding them (a UAC prompt may appear)..."
        $setup = Join-Path $vsInstallerDir 'setup.exe'
        $a = @('modify', '--installPath', "`"$existing`"", '--passive', '--norestart')
        foreach ($c in $components) { $a += '--add'; $a += $c }
        $proc = Start-Process -FilePath $setup -ArgumentList $a -Wait -PassThru
        if ($proc.ExitCode -notin 0, 3010) { Fail "Visual Studio Installer exited with code $($proc.ExitCode)." }
    } else {
        if (-not $haveWinget) { Fail 'Build Tools for Visual Studio 2022 are missing and winget is not available. Install them from https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022 and run this again.' }
        Write-Host '  Installing Build Tools for Visual Studio 2022 (about 2-3 GB, takes a while; a UAC prompt may appear)...'
        $override = '--passive --wait --norestart ' + (($components | ForEach-Object { "--add $_" }) -join ' ')
        winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-source-agreements --accept-package-agreements --override $override
    }
    $vsReady = Find-VS $components
    if (-not $vsReady) { Fail 'Visual Studio Build Tools still not detected. A restart may be needed: reboot and run it again.' }
    Ok "ready at $vsReady"
}

if ($PrereqsOnly) {
    Write-Host "`nEverything needed to build is installed." -ForegroundColor Green
    exit 0
}

# --- 3. Build ----------------------------------------------------------------
Step 'Building (first build takes several minutes)'
& cmd.exe /c build.bat
if ($LASTEXITCODE -ne 0) { Fail 'build.bat failed. See the messages above.' }

if (Test-Path 'LostWorld.exe') {
    Write-Host "`nAll done! Run LostWorld.exe in this folder to play." -ForegroundColor Green
    Write-Host 'Controls: 5 = coin, 1 = start, left click = fire, right click = reload, F11 = fullscreen.'
} else {
    Fail 'Build finished but LostWorld.exe was not found.'
}
