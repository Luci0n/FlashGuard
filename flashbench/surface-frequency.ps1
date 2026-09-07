param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/artifacts/surface-frequency'
)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
$output = [IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$steps = @(
    @{ Name='shaders'; Args=@('--validate-shaders') },
    @{ Name='risk-integrator'; Args=@('--validate-risk-integrator') },
    @{ Name='focused'; Args=@('--surface-frequency-self-test', ('"' + (Join-Path $output 'self-test.json') + '"')) },
    @{ Name='canonical'; Args=@('--surface-frequency', '--synthetic-replay', ('"' + (Join-Path $output 'synthetic-replay.json') + '"'), '--replay-width', '640', '--replay-height', '360', '--replay-fps', '60') },
    @{ Name='1080p'; Args=@('--surface-frequency-benchmark', ('"' + (Join-Path $output '1080p-timing.json') + '"')) }
)
$results = @()
foreach ($step in $steps) {
    Write-Host "Surface frequency: $($step.Name)"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $exe -ArgumentList $step.Args -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $output ($step.Name + '.log')) `
        -RedirectStandardError (Join-Path $output ($step.Name + '-error.log'))
    $watch.Stop()
    $results += [pscustomobject]@{ name=$step.Name; exit_code=$process.ExitCode; elapsed_ms=$watch.ElapsedMilliseconds }
}
$replay = Get-Content -Raw -LiteralPath (Join-Path $output 'synthetic-replay.json') | ConvertFrom-Json
$focused = Get-Content -Raw -LiteralPath (Join-Path $output 'self-test.json') | ConvertFrom-Json
$sweep = Get-Content -Raw -LiteralPath (Join-Path $output 'flash-sweep.json') | ConvertFrom-Json
$perceptual = Get-Content -Raw -LiteralPath (Join-Path $output 'perceptual-sweep.json') | ConvertFrom-Json
$failedFlashCases = @($sweep.cases | Where-Object { -not $_.wcag_sc_2_3_1_pass -or -not $_.wcag_sc_2_3_2_pass })
$perceptualMinimum = ($perceptual.cases | Measure-Object -Property reduction -Minimum).Minimum
$pass = @($results | Where-Object exit_code -ne 0).Count -eq 0 -and
    $replay.pipeline -eq 'surface-frequency-v1' -and $focused.pass -and
    $failedFlashCases.Count -eq 0 -and $replay.moving_flash_reduction -ge .45 -and $perceptualMinimum -ge .70
[ordered]@{
    schema='SURFACE_FREQUENCY_VALIDATION/1'
    exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    passed=$pass
    focused_case_count=$focused.cases.Count
    regression_case_count=$focused.regressions.Count
    failed_regression_cases=@($focused.regressions | Where-Object { -not $_.pass }).Count
    flash_sweep_failed_cases=$failedFlashCases.Count
    perceptual_minimum_reduction=$perceptualMinimum
    steps=$results
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'validation.json') -Encoding utf8
if (-not $pass) { throw "Surface-frequency validation failed; inspect $output" }
Write-Host "Surface-frequency validation passed: $output"
