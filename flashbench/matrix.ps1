param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

function Measure-Replay {
    param(
        [string]$Name,
        [int]$Profile,
        [int]$FullSensitivity,
        [int]$SmallSensitivity,
        [int]$Fps = 60,
        [double]$MotionScale = 1.0,
        [string]$Group = 'cases'
    )

    $caseDir = Join-Path (Join-Path $OutputDir $Group) $Name
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $reportPath = Join-Path $caseDir 'synthetic-replay.json'

    & $exe --synthetic-replay $reportPath `
        --replay-profile $Profile `
        --replay-full-sensitivity $FullSensitivity `
        --replay-small-sensitivity $SmallSensitivity `
        --replay-fps $Fps `
        --replay-motion-scale $MotionScale
    $exitCode = $LASTEXITCODE

    $sweepPath = Join-Path $caseDir 'flash-sweep.json'
    if (-not (Test-Path $reportPath) -or -not (Test-Path $sweepPath)) {
        return [pscustomobject]@{
            name = $Name
            profile = $Profile
            full_sensitivity = $FullSensitivity
            small_sensitivity = $SmallSensitivity
            fps = $Fps
            motion_scale = $MotionScale
            exit_code = $exitCode
            pass = $false
            score = 1e12
            sc231_failures = 999
            sc232_failures = 999
            max_general_flashes_per_second = 999.0
            max_red_flashes_per_second = 999.0
            max_strict_transitions_per_second = 999.0
            motion_penalty = 999.0
        }
    }

    $replay = Get-Content -Raw $reportPath | ConvertFrom-Json
    $sweep = Get-Content -Raw $sweepPath | ConvertFrom-Json
    $sc231Failures = @(
        $sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true }
    ).Count
    $sc232Failures = @(
        $sweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true }
    ).Count
    $maxGeneral = [double](
        $sweep.cases |
            Measure-Object -Property output_general_flashes_per_second -Maximum
    ).Maximum
    $maxRed = [double](
        $sweep.cases |
            Measure-Object -Property output_red_flashes_per_second -Maximum
    ).Maximum
    $maxStrict = [double](
        $sweep.cases |
            Measure-Object -Property output_strict_transitions_per_second -Maximum
    ).Maximum

    $motionPenalty =
        [double]$replay.moving_square_ghost_mae / 0.005 +
        [double]$replay.moving_square_edge_mae / 0.005 +
        [double]$replay.small_moving_square_ghost_mae / 0.003 +
        [double]$replay.pan_mae / 0.010 +
        [double]$replay.fast_pan_mae / 0.020 +
        [double]$replay.extreme_pan_mae / 0.030

    # Safety failures dominate. Motion then breaks ties between similarly safe
    # candidates, preventing a "winning" curve from simply smearing the scene.
    $score =
        1000.0 * ($sc231Failures + $sc232Failures) +
        50.0 * $maxRed +
        25.0 * $maxGeneral +
        8.0 * ($maxStrict / 6.0) +
        20.0 * $motionPenalty

    [pscustomobject]@{
        name = $Name
        profile = $Profile
        full_sensitivity = $FullSensitivity
        small_sensitivity = $SmallSensitivity
        fps = $Fps
        motion_scale = $MotionScale
        exit_code = $exitCode
        pass = ($replay.status -eq 'SUCCESS' -and
            $sweep.status -eq 'SUCCESS')
        score = $score
        sc231_failures = $sc231Failures
        sc232_failures = $sc232Failures
        max_general_flashes_per_second = $maxGeneral
        max_red_flashes_per_second = $maxRed
        max_strict_transitions_per_second = $maxStrict
        motion_penalty = $motionPenalty
        moving_ghost_mae = [double]$replay.moving_square_ghost_mae
        moving_edge_mae = [double]$replay.moving_square_edge_mae
        pan_mae = [double]$replay.pan_mae
        fast_pan_mae = [double]$replay.fast_pan_mae
        extreme_pan_mae = [double]$replay.extreme_pan_mae
    }
}

$candidateSpecs = @(
    @{ name = 'balanced'; profile = 1; full = 1; small = 1 },
    @{ name = 'balanced_high_detection'; profile = 1; full = 2; small = 2 },
    @{ name = 'maximum_curves'; profile = 2; full = 1; small = 1 },
    @{ name = 'maximum_high_detection'; profile = 2; full = 2; small = 2 },
    @{ name = 'performance_curves'; profile = 0; full = 1; small = 1 }
)

$tuning = @()
foreach ($candidate in $candidateSpecs) {
    Write-Host "Tuning $($candidate.name)..."
    $tuning += Measure-Replay `
        -Name $candidate.name `
        -Profile $candidate.profile `
        -FullSensitivity $candidate.full `
        -SmallSensitivity $candidate.small `
        -Fps 60 -MotionScale 1.0 -Group 'tuning'
}

$best = $tuning |
    Sort-Object `
        @{ Expression = { if ($_.pass) { 0 } else { 1 } } }, `
        @{ Expression = { $_.score } } |
    Select-Object -First 1

[pscustomobject]@{
    schema = 'FLASHGUARD_TUNING/1'
    candidates = $tuning
    selected = $best
} | ConvertTo-Json -Depth 8 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'tuning.json')

Write-Host ''
Write-Host "Selected candidate: $($best.name) (score $([math]::Round($best.score, 3)))"
Write-Host ''

$matrix = @()
$fpsValues = @(30, 60, 120, 144, 240)
foreach ($fps in $fpsValues) {
    $name = "best_${fps}fps"
    Write-Host "Matrix $name..."
    $matrix += Measure-Replay `
        -Name $name `
        -Profile $best.profile `
        -FullSensitivity $best.full_sensitivity `
        -SmallSensitivity $best.small_sensitivity `
        -Fps $fps -MotionScale 1.0 -Group 'matrix'
}

foreach ($scale in @(0.5, 2.0)) {
    $label = ($scale.ToString('0.0',
        [Globalization.CultureInfo]::InvariantCulture)).Replace('.', 'p')
    $name = "best_60fps_motion_${label}x"
    Write-Host "Matrix $name..."
    $matrix += Measure-Replay `
        -Name $name `
        -Profile $best.profile `
        -FullSensitivity $best.full_sensitivity `
        -SmallSensitivity $best.small_sensitivity `
        -Fps 60 -MotionScale $scale -Group 'matrix'
}

[pscustomobject]@{
    schema = 'FLASHGUARD_MATRIX/1'
    note = 'Each replay includes the valid 5, 7.5, 10, 12, 15, 20, 25 and 30 Hz WCAG sweeps plus several NVIDIA-flow motion speeds. Frequencies above Nyquist are skipped for that FPS.'
    selected_candidate = $best
    results = $matrix
} | ConvertTo-Json -Depth 8 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')

$tuning |
    Select-Object name, pass, score, sc231_failures, sc232_failures,
        max_red_flashes_per_second, max_strict_transitions_per_second,
        motion_penalty |
    Format-Table -AutoSize

$matrix |
    Select-Object name, fps, motion_scale, pass, score, sc231_failures,
        sc232_failures, max_red_flashes_per_second,
        max_strict_transitions_per_second, motion_penalty |
    Format-Table -AutoSize

exit 0
