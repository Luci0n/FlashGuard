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
    if ($p.ExitCode -ne 0) { throw "Single-frame phase-hold matrix failed with exit code $($p.ExitCode)" }
    [pscustomobject]@{ directory=$dir; elapsed_ms=$sw.ElapsedMilliseconds }
}
function Read-Candidate([object]$Spec, [string]$BatchDir) {
    $dir = Join-Path $BatchDir $Spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') | ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') | ConvertFrom-Json
    $per = Get-Content -Raw (Join-Path $dir 'perceptual-sweep.json') | ConvertFrom-Json
    $sweep = Get-Content -Raw (Join-Path $dir 'flash-sweep.json') | ConvertFrom-Json
    $failed231 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_1_pass -ne $true })
    $failed232 = @($sweep.cases | Where-Object { $_.wcag_sc_2_3_2_pass -ne $true })
    $clear = [double]$trail.moving_square_clear_to_0_05_ms
    $moving = [double]$replay.moving_flash_reduction
    $perceptual = [double](($per.cases | Measure-Object reduction -Minimum).Minimum)
    $wcag = ($failed231.Count -eq 0 -and $failed232.Count -eq 0)
    $primary = ($clear -le 70.0 -and $moving -ge 0.45 -and $perceptual -ge 0.70 -and $wcag)
    [pscustomobject][ordered]@{
        name=[string]$Spec.name
        architecture_mode=[int]$Spec.architecture_mode
        surface_risk_tau=$Spec.surface_risk_tau
        risk_gain=$Spec.risk_gain
        moving_clear_05_ms=$clear
        moving_flash_reduction=$moving
        perceptual_min_reduction=$perceptual
        pan_mae=[double]$replay.pan_mae
        wcag_sc_2_3_1_pass=($failed231.Count -eq 0)
        wcag_sc_2_3_2_pass=($failed232.Count -eq 0)
        primary_pass=$primary
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
    (New-Spec 'opposition_tau100_gain200' 8 (Merge-Tune $base @{surface_risk_tau=.100; risk_gain=2.0})),
    (New-Spec 'single_frame_phase_hold_tau100_gain200' 10 (Merge-Tune $base @{surface_risk_tau=.100; risk_gain=2.0}))
)

Write-Host "FlashBench single-frame phase-hold matrix: $($specs.Count) accept/reject configurations"
$screen = Invoke-Batch $specs
$candidates = @($specs | ForEach-Object { Read-Candidate $_ $screen.directory })
$selected = @($candidates | Where-Object { $_.architecture_mode -eq 10 -and $_.primary_pass } | Select-Object -First 1)
$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/18'
    purpose='direct accept/reject test of a one-frame nonrecursive phase hold after intrinsic moving-flash events'
    screening_resolution='320x180'
    screening_fps=30
    screen_elapsed_ms=$screen.elapsed_ms
    candidate_count=$candidates.Count
    invariant='benchmark architecture 10 only: production architecture 0 is unchanged; phase hold stores only previous-frame event strength and does not recurse'
    primary_gate=[ordered]@{
        moving_clear_05_ms_max=70.0
        moving_flash_reduction_min=0.45
        perceptual_min_reduction_min=0.70
        wcag_sc_2_3_1_required=$true
        wcag_sc_2_3_2_required=$true
    }
    selected=if ($selected.Count -gt 0) { $selected[0] } else { $null }
    screen_candidates=$candidates
}
$report | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Single-frame phase-hold matrix complete in $($screen.elapsed_ms) ms"
exit 0
