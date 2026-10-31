# Lost World Native LG: makes a drag-and-drop update for a release copy of the
# game (a folder made with "Cleanup - Remove Build Files.bat").
#
# Run from the build folder (Make Update Pack.bat). It takes the files a
# release needs to play, compares them with the last update made (Update
# Packs\release-manifest.txt) and copies only the new or changed ones into
#   Update Packs\LostWorld LG Update <date>\        (and a .zip of it)
# with the same folders as the game. Dragging that folder's contents into the
# release folder (Replace the files) updates it.
#
# Never included, so an update never overwrites them: the ROM data, save
# (settings, records), out, LightgunHID.ini, display.ini and textures\text
# (the lettering picked with the Text bats).
#
#   -Full       every release file, whatever changed (a complete update)
#   -Baseline   only record the current files as released, no pack (for a
#               release made by hand, such as the first one)
param([switch]$Full, [switch]$Baseline)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path 'LostWorld.exe')) { Write-Host 'LostWorld.exe is missing: run build.bat first.'; exit 1 }

# ---- What a release has --------------------------------------------------------
$topFiles = @('LostWorld.exe', 'nvngx_dlss.dll', 'Launch Lost World.ahk', 'LICENSE', 'game poster.webp',
              '1 - Light Gun Setup.exe', '1 - Getting Started Readme.txt')
$topPatterns = @('Crosshairs - *.bat', 'Display - *.bat', 'Graphics - *.bat', 'Text - *.bat', 'Debug - *.bat',
                 'Cleanup - *.bat')
$folders = @('16x9', '4x3', 'res', 'textures', 'third_party\fonts', 'third_party\menu', 'Hook Of The Reaper')
# Inside those folders: build-only files and what belongs to the player
$skip = @('res\lostworld.rc', 'res\lostworld.ico', 'res\logo.png')
$skipDirs = @('textures\text\')

$files = New-Object System.Collections.Generic.List[string]
foreach ($f in $topFiles) { if (Test-Path -LiteralPath $f -PathType Leaf) { $files.Add($f) } }
foreach ($p in $topPatterns) { Get-ChildItem -File -Filter $p | ForEach-Object { $files.Add($_.Name) } }
foreach ($d in $folders) {
    if (-not (Test-Path -LiteralPath $d)) { continue }
    Get-ChildItem -LiteralPath $d -File -Recurse | ForEach-Object {
        $rel = $_.FullName.Substring($root.Length + 1)
        $bad = $skip -contains $rel
        foreach ($s in $skipDirs) { if ($rel.StartsWith($s, [StringComparison]::OrdinalIgnoreCase)) { $bad = $true } }
        if (-not $bad) { $files.Add($rel) }
    }
}

# ---- What changed since the last update -------------------------------------------
$packs = Join-Path $root 'Update Packs'
New-Item -ItemType Directory -Force -Path $packs | Out-Null
$manifestPath = Join-Path $packs 'release-manifest.txt'
$old = @{}
if (Test-Path $manifestPath) {
    foreach ($line in Get-Content $manifestPath) {
        $i = $line.IndexOf(' ')
        if ($i -gt 0) { $old[$line.Substring($i + 1)] = $line.Substring(0, $i) }
    }
}
$hashes = @{}
$changed = New-Object System.Collections.Generic.List[string]
foreach ($f in $files) {
    $h = (Get-FileHash -LiteralPath $f -Algorithm SHA256).Hash
    $hashes[$f] = $h
    if ($Full -or -not $old.ContainsKey($f) -or $old[$f] -ne $h) { $changed.Add($f) }
}
function Save-Manifest {
    $lines = $hashes.Keys | Sort-Object | ForEach-Object { "$($hashes[$_]) $_" }
    Set-Content -Path $manifestPath -Value $lines -Encoding UTF8
}

if ($Baseline) {
    Save-Manifest
    Write-Host "Recorded $($files.Count) files as the current release. The next update holds only what changes after this."
    exit 0
}
if ($old.Count -eq 0 -and -not $Full) {
    Write-Host 'No earlier update recorded: this one holds every release file (a complete update).'
}
if ($changed.Count -eq 0) {
    Write-Host 'Nothing changed since the last update: no pack made.'
    exit 0
}

# ---- The pack ---------------------------------------------------------------------
$name = 'LostWorld LG Update ' + (Get-Date -Format 'yyyy-MM-dd HHmm')
$dir = Join-Path $packs $name
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($f in $changed) {
    $dst = Join-Path $dir $f
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
    Copy-Item -LiteralPath $f -Destination $dst -Force
}
$removed = @($old.Keys | Where-Object { -not $hashes.ContainsKey($_) } | Sort-Object)
$readme = @(
    "Lost World Native LG - update made $(Get-Date -Format 'yyyy-MM-dd HH:mm')",
    '',
    'HOW TO UPDATE',
    '  1. Close the game.',
    '  2. Open this folder, select everything in it (Ctrl+A) except this file,',
    '     and drag it into your game folder (the one with LostWorld.exe).',
    '  3. When Windows asks, choose "Replace the files in the destination".',
    '',
    'Your settings, records, guns (LightgunHID.ini), display.ini, the ROM data',
    'and your choice of in-game lettering are never in an update: they are kept.',
    'If this update lists files in textures\text_jurassic or textures\text_original,',
    'run your Text bat again (Text - Jurassic Park.bat or Text - Original.bat).',
    '',
    "FILES IN THIS UPDATE ($($changed.Count))"
) + ($changed | ForEach-Object { '  ' + $_ })
if ($removed.Count -gt 0) {
    $readme += @('', 'NO LONGER USED (you can delete them from your game folder)') + ($removed | ForEach-Object { '  ' + $_ })
}
Set-Content -Path (Join-Path $dir '_UPDATE - READ ME.txt') -Value $readme -Encoding UTF8
$zip = "$dir.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $dir '*') -DestinationPath $zip
Save-Manifest

Write-Host ''
Write-Host "Update made: $($changed.Count) files"
Write-Host "  $dir"
Write-Host "  $zip"
if ($removed.Count -gt 0) { Write-Host "  ($($removed.Count) files no longer used: listed in its READ ME)" }
