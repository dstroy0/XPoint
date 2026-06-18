# Build, run tests, write TEST_REPORT.md, and open it (Windows PowerShell)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root      = Join-Path $ScriptDir ".."
$Out       = Join-Path $Root "test_xpoint.exe"
$OutCustom = Join-Path $Root "test_custom.exe"
$Report    = Join-Path $ScriptDir "TEST_REPORT.md"

$Srcs = @(
    "src/XPoint.cpp",
    "src/drivers/BitPool.cpp",
    "src/drivers/ShiftRegisterDriver.cpp",
    "src/drivers/DirectGPIODriver.cpp",
    "src/drivers/MCP23017Driver.cpp",
    "src/drivers/TLC59711Driver.cpp"
)

Write-Host "Building host tests..."
$cmd = "g++ -std=c++11 -Wall -Wextra -Wpedantic -Isrc -Itest test/test_xpoint.cpp " + ($Srcs -join " ") + " -o `"$Out`""
Invoke-Expression $cmd 2>&1
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit $LASTEXITCODE }

Write-Host "Building custom test binary..."
$cmd2 = "g++ -std=c++11 -Wall -Wextra -Wpedantic -Isrc -Itest test/test_custom.cpp " + ($Srcs -join " ") + " -o `"$OutCustom`""
Invoke-Expression $cmd2 2>&1
if ($LASTEXITCODE -ne 0) { Write-Error "Custom build failed"; exit $LASTEXITCODE }

Write-Host "Running unit tests..."
# Forward any extra args (e.g. --skip-range) directly to the binary.
# The binary writes TEST_REPORT.md and also prints to stdout.
& $Out @args
$unitExit = $LASTEXITCODE

if ($unitExit -ne 0) { Write-Error "Unit tests failed (exit $unitExit)"; exit $unitExit }

Write-Host "Running custom tests..."
& $OutCustom --rows=4  --cols=4
& $OutCustom --rows=8  --cols=8  --latching --pulse=20
& $OutCustom --rows=1  --cols=16 --latching --pulse=50

Write-Host "Opening $Report ..."
Invoke-Item $Report
