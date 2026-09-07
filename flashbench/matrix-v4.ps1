param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$invariant = [Globalization.CultureInfo]::InvariantCulture

$tuningKeys = @(
    'local_delta',
    'global_delta',
    'affected_area',
    'coherence',
    'small_area',
    'local_support',
    'flash_energy',
    'rise_rate',
    'fall_rate',
    'minimum_hold',
    'release_time',
    'camera_motion'
)

function Write-Utf8NoBom {
    param([string]$Path, [string[]]$Lines)
    $encoding = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllLines($Path, $Lines, $encoding)
}

function Format-OptionalDouble {
    param([object]$Value)
    if ($null -eq $Value -or [string]::IsNullOrWhiteSpace([string]$Value)) { return '' }
    return ([double]$Value).ToString('0.######', $invariant)
}

function New-DirectSpec {
    param(
        [string]$Name,
        [hashtable]$Tune = @{},
        [int]$Fps = 30
    )
    $row = [ordered]@{
        name = $Name
        profile = 1
        full = 1
        small = 1
        fps = $Fps
        motion_scale = 1.0
        local_delta = $null
        global_delta = $null
        affected_area = $null
        coherence = $null
        small_area = $null
        local_support = $null
        flash_energy = $null
        rise_rate = $null
        fall_rate = $null
        minimum_hold = $null
        release_time = $null
        camera_motion = $null
    }
    foreach ($key in $Tune.Keys) {
        if (-not $row.Contains($key)) { throw "Unknown direct tuning key '$key'" }
        $row[$key] = [double]$Tune[$key]
    }
    return [pscustomobject]$row
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
    $lines = @(
        '# name<TAB>profile<TAB>full<TAB>small<TAB>fps<TAB>motion_scale' +
        '<TAB>local_delta<TAB>global_delta<TAB>affected_area<TAB>coherence' +
        '<TAB>small_area<TAB>local_support<TAB>flash_energy<TAB>rise_rate' +
        '<TAB>fall_rate<TAB>minimum_hold<TAB>release_time<TAB>camera_motion'
    )
    foreach ($spec in $Specs) {
        $fields = @(
            [string]$spec.name,
            [string]$spec.profile,
            [string]$spec.full,
            [string]$spec.small,
            [string]$spec.fps,
            ([double]$spec.motion_scale).ToString('0.###', $invariant)
        )
        foreach ($key in $tuningKeys) {
            $property = $spec.PSObject.Properties[$key]
            $value = if ($property) { $property.Value } else { $null }
            $fields += (Format-OptionalDouble $value)
        }
        $lines += ($fields -join "`t")
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
    [pscustomobject]@{
        directory = $batchDir
        elapsed_ms = $sw.ElapsedMilliseconds
        batch = $batch
    }
}

function Read-Candidate {
    param([object]$Spec, [string]$BatchDir)
    $caseDir = Join-Path $BatchDir $Spec.name
    $replay = Get-Content -Raw (Join-Path $caseDir 'synthetic-replay.json') | ConvertFrom-Json
    $sweep = Get-Content -Raw (Join-Path $caseDir 'flash-sweep.json') | ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $caseDir 'trail-metrics.json') | ConvertFrom-Json
    $perceptualPath = Join-Path $caseDir 'perceptual-sweep.json'
    $perceptualExecuted = Test-Path $perceptualPath
    $perceptualMin = 0.0
    $perceptualMax = 0.0
    if ($perceptualExecuted) {
        $perceptual = Get-Content -Raw $perceptualPath | ConvertFrom-Json
        if (@($perceptual.cases).Count -gt 0) {
            $perceptualMin = [double](($perceptual.cases |
                Measure-Object -Property reduction -Minimum).Minimum)
            $perceptualMax = [double](($perceptual.cases |
                Measure-Object -Property peak_output_delta -Maximum).Maximum)
        }
    }

    $row = [ordered]@{
        name = $Spec.name
        profile = [int]$Spec.profile
        full_sensitivity = [int]$Spec.full
        small_sensitivity = [int]$Spec.small
        fps = [int]$Spec.fps
        motion_scale = [double]$Spec.motion_scale
        replay_status = [string]$replay.status
        sc231_pass = (@($sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true }).Count -eq 0)
        sc232_pass = (@($sweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true }).Count -eq 0)
        static_mae = [double]$replay.static_mae
        flash_reduction = [double]$replay.flash_reduction
        moving_flash_reduction = [double]$replay.moving_flash_reduction
        moving_vacated_p99_max = [double]$trail.moving_square_vacated_p99_max
        moving_vacated_peak = [double]$trail.moving_square_vacated_peak
        moving_vacated_area_05_max = [double]$trail.moving_square_vacated_area_above_0_05_max
        moving_clear_05_ms = [double]$trail.moving_square_clear_to_0_05_ms
        small_vacated_p99_max = [double]$trail.small_moving_square_vacated_p99_max
        small_vacated_peak = [double]$trail.small_moving_square_vacated_peak
        pan_mae = [double]$replay.pan_mae
        perceptual_sweep_executed = [bool]$perceptualExecuted
        perceptual_sweep_min_reduction = [double]$perceptualMin
        perceptual_sweep_max_output_delta = [double]$perceptualMax
    }
    foreach ($key in $tuningKeys) {
        $property = $Spec.PSObject.Properties[$key]
        $row[$key] = if ($property) { $property.Value } else { $null }
    }
    [pscustomobject]$row
}

