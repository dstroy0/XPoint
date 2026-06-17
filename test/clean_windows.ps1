# Clean built binary and report (Windows PowerShell)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Join-Path $ScriptDir ".."
Remove-Item -Path (Join-Path $Root "test_xpoint.exe") -ErrorAction SilentlyContinue
Remove-Item -Path (Join-Path $Root "report.txt") -ErrorAction SilentlyContinue
Write-Output "Removed binary and report."
