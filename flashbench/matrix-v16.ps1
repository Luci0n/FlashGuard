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
    if ($p.ExitCode -ne 0) { throw "Moving-flash authority matrix failed with exit code $($p.ExitCode)" }
    [pscustomobject]@{ directory=$dir; elapsed_ms=$sw.ElapsedMilliseconds }
}
function Read-Candidate([object]$Spec, [string]$BatchDir) {
    $dir = Join-Path $BatchDir $Spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') | ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') | ConvertFrom-Json
    $per = Get-Content -Raw (Join-Path $dir 'perceptual-sweep.json') | ConvertFrom-Json
    $sweep = Get-Content -Raw (Join-Path $dir 'flash-sweep.json') | ConvertFrom-Json
    $auth = Get-Content -Raw (Join-Path $dir 'authority-diagnostics.json') | ConvertFrom-Json
    $mf = $auth.cases.moving_flash
    $failed231 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true })
    $failed232 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true })
    [pscustomobject][ordered]@{
        name=[string]$Spec.name
        architecture_mode=[int]$Spec.architecture_mode
        surface_risk_tau=$Spec.surface_risk_tau
        risk_gain=$Spec.risk_gain
        moving_clear_05_ms=[double]$trail.moving_square_clear_to_0_05_ms
        moving_flash_reduction=[double]$replay.moving_flash_reduction
        perceptual_min_reduction=[double](($per.cases | Measure-Object reduction -Minimum).Minimum)
        pan_mae=[double]$replay.pan_mae
        wcag_pass=($failed231.Count -eq 0 -and $failed232.Count -eq 0)
        moving_flash_error_pixel_fraction=[double]$mf.error_pixel_fraction
        moving_flash_mean_error=[double]$mf.mean_error_on_error_pixels
        current_event_active=[double]$mf.channels.current_event_strength.active_fraction_on_error_pixels
        current_event_weighted=[double]$mf.channels.current_event_strength.error_weighted_mean
        surface_memory_active=[double]$mf.channels.surface_memory_strength.active_fraction_on_error_pixels
        surface_memory_weighted=[double]$mf.channels.surface_memory_strength.error_weighted_mean
        architecture_delta_weighted=[double]$mf.channels.architecture_luma_delta.error_weighted_mean
        current_surface_active=[double]$mf.geometry_on_error_pixels.current_surface.active_fraction_on_error_pixels
        hardware_active=[double]$mf.geometry_on_error_pixels.hardware.active_fraction_on_error_pixels
        effective_motion_active=[double]$mf.geometry_on_error_pixels.effective_motion.active_fraction_on_error_pixels
        repeated_risk_active=[double]$mf.geometry_on_error_pixels.repeated_risk.active_fraction_on_error_pixels
    }
}

$base = @{
    event_delta_low=.003; event_delta_high=.018; intrinsic_residual_low=.004; intrinsic_residual_high=.035
    hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018; event_seed_low=.010; event_seed_high=.075
    repeated_memory_low=.20; repeated_memory_high=.45
    risk_neutral=.12
}
$specs = @(
    (New-Spec 'surface_risk_v2_default' 3 (Merge-Tune $base @{risk_gain=.92})),
    (New-Spec 'event_only_gain200' 6 (Merge-Tune $base @{risk_gain=2.0})),
    (New-Spec 'opposition_tau100_gain200' 8 (Merge-Tune $base @{surface_risk_tau=.100; risk_gain=2.0})),
    (New-Spec 'opposition_tau200_gain200' 8 (Merge-Tune $base @{surface_risk_tau=.200; risk_gain=2.0}))
)

Write-Host "FlashBench moving-flash authority matrix: $($specs.Count) focused configurations"
$screen = Invoke-Batch $specs
$candidates = @($specs | ForEach-Object { Read-Candidate $_ $screen.directory })
$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/16'
    purpose='localize the remaining moving-flash suppression deficit by measuring current-event, persistent-memory, and motion-correspondence authority on underprotected moving-flash pixels'
    screening_resolution='320x180'
    screening_fps=30
    screen_elapsed_ms=$screen.elapsed_ms
    candidate_count=$candidates.Count
    invariant='diagnostic-only: protection algorithms and production architecture 0 are unchanged'
    authority_protocol='FLASHGUARD_AUTHORITY_DIAGNOSTICS/3'
    screen_candidates=$candidates
}
$report | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Moving-flash authority matrix complete in $($screen.elapsed_ms) ms"
exit 0
