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
    if ($p.ExitCode -ne 0) { throw "Authority replay batch failed with exit code $($p.ExitCode)" }
    [pscustomobject]@{ directory=$dir; elapsed_ms=$sw.ElapsedMilliseconds }
}
function Channel([object]$Case, [string]$Name) {
    $Case.channels.$Name
}
function Read-Candidate([object]$Spec, [string]$BatchDir) {
    $dir = Join-Path $BatchDir $Spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') | ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') | ConvertFrom-Json
    $per = Get-Content -Raw (Join-Path $dir 'perceptual-sweep.json') | ConvertFrom-Json
    $auth = Get-Content -Raw (Join-Path $dir 'authority-diagnostics.json') | ConvertFrom-Json
    $move = $auth.cases.moving_square
    $recovery = $auth.cases.moving_square_recovery
    $movePre = Channel $move 'preprocess_luma_delta'
    $moveArch = Channel $move 'architecture_luma_delta'
    $moveEvent = Channel $move 'current_event_strength'
    $moveMemory = Channel $move 'surface_memory_strength'
    $recPre = Channel $recovery 'preprocess_luma_delta'
    $recArch = Channel $recovery 'architecture_luma_delta'
    $recEvent = Channel $recovery 'current_event_strength'
    $recMemory = Channel $recovery 'surface_memory_strength'
    [pscustomobject][ordered]@{
        name=[string]$Spec.name; architecture_mode=[int]$Spec.architecture_mode
        moving_clear_05_ms=[double]$trail.moving_square_clear_to_0_05_ms
        moving_trail_p99_frame_p95=[double]$trail.moving_square_trail_p99_frame_p95
        moving_flash_reduction=[double]$replay.moving_flash_reduction
        perceptual_min_reduction=[double](($per.cases | Measure-Object reduction -Minimum).Minimum)
        moving_error_pixel_fraction=[double]$move.error_pixel_fraction
        moving_unexplained_error_fraction=[double]$move.unexplained_error_fraction
        moving_preprocess_error_weighted_mean=[double]$movePre.error_weighted_mean
        moving_architecture_error_weighted_mean=[double]$moveArch.error_weighted_mean
        moving_current_event_error_weighted_mean=[double]$moveEvent.error_weighted_mean
        moving_surface_memory_error_weighted_mean=[double]$moveMemory.error_weighted_mean
        recovery_error_pixel_fraction=[double]$recovery.error_pixel_fraction
        recovery_unexplained_error_fraction=[double]$recovery.unexplained_error_fraction
        recovery_preprocess_error_weighted_mean=[double]$recPre.error_weighted_mean
        recovery_architecture_error_weighted_mean=[double]$recArch.error_weighted_mean
        recovery_current_event_error_weighted_mean=[double]$recEvent.error_weighted_mean
        recovery_surface_memory_error_weighted_mean=[double]$recMemory.error_weighted_mean
        authority=$auth
    }
}

$lowContrast = @{
    event_delta_low=.003; event_delta_high=.018; intrinsic_residual_low=.004; intrinsic_residual_high=.035
    repeated_memory_low=.20; repeated_memory_high=.45; hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018; event_seed_low=.010; event_seed_high=.075
    risk_neutral=.12
}
$specs = @(
    (New-Spec 'legacy_default' 0),
    (New-Spec 'surface_risk_v2_neutral12' 3 $lowContrast),
    (New-Spec 'seed_veto_neutral12' 4 $lowContrast)
)

Write-Host "FlashBench authority attribution: $($specs.Count) focused configurations"
$screen = Invoke-Batch $specs
$candidates = @($specs | ForEach-Object { Read-Candidate $_ $screen.directory })
$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/10'
    purpose='focused per-pixel trail authority attribution; diagnostic only, no selection'
    screening_resolution='320x180'
    screen_elapsed_ms=$screen.elapsed_ms
    candidate_count=$candidates.Count
    screen_candidates=$candidates
}
$report | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Authority attribution complete in $($screen.elapsed_ms) ms"
exit 0
