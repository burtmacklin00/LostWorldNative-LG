# Lost World Native LG: makes drag-and-drop updates for a release copy of the
# game (a folder made with "Cleanup - Remove Build Files.bat").
#
# Run from the build folder (Make Update Pack.bat, Make Release.bat). It takes
# the files a release needs to play and copies the ones to update into a
# folder (and a .zip) with the same folders as the game. Dragging that
# folder's contents into the release folder (Replace the files) updates it.
#
#   (nothing)        a test update: the files changed since the last update
#                    made (Update Packs\release-manifest.txt)
#   -Full            a test update with every release file
#   -Baseline        only record the current files as the last update, no pack
#   -Release 2.0     a version: records it (Update Packs\Versions\v2.0.txt) and
#                    makes "LostWorld LG v2.0 (full)", every release file, for
#                    a new copy, and "LostWorld LG v1.0 to v2.0", only what
#                    differs from the version before, however many test
#                    updates came in between. The full one also carries
#                    the game data (data\) and the arenas and boss fights
#                    kept so far (save\survival, save\boss): a new copy plays
#                    at once and starts them without playing its way there.
#                    All of it is from the ROM: for private full copies only
#
# Never included, so an update never overwrites them: the ROM data, save
# (settings, records), out, LightgunHID.ini, display.ini and textures\text
# (the lettering picked with the Text bats), and the Debug bats (kept in the
# build folder only).
param([switch]$Full, [switch]$Baseline, [string]$Release)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path 'LostWorld.exe')) { Write-Host 'LostWorld.exe is missing: run build.bat first.'; exit 1 }

# ---- What a release has --------------------------------------------------------
# Everything in this folder that the game uses, so whatever is added later
# (features, fonts, music, pictures) goes with it, except:
# - what only builds or updates it (the code, the build, the tools, the repo)
$buildDirs = @('.git', '.github', '.vs', 'build', 'docs', 'src', 'tests', 'tools', 'out', 'Update Packs',
               'third_party\glslang', 'third_party\musashi', 'third_party\ngx', 'third_party\volk',
               'third_party\vulkan-headers', 'res\pad')   # (res\pad: gamepad button pictures, no gamepads here)
$buildFiles = @('.gitattributes', '.gitignore', 'build.bat', 'CMakeLists.txt', 'README.md', 'setup-and-build.bat',
                'setup-and-build.ps1', 'update.bat', 'update.bat.new', 'Make Update Pack.bat', 'Make Release.bat',
                'Getting Started - Release Copy.txt', 'res\lostworld.rc', 'res\lostworld.ico', 'res\logo.png')
$buildPatterns = @('Debug - *.bat', 'Cleanup - *.bat', '*.log', '*.tmp')
# - what is the player's own (kept by every update): their guns, the display
#   picked with the Display bats, the settings and records, the lettering
#   picked with the Text bats, and the ROM data (a full version adds the data,
#   the kept arenas and boss fights and the lettering; see -Release)
$ownDirs = @('data', 'save', 'textures\text')
$ownFiles = @('LightgunHID.ini', 'display.ini')
# - at the top, only files of the game's kinds (not pictures or archives left
#   lying here); the clear logo is the exception
$topKinds = @('.exe', '.dll', '.ahk', '.bat', '.txt', '.webp', '.ini', '')
# - third_party\menu: only the pictures and music the menu uses (not backups kept there)
$menuFiles = @('logo.png', 'background.jpg', 'background.png', 'music.mp3', 'music.wav', 'sega.png')

