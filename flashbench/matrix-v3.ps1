param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)

function Write-Utf8NoBom {
    param([string]$Path, [string[]]$Lines)
    $encoding = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllLines($Path, $Lines, $encoding)
}

function Invoke-ReplayBatch {
    param(
        [string]$Name,
        [object[]]$Specs,
        [int]$Width,
        [int]$Height,
        [switch]$Screening
    )
    $batchDir = Join-Path $OutputDir $Name
    New-Item -ItemType Directory -Force -Path $batchDir | Out-Null
    $planPath = Join-Path $batchDir 'plan.tsv'
    $lines = @('# name<TAB>profile<TAB>full<TAB>small<TAB>fps<TAB>motion_scale')
    foreach ($spec in $Specs) {
        $scale = ([double]$spec.motion_scale).ToString(
            '0.###', [Globalization.CultureInfo]::InvariantCulture)
        $lines += @(
            "$($spec.name)`t$($spec.profile)`t$($spec.full)`t$($spec.small)`t$($spec.fps)`t$scale"
        )
    }
    Write-Utf8NoBom -Path $planPath -Lines $lines

    $arguments = @(
        '--synthetic-replay-batch', ('"' + $planPath + '"'),
        '--synthetic-replay-batch-output', ('"' + $batchDir + '"'),
        '--replay-width', [string]$Width,
        '--replay-height', [string]$Height
    )
    if ($Screening) { $arguments += '--replay-screening' }

    $sw = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -Wait -PassThru
    $sw.Stop()
    if ($process.ExitCode -ne 0) {
        throw "Replay batch '$Name' failed with exit code $($process.ExitCode)"
    }
    $batchReport = Join-Path $batchDir 'batch.json'
    if (-not (Test-Path $batchReport)) {
        throw "Replay batch '$Name' did not produce batch.json"
    }
    $batch = Get-Content -Raw $batchReport | ConvertFrom-Json
    if ($batch.status -ne 'SUCCESS') {
        throw "Replay batch '$Name' reported $($batch.status)"
    }
    return [pscustomobject]@{
        directory = $batchDir
        elapsed_ms = $sw.ElapsedMilliseconds
        batch = $batch
    }
}

