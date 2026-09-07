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
function New-Spec([string]$Name, [hashtable]$Tune = @{}, [int]$Fps = 30) {
    $r = [ordered]@{ name=$Name; profile=1; full=1; small=1; fps=$Fps; motion_scale=1.0 }
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
    $lines = @('# ' + ((@('name','profile','full','small','fps','motion_scale') + $allKeys) -join '<TAB>'))
    foreach ($s in $Specs) {
        $f = @([string]$s.name,[string]$s.profile,[string]$s.full,[string]$s.small,[string]$s.fps,([double]$s.motion_scale).ToString('0.###',$inv))
        foreach ($k in $allKeys) { $f += (Fmt $s.$k) }
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
    $perPath = Join-Path $dir 'perceptual-sweep.json'
    $perExecuted = Test-Path $perPath
    $perMin = 0.0; $perMax = 0.0
    if ($perExecuted) {
        $per = Get-Content -Raw $perPath | ConvertFrom-Json
        if (@($per.cases).Count) {
            $perMin = [double](($per.cases | Measure-Object reduction -Minimum).Minimum)
            $perMax = [double](($per.cases | Measure-Object peak_output_delta -Maximum).Maximum)
        }
    }
    $r = [ordered]@{
        name=[string]$Spec.name; replay_status=[string]$replay.status
        sc231_pass=(@($sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true }).Count -eq 0)
        sc232_pass=(@($sweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true }).Count -eq 0)
        static_mae=[double]$replay.static_mae
        flash_reduction=[double]$replay.flash_reduction
        moving_flash_reduction=[double]$replay.moving_flash_reduction
        moving_trail_p99_frame_mean=[double]$trail.moving_square_trail_p99_frame_mean
        moving_trail_p99_frame_p95=[double]$trail.moving_square_trail_p99_frame_p95
        moving_trail_area05_frame_mean=[double]$trail.moving_square_trail_area_above_0_05_frame_mean
        moving_trail_area05_frame_p95=[double]$trail.moving_square_trail_area_above_0_05_frame_p95
        moving_recovery_p99_auc=[double]$trail.moving_square_recovery_p99_auc
        moving_recovery_area05_auc=[double]$trail.moving_square_recovery_area_above_0_05_auc
        moving_recovery_p99_final=[double]$trail.moving_square_recovery_p99_final
        moving_clear_05_observed=[bool]$trail.moving_square_clear_to_0_05_observed
        moving_clear_05_ms=[double]$trail.moving_square_clear_to_0_05_ms
        moving_clear_05_lower_bound_ms=[double]$trail.moving_square_clear_to_0_05_lower_bound_ms
        moving_vacated_p99_max=[double]$trail.moving_square_vacated_p99_max
        small_vacated_p99_max=[double]$trail.small_moving_square_vacated_p99_max
        pan_mae=[double]$replay.pan_mae
        perceptual_sweep_executed=[bool]$perExecuted
        perceptual_sweep_min_reduction=[double]$perMin
        perceptual_sweep_max_output_delta=[double]$perMax
    }
    foreach ($k in $shaderKeys) { $r[$k] = $Spec.$k }
    [pscustomobject]$r
}
function Dominates($A,$B) {
    $pc = $A.perceptual_sweep_executed -and $B.perceptual_sweep_executed
    $noWorse = $A.flash_reduction -ge $B.flash_reduction -and $A.moving_flash_reduction -ge $B.moving_flash_reduction -and
        $A.moving_trail_p99_frame_p95 -le $B.moving_trail_p99_frame_p95 -and $A.moving_trail_area05_frame_mean -le $B.moving_trail_area05_frame_mean -and
        $A.moving_recovery_p99_auc -le $B.moving_recovery_p99_auc -and $A.moving_recovery_area05_auc -le $B.moving_recovery_area05_auc -and
        $A.small_vacated_p99_max -le $B.small_vacated_p99_max -and $A.pan_mae -le $B.pan_mae -and $A.static_mae -le $B.static_mae -and
        (-not $pc -or ($A.perceptual_sweep_min_reduction -ge $B.perceptual_sweep_min_reduction -and $A.perceptual_sweep_max_output_delta -le $B.perceptual_sweep_max_output_delta))
    if (-not $noWorse) { return $false }
    $A.flash_reduction -gt $B.flash_reduction -or $A.moving_flash_reduction -gt $B.moving_flash_reduction -or
        $A.moving_trail_p99_frame_p95 -lt $B.moving_trail_p99_frame_p95 -or $A.moving_trail_area05_frame_mean -lt $B.moving_trail_area05_frame_mean -or
        $A.moving_recovery_p99_auc -lt $B.moving_recovery_p99_auc -or $A.moving_recovery_area05_auc -lt $B.moving_recovery_area05_auc -or
        $A.small_vacated_p99_max -lt $B.small_vacated_p99_max -or $A.pan_mae -lt $B.pan_mae -or $A.static_mae -lt $B.static_mae -or
        ($pc -and ($A.perceptual_sweep_min_reduction -gt $B.perceptual_sweep_min_reduction -or $A.perceptual_sweep_max_output_delta -lt $B.perceptual_sweep_max_output_delta))
}
function Pareto([object[]]$C) {
    @($C | Where-Object { $x=$_; -not @($C | Where-Object { $_.name -ne $x.name -and (Dominates $_ $x) }).Count })
}
function Add-Regret([object[]]$C,$Base) {
    $e=1e-7
    foreach ($x in $C) {
        $ratios=@(
            (([double]$x.moving_trail_p99_frame_p95+$e)/([double]$Base.moving_trail_p99_frame_p95+$e)),
            (([double]$x.moving_trail_area05_frame_mean+$e)/([double]$Base.moving_trail_area05_frame_mean+$e)),
            (([double]$x.moving_recovery_p99_auc+$e)/([double]$Base.moving_recovery_p99_auc+$e)),
            (([double]$x.moving_recovery_area05_auc+$e)/([double]$Base.moving_recovery_area05_auc+$e)),
            (([double]$x.small_vacated_p99_max+$e)/([double]$Base.small_vacated_p99_max+$e)),
            (([double]$x.pan_mae+$e)/([double]$Base.pan_mae+$e)),
            ([Math]::Max($e,1-[double]$x.flash_reduction)/[Math]::Max($e,1-[double]$Base.flash_reduction)),
            ([Math]::Max($e,1-[double]$x.moving_flash_reduction)/[Math]::Max($e,1-[double]$Base.moving_flash_reduction))
        )
        if ($x.perceptual_sweep_executed -and $Base.perceptual_sweep_executed) {
            $ratios += [Math]::Max($e,1-[double]$x.perceptual_sweep_min_reduction)/[Math]::Max($e,1-[double]$Base.perceptual_sweep_min_reduction)
            $ratios += ([double]$x.perceptual_sweep_max_output_delta+$e)/([double]$Base.perceptual_sweep_max_output_delta+$e)
        }
        $x | Add-Member max_relative_regret (($ratios | Measure-Object -Maximum).Maximum) -Force
    }
    $C
}

