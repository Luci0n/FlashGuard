param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$inv = [Globalization.CultureInfo]::InvariantCulture

$outerKeys = @('local_delta','global_delta','affected_area','coherence','small_area','local_support','flash_energy','rise_rate','fall_rate','minimum_hold','release_time','camera_motion')
$shaderKeys = @(
    'event_delta_low','event_delta_high','hold_delta_low','hold_delta_high',
    'stable_source_low','stable_source_high','intrinsic_residual_low','intrinsic_residual_high',
    'repeated_memory_low','repeated_memory_high','hold_gate_low','hold_gate_high',
    'transport_conf_low','transport_conf_high','disocclusion_reset','surface_risk_tau',
    'event_tau_scale','release_tau_scale','exact_hold_threshold','moving_hold_floor',
    'direct_intrinsic_low','direct_intrinsic_high','event_seed_low','event_seed_high'
)
$allKeys = @($outerKeys + $shaderKeys)
$riskKeys = @('risk_neutral','risk_gain')

function Fmt([object]$v) {
    if ($null -eq $v -or [string]::IsNullOrWhiteSpace([string]$v)) { return '' }
    ([double]$v).ToString('0.######', $inv)
}
function New-Spec([string]$Name, [int]$Architecture, [hashtable]$Tune = @{}) {
    $r = [ordered]@{
        name=$Name; profile=1; full=1; small=1; fps=30; motion_scale=1.0
        architecture_mode=$Architecture; risk_neutral=$null; risk_gain=$null
    }
    foreach ($k in $allKeys) { $r[$k] = $null }
    foreach ($k in $Tune.Keys) {
        if (-not $r.Contains($k)) { throw "Unknown tuning key '$k'" }
        $r[$k] = [double]$Tune[$k]
    }
    [pscustomobject]$r
}
function Merge-Tune([hashtable]$Base, [hashtable]$Extra) {
    $r = @{}
    foreach ($k in $Base.Keys) { $r[$k] = $Base[$k] }
    foreach ($k in $Extra.Keys) { $r[$k] = $Extra[$k] }
    $r
}
function Invoke-Batch([object[]]$Specs) {
    $dir = Join-Path $OutputDir 'screen'
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $plan = Join-Path $dir 'plan.tsv'
    $header = @('name','profile','full','small','fps','motion_scale') + $allKeys + @('architecture_mode') + $riskKeys
    $lines = @('# ' + ($header -join '<TAB>'))
    foreach ($s in $Specs) {
        $f = @([string]$s.name,[string]$s.profile,[string]$s.full,[string]$s.small,[string]$s.fps,([double]$s.motion_scale).ToString('0.###',$inv))
        foreach ($k in $allKeys) { $f += (Fmt $s.$k) }
        $f += [string]$s.architecture_mode
        foreach ($k in $riskKeys) { $f += (Fmt $s.$k) }
        $lines += ($f -join "`t")
    }
    [IO.File]::WriteAllLines($plan, $lines, [Text.UTF8Encoding]::new($false))
    $args = @('--synthetic-replay-batch',('"'+$plan+'"'),'--synthetic-replay-batch-output',('"'+$dir+'"'),'--replay-width','320','--replay-height','180','--replay-screening')
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
    $sw.Stop()
    if ($p.ExitCode -ne 0) { throw "Signed-opposition matrix failed with exit code $($p.ExitCode)" }
    [pscustomobject]@{ directory=$dir; elapsed_ms=$sw.ElapsedMilliseconds }
}
function Read-Candidate([object]$Spec, [string]$BatchDir) {
    $dir = Join-Path $BatchDir $Spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') | ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') | ConvertFrom-Json
    $per = Get-Content -Raw (Join-Path $dir 'perceptual-sweep.json') | ConvertFrom-Json
    $sweep = Get-Content -Raw (Join-Path $dir 'flash-sweep.json') | ConvertFrom-Json
    $auth = Get-Content -Raw (Join-Path $dir 'authority-diagnostics.json') | ConvertFrom-Json
    $move = $auth.cases.moving_square
    $recovery = $auth.cases.moving_square_recovery
    $minPerceptual = [double](($per.cases | Measure-Object reduction -Minimum).Minimum)
    $failed231 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true })
    $failed232 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true })
    $clear05 = [double]$trail.moving_square_clear_to_0_05_ms
    $movingReduction = [double]$replay.moving_flash_reduction
    [pscustomobject][ordered]@{
        name=[string]$Spec.name
        architecture_mode=[int]$Spec.architecture_mode
        opposition_low=$Spec.repeated_memory_low
        opposition_high=$Spec.repeated_memory_high
        surface_risk_tau=$Spec.surface_risk_tau
        prime_tau_scale=$Spec.event_tau_scale
        risk_gain=$Spec.risk_gain
        moving_clear_05_observed=[bool]$trail.moving_square_clear_to_0_05_observed
        moving_clear_05_ms=$clear05
        moving_trail_p99_frame_p95=[double]$trail.moving_square_trail_p99_frame_p95
        moving_trail_area05_frame_mean=[double]$trail.moving_square_trail_area_above_0_05_frame_mean
        moving_recovery_p99_auc=[double]$trail.moving_square_recovery_p99_auc
        moving_recovery_area05_auc=[double]$trail.moving_square_recovery_area_above_0_05_auc
        moving_flash_reduction=$movingReduction
        perceptual_min_reduction=$minPerceptual
        pan_mae=[double]$replay.pan_mae
        static_mae=[double]$replay.static_mae
        sc231_pass=($failed231.Count -eq 0)
        sc232_pass=($failed232.Count -eq 0)
        wcag_fail_count=[Math]::Max($failed231.Count,$failed232.Count)
        moving_error_pixel_fraction=[double]$move.error_pixel_fraction
        recovery_error_pixel_fraction=[double]$recovery.error_pixel_fraction
        recovery_surface_memory_error_weighted_mean=[double]$recovery.channels.surface_memory_strength.error_weighted_mean
        recovery_current_event_error_weighted_mean=[double]$recovery.channels.current_event_strength.error_weighted_mean
        primary_gate_pass=([bool]$trail.moving_square_clear_to_0_05_observed -and $clear05 -le 70.0 -and $minPerceptual -ge 0.70 -and $movingReduction -ge 0.45 -and $failed231.Count -eq 0 -and $failed232.Count -eq 0)
    }
}

