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

function Fmt([object]$v) {
    if ($null -eq $v -or [string]::IsNullOrWhiteSpace([string]$v)) { return '' }
    ([double]$v).ToString('0.######', $inv)
}
function New-Spec([string]$Name, [int]$Architecture, [hashtable]$Tune = @{}, [int]$Fps = 30) {
    $r = [ordered]@{ name=$Name; profile=1; full=1; small=1; fps=$Fps; motion_scale=1.0; architecture_mode=$Architecture }
    foreach ($k in $allKeys) { $r[$k] = $null }
    foreach ($k in $Tune.Keys) {
        if (-not $r.Contains($k)) { throw "Unknown tuning key '$k'" }
        $r[$k] = [double]$Tune[$k]
    }
    [pscustomobject]$r
}
function Invoke-Batch([string]$Name, [object[]]$Specs, [int]$Width, [int]$Height, [switch]$Screening) {
    $dir = Join-Path $OutputDir $Name
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $plan = Join-Path $dir 'plan.tsv'
    $header = @('name','profile','full','small','fps','motion_scale') + $allKeys + @('architecture_mode')
    $lines = @('# ' + ($header -join '<TAB>'))
    foreach ($s in $Specs) {
        $f = @([string]$s.name,[string]$s.profile,[string]$s.full,[string]$s.small,[string]$s.fps,([double]$s.motion_scale).ToString('0.###',$inv))
        foreach ($k in $allKeys) { $f += (Fmt $s.$k) }
        $f += [string]$s.architecture_mode
        $lines += ($f -join "`t")
    }
    [IO.File]::WriteAllLines($plan, $lines, [Text.UTF8Encoding]::new($false))
    $args = @('--synthetic-replay-batch',('"'+$plan+'"'),'--synthetic-replay-batch-output',('"'+$dir+'"'),'--replay-width',[string]$Width,'--replay-height',[string]$Height)
    if ($Screening) { $args += '--replay-screening' }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
    $sw.Stop()
    if ($p.ExitCode -ne 0) { throw "Replay batch '$Name' failed with exit code $($p.ExitCode)" }
    $batch = Get-Content -Raw (Join-Path $dir 'batch.json') | ConvertFrom-Json
    if ($batch.status -ne 'SUCCESS') { throw "Replay batch '$Name' reported $($batch.status)" }
    [pscustomobject]@{ directory=$dir; elapsed_ms=$sw.ElapsedMilliseconds; batch=$batch }
}
function Read-Candidate([object]$Spec, [string]$BatchDir) {
    $dir = Join-Path $BatchDir $Spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') | ConvertFrom-Json
    $sweep = Get-Content -Raw (Join-Path $dir 'flash-sweep.json') | ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') | ConvertFrom-Json
    $per = Get-Content -Raw (Join-Path $dir 'perceptual-sweep.json') | ConvertFrom-Json
    $perMin = [double](($per.cases | Measure-Object reduction -Minimum).Minimum)
    $perMax = [double](($per.cases | Measure-Object peak_output_delta -Maximum).Maximum)
    $clear05 = [double]$trail.moving_square_clear_to_0_05_ms
    $nextFrameTrailPass = [bool]$trail.moving_square_clear_to_0_05_observed -and $clear05 -le 70.0 -and [double]$trail.moving_square_trail_p99_frame_p95 -le 0.05
    $weakFlashPass = $perMin -ge 0.70
    $movingFlashPass = [double]$replay.moving_flash_reduction -ge 0.45
    $fidelityPass = [double]$replay.static_mae -lt 0.005 -and [double]$replay.pan_mae -lt 0.010
    $wcagPass = (@($sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true -or $_.wcag_sc_2_3_2_pass -ne $true }).Count -eq 0)
    $failed = @($nextFrameTrailPass,$weakFlashPass,$movingFlashPass,$fidelityPass,$wcagPass) | Where-Object { -not $_ }
    [pscustomobject][ordered]@{
        name=[string]$Spec.name; architecture_mode=[int]$Spec.architecture_mode; replay_status=[string]$replay.status
        wcag_pass=[bool]$wcagPass; next_frame_trail_pass=[bool]$nextFrameTrailPass; weak_flash_pass=[bool]$weakFlashPass
        moving_flash_pass=[bool]$movingFlashPass; fidelity_pass=[bool]$fidelityPass; failed_architecture_gates=@($failed).Count
        static_mae=[double]$replay.static_mae; pan_mae=[double]$replay.pan_mae
        flash_reduction=[double]$replay.flash_reduction; moving_flash_reduction=[double]$replay.moving_flash_reduction
        perceptual_sweep_min_reduction=$perMin; perceptual_sweep_max_output_delta=$perMax
        moving_trail_p99_frame_p95=[double]$trail.moving_square_trail_p99_frame_p95
        moving_trail_area05_frame_mean=[double]$trail.moving_square_trail_area_above_0_05_frame_mean
        moving_recovery_p99_auc=[double]$trail.moving_square_recovery_p99_auc
        moving_recovery_area05_auc=[double]$trail.moving_square_recovery_area_above_0_05_auc
        moving_recovery_p99_final=[double]$trail.moving_square_recovery_p99_final
        moving_clear_05_observed=[bool]$trail.moving_square_clear_to_0_05_observed; moving_clear_05_ms=$clear05
        small_vacated_p99_max=[double]$trail.small_moving_square_vacated_p99_max
    }
}