function Test-Dominates {
    param([object]$A, [object]$B)
    $perceptualComparable =
        $A.perceptual_sweep_executed -and $B.perceptual_sweep_executed
    $noWorse =
        $A.flash_reduction -ge $B.flash_reduction -and
        $A.moving_flash_reduction -ge $B.moving_flash_reduction -and
        $A.moving_vacated_p99_max -le $B.moving_vacated_p99_max -and
        $A.moving_vacated_peak -le $B.moving_vacated_peak -and
        $A.small_vacated_p99_max -le $B.small_vacated_p99_max -and
        $A.pan_mae -le $B.pan_mae -and
        $A.static_mae -le $B.static_mae -and
        (-not $perceptualComparable -or (
            $A.perceptual_sweep_min_reduction -ge $B.perceptual_sweep_min_reduction -and
            $A.perceptual_sweep_max_output_delta -le $B.perceptual_sweep_max_output_delta))
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
        # Compare residual flash energy, not reduction itself. This remains
        # meaningful when a weak flash has zero or slightly negative reduction.
        $candidateFlashResidual = [Math]::Max($eps, 1.0 - [double]$candidate.flash_reduction)
        $baselineFlashResidual = [Math]::Max($eps, 1.0 - [double]$Baseline.flash_reduction)
        $candidateMovingResidual = [Math]::Max($eps, 1.0 - [double]$candidate.moving_flash_reduction)
        $baselineMovingResidual = [Math]::Max($eps, 1.0 - [double]$Baseline.moving_flash_reduction)
        $ratios = @(
            (([double]$candidate.moving_vacated_p99_max + $eps) /
                ([double]$Baseline.moving_vacated_p99_max + $eps)),
            (([double]$candidate.moving_vacated_peak + $eps) /
                ([double]$Baseline.moving_vacated_peak + $eps)),
            (([double]$candidate.small_vacated_p99_max + $eps) /
                ([double]$Baseline.small_vacated_p99_max + $eps)),
            (([double]$candidate.pan_mae + $eps) / ([double]$Baseline.pan_mae + $eps)),
            ($candidateFlashResidual / $baselineFlashResidual),
            ($candidateMovingResidual / $baselineMovingResidual)
        )
        if ($candidate.perceptual_sweep_executed -and $Baseline.perceptual_sweep_executed) {
            $candidatePerceptualResidual = [Math]::Max(
                $eps, 1.0 - [double]$candidate.perceptual_sweep_min_reduction)
            $baselinePerceptualResidual = [Math]::Max(
                $eps, 1.0 - [double]$Baseline.perceptual_sweep_min_reduction)
            $ratios += ($candidatePerceptualResidual / $baselinePerceptualResidual)
            $ratios += (([double]$candidate.perceptual_sweep_max_output_delta + $eps) /
                ([double]$Baseline.perceptual_sweep_max_output_delta + $eps))
        }
        $candidate | Add-Member -NotePropertyName max_relative_regret `
            -NotePropertyValue (($ratios | Measure-Object -Maximum).Maximum) -Force
    }
    return $Candidates
}

# Direct algorithm search. Every row starts from production profile/full/small
# defaults and changes only the explicitly listed benchmark-only fields.
$screenSpecs = @(
    (New-DirectSpec 'production_default')

    (New-DirectSpec 'local_delta_low' @{ local_delta = 0.07 })
    (New-DirectSpec 'local_delta_high' @{ local_delta = 0.13 })
    (New-DirectSpec 'global_delta_low' @{ global_delta = 0.11 })
    (New-DirectSpec 'global_delta_high' @{ global_delta = 0.21 })
    (New-DirectSpec 'affected_area_low' @{ affected_area = 0.10 })
    (New-DirectSpec 'affected_area_high' @{ affected_area = 0.26 })
    (New-DirectSpec 'coherence_low' @{ coherence = 0.58 })
    (New-DirectSpec 'coherence_high' @{ coherence = 0.82 })
    (New-DirectSpec 'small_area_low' @{ small_area = 0.003 })
    (New-DirectSpec 'small_area_high' @{ small_area = 0.016 })
    (New-DirectSpec 'local_support_low' @{ local_support = 0.015 })
    (New-DirectSpec 'local_support_high' @{ local_support = 0.055 })
    (New-DirectSpec 'flash_energy_low' @{ flash_energy = 0.015 })
    (New-DirectSpec 'flash_energy_high' @{ flash_energy = 0.050 })
    (New-DirectSpec 'rise_rate_low' @{ rise_rate = 0.85 })
    (New-DirectSpec 'rise_rate_high' @{ rise_rate = 2.10 })
    (New-DirectSpec 'fall_rate_low' @{ fall_rate = 0.90 })
    (New-DirectSpec 'fall_rate_high' @{ fall_rate = 2.60 })
    (New-DirectSpec 'minimum_hold_low' @{ minimum_hold = 0.12 })
    (New-DirectSpec 'minimum_hold_high' @{ minimum_hold = 0.34 })
    (New-DirectSpec 'release_time_low' @{ release_time = 0.25 })
    (New-DirectSpec 'release_time_high' @{ release_time = 0.70 })
    (New-DirectSpec 'camera_motion_low' @{ camera_motion = 0.20 })
    (New-DirectSpec 'camera_motion_high' @{ camera_motion = 0.48 })

    (New-DirectSpec 'detector_sensitive' @{
        local_delta = 0.075; global_delta = 0.12; affected_area = 0.12
        coherence = 0.62; small_area = 0.004; local_support = 0.020
        flash_energy = 0.018
    })
    (New-DirectSpec 'detector_conservative' @{
        local_delta = 0.125; global_delta = 0.20; affected_area = 0.24
        coherence = 0.80; small_area = 0.015; local_support = 0.050
        flash_energy = 0.045
    })
    (New-DirectSpec 'temporal_fast' @{
        rise_rate = 2.0; fall_rate = 2.8; minimum_hold = 0.12; release_time = 0.24
    })
    (New-DirectSpec 'temporal_slow' @{
        rise_rate = 0.85; fall_rate = 0.95; minimum_hold = 0.34; release_time = 0.65
    })
    (New-DirectSpec 'motion_fidelity' @{
        local_delta = 0.115; flash_energy = 0.040; fall_rate = 2.4
        release_time = 0.25; camera_motion = 0.50
    })
    (New-DirectSpec 'flash_priority' @{
        local_delta = 0.075; global_delta = 0.11; affected_area = 0.10
        flash_energy = 0.015; rise_rate = 0.85; fall_rate = 1.0
        minimum_hold = 0.30; release_time = 0.60; camera_motion = 0.18
    })
    (New-DirectSpec 'balanced_fast' @{
        local_delta = 0.09; global_delta = 0.14; affected_area = 0.15
        flash_energy = 0.024; rise_rate = 1.6; fall_rate = 2.1
        minimum_hold = 0.16; release_time = 0.32; camera_motion = 0.38
    })
    (New-DirectSpec 'balanced_strong' @{
        local_delta = 0.085; global_delta = 0.13; affected_area = 0.14
        small_area = 0.006; flash_energy = 0.022; rise_rate = 1.1
        fall_rate = 1.3; minimum_hold = 0.28; release_time = 0.52
        camera_motion = 0.32
    })
)

Write-Host "FlashBench v7 direct screen: $($screenSpecs.Count) configurations in one GPU session"
$screenBatch = Invoke-ReplayBatch -Name 'screen' -Specs $screenSpecs `
    -Width 320 -Height 180 -Screening
$screenResults = @($screenSpecs | ForEach-Object {
    Read-Candidate -Spec $_ -BatchDir $screenBatch.directory
})
$screenEligible = @($screenResults | Where-Object { $_.replay_status -eq 'SUCCESS' })
if ($screenEligible.Count -eq 0) { $screenEligible = $screenResults }
$screenBaseline = $screenResults | Where-Object { $_.name -eq 'production_default' } |
    Select-Object -First 1
if (-not $screenBaseline) { throw 'production_default result missing' }
$screenEligible = @(Add-RelativeRegret -Candidates $screenEligible -Baseline $screenBaseline)
$screenFrontier = @(Get-ParetoFrontier -Candidates $screenEligible)
$screenBest = $screenFrontier | Sort-Object max_relative_regret | Select-Object -First 1
if (-not $screenBest) {
    $screenBest = $screenEligible | Sort-Object max_relative_regret | Select-Object -First 1
}

if ($ScreenOnly) {
    [pscustomobject]@{
        schema = 'FLASHGUARD_MATRIX/4'
        mode = 'direct-screen-only'
        search_space = 'benchmark-only direct SafetySettings overrides'
        selection = 'Pareto frontier then minimum worst relative regression versus production default'
        screen_batch_elapsed_ms = $screenBatch.elapsed_ms
        screen_candidates = $screenResults
        screen_pareto_frontier = @($screenFrontier | ForEach-Object { $_.name })
        selected = $screenBest
    } | ConvertTo-Json -Depth 10 | Set-Content -Encoding utf8 `
        (Join-Path $OutputDir 'matrix.json')
    exit 0
}

function Copy-ForVerify {
    param([object]$Candidate)
    $tune = @{}
    foreach ($key in $tuningKeys) {
        $value = $Candidate.PSObject.Properties[$key].Value
        if ($null -ne $value) { $tune[$key] = [double]$value }
    }
    New-DirectSpec -Name $Candidate.name -Tune $tune -Fps 60
}

$verifySeed = @($screenBest)
if ($screenBest.name -ne 'production_default') { $verifySeed += $screenBaseline }
if ($verifySeed.Count -lt 2 -and $screenFrontier.Count -gt 1) {
    $alternative = $screenFrontier | Where-Object { $_.name -ne $screenBest.name } |
        Sort-Object max_relative_regret | Select-Object -First 1
    if ($alternative) { $verifySeed += $alternative }
}
$verifySpecs = @($verifySeed | Select-Object -First 2 | ForEach-Object {
    Copy-ForVerify $_
})

Write-Host "FlashBench v7 verify: $($verifySpecs.Count) configurations in one GPU session"
$verifyBatch = Invoke-ReplayBatch -Name 'verify' -Specs $verifySpecs -Width 640 -Height 360
$verifyResults = @($verifySpecs | ForEach-Object {
    Read-Candidate -Spec $_ -BatchDir $verifyBatch.directory
})
$verifyBaseline = $verifyResults | Where-Object { $_.name -eq 'production_default' } |
    Select-Object -First 1
if (-not $verifyBaseline) { $verifyBaseline = $screenBaseline }
$verifyResults = @(Add-RelativeRegret -Candidates $verifyResults -Baseline $verifyBaseline)
$verifyEligible = @($verifyResults | Where-Object {
    $_.replay_status -eq 'SUCCESS' -and $_.sc231_pass -and $_.sc232_pass
})
if ($verifyEligible.Count -eq 0) { $verifyEligible = $verifyResults }
$verifyFrontier = @(Get-ParetoFrontier -Candidates $verifyEligible)
$selected = $verifyFrontier | Sort-Object max_relative_regret | Select-Object -First 1
if (-not $selected) {
    $selected = $verifyEligible | Sort-Object max_relative_regret | Select-Object -First 1
}

[pscustomobject]@{
    schema = 'FLASHGUARD_MATRIX/4'
    mode = 'direct-screen-then-verify'
    search_space = 'benchmark-only direct SafetySettings overrides'
    selection = 'Pareto frontier then minimum worst relative regression versus production default'
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