$base = @{
    event_delta_low=.003; event_delta_high=.018; intrinsic_residual_low=.004; intrinsic_residual_high=.035
    hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018; event_seed_low=.010; event_seed_high=.075
    risk_neutral=.12
}
$early = Merge-Tune $base @{repeated_memory_low=.10; repeated_memory_high=.30}
$defaultGate = Merge-Tune $base @{repeated_memory_low=.20; repeated_memory_high=.45}
$strict = Merge-Tune $base @{repeated_memory_low=.35; repeated_memory_high=.60}

$specs = @(
    (New-Spec 'legacy_default' 0),
    (New-Spec 'surface_risk_v2_default' 3 (Merge-Tune $defaultGate @{risk_gain=.92})),
    (New-Spec 'event_only_gain200' 6 (Merge-Tune $defaultGate @{risk_gain=2.0})),
    (New-Spec 'analyzer_repeat_gate_gain200' 7 (Merge-Tune $defaultGate @{risk_gain=2.0})),
    (New-Spec 'opposition_early_tau050_gain200' 8 (Merge-Tune $early @{surface_risk_tau=.050; risk_gain=2.0})),
    (New-Spec 'opposition_early_tau100_gain200' 8 (Merge-Tune $early @{surface_risk_tau=.100; risk_gain=2.0})),
    (New-Spec 'opposition_early_tau200_gain200' 8 (Merge-Tune $early @{surface_risk_tau=.200; risk_gain=2.0})),
    (New-Spec 'opposition_default_tau100_gain160' 8 (Merge-Tune $defaultGate @{surface_risk_tau=.100; risk_gain=1.6})),
    (New-Spec 'opposition_default_tau100_gain200' 8 (Merge-Tune $defaultGate @{surface_risk_tau=.100; risk_gain=2.0})),
    (New-Spec 'opposition_default_tau100_gain240' 8 (Merge-Tune $defaultGate @{surface_risk_tau=.100; risk_gain=2.4})),
    (New-Spec 'opposition_default_tau200_gain200' 8 (Merge-Tune $defaultGate @{surface_risk_tau=.200; risk_gain=2.0})),
    (New-Spec 'opposition_strict_tau100_gain200' 8 (Merge-Tune $strict @{surface_risk_tau=.100; risk_gain=2.0})),
    (New-Spec 'opposition_default_tau100_prime060' 8 (Merge-Tune $defaultGate @{surface_risk_tau=.100; event_tau_scale=.60; risk_gain=2.0})),
    (New-Spec 'opposition_default_tau100_prime160' 8 (Merge-Tune $defaultGate @{surface_risk_tau=.100; event_tau_scale=1.60; risk_gain=2.0}))
)

Write-Host "FlashBench signed-opposition surface-risk matrix: $($specs.Count) focused configurations"
$screen = Invoke-Batch $specs
$candidates = @($specs | ForEach-Object { Read-Candidate $_ $screen.directory })
$passing = @($candidates | Where-Object { $_.primary_gate_pass })
$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/15'
    purpose='require an opposing signed intrinsic transition on the same transported raw surface before display-active local risk persistence can be seeded'
    screening_resolution='320x180'
    screening_fps=30
    screen_elapsed_ms=$screen.elapsed_ms
    candidate_count=$candidates.Count
    primary_gate='clear<=70ms; perceptual>=0.70; moving-flash>=0.45; WCAG SC 2.3.1/2.3.2 pass'
    invariant='mode8 immediately mitigates a current intrinsic event but only primes signed surface state; persistent display risk is seeded by a later opposite intrinsic transition on the same surface; production remains architecture 0'
    primary_gate_pass_count=$passing.Count
    primary_gate_pass_names=@($passing | ForEach-Object { $_.name })
    screen_candidates=$candidates
}
$report | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Signed-opposition matrix complete in $($screen.elapsed_ms) ms; primary passes: $($passing.Count)"
exit 0