function Read-Candidate {
    param([object]$Spec, [string]$BatchDir)
    $caseDir = Join-Path $BatchDir $Spec.name
    $replayPath = Join-Path $caseDir 'synthetic-replay.json'
    $sweepPath = Join-Path $caseDir 'flash-sweep.json'
    $trailPath = Join-Path $caseDir 'trail-metrics.json'
    $perceptualPath = Join-Path $caseDir 'perceptual-sweep.json'
    if (-not (Test-Path $replayPath) -or -not (Test-Path $sweepPath) -or
        -not (Test-Path $trailPath)) {
        throw "Missing replay/trail result for $($Spec.name)"
    }
    $replay = Get-Content -Raw $replayPath | ConvertFrom-Json
    $sweep = Get-Content -Raw $sweepPath | ConvertFrom-Json
    $trail = Get-Content -Raw $trailPath | ConvertFrom-Json
    $perceptualExecuted = Test-Path $perceptualPath
    $perceptualMin = 0.0
    $perceptualMax = 0.0
    if ($perceptualExecuted) {
        $perceptual = Get-Content -Raw $perceptualPath | ConvertFrom-Json
        if (@($perceptual.cases).Count -gt 0) {
            $perceptualMin = [double](
                ($perceptual.cases | Measure-Object -Property reduction -Minimum).Minimum)
            $perceptualMax = [double](
                ($perceptual.cases | Measure-Object -Property peak_output_delta -Maximum).Maximum)
        }
    }
    $sc231 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true }).Count -eq 0
    $sc232 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true }).Count -eq 0
    $minSweepReduction = ($sweep.cases | Measure-Object -Property reduction -Minimum).Minimum
    [pscustomobject]@{
        name = $Spec.name
        profile = [int]$Spec.profile
        full_sensitivity = [int]$Spec.full
        small_sensitivity = [int]$Spec.small
        fps = [int]$Spec.fps
        motion_scale = [double]$Spec.motion_scale
        replay_status = $replay.status
        replay_schema = $replay.schema
        trail_schema = $trail.schema
        sc231_pass = $sc231
        sc232_pass = $sc232
        static_mae = [double]$replay.static_mae
        flash_reduction = [double]$replay.flash_reduction
        moving_flash_reduction = [double]$replay.moving_flash_reduction
        moving_ghost_mae = [double]$replay.moving_square_ghost_mae
        moving_vacated_mean_mae = [double]$trail.moving_square_vacated_mean_mae
        moving_vacated_p95_max = [double]$trail.moving_square_vacated_p95_max
        moving_vacated_p99_max = [double]$trail.moving_square_vacated_p99_max
        moving_vacated_peak = [double]$trail.moving_square_vacated_peak
        moving_vacated_area_02_max = [double]$trail.moving_square_vacated_area_above_0_02_max
        moving_vacated_area_05_max = [double]$trail.moving_square_vacated_area_above_0_05_max
        moving_clear_01_ms = [double]$trail.moving_square_clear_to_0_01_ms
        moving_clear_02_ms = [double]$trail.moving_square_clear_to_0_02_ms
        moving_clear_05_ms = [double]$trail.moving_square_clear_to_0_05_ms
        small_vacated_p99_max = [double]$trail.small_moving_square_vacated_p99_max
        small_vacated_peak = [double]$trail.small_moving_square_vacated_peak
        pan_mae = [double]$replay.pan_mae
        min_wcag_sweep_reduction = [double]$minSweepReduction
        perceptual_sweep_executed = [bool]$perceptualExecuted
        perceptual_sweep_min_reduction = [double]$perceptualMin
        perceptual_sweep_max_output_delta = [double]$perceptualMax
    }
}

function Test-Dominates {
    param([object]$A, [object]$B)
    # No scalar weighted score: retain candidates that are Pareto-optimal across
    # safety attenuation and perceptual motion damage.
    $perceptualComparable =
        $A.perceptual_sweep_executed -and $B.perceptual_sweep_executed
    $perceptualNoWorse = -not $perceptualComparable -or (
        $A.perceptual_sweep_min_reduction -ge $B.perceptual_sweep_min_reduction -and
        $A.perceptual_sweep_max_output_delta -le $B.perceptual_sweep_max_output_delta)
    $noWorse =
        $A.flash_reduction -ge $B.flash_reduction -and
        $A.moving_flash_reduction -ge $B.moving_flash_reduction -and
        $A.moving_vacated_p99_max -le $B.moving_vacated_p99_max -and
        $A.moving_vacated_peak -le $B.moving_vacated_peak -and
        $A.small_vacated_p99_max -le $B.small_vacated_p99_max -and
        $A.pan_mae -le $B.pan_mae -and
        $A.static_mae -le $B.static_mae -and
        $perceptualNoWorse
    if (-not $noWorse) { return $false }
    return (
        $A.flash_reduction -gt $B.flash_reduction -or
        $A.moving_flash_reduction -gt $B.moving_flash_reduction -or
        $A.moving_vacated_p99_max -lt $B.moving_vacated_p99_max -or
        $A.moving_vacated_peak -lt $B.moving_vacated_peak -or
        $A.small_vacated_p99_max -lt $B.small_vacated_p99_max -or
        $A.pan_mae -lt $B.pan_mae -or
        $A.static_mae -lt $B.static_mae -or
        ($perceptualComparable -and (
            $A.perceptual_sweep_min_reduction -gt $B.perceptual_sweep_min_reduction -or
            $A.perceptual_sweep_max_output_delta -lt $B.perceptual_sweep_max_output_delta))
    )
}

