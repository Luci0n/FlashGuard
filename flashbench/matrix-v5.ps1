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

# Keep the 12 matrix-v4 outer settings in the TSV layout for backward
# compatibility. Matrix v5 leaves them blank and searches the 24 full-resolution
# shader transport/hold settings appended after them.
$outerKeys = @(
    'local_delta','global_delta','affected_area','coherence','small_area',
    'local_support','flash_energy','rise_rate','fall_rate','minimum_hold',
    'release_time','camera_motion'
)
$shaderKeys = @(
    'event_delta_low','event_delta_high','hold_delta_low','hold_delta_high',
    'stable_source_low','stable_source_high','intrinsic_residual_low','intrinsic_residual_high',
    'repeated_memory_low','repeated_memory_high','hold_gate_low','hold_gate_high',
    'transport_conf_low','transport_conf_high','disocclusion_reset','surface_risk_tau',
    'event_tau_scale','release_tau_scale','exact_hold_threshold','moving_hold_floor',
    'direct_intrinsic_low','direct_intrinsic_high','event_seed_low','event_seed_high'
)
$allTuningKeys = @($outerKeys + $shaderKeys)

function Write-Utf8NoBom {
    param([string]$Path, [string[]]$Lines)
    [IO.File]::WriteAllLines($Path, $Lines, [Text.UTF8Encoding]::new($false))
}

function Format-OptionalDouble {
    param([object]$Value)
    if ($null -eq $Value -or [string]::IsNullOrWhiteSpace([string]$Value)) { return '' }
    return ([double]$Value).ToString('0.######', $invariant)
}

function New-ShaderSpec {
    param([string]$Name, [hashtable]$Tune = @{}, [int]$Fps = 30)
    $row = [ordered]@{
        name = $Name; profile = 1; full = 1; small = 1; fps = $Fps; motion_scale = 1.0
    }
    foreach ($key in $allTuningKeys) { $row[$key] = $null }
    foreach ($key in $Tune.Keys) {
        if (-not $row.Contains($key)) { throw "Unknown shader tuning key '$key'" }
        $row[$key] = [double]$Tune[$key]
    }
    [pscustomobject]$row
}

function Invoke-ReplayBatch {
    param([string]$Name, [object[]]$Specs, [int]$Width, [int]$Height, [switch]$Screening)
    $batchDir = Join-Path $OutputDir $Name
    New-Item -ItemType Directory -Force -Path $batchDir | Out-Null
    $planPath = Join-Path $batchDir 'plan.tsv'
    $header = @('name','profile','full','small','fps','motion_scale') + $allTuningKeys
    $lines = @('# ' + ($header -join '<TAB>'))
    foreach ($spec in $Specs) {
        $fields = @(
            [string]$spec.name, [string]$spec.profile, [string]$spec.full,
            [string]$spec.small, [string]$spec.fps,
            ([double]$spec.motion_scale).ToString('0.###', $invariant)
        )
        foreach ($key in $allTuningKeys) {
            $p = $spec.PSObject.Properties[$key]
            $fields += (Format-OptionalDouble $(if ($p) { $p.Value } else { $null }))
        }
        $lines += ($fields -join "`t")
    }
    Write-Utf8NoBom $planPath $lines

    $args = @(
        '--synthetic-replay-batch', ('"' + $planPath + '"'),
        '--synthetic-replay-batch-output', ('"' + $batchDir + '"'),
        '--replay-width', [string]$Width, '--replay-height', [string]$Height
    )
    if ($Screening) { $args += '--replay-screening' }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
    $sw.Stop()
    if ($p.ExitCode -ne 0) { throw "Replay batch '$Name' failed with exit code $($p.ExitCode)" }
    $batchPath = Join-Path $batchDir 'batch.json'
    if (-not (Test-Path $batchPath)) { throw "Replay batch '$Name' did not produce batch.json" }
    $batch = Get-Content -Raw $batchPath | ConvertFrom-Json
    if ($batch.status -ne 'SUCCESS') { throw "Replay batch '$Name' reported $($batch.status)" }
    [pscustomobject]@{ directory = $batchDir; elapsed_ms = $sw.ElapsedMilliseconds; batch = $batch }
}