function Under($rel, $dirs) {
    foreach ($d in $dirs) { if ($rel.StartsWith($d + '\', [StringComparison]::OrdinalIgnoreCase)) { return $true } }
    return $false
}
$files = New-Object System.Collections.Generic.List[string]
# (the folders left out at the top are not even looked into: build and .git hold many thousands)
$items = @(Get-ChildItem -LiteralPath $root -File -Force) + @(Get-ChildItem -LiteralPath $root -Directory -Force |
    Where-Object { $buildDirs -notcontains $_.Name -and $ownDirs -notcontains $_.Name } |
    ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -File -Recurse -Force })
$items | ForEach-Object {
    $rel = $_.FullName.Substring($root.Length + 1)
    $key = $rel -replace '/', '\'
    if (Under $key $buildDirs) { return }
    if (Under $key $ownDirs) { return }
    if ($buildFiles -contains $key -or $ownFiles -contains $key) { return }
    foreach ($p in $buildPatterns) { if ($_.Name -like $p) { return } }
    if ($key -notmatch '\\') {   # at the top
        if ($topKinds -notcontains $_.Extension.ToLower() -and $_.Name -notlike '*Clear Logo*') { return }
    }
    if ((Under $key @('third_party\menu')) -and $menuFiles -notcontains $_.Name) { return }
    $files.Add($rel)
}
# A release has its own readme: 'Getting Started - Release Copy.txt' here (kept
# out of the repo) is shipped as its '1 - Getting Started Readme.txt'
$from = @{}
$releaseReadme = 'Getting Started - Release Copy.txt'
if (Test-Path -LiteralPath $releaseReadme -PathType Leaf) { $from['1 - Getting Started Readme.txt'] = $releaseReadme }
function Source($f) { if ($from.ContainsKey($f)) { $from[$f] } else { $f } }

# ---- The files now, and what was released -------------------------------------------
$packs = Join-Path $root 'Update Packs'
$versions = Join-Path $packs 'Versions'
New-Item -ItemType Directory -Force -Path $packs | Out-Null
$manifestPath = Join-Path $packs 'release-manifest.txt'
function Read-Manifest($path) {
    $m = @{}
    if (Test-Path -LiteralPath $path) {
        foreach ($line in Get-Content -LiteralPath $path) {
            $i = $line.IndexOf(' ')
            if ($i -gt 0) { $m[$line.Substring($i + 1)] = $line.Substring(0, $i) }
        }
    }
    return $m
}
$hashes = @{}
foreach ($f in $files) { $hashes[$f] = (Get-FileHash -LiteralPath (Source $f) -Algorithm SHA256).Hash }
function Save-Manifest($path) {
    $lines = $hashes.Keys | Sort-Object | ForEach-Object { "$($hashes[$_]) $_" }
    Set-Content -LiteralPath $path -Value $lines -Encoding UTF8
}
function Changed-Since($old) {
    $list = New-Object System.Collections.Generic.List[string]
    foreach ($f in ($files | Sort-Object)) { if (-not $old.ContainsKey($f) -or $old[$f] -ne $hashes[$f]) { $list.Add($f) } }
    return ,$list
}

# A pack: the files, a READ ME and a .zip of it
function Make-Pack($name, $title, $list, $removed) {
    $dir = Join-Path $packs $name
    if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    foreach ($f in $list) {
        $dst = Join-Path $dir $f
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item -LiteralPath (Source $f) -Destination $dst -Force
    }
    $how = if ($name -like '*(full)') { @(
        'A NEW COPY',
        '  1. Copy everything in this folder (except this file) into a new folder.',
        '  2. Read 1 - Getting Started Readme.txt (light guns first), then play.',
        '',
        'OR, TO UPDATE A COPY YOU HAVE') } else { @('HOW TO UPDATE') }
    $readme = @(
        "Lost World Native LG - $title",
        "Made $(Get-Date -Format 'yyyy-MM-dd HH:mm')",
        '') + $how + @(
        '  1. Close the game.',
        '  2. Open this folder, select everything in it (Ctrl+A) except this file,',
        '     and drag it into your game folder (the one with LostWorld.exe).',
        '  3. When Windows asks, choose "Replace the files in the destination".',
        '',
        'Your settings, records, guns (LightgunHID.ini), display.ini, the ROM data',
        'and your choice of in-game lettering are never in an update: they are kept.',
        'If this update lists files in textures\text_jurassic or textures\text_original,',
        'run your Text bat again (Text - Jurassic Park.bat or Text - Original.bat).',
        'This READ ME can be deleted afterwards.',
        '',
        "FILES IN THIS UPDATE ($($list.Count))"
    ) + ($list | ForEach-Object { '  ' + $_ })
    if ($removed.Count -gt 0) {
        $readme += @('', 'NO LONGER USED (you can delete them from your game folder)') + ($removed | ForEach-Object { '  ' + $_ })
    }
    Set-Content -LiteralPath (Join-Path $dir '_UPDATE - READ ME.txt') -Value $readme -Encoding UTF8
    $zip = "$dir.zip"
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip }
    Compress-Archive -Path (Join-Path $dir '*') -DestinationPath $zip
    Write-Host "  $name  ($($list.Count) files)"
    return $dir
}
function Removed-Since($old) { return @($old.Keys | Where-Object { -not $hashes.ContainsKey($_) } | Sort-Object) }

# ---- A version ------------------------------------------------------------------------
if ($Release) {
    $ver = $Release.Trim().TrimStart('v', 'V')
    if ($ver -notmatch '^\d+(\.\d+){0,3}$') { Write-Host "Not a version number: $Release (for example 2.0)"; exit 1 }
    if ($ver -notmatch '\.') { $ver += '.0' }   # 2 is 2.0
    New-Item -ItemType Directory -Force -Path $versions | Out-Null
    function As-Version($v) { $p = $v.Split('.'); while ($p.Count -lt 2) { $p += '0' }; [version]($p -join '.') }
    $mine = As-Version $ver
    $prev = Get-ChildItem -LiteralPath $versions -File -Filter 'v*.txt' |
        ForEach-Object { $v = $_.BaseName.Substring(1); if ($v -match '^\d+(\.\d+){0,3}$') { [pscustomobject]@{ Name = $v; Ver = (As-Version $v); Path = $_.FullName } } } |
        Where-Object { $_.Ver -lt $mine } | Sort-Object Ver | Select-Object -Last 1
    $target = Join-Path $versions "v$ver.txt"
    if (Test-Path -LiteralPath $target) {
        $answer = Read-Host "v$ver was already released. Replace it with the files now? Type YES"
        if ($answer -ne 'YES') { Write-Host 'Nothing made.'; exit 0 }
    }
    Write-Host ''
    Write-Host "Version $ver"
    $all = New-Object System.Collections.Generic.List[string]
    foreach ($f in ($files | Sort-Object)) { $all.Add($f) }
    # The kept arenas and boss fights (made by this build: start each once first)
    $kept = 0
    # (and the in-game lettering in use, textures\text, which updates leave alone: a new copy needs one)
    foreach ($pattern in @('save\survival\*.state', 'save\survival\unlocked.ini', 'save\boss\*.state', 'data\*.bin', 'textures\text\*', 'display.ini', 'LightgunHID.ini')) {
        Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue | ForEach-Object {
            $all.Add($_.FullName.Substring($root.Length + 1))
            $kept++
        }
    }
    Make-Pack "LostWorld LG v$ver (full)" "version $ver, complete and ready to play (with the game data)" $all @() | Out-Null
    Write-Host "  (with $kept of the player's files: data, kept arenas and boss fights, lettering, display.ini, LightgunHID.ini)"
    if ($prev) {
        $old = Read-Manifest $prev.Path
        $diff = Changed-Since $old
        if ($diff.Count -eq 0) { Write-Host "  (nothing differs from v$($prev.Name): no v$($prev.Name) to v$ver update)" }
        else { Make-Pack "LostWorld LG v$($prev.Name) to v$ver" "update from version $($prev.Name) to $ver" $diff (Removed-Since $old) | Out-Null }
    } else {
        Write-Host '  (the first version recorded: no update from an earlier one)'
    }
    Save-Manifest $target
    Save-Manifest $manifestPath   # test updates go on from here
    Write-Host ''
    Write-Host "Recorded as v$ver ($($files.Count) files), in Update Packs\Versions"
    exit 0
}

# ---- A test update ----------------------------------------------------------------------
$old = Read-Manifest $manifestPath
if ($Baseline) {
    Save-Manifest $manifestPath
    Write-Host "Recorded $($files.Count) files as the current release. The next update holds only what changes after this."
    exit 0
}
if ($old.Count -eq 0 -and -not $Full) {
    Write-Host 'No earlier update recorded: this one holds every release file (a complete update).'
}
$changed = if ($Full) { $l = New-Object System.Collections.Generic.List[string]; foreach ($f in ($files | Sort-Object)) { $l.Add($f) }; ,$l } else { Changed-Since $old }
if ($changed.Count -eq 0) {
    Write-Host 'Nothing changed since the last update: no pack made.'
    exit 0
}
$name = 'LostWorld LG Update ' + (Get-Date -Format 'yyyy-MM-dd HHmm')
Write-Host ''
Write-Host 'Update made:'
$dir = Make-Pack $name 'test update' $changed (Removed-Since $old)
Save-Manifest $manifestPath
Write-Host "  in $packs"