$lowContrast = @{
    event_delta_low=.003; event_delta_high=.018; intrinsic_residual_low=.004; intrinsic_residual_high=.035
    repeated_memory_low=.20; repeated_memory_high=.45; hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018; event_seed_low=.010; event_seed_high=.075
}
$lowContrastLate = @{}
foreach ($k in $lowContrast.Keys) { $lowContrastLate[$k] = $lowContrast[$k] }
$lowContrastLate['repeated_memory_low']=.45; $lowContrastLate['repeated_memory_high']=.78

$screenSpecs = @(
    (New-Spec 'legacy_default' 0),
    (New-Spec 'legacy_repeat_later' 0 @{repeated_memory_low=.45;repeated_memory_high=.78}),
    (New-Spec 'legacy_low_contrast' 0 $lowContrast),
    (New-Spec 'risk_only_default' 1),
    (New-Spec 'risk_only_low_contrast' 1 $lowContrast),
    (New-Spec 'risk_only_low_contrast_late_memory' 1 $lowContrastLate),
    (New-Spec 'hybrid_default' 2),
    (New-Spec 'hybrid_low_contrast' 2 $lowContrast),
    (New-Spec 'hybrid_low_contrast_late_memory' 2 $lowContrastLate)
)

Write-Host "FlashBench architecture screen: $($screenSpecs.Count) configurations"
$screen = Invoke-Batch 'screen' $screenSpecs 320 180 -Screening
$candidates = @($screenSpecs | ForEach-Object { Read-Candidate $_ $screen.directory })
$selected = @($candidates | Where-Object replay_status -eq 'SUCCESS' | Sort-Object failed_architecture_gates, moving_recovery_p99_auc, moving_trail_p99_frame_p95, @{Expression='perceptual_sweep_min_reduction';Descending=$true}, @{Expression='moving_flash_reduction';Descending=$true})[0]

$verifyRows = @(); $verifyElapsed = 0
if (-not $ScreenOnly) {
    $lookup=@{}; foreach ($s in $screenSpecs) { $lookup[$s.name]=$s }
    $verifySpecs=@($lookup['legacy_default'])
    if ($selected.name -ne 'legacy_default') { $verifySpecs += $lookup[$selected.name] }
    $verify = Invoke-Batch 'verify' $verifySpecs 640 360
    $verifyElapsed = $verify.elapsed_ms
    $verifyRows = @($verifySpecs | ForEach-Object { Read-Candidate $_ $verify.directory })
}

$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/7'; purpose='risk-only-current-frame architecture experiment'
    architecture_modes=[ordered]@{ '0'='legacy transported protected luminance'; '1'='risk-only current-frame compression'; '2'='risk-only plus one-frame same-surface raw-source limiter' }
    screening_resolution='320x180'; screen_elapsed_ms=$screen.elapsed_ms; candidate_count=$candidates.Count
    selection_gates=[ordered]@{ clear_to_05_ms_max=70.0; trail_p99_frame_p95_max=0.05; perceptual_min_reduction_min=0.70; moving_flash_reduction_min=0.45; static_mae_max=0.005; pan_mae_max=0.010; wcag_required=$true }
    selected=$selected; screen_candidates=$candidates; verify_elapsed_ms=$verifyElapsed; verify_candidates=$verifyRows
}
$report | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Selected architecture candidate: $($selected.name) (failed gates=$($selected.failed_architecture_gates))"
exit 0
