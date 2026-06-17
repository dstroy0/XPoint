<#
.SYNOPSIS
Build and run the XPoint custom parameterised test.

.DESCRIPTION
Compiles test_xpoint and invokes it with --custom and the supplied matrix
configuration. If parameters are omitted the script prompts interactively.

.PARAMETER Driver
Hardware profile preset. Determines default rows/cols:
  gpio            Arduino Direct GPIO        4 x 4  non-latching
  shift_register  74HC595 Shift Register     8 x 8  non-latching
  mcp23017        MCP23017 I2C Expander      2 x 8  non-latching
  tlc59711        TLC59711 PWM Expander      1 x 12 non-latching
  latching        Dual-coil latching relay   4 x 4  latching (20 ms)
  custom          Specify every value manually

.PARAMETER Rows
Matrix row count. Overrides the profile default.

.PARAMETER Cols
Matrix column count. Overrides the profile default.

.PARAMETER Latching
Switch — enable latching dual-coil relay mode.

.PARAMETER PulseDuration
Coil pulse duration in milliseconds (default 20, latching mode only).

.EXAMPLE
.\test\run_custom_test.ps1 -Driver shift_register
.\test\run_custom_test.ps1 -Driver custom -Rows 6 -Cols 6
.\test\run_custom_test.ps1 -Driver latching -Rows 8 -Cols 8 -PulseDuration 50
#>
[CmdletBinding()]
param(
    [string] $Driver = "",
    [int]    $Rows = 0,
    [int]    $Cols = 0,
    [switch] $Latching,
    [int]    $PulseDuration = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

# ---------------------------------------------------------------------------
# Hardware profile table
# ---------------------------------------------------------------------------
$Profiles = [ordered]@{
    "gpio"           = @{ Rows = 4; Cols = 4; Latching = $false; Label = "Arduino Direct GPIO       4 x 4  non-latching" }
    "shift_register" = @{ Rows = 8; Cols = 8; Latching = $false; Label = "74HC595 Shift Register    8 x 8  non-latching" }
    "mcp23017"       = @{ Rows = 2; Cols = 8; Latching = $false; Label = "MCP23017 I2C Expander     2 x 8  non-latching" }
    "tlc59711"       = @{ Rows = 1; Cols = 12; Latching = $false; Label = "TLC59711 PWM Expander     1 x 12 non-latching" }
    "latching"       = @{ Rows = 4; Cols = 4; Latching = $true; Label = "Dual-coil latching relay  4 x 4  latching (20 ms)" }
    "custom"         = @{ Rows = 0; Cols = 0; Latching = $false; Label = "Custom — specify manually" }
}

# ---------------------------------------------------------------------------
# Interactive profile selection (if not supplied on the command line)
# ---------------------------------------------------------------------------
if (-not $Driver) {
    Write-Host ""
    Write-Host "XPoint custom test — select a hardware profile"
    Write-Host "----------------------------------------------"
    $i = 1
    foreach ($key in $Profiles.Keys) {
        Write-Host ("  {0})  {1,-18}  {2}" -f $i, $key, $Profiles[$key].Label)
        $i++
    }
    Write-Host ""
    $Choice = (Read-Host "Profile (name or 1-$($Profiles.Count))").Trim()

    $num = 0
    if ([int]::TryParse($Choice, [ref]$num) -and $num -ge 1 -and $num -le $Profiles.Count) {
        $Driver = @($Profiles.Keys)[$num - 1]
    }
    else {
        $Driver = $Choice.ToLower()
    }
}

if (-not $Profiles.ContainsKey($Driver)) {
    Write-Error "Unknown profile '$Driver'. Valid choices: $($Profiles.Keys -join ', ')"
    exit 1
}

# Apply profile defaults (command-line values take precedence)
$P = $Profiles[$Driver]
if ($Rows -eq 0) { $Rows = $P.Rows }
if ($Cols -eq 0) { $Cols = $P.Cols }
if (-not $Latching) { $Latching = $P.Latching }

# Prompt for anything still unresolved
if ($Rows -eq 0) {
    $Rows = [int](Read-Host "Rows")
}
if ($Cols -eq 0) {
    $Cols = [int](Read-Host "Cols")
}
if (-not $Latching) {
    $ans = (Read-Host "Latching relay mode? [y/N]").Trim()
    if ($ans -match '^[Yy]') { $Latching = $true }
}
if ($Latching -and $PulseDuration -eq 20) {
    $ans = (Read-Host "Pulse duration ms [20]").Trim()
    if ($ans -match '^\d+$') { $PulseDuration = [int]$ans }
}

Write-Host ""
$ModeStr = if ($Latching) { "latching  pulse=${PulseDuration}ms" } else { "non-latching" }
Write-Host "Profile   : $Driver"
Write-Host "Matrix    : $Rows x $Cols"
Write-Host "Mode      : $ModeStr"
Write-Host ""

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
$Exe = Join-Path $Root "test_xpoint_custom.exe"

$Sources = @(
    "test/test_xpoint.cpp",
    "src/XPoint.cpp",
    "src/drivers/DirectGPIODriver.cpp",
    "src/drivers/ShiftRegisterDriver.cpp",
    "src/drivers/MCP23017Driver.cpp",
    "src/drivers/TLC59711Driver.cpp"
)

$CmdLine = "g++ -std=c++11 -Isrc -Itest " + ($Sources -join " ") + " -o `"$Exe`""

Push-Location $Root
try {
    Write-Host "Building..."
    Invoke-Expression $CmdLine 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed (exit $LASTEXITCODE)." -ForegroundColor Red
        exit 1
    }

    # ---------------------------------------------------------------------------
    # Run
    # ---------------------------------------------------------------------------
    $RunArgs = @("--custom", "--rows=$Rows", "--cols=$Cols")
    if ($Latching) {
        $RunArgs += "--latching"
        $RunArgs += "--pulse=$PulseDuration"
    }

    & $Exe @RunArgs
    $ExitCode = $LASTEXITCODE
}
finally {
    Pop-Location
    Remove-Item $Exe -ErrorAction SilentlyContinue
}

exit $ExitCode
