# One-click setup for The Movies static recompilation. Run it by double-clicking
# Setup.cmd in the repo folder; the README's "Step by step" is the same thing by
# hand, command for command.
#
# The project is at P1 (see the README), so this takes your copy as far as the
# pipeline goes today: copy your install into game\, unpack the executables,
# analyse them, and build the function catalog. Nothing is built or run yet, so
# there is no shortcut at the end. Each step is skipped when its output already
# exists (-Force redoes them). Everything it does goes to setup.log.
param([switch]$Force, [string]$Game = "")

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Toolkit = Join-Path (Split-Path -Parent $Root) 'tools'
$T = Join-Path $Toolkit 'tools'
$Log = Join-Path $Root 'setup.log'
Set-Location $Root
function Log($t) { Add-Content -Path $Log -Value $t -Encoding UTF8 }
Log "==== setup $(Get-Date -Format s)"

function Say($t, $c = 'Gray') { Write-Host $t -ForegroundColor $c; Log $t }
function Step($n, $t) { Write-Host ""; Say "[$n/5] $t" 'Cyan' }
function Fail($t) {
  Say "" ; Say "Setup stopped: $t" 'Red'
  Say "The details are in $Log. Fix that and run Setup.cmd again; finished steps are skipped." 'Yellow'
  Read-Host "Press Enter to close" | Out-Null; exit 1
}
function Ask($q) { $a = Read-Host "$q [Y/n]"; return -not ($a -match '^[nN]') }   # Enter = yes
function Refresh-Path {
  $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}
# Run a program with its output in the log. Windows PowerShell turns a native
# program's stderr into errors, so 'Stop' is off while it runs.
function Exec([string[]]$cmd) {
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  $rest = @($cmd | Select-Object -Skip 1)   # @(): a one-element slice would splat as characters
  & $cmd[0] @rest 2>&1 | ForEach-Object { Log "$_" }
  $code = $LASTEXITCODE
  $ErrorActionPreference = $old
  return $code
}
function Run($what, [string[]]$cmd) {
  Say "  $what..."
  $code = Exec $cmd
  if ($code -ne 0) { Fail "$what failed (exit code $code)." }
}

Clear-Host
Say "The Movies: static recompilation setup" 'White'
Say "You need your installed copy of The Movies (the Steam build) and about 4 GB free."
Say "Most of this is quick; the last step, the function catalog, takes a long time once."

# ---------------------------------------------------------------- tools
Step 1 "Checking the tools the pipeline needs"
# A Python that answers "Python 3.x". The Store's placeholder "python" (the
# one that opens the Store) answers nothing, so asking is the reliable test.
function Find-Python {
  foreach ($c in @(@('py', '-3'), @('python'), @('python3'))) {
    if (-not (Get-Command $c[0] -ErrorAction SilentlyContinue)) { continue }
    $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $rest = @($c | Select-Object -Skip 1)
    $v = (& $c[0] @rest --version 2>&1 | Out-String).Trim()
    $ErrorActionPreference = $old
    if ($v -match '^Python 3\.(\d+)' -and [int]$Matches[1] -ge 10) { return ,$c }
  }
  return $null
}
$pyargs = Find-Python
if (-not $pyargs) {
  Say "  Python 3.10 or newer is not installed."
  if (-not (Ask "  Install Python 3.12 now (winget, for your user only, about 30 MB)?")) { Fail "Python 3 is required." }
  if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Fail "winget is missing. Install 'App Installer' from the Microsoft Store, or install Python yourself."
  }
  Exec @('winget', 'install', '-e', '--id', 'Python.Python.3.12', '--scope', 'user', '--accept-package-agreements', '--accept-source-agreements') | Out-Null
  Refresh-Path
  $pyargs = Find-Python
  if (-not $pyargs) { Fail "Python installed, but Windows has not picked it up yet: close this window and run Setup.cmd again." }
}
Say "  Python: $($pyargs -join ' ')"

if ((Exec ($pyargs + @('-c', 'import pefile, capstone, unicorn'))) -ne 0) {
  Say "  The Python packages pefile, capstone and unicorn are missing (about 40 MB)."
  Say "  unicorn runs the executables' unpacker without starting the game."
  if (-not (Ask "  Install them now (pip, for your user only)?")) { Fail "pefile, capstone and unicorn are required." }
  Run "Installing pefile, capstone and unicorn" ($pyargs + @('-m', 'pip', 'install', '--user', 'pefile', 'capstone', 'unicorn'))
}