function Read-Candidate {
    param([object]$Spec, [string]$BatchDir)
    $dir = Join-Path $BatchDir $Spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') | ConvertFrom-Json
    $sweep = Get-Content -Raw (Join-Path $dir 'flash-sweep.json') | ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') | ConvertFrom-Json
    $perceptualPath = Join-Path $dir 'perceptual-sweep.json'
    $perceptualExecuted = Test-Path $perceptualPath
    $perceptualMin = 0.0
    $perceptualMax = 0.0
    if ($perceptualExecuted) {
        $perceptual = Get-Content -Raw $perceptualPath | ConvertFrom-Json
        if (@($perceptual.cases).Count -gt 0) {
            $perceptualMin = [double](($perceptual.cases | Measure-Object reduction -Minimum).Minimum)
            $perceptualMax = [double](($perceptual.cases | Measure-Object peak_output_delta -Maximum).Maximum)
        }
    }
    $row = [ordered]@{
        name = $Spec.name
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
        pan_mae = [double]$replay.pan_mae
        perceptual_sweep_executed = [bool]$perceptualExecuted
        perceptual_sweep_min_reduction = [double]$perceptualMin
        perceptual_sweep_max_output_delta = [double]$perceptualMax
    }
    foreach ($key in $shaderKeys) { $row[$key] = $Spec.$key }
    [pscustomobject]$row
}

function Test-Dominates {
    param([object]$A, [object]$B)
    $pc = $A.perceptual_sweep_executed -and $B.perceptual_sweep_executed
    $noWorse = $A.flash_reduction -ge $B.flash_reduction -and
        $A.moving_flash_reduction -ge $B.moving_flash_reduction -and
        $A.moving_vacated_p99_max -le $B.moving_vacated_p99_max -and
        $A.moving_vacated_peak -le $B.moving_vacated_peak -and
        $A.small_vacated_p99_max -le $B.small_vacated_p99_max -and
        $A.pan_mae -le $B.pan_mae -and $A.static_mae -le $B.static_mae -and
        (-not $pc -or ($A.perceptual_sweep_min_reduction -ge $B.perceptual_sweep_min_reduction -and
                       $A.perceptual_sweep_max_output_delta -le $B.perceptual_sweep_max_output_delta))
    if (-not $noWorse) { return $false }
    return $A.flash_reduction -gt $B.flash_reduction -or
        $A.moving_flash_reduction -gt $B.moving_flash_reduction -or
        $A.moving_vacated_p99_max -lt $B.moving_vacated_p99_max -or
        $A.moving_vacated_peak -lt $B.moving_vacated_peak -or
        $A.small_vacated_p99_max -lt $B.small_vacated_p99_max -or
        $A.pan_mae -lt $B.pan_mae -or $A.static_mae -lt $B.static_mae -or
        ($pc -and ($A.perceptual_sweep_min_reduction -gt $B.perceptual_sweep_min_reduction -or
                   $A.perceptual_sweep_max_output_delta -lt $B.perceptual_sweep_max_output_delta))
}

function Get-ParetoFrontier {
    param([object[]]$Candidates)
    @($Candidates | Where-Object {
        $c = $_
        -not @($Candidates | Where-Object { $_.name -ne $c.name -and (Test-Dominates $_ $c) }).Count
    })
}

function Add-RelativeRegret {
    param([object[]]$Candidates, [object]$Baseline)
    $eps = 1e-7
    foreach ($c in $Candidates) {
        $ratios = @(
            (([double]$c.moving_vacated_p99_max + $eps) / ([double]$Baseline.moving_vacated_p99_max + $eps)),
            (([double]$c.moving_vacated_peak + $eps) / ([double]$Baseline.moving_vacated_peak + $eps)),
            (([double]$c.small_vacated_p99_max + $eps) / ([double]$Baseline.small_vacated_p99_max + $eps)),
            (([double]$c.pan_mae + $eps) / ([double]$Baseline.pan_mae + $eps)),
            ([Math]::Max($eps, 1.0 - [double]$c.flash_reduction) /
             [Math]::Max($eps, 1.0 - [double]$Baseline.flash_reduction)),
            ([Math]::Max($eps, 1.0 - [double]$c.moving_flash_reduction) /
             [Math]::Max($eps, 1.0 - [double]$Baseline.moving_flash_reduction))
        )
        if ($c.perceptual_sweep_executed -and $Baseline.perceptual_sweep_executed) {
            $ratios += [Math]::Max($eps, 1.0 - [double]$c.perceptual_sweep_min_reduction) /
                [Math]::Max($eps, 1.0 - [double]$Baseline.perceptual_sweep_min_reduction)
            $ratios += ([double]$c.perceptual_sweep_max_output_delta + $eps) /
                ([double]$Baseline.perceptual_sweep_max_output_delta + $eps)
        }
        $c | Add-Member -NotePropertyName max_relative_regret -NotePropertyValue (($ratios | Measure-Object -Maximum).Maximum) -Force
    }
    $Candidates
}

