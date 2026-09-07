param([string]$RunnerDir = 'C:\actions-runner')

$ErrorActionPreference = 'Stop'
$run = Join-Path $RunnerDir 'run.cmd'
$config = Join-Path $RunnerDir '.runner'
if (-not (Test-Path $run) -or -not (Test-Path $config)) {
    throw "No configured GitHub Actions runner found at $RunnerDir. Register this repo's Windows x64 self-hosted runner first and add label: flashguard-gpu"
}

Write-Host 'Starting FlashGuard GPU runner in the interactive Windows session.'
Write-Host 'Keep this terminal/session logged in while GPU tests are expected.'
& $run
exit $LASTEXITCODE