if (-not (Test-Path (Join-Path $T 'lift\lift32.py'))) {
  Say "  The pcrecomp toolkit is not beside this folder ($Toolkit)."
  if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail "git is needed to fetch pcrecomp. Install Git for Windows, then run Setup.cmd again." }
  if (-not (Ask "  Download it now (git, about 20 MB)?")) { Fail "pcrecomp is required at $Toolkit." }
  Run "Cloning pcrecomp" @('git', 'clone', '--depth', '1', 'https://github.com/sp00nznet/pcrecomp', $Toolkit)
}
if (-not (Test-Path (Join-Path $T 'drm\emu_unpack.py'))) {
  Fail "your pcrecomp at $Toolkit predates drm\emu_unpack.py. Update it: git -C `"$Toolkit`" pull"
}
Say "  pcrecomp: $Toolkit"

# ---------------------------------------------------------------- the game
Step 2 "Copying your copy of The Movies into game\"
# A folder is the install if it holds Movies.exe; MoviesSE.exe (Stunts &
# Effects) is used when it is there too.
function Is-Install([string]$d) { return ($d -and (Test-Path (Join-Path $d 'Movies.exe'))) }
function Find-Steam {
  $roots = @()
  foreach ($k in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam') {
    try { $roots += (Get-ItemProperty $k -ErrorAction Stop).SteamPath } catch {}
    try { $roots += (Get-ItemProperty $k -ErrorAction Stop).InstallPath } catch {}
  }
  foreach ($r in ($roots | Where-Object { $_ } | Select-Object -Unique)) {
    $libs = @($r)
    $vdf = Join-Path $r 'steamapps\libraryfolders.vdf'
    if (Test-Path $vdf) {
      $libs += (Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"' | ForEach-Object { $_.Matches[0].Groups[1].Value -replace '\\\\', '\' })
    }
    foreach ($l in $libs) {
      $d = Join-Path $l 'steamapps\common\The Movies'
      if (Is-Install $d) { return $d }
    }
  }
  return ""
}

if ((Test-Path 'game\Movies.exe') -and -not $Force) {
  Say "  Already in game\ (skipping)."
} else {
  $Game = $Game.Trim('"', ' ')
  if (-not (Is-Install $Game)) {
    $Game = Find-Steam
    if ($Game) { Say "  Found it in your Steam library: $Game" }
  }
  while (-not (Is-Install $Game)) {
    $Game = (Read-Host "  Paste the folder The Movies is installed in (the one with Movies.exe)").Trim('"', ' ')
    if (-not (Is-Install $Game)) { Say "  No Movies.exe in that folder." 'Yellow' }
  }
  Say "  Copying $Game (about 2.6 GB)..."
  $code = Exec @('robocopy', $Game, (Join-Path $Root 'game'), '/E', '/NFL', '/NDL', '/NJH', '/NP')
  if ($code -ge 8) { Fail "copying the game failed (robocopy exit code $code)." }   # robocopy: <8 is success
}
$Target = if (Test-Path 'game\MoviesSE.exe') { 'MoviesSE' } else { 'Movies' }
Say "  Recompilation target: $Target.exe"

# ---------------------------------------------------------------- unpack
Step 3 "Unpacking the executables (a few seconds each)"
New-Item -ItemType Directory -Force work | Out-Null
foreach ($e in 'Movies', 'MoviesSE', 'StarMaker') {
  if (-not (Test-Path "game\$e.exe")) { continue }
  if ((Test-Path "work\$e.unpacked.exe") -and -not $Force) { Say "  $e.exe already unpacked (skipping)."; continue }
  Run "Unpacking $e.exe" ($pyargs + @("$T\drm\emu_unpack.py", "game\$e.exe", "work\$e.unpacked.exe"))
}

# ---------------------------------------------------------------- analyse
Step 4 "Analysing $Target.exe (a minute)"
$U = "work\$Target.unpacked.exe"
if ((Test-Path 'work\rtti_seeds.json') -and -not $Force) { Say "  Already done (skipping)." }
else {
  Run "Headers and imports" ($pyargs + @("$T\pe\pe_analyze.py", $U, '--json', 'work\pe_analysis.json'))
  Run "C++ classes from RTTI" ($pyargs + @("$T\cpp\rtti.py", $U, '-o', 'work\rtti.json', '--seeds', 'work\rtti_seeds.json'))
}

# ---------------------------------------------------------------- catalog
Step 5 "Finding every function (this is the long one: a couple of hours, once)"
if ((Test-Path 'work\functions.json') -and -not $Force) { Say "  Already done (skipping)." }
else {
  Run "Disassembling" ($pyargs + @("$T\disasm\disasm32.py", $U, '-o', 'work\functions.json', '--seed-functions', 'work\rtti_seeds.json'))
}

Write-Host ""
Say "Done: work\ holds the unpacked executables and the function catalog." 'Green'
Say "There is nothing to play yet. The README's Status section says how far the project is."
Read-Host "Press Enter to close" | Out-Null
