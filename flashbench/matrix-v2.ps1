param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path

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

    $caseDir = [IO.Path]::GetFullPath(
        (Join-Path (Join-Path $OutputDir $Group) $Name))
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $reportPath = Join-Path $caseDir 'synthetic-replay.json'
    $sweepPath = Join-Path $caseDir 'flash-sweep.json'
    Remove-Item $reportPath, $sweepPath -ErrorAction SilentlyContinue

    $motionArg = $MotionScale.ToString(
        '0.###', [Globalization.CultureInfo]::InvariantCulture)
    $arguments = @(
        '--synthetic-replay', ('"' + $reportPath + '"'),
        '--replay-profile', [string]$Profile,
        '--replay-full-sensitivity', [string]$FullSensitivity,
        '--replay-small-sensitivity', [string]$SmallSensitivity,
        '--replay-fps', [string]$Fps,
        '--replay-motion-scale', $motionArg
    )

    # FlashGuard is a GUI-subsystem executable. Explicit process waiting avoids
    # inheriting a stale LASTEXITCODE when this script is nested inside run.ps1.
    $process = Start-Process -FilePath $exe -ArgumentList $arguments `
        -Wait -PassThru
    $exitCode = $process.ExitCode

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

    # Include interiors, boundaries, oblique transport and whole-scene pans.
    # This prevents a candidate from "winning" by making flashes safe through
    # obvious temporal smear.
    $motionPenalty =
        [double]$replay.moving_square_ghost_mae / 0.005 +
        [double]$replay.moving_square_inside_mae / 0.010 +
        [double]$replay.moving_square_edge_mae / 0.010 +
        [double]$replay.bright_oblique_ghost_mae / 0.005 +
        [double]$replay.bright_oblique_inside_mae / 0.015 +
        [double]$replay.bright_oblique_edge_mae / 0.015 +
        [double]$replay.small_moving_square_ghost_mae / 0.003 +
        [double]$replay.pan_mae / 0.010 +
        [double]$replay.fast_pan_mae / 0.020 +
        [double]$replay.extreme_pan_mae / 0.030

    # Passing always sorts first. If nobody passes, actual WCAG failures dominate
    # the score and motion quality breaks ties between similarly safe candidates.
    $score =
        10000.0 * ($sc231Failures + $sc232Failures) +
        100.0 * $maxRed +
        50.0 * $maxGeneral +
        10.0 * ($maxStrict / 6.0) +
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
        moving_inside_mae = [double]$replay.moving_square_inside_mae
        moving_edge_mae = [double]$replay.moving_square_edge_mae
        oblique_ghost_mae = [double]$replay.bright_oblique_ghost_mae
        oblique_inside_mae = [double]$replay.bright_oblique_inside_mae
        oblique_edge_mae = [double]$replay.bright_oblique_edge_mae
        pan_mae = [double]$replay.pan_mae
        fast_pan_mae = [double]$replay.fast_pan_mae
        extreme_pan_mae = [double]$replay.extreme_pan_mae
        moving_flow_frames = [int]$replay.moving_flow_frames
        oblique_flow_frames = [int]$replay.bright_oblique_flow_frames
        pan_flow_frames = [int]$replay.pan_flow_frames
        fast_pan_flow_frames = [int]$replay.fast_pan_flow_frames
        extreme_pan_flow_frames = [int]$replay.extreme_pan_flow_frames
    }
}

# First stage: cross all three curve profiles with all three paired detector
# sensitivities. This is large enough to expose useful curve differences without
# turning every push into an hours-long exhaustive search.
$candidateSpecs = @()
foreach ($profile in 0..2) {
    foreach ($sensitivity in 0..2) {
        $candidateSpecs += @{
            name = "profile_${profile}_sensitivity_${sensitivity}"
            profile = $profile
            full = $sensitivity
            small = $sensitivity
        }
    }
}

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
    schema = 'FLASHGUARD_TUNING/2'
    candidates = $tuning
    selected = $best
} | ConvertTo-Json -Depth 8 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'tuning.json')

Write-Host ''
Write-Host "Selected candidate: $($best.name) (score $([math]::Round($best.score, 3)))"
Write-Host ''

# Second stage: hold the selected curves fixed and stress frame rate plus
# translation velocity. Every replay still contains all representable 5-30 Hz
# luminance/red/quarter-screen sweep cases.
$matrix = @()
foreach ($fps in @(30, 60, 120, 144, 240)) {
    $name = "best_${fps}fps"
    Write-Host "Matrix $name..."
    $matrix += Measure-Replay `
        -Name $name `
        -Profile $best.profile `
        -FullSensitivity $best.full_sensitivity `
        -SmallSensitivity $best.small_sensitivity `
        -Fps $fps -MotionScale 1.0 -Group 'matrix'
}

foreach ($scale in @(0.25, 0.5, 2.0, 4.0)) {
    $label = $scale.ToString(
        '0.00', [Globalization.CultureInfo]::InvariantCulture)
    $label = $label.TrimEnd('0').TrimEnd('.').Replace('.', 'p')
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
    schema = 'FLASHGUARD_MATRIX/2'
    note = 'Each replay includes representable 5, 7.5, 10, 12, 15, 20, 25 and 30 Hz sweeps plus NVIDIA-flow motion cases. Frequencies above Nyquist are skipped.'
    selected_candidate = $best
    results = $matrix
} | ConvertTo-Json -Depth 8 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')

$tuning |
    Select-Object name, pass, score, sc231_failures, sc232_failures,
        max_red_flashes_per_second, max_strict_transitions_per_second,
        motion_penalty, pan_flow_frames |
    Format-Table -AutoSize

$matrix |
    Select-Object name, fps, motion_scale, pass, score, sc231_failures,
        sc232_failures, max_red_flashes_per_second,
        max_strict_transitions_per_second, motion_penalty,
        moving_flow_frames, oblique_flow_frames, pan_flow_frames |
    Format-Table -AutoSize

exit 0