$screenSpecs = @(
    (New-Spec 'production_default'),
    (New-Spec 'event_delta_sensitive' @{event_delta_low=.003;event_delta_high=.018}),
    (New-Spec 'event_delta_conservative' @{event_delta_low=.014;event_delta_high=.055}),
    (New-Spec 'hold_delta_lower' @{hold_delta_low=.012;hold_delta_high=.050}),
    (New-Spec 'hold_delta_higher' @{hold_delta_low=.045;hold_delta_high=.120}),
    (New-Spec 'stable_source_strict' @{stable_source_low=.004;stable_source_high=.028}),
    (New-Spec 'stable_source_lenient' @{stable_source_low=.020;stable_source_high=.085}),
    (New-Spec 'intrinsic_sensitive' @{intrinsic_residual_low=.004;intrinsic_residual_high=.035}),
    (New-Spec 'intrinsic_conservative' @{intrinsic_residual_low=.035;intrinsic_residual_high=.130}),
    (New-Spec 'repeat_earlier' @{repeated_memory_low=.20;repeated_memory_high=.45}),
    (New-Spec 'repeat_later' @{repeated_memory_low=.45;repeated_memory_high=.78}),
    (New-Spec 'hold_gate_earlier' @{hold_gate_low=.08;hold_gate_high=.38}),
    (New-Spec 'hold_gate_later' @{hold_gate_low=.25;hold_gate_high=.72}),
    (New-Spec 'transport_lenient' @{transport_conf_low=.30;transport_conf_high=.60}),
    (New-Spec 'transport_strict' @{transport_conf_low=.60;transport_conf_high=.88}),
    (New-Spec 'disocclusion_reset_low' @{disocclusion_reset=.35}),
    (New-Spec 'disocclusion_reset_high' @{disocclusion_reset=.75}),
    (New-Spec 'surface_risk_short' @{surface_risk_tau=.22}),
    (New-Spec 'surface_risk_long' @{surface_risk_tau=.90}),
    (New-Spec 'event_state_fast' @{event_tau_scale=.55}),
    (New-Spec 'event_state_slow' @{event_tau_scale=1.55}),
    (New-Spec 'release_state_fast' @{release_tau_scale=.45}),
    (New-Spec 'release_state_slow' @{release_tau_scale=1.70}),
    (New-Spec 'exact_hold_higher' @{exact_hold_threshold=.88}),
    (New-Spec 'exact_hold_lower' @{exact_hold_threshold=.58}),
    (New-Spec 'moving_hold_zero' @{moving_hold_floor=0}),
    (New-Spec 'moving_hold_high' @{moving_hold_floor=.08}),
    (New-Spec 'direct_intrinsic_sensitive' @{direct_intrinsic_low=.003;direct_intrinsic_high=.018}),
    (New-Spec 'direct_intrinsic_conservative' @{direct_intrinsic_low=.018;direct_intrinsic_high=.065}),
    (New-Spec 'event_seed_sensitive' @{event_seed_low=.010;event_seed_high=.075}),
    (New-Spec 'event_seed_conservative' @{event_seed_low=.060;event_seed_high=.240}),
    (New-Spec 'trail_priority' @{stable_source_low=.004;stable_source_high=.028;transport_conf_low=.30;transport_conf_high=.60;disocclusion_reset=.35;surface_risk_tau=.22;release_tau_scale=.45;exact_hold_threshold=.88;moving_hold_floor=0}),
    (New-Spec 'low_contrast_priority' @{event_delta_low=.003;event_delta_high=.018;intrinsic_residual_low=.004;intrinsic_residual_high=.035;repeated_memory_low=.20;repeated_memory_high=.45;hold_gate_low=.08;hold_gate_high=.38;direct_intrinsic_low=.003;direct_intrinsic_high=.018;event_seed_low=.010;event_seed_high=.075})
)

