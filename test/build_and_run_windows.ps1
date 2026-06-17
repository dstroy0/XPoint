# Build and run tests on Windows (PowerShell)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Join-Path $ScriptDir ".."
$Out = Join-Path $Root "test_xpoint.exe"
$Report = Join-Path $Root "report.txt"

Write-Output "Building host tests..." | Tee-Object -FilePath $Report
$cmd = "g++ -std=c++11 -Isrc -Itest test/test_xpoint.cpp src/XPoint.cpp src/drivers/ShiftRegisterDriver.cpp src/drivers/DirectGPIODriver.cpp src/drivers/MCP23017Driver.cpp src/drivers/TLC59711Driver.cpp -o `"$Out`""
Write-Output $cmd | Tee-Object -FilePath $Report -Append
Invoke-Expression $cmd 2>&1 | Tee-Object -FilePath $Report -Append
if ($LASTEXITCODE -ne 0) { Write-Output "Build failed with exit $LASTEXITCODE" | Tee-Object -FilePath $Report -Append; exit $LASTEXITCODE }

Write-Output "Running tests..." | Tee-Object -FilePath $Report -Append
& $Out 2>&1 | Tee-Object -FilePath $Report -Append
$exitCode = $LASTEXITCODE
Write-Output "TEST EXIT CODE: $exitCode" | Tee-Object -FilePath $Report -Append
exit $exitCode