# 15 orthogonal two-sided probes + production default + two combined candidates.
$screenSpecs = @(
    (New-ShaderSpec 'production_default'),
    (New-ShaderSpec 'event_delta_sensitive' @{ event_delta_low=0.003; event_delta_high=0.018 }),
    (New-ShaderSpec 'event_delta_conservative' @{ event_delta_low=0.014; event_delta_high=0.055 }),
    (New-ShaderSpec 'hold_delta_lower' @{ hold_delta_low=0.012; hold_delta_high=0.050 }),
    (New-ShaderSpec 'hold_delta_higher' @{ hold_delta_low=0.045; hold_delta_high=0.120 }),
    (New-ShaderSpec 'stable_source_strict' @{ stable_source_low=0.004; stable_source_high=0.028 }),
    (New-ShaderSpec 'stable_source_lenient' @{ stable_source_low=0.020; stable_source_high=0.085 }),
    (New-ShaderSpec 'intrinsic_sensitive' @{ intrinsic_residual_low=0.004; intrinsic_residual_high=0.035 }),
    (New-ShaderSpec 'intrinsic_conservative' @{ intrinsic_residual_low=0.035; intrinsic_residual_high=0.130 }),
    (New-ShaderSpec 'repeat_earlier' @{ repeated_memory_low=0.20; repeated_memory_high=0.45 }),
    (New-ShaderSpec 'repeat_later' @{ repeated_memory_low=0.45; repeated_memory_high=0.78 }),
    (New-ShaderSpec 'hold_gate_earlier' @{ hold_gate_low=0.08; hold_gate_high=0.38 }),
    (New-ShaderSpec 'hold_gate_later' @{ hold_gate_low=0.25; hold_gate_high=0.72 }),
    (New-ShaderSpec 'transport_lenient' @{ transport_conf_low=0.30; transport_conf_high=0.60 }),
    (New-ShaderSpec 'transport_strict' @{ transport_conf_low=0.60; transport_conf_high=0.88 }),
    (New-ShaderSpec 'disocclusion_reset_low' @{ disocclusion_reset=0.35 }),
    (New-ShaderSpec 'disocclusion_reset_high' @{ disocclusion_reset=0.75 }),
    (New-ShaderSpec 'surface_risk_short' @{ surface_risk_tau=0.22 }),
    (New-ShaderSpec 'surface_risk_long' @{ surface_risk_tau=0.90 }),
    (New-ShaderSpec 'event_state_fast' @{ event_tau_scale=0.55 }),
    (New-ShaderSpec 'event_state_slow' @{ event_tau_scale=1.55 }),
    (New-ShaderSpec 'release_state_fast' @{ release_tau_scale=0.45 }),
    (New-ShaderSpec 'release_state_slow' @{ release_tau_scale=1.70 }),
    (New-ShaderSpec 'exact_hold_higher' @{ exact_hold_threshold=0.88 }),
    (New-ShaderSpec 'exact_hold_lower' @{ exact_hold_threshold=0.58 }),
    (New-ShaderSpec 'moving_hold_zero' @{ moving_hold_floor=0.0 }),
    (New-ShaderSpec 'moving_hold_high' @{ moving_hold_floor=0.08 }),
    (New-ShaderSpec 'direct_intrinsic_sensitive' @{ direct_intrinsic_low=0.003; direct_intrinsic_high=0.018 }),
    (New-ShaderSpec 'direct_intrinsic_conservative' @{ direct_intrinsic_low=0.018; direct_intrinsic_high=0.065 }),
    (New-ShaderSpec 'event_seed_sensitive' @{ event_seed_low=0.010; event_seed_high=0.075 }),
    (New-ShaderSpec 'event_seed_conservative' @{ event_seed_low=0.060; event_seed_high=0.240 }),
    (New-ShaderSpec 'trail_priority' @{
        stable_source_low=0.004; stable_source_high=0.028; transport_conf_low=0.30; transport_conf_high=0.60;
        disocclusion_reset=0.35; surface_risk_tau=0.22; release_tau_scale=0.45;
        exact_hold_threshold=0.88; moving_hold_floor=0.0
    }),
    (New-ShaderSpec 'low_contrast_priority' @{
        event_delta_low=0.003; event_delta_high=0.018; intrinsic_residual_low=0.004; intrinsic_residual_high=0.035;
        repeated_memory_low=0.20; repeated_memory_high=0.45; hold_gate_low=0.08; hold_gate_high=0.38;
        direct_intrinsic_low=0.003; direct_intrinsic_high=0.018; event_seed_low=0.010; event_seed_high=0.075
    })
)

