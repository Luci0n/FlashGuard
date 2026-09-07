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
    if ($p.ExitCode -ne 0) { throw "Disocclusion separation matrix failed with exit code $($p.ExitCode)" }
    [pscustomobject]@{ directory=$dir; elapsed_ms=$sw.ElapsedMilliseconds }
}
function Read-Phase([object]$Case) {
    [pscustomobject][ordered]@{
        sample_pixel_count=[uint64]$Case.trail_pixel_count
        changed_fraction=[double]$Case.error_pixel_fraction
        current_event_active=[double]$Case.channels.current_event_strength.active_fraction_on_sample_pixels
        surface_memory_active=[double]$Case.channels.surface_memory_strength.active_fraction_on_sample_pixels
        architecture_delta_active=[double]$Case.channels.architecture_luma_delta.active_fraction_on_sample_pixels
        current_surface_active=[double]$Case.geometry_on_error_pixels.current_surface.active_fraction_on_sample_pixels
        vacated_active=[double]$Case.geometry_on_error_pixels.vacated.active_fraction_on_sample_pixels
        infill_active=[double]$Case.geometry_on_error_pixels.infill.active_fraction_on_sample_pixels
        effective_motion_active=[double]$Case.geometry_on_error_pixels.effective_motion.active_fraction_on_sample_pixels
    }
}
function Read-Candidate([object]$Spec, [string]$BatchDir) {
    $dir = Join-Path $BatchDir $Spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') | ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') | ConvertFrom-Json
    $per = Get-Content -Raw (Join-Path $dir 'perceptual-sweep.json') | ConvertFrom-Json
    $sweep = Get-Content -Raw (Join-Path $dir 'flash-sweep.json') | ConvertFrom-Json
    $authority = Get-Content -Raw (Join-Path $dir 'authority-diagnostics.json') | ConvertFrom-Json
    if ($authority.schema -ne 'FLASHGUARD_AUTHORITY_DIAGNOSTICS/4') {
        throw "Expected authority diagnostics v4 for $($Spec.name), got $($authority.schema)"
    }
    $transition = Read-Phase $authority.cases.moving_flash_transition
    $stable = Read-Phase $authority.cases.moving_flash_stable
    $failed231 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true })
    $failed232 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true })
    $clear = [double]$trail.moving_square_clear_to_0_05_ms
    $movingFlash = [double]$replay.moving_flash_reduction
    $perceptual = [double](($per.cases | Measure-Object reduction -Minimum).Minimum)
    $pan = [double]$replay.pan_mae
    $wcag231 = ($failed231.Count -eq 0)
    $wcag232 = ($failed232.Count -eq 0)
    $behaviorPass = ([string]$replay.status -eq 'SUCCESS')
    [pscustomobject][ordered]@{
        name=[string]$Spec.name
        architecture_mode=[int]$Spec.architecture_mode
        replay_status=[string]$replay.status
        moving_clear_05_ms=$clear
        moving_flash_reduction=$movingFlash
        perceptual_min_reduction=$perceptual
        pan_mae=$pan
        wcag_sc_2_3_1_pass=$wcag231
        wcag_sc_2_3_2_pass=$wcag232
        primary_pass=($behaviorPass -and $pan -le 0.010 -and $clear -le 70.0 -and
            $movingFlash -ge 0.45 -and $perceptual -ge 0.70 -and $wcag231 -and $wcag232)
        transition=$transition
        stable=$stable
    }
}

$base = @{
    event_delta_low=.003; event_delta_high=.018; intrinsic_residual_low=.004; intrinsic_residual_high=.035
    hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018; event_seed_low=.010; event_seed_high=.075
    repeated_memory_low=.20; repeated_memory_high=.45
    risk_neutral=.12
}
$tune = @{surface_risk_tau=.100; risk_gain=2.0}
$specs = @(
    (New-Spec 'uniform_interior_intrinsic_tau100_gain200' 12 (Merge-Tune $base $tune)),
    (New-Spec 'full_corrected_disocclusion_tau100_gain200' 13 (Merge-Tune $base $tune)),
    (New-Spec 'event_only_disocclusion_tau100_gain200' 14 (Merge-Tune $base $tune)),
    (New-Spec 'event_plus_residual_disocclusion_tau100_gain200' 15 (Merge-Tune $base $tune))
)

Write-Host "FlashBench disocclusion separation matrix: $($specs.Count) configurations"
$screen = Invoke-Batch $specs
$candidates = @($specs | ForEach-Object { Read-Candidate $_ $screen.directory })
$passing = @($candidates | Where-Object { $_.primary_pass })
$selected = if ($passing.Count -gt 0) {
    $passing | Sort-Object @{Expression='moving_flash_reduction';Descending=$true},
        @{Expression='pan_mae';Descending=$false} | Select-Object -First 1
} else { $null }
$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/24'
    purpose='single bounded tuning pass after Matrix 23: separate corrected current-event disocclusion from persistent-history rejection and optionally from compensated-residual preservation'
    screening_resolution='320x180'
    screening_fps=30
    screen_elapsed_ms=$screen.elapsed_ms
    candidate_count=$candidates.Count
    invariant='production architecture 0 is unchanged; mode 13 is the full geometry correction, mode 14 corrects current-event disocclusion only while keeping raw disocclusion for history/motion bypass, and mode 15 additionally preserves compensated intrinsic residual under verified current-surface overlap'
    primary_gate=[ordered]@{
        replay_status_required='SUCCESS'
        pan_mae_max=0.010
        moving_clear_05_ms_max=70
        moving_flash_reduction_min=0.45
        perceptual_min_reduction_min=0.70
        wcag_sc_2_3_1_required=$true
        wcag_sc_2_3_2_required=$true
    }
    selected=$selected
    screen_candidates=$candidates
}
$report | ConvertTo-Json -Depth 10 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Disocclusion separation matrix complete in $($screen.elapsed_ms) ms"
exit 0