function Get-ParetoFrontier {
    param([object[]]$Candidates)
    $frontier = @()
    foreach ($candidate in $Candidates) {
        $dominated = $false
        foreach ($other in $Candidates) {
            if ($other.name -eq $candidate.name) { continue }
            if (Test-Dominates -A $other -B $candidate) {
                $dominated = $true
                break
            }
        }
        if (-not $dominated) { $frontier += $candidate }
    }
    return $frontier
}

function Add-RelativeRegret {
    param([object[]]$Candidates, [object]$Baseline)
    $eps = 1e-7
    foreach ($candidate in $Candidates) {
        $candidateFlash = [Math]::Max($eps, [double]$candidate.flash_reduction)
        $candidateMovingFlash = [Math]::Max(
            $eps, [double]$candidate.moving_flash_reduction)
        $ratios = @(
            (([double]$candidate.moving_vacated_p99_max + $eps) / ([double]$Baseline.moving_vacated_p99_max + $eps)),
            (([double]$candidate.moving_vacated_peak + $eps) / ([double]$Baseline.moving_vacated_peak + $eps)),
            (([double]$candidate.small_vacated_p99_max + $eps) / ([double]$Baseline.small_vacated_p99_max + $eps)),
            (([double]$candidate.pan_mae + $eps) / ([double]$Baseline.pan_mae + $eps)),
            (([double]$Baseline.flash_reduction + $eps) / $candidateFlash),
            (([double]$Baseline.moving_flash_reduction + $eps) / $candidateMovingFlash)
        )
        if ($candidate.perceptual_sweep_executed -and $Baseline.perceptual_sweep_executed) {
            $candidatePerceptual = [Math]::Max(
                $eps, [double]$candidate.perceptual_sweep_min_reduction)
            $ratios += (([double]$Baseline.perceptual_sweep_min_reduction + $eps) /
                $candidatePerceptual)
            $ratios += (([double]$candidate.perceptual_sweep_max_output_delta + $eps) /
                ([double]$Baseline.perceptual_sweep_max_output_delta + $eps))
        }
        $candidate | Add-Member -NotePropertyName max_relative_regret `
            -NotePropertyValue (($ratios | Measure-Object -Maximum).Maximum) -Force
    }
    return $Candidates
}

# Stage 1: all independent profile/full/small combinations, not just paired
# sensitivities. 320x180 + 30 fps + screening durations makes this cheap enough
# to run on every normal experiment while still exercising real D3D11/NVOFA.
$screenSpecs = @()
foreach ($profile in 0..2) {
    foreach ($full in 0..2) {
        foreach ($small in 0..2) {
            $screenSpecs += [pscustomobject]@{
                name = "profile_${profile}_full_${full}_small_${small}"
                profile = $profile
                full = $full
                small = $small
                fps = 30
                motion_scale = 1.0
            }
        }
    }
}

Write-Host "FlashBench v6 screen: $($screenSpecs.Count) configurations in one GPU session"
$screenBatch = Invoke-ReplayBatch -Name 'screen' -Specs $screenSpecs `
    -Width 320 -Height 180 -Screening
$screenResults = @($screenSpecs | ForEach-Object {
    Read-Candidate -Spec $_ -BatchDir $screenBatch.directory
})
$screenEligible = @($screenResults | Where-Object {
    $_.replay_status -eq 'SUCCESS' -and $_.sc231_pass -and $_.sc232_pass
})
if ($screenEligible.Count -eq 0) { $screenEligible = $screenResults }
$screenBaseline = $screenResults | Where-Object {
    $_.profile -eq 1 -and $_.full_sensitivity -eq 1 -and $_.small_sensitivity -eq 1
} | Select-Object -First 1
if (-not $screenBaseline) { $screenBaseline = $screenResults | Select-Object -First 1 }
$screenEligible = @(Add-RelativeRegret -Candidates $screenEligible -Baseline $screenBaseline)
$screenFrontier = @(Get-ParetoFrontier -Candidates $screenEligible)
$screenBest = $screenFrontier | Sort-Object max_relative_regret | Select-Object -First 1
if (-not $screenBest) { $screenBest = $screenEligible | Sort-Object max_relative_regret | Select-Object -First 1 }

if ($ScreenOnly) {
    [pscustomobject]@{
        schema = 'FLASHGUARD_MATRIX/3'
        mode = 'screen-only'
        selection = 'pareto frontier then minimum worst relative regression versus production default'
        screen_batch_elapsed_ms = $screenBatch.elapsed_ms
        screen_candidates = $screenResults
        screen_pareto_frontier = @($screenFrontier | ForEach-Object { $_.name })
        selected = $screenBest
    } | ConvertTo-Json -Depth 10 | Set-Content -Encoding utf8 `
        (Join-Path $OutputDir 'matrix.json')
    exit 0
}

# Stage 2: only the screen winner and production default pay for canonical
# 640x360/60 Hz verification and the low-contrast perceptual sweep.
$verifySeed = @($screenBest)
if ($screenBest.name -ne $screenBaseline.name) { $verifySeed += $screenBaseline }
if ($verifySeed.Count -lt 2 -and $screenFrontier.Count -gt 1) {
    $alternative = $screenFrontier | Where-Object { $_.name -ne $screenBest.name } |
        Sort-Object max_relative_regret | Select-Object -First 1
    if ($alternative) { $verifySeed += $alternative }
}
$verifySpecs = @($verifySeed | Select-Object -First 2 | ForEach-Object {
    [pscustomobject]@{
        name = $_.name
        profile = $_.profile
        full = $_.full_sensitivity
        small = $_.small_sensitivity
        fps = 60
        motion_scale = 1.0
    }
})

Write-Host "FlashBench v6 verify: $($verifySpecs.Count) configurations in one GPU session"
$verifyBatch = Invoke-ReplayBatch -Name 'verify' -Specs $verifySpecs -Width 640 -Height 360
$verifyResults = @($verifySpecs | ForEach-Object {
    Read-Candidate -Spec $_ -BatchDir $verifyBatch.directory
})
$verifyBaseline = $verifyResults | Where-Object {
    $_.profile -eq 1 -and $_.full_sensitivity -eq 1 -and $_.small_sensitivity -eq 1
} | Select-Object -First 1
if (-not $verifyBaseline) { $verifyBaseline = $verifyResults | Select-Object -First 1 }
$verifyResults = @(Add-RelativeRegret -Candidates $verifyResults -Baseline $verifyBaseline)
$verifyEligible = @($verifyResults | Where-Object {
    $_.replay_status -eq 'SUCCESS' -and $_.sc231_pass -and $_.sc232_pass
})
if ($verifyEligible.Count -eq 0) { $verifyEligible = $verifyResults }
$verifyFrontier = @(Get-ParetoFrontier -Candidates $verifyEligible)
$selected = $verifyFrontier | Sort-Object max_relative_regret | Select-Object -First 1
if (-not $selected) { $selected = $verifyEligible | Sort-Object max_relative_regret | Select-Object -First 1 }

[pscustomobject]@{
    schema = 'FLASHGUARD_MATRIX/3'
    mode = 'screen-then-verify'
    selection = 'pareto frontier then minimum worst relative regression versus production default'
    screen_batch_elapsed_ms = $screenBatch.elapsed_ms
    verify_batch_elapsed_ms = $verifyBatch.elapsed_ms
    screen_candidates = $screenResults
    screen_pareto_frontier = @($screenFrontier | ForEach-Object { $_.name })
    verify_candidates = $verifyResults
    verify_pareto_frontier = @($verifyFrontier | ForEach-Object { $_.name })
    selected = $selected
} | ConvertTo-Json -Depth 10 | Set-Content -Encoding utf8 `
    (Join-Path $OutputDir 'matrix.json')

Write-Host "Selected: $($selected.name)"
exit 0