Write-Host "FlashBench v6 shader screen: $($screenSpecs.Count) configurations in one GPU session"
$screenBatch = Invoke-ReplayBatch 'screen' $screenSpecs 320 180 -Screening
$screenResults = @($screenSpecs | ForEach-Object { Read-Candidate $_ $screenBatch.directory })
$baseline = $screenResults | Where-Object name -eq 'production_default' | Select-Object -First 1
$eligible = @($screenResults | Where-Object { $_.replay_status -eq 'SUCCESS' })
if ($eligible.Count -eq 0) { $eligible = $screenResults }
$eligible = @(Add-RelativeRegret $eligible $baseline)
$frontier = @(Get-ParetoFrontier $eligible)
$best = $frontier | Sort-Object max_relative_regret | Select-Object -First 1
if (-not $best) { $best = $eligible | Sort-Object max_relative_regret | Select-Object -First 1 }

if ($ScreenOnly) {
    [pscustomobject]@{
        schema = 'FLASHGUARD_MATRIX/5'
        mode = 'shader-screen-only'
        selection = 'pareto frontier then minimum worst relative regression versus production default'
        screen_batch_elapsed_ms = $screenBatch.elapsed_ms
        screen_candidates = $screenResults
        screen_pareto_frontier = @($frontier | ForEach-Object name)
        selected = $best
    } | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
    exit 0
}

$verifySpecs = @()
foreach ($name in @($best.name, 'production_default') | Select-Object -Unique) {
    $copy = ($screenSpecs | Where-Object name -eq $name | Select-Object -First 1) | Select-Object *
    $copy.fps = 60
    $verifySpecs += $copy
}
Write-Host "FlashBench v6 shader verify: $($verifySpecs.Count) configurations"
$verifyBatch = Invoke-ReplayBatch 'verify' $verifySpecs 640 360
$verifyResults = @($verifySpecs | ForEach-Object { Read-Candidate $_ $verifyBatch.directory })
$verifyBaseline = $verifyResults | Where-Object name -eq 'production_default' | Select-Object -First 1
$verifyResults = @(Add-RelativeRegret $verifyResults $verifyBaseline)
$verifyEligible = @($verifyResults | Where-Object { $_.replay_status -eq 'SUCCESS' -and $_.sc231_pass -and $_.sc232_pass })
if ($verifyEligible.Count -eq 0) { $verifyEligible = $verifyResults }
$verifyFrontier = @(Get-ParetoFrontier $verifyEligible)
$selected = $verifyFrontier | Sort-Object max_relative_regret | Select-Object -First 1
if (-not $selected) { $selected = $verifyEligible | Sort-Object max_relative_regret | Select-Object -First 1 }

[pscustomobject]@{
    schema = 'FLASHGUARD_MATRIX/5'
    mode = 'shader-screen-then-verify'
    selection = 'pareto frontier then minimum worst relative regression versus production default'
    screen_batch_elapsed_ms = $screenBatch.elapsed_ms
    verify_batch_elapsed_ms = $verifyBatch.elapsed_ms
    screen_candidates = $screenResults
    screen_pareto_frontier = @($frontier | ForEach-Object name)
    verify_candidates = $verifyResults
    verify_pareto_frontier = @($verifyFrontier | ForEach-Object name)
    selected = $selected
} | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Selected: $($selected.name)"
exit 0