Write-Host "FlashBench v6 trail-aware shader screen: $($screenSpecs.Count) configurations"
$screen = Invoke-Batch 'screen' $screenSpecs 320 180 -Screening
$candidates = @($screenSpecs | ForEach-Object { Read-Candidate $_ $screen.directory })
$base = @($candidates | Where-Object name -eq 'production_default')[0]
$candidates = @(Add-Regret $candidates $base)
$eligible = @($candidates | Where-Object { $_.replay_status -eq 'SUCCESS' -and $_.sc231_pass -and $_.sc232_pass })
$front = @(Pareto $eligible)
$selected = @($front | Sort-Object max_relative_regret, moving_recovery_p99_auc, moving_trail_p99_frame_p95, @{Expression='perceptual_sweep_min_reduction';Descending=$true})[0]

$verifyRows = @()
$verifyElapsed = 0
if (-not $ScreenOnly) {
    $lookup = @{}; foreach ($s in $screenSpecs) { $lookup[$s.name]=$s }
    $verifySpecs = @($lookup['production_default'])
    if ($selected.name -ne 'production_default') { $verifySpecs += $lookup[$selected.name] }
    $verify = Invoke-Batch 'verify' $verifySpecs 640 360
    $verifyElapsed = $verify.elapsed_ms
    $verifyRows = @($verifySpecs | ForEach-Object { Read-Candidate $_ $verify.directory })
}

$out = [ordered]@{
    schema='FLASHGUARD_MATRIX/6'
    mode=$(if($ScreenOnly){'trail-aware-screen-only'}else{'trail-aware-screen-and-verify'})
    selection='hard WCAG eligibility; Pareto; minimum worst relative regret using time-distributed trail and recovery AUC metrics'
    screen_batch_elapsed_ms=$screen.elapsed_ms
    screen_candidates=$candidates
    screen_pareto_frontier=@($front.name)
    selected=$selected
    verify_batch_elapsed_ms=$verifyElapsed
    verify_candidates=$verifyRows
}
$out | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Selected: $($selected.name)"
