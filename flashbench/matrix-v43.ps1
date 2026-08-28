param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$inv = [Globalization.CultureInfo]::InvariantCulture

# Matrix 43 starts from Matrix 42's clean R+L architecture. The remaining 5 Hz
# phase asymmetry is diagnostic: at 30 fps phase 0 keeps the first sign for three
# frames (100 ms) before reversal, while phase 0.5 reverses after two frames
# (66.7 ms). This matrix separates signed-PRIME establishment amplitude from
# signed-PRIME survival duration without changing D/G/S or the surface-risk L fix.
# A = full signed PRIME write once the existing weak-prime qualifier fires.
# P = 0.40 s stationary signed-PRIME lifetime.
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
function Merge-Tune([hashtable]$Base, [hashtable]$Extra) {
    $r = @{}
    foreach ($k in $Base.Keys) { $r[$k] = $Base[$k] }
    foreach ($k in $Extra.Keys) { $r[$k] = $Extra[$k] }
    $r
}
function New-Spec(
    [string]$Name,
    [int]$Architecture,
    [bool]$FullWeakPrime,
    [bool]$LongPrime,
    [hashtable]$Tune = @{}) {
    $r = [ordered]@{
        name=$Name; profile=1; full=1; small=1; fps=30; motion_scale=1.0
        architecture_mode=$Architecture
        factor_full_weak_prime=$FullWeakPrime
        factor_long_prime=$LongPrime
        risk_neutral=$null; risk_gain=$null
    }
    foreach ($k in $allKeys) { $r[$k] = $null }
    foreach ($k in $Tune.Keys) {
        if (-not $r.Contains($k)) { throw "Unknown tuning key '$k'" }
        $r[$k] = [double]$Tune[$k]
    }
    [pscustomobject]$r
}

$base = @{
    event_delta_low=.003; event_delta_high=.018; intrinsic_residual_low=.004; intrinsic_residual_high=.035
    hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018; event_seed_low=.010; event_seed_high=.075
    repeated_memory_low=.20; repeated_memory_high=.45
    risk_neutral=.12
}
$tune = @{surface_risk_tau=.100; risk_gain=2.0}
$mergedTune = Merge-Tune $base $tune

# Mode 46 is Matrix 42's R+L candidate. New modes keep that exact architecture
# and add only A, P, or A+P.
$specs = @(
    (New-Spec 'rl00_control' 46 $false $false $mergedTune),
    (New-Spec 'rl10_full_weak_prime' 48 $true $false $mergedTune),
    (New-Spec 'rl01_long_prime' 49 $false $true $mergedTune),
    (New-Spec 'rl11_full_weak_long_prime' 50 $true $true $mergedTune)
)

$plan = Join-Path $OutputDir 'plan.tsv'
$header = @('name','profile','full','small','fps','motion_scale') +
    $allKeys + @('architecture_mode') + $riskKeys
$lines = @('# ' + ($header -join '<TAB>'))
foreach ($s in $specs) {
    $fields = @(
        [string]$s.name, [string]$s.profile, [string]$s.full,
        [string]$s.small, [string]$s.fps,
        ([double]$s.motion_scale).ToString('0.###', $inv)
    )
    foreach ($k in $allKeys) { $fields += (Fmt $s.$k) }
    $fields += [string]$s.architecture_mode
    foreach ($k in $riskKeys) { $fields += (Fmt $s.$k) }
    $lines += ($fields -join "`t")
}
[IO.File]::WriteAllLines($plan, $lines, [Text.UTF8Encoding]::new($false))

$args = @(
    '--synthetic-replay-batch', ('"' + $plan + '"'),
    '--synthetic-replay-batch-output', ('"' + $OutputDir + '"'),
    '--replay-width', '320', '--replay-height', '180'
)
Write-Host "FlashBench Matrix 43: $($specs.Count) R+L PRIME-establishment combinations"
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
$sw.Stop()
if ($p.ExitCode -ne 0) {
    throw "Matrix 43 replay batch failed with exit code $($p.ExitCode)"
}

function Perceptual-Key([object]$Case) {
    '{0}|{1}|{2}' -f [int]$Case.delta_code,
        ([double]$Case.frequency_hz).ToString('0.###', $inv),
        ([double]$Case.phase_frames).ToString('0.###', $inv)
}
function Read-PerceptualMap([string]$CandidateDir) {
    $per = Get-Content -Raw (Join-Path $CandidateDir 'perceptual-sweep.json') |
        ConvertFrom-Json
    $map = @{}
    foreach ($case in @($per.cases)) {
        $map[(Perceptual-Key $case)] = [double]$case.reduction
    }
    [pscustomobject]@{ report=$per; map=$map }
}
function Mean-Metric([object[]]$Candidates, [string]$Metric) {
    if ($Candidates.Count -lt 1) { return 0.0 }
    [double](($Candidates | Measure-Object -Property $Metric -Average).Average)
}
function Factor-Effect([object[]]$Candidates, [string]$Factor, [string]$Metric) {
    $on = @($Candidates | Where-Object { [bool]$_.$Factor })
    $off = @($Candidates | Where-Object { -not [bool]$_.$Factor })
    [double]((Mean-Metric $on $Metric) - (Mean-Metric $off $Metric))
}

$deltas = @(4, 8, 12, 16, 24, 32)
$candidates = @()
foreach ($spec in $specs) {
    $dir = Join-Path $OutputDir $spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') |
        ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') |
        ConvertFrom-Json
    $perceptualData = Read-PerceptualMap $dir

    $phasePairs = [ordered]@{}
    $phase0Values = @()
    $phase05Values = @()
    $phaseGaps = @()
    foreach ($delta in $deltas) {
        $key0 = "$delta|5|0"
        $key05 = "$delta|5|0.5"
        if (-not $perceptualData.map.ContainsKey($key0) -or
            -not $perceptualData.map.ContainsKey($key05)) {
            throw "Missing 5 Hz phase pair for delta $delta in $($spec.name)"
        }
        $v0 = [double]$perceptualData.map[$key0]
        $v05 = [double]$perceptualData.map[$key05]
        $gap = $v05 - $v0
        $phase0Values += $v0
        $phase05Values += $v05
        $phaseGaps += $gap
        $phasePairs["delta$delta"] = [ordered]@{
            phase0=$v0
            phase0_5=$v05
            phase0_5_minus_phase0=$gap
        }
    }

    $candidates += [pscustomobject][ordered]@{
        name=[string]$spec.name
        architecture_mode=[int]$spec.architecture_mode
        factor_full_weak_prime=[bool]$spec.factor_full_weak_prime
        factor_long_prime=[bool]$spec.factor_long_prime
        replay_status=[string]$replay.status
        moving_clear_05_ms=[double]$trail.moving_square_clear_to_0_05_ms
        moving_flash_reduction=[double]$replay.moving_flash_reduction
        perceptual_min_reduction=[double](
            ($perceptualData.report.cases | Measure-Object reduction -Minimum).Minimum)
        phase0_5hz_min_reduction=[double](
            ($phase0Values | Measure-Object -Minimum).Minimum)
        phase0_5hz_mean_reduction=[double](
            ($phase0Values | Measure-Object -Average).Average)
        phase0_5hz_half_min_reduction=[double](
            ($phase05Values | Measure-Object -Minimum).Minimum)
        phase0_5hz_half_mean_reduction=[double](
            ($phase05Values | Measure-Object -Average).Average)
        phase_gap_mean=[double](($phaseGaps | Measure-Object -Average).Average)
        phase_gap_max=[double](($phaseGaps | Measure-Object -Maximum).Maximum)
        pan_mae=[double]$replay.pan_mae
        fast_pan_mae=[double]$replay.fast_pan_mae
        extreme_pan_mae=[double]$replay.extreme_pan_mae
        phase_pairs=$phasePairs
    }
}

$control = @($candidates | Where-Object { $_.architecture_mode -eq 46 })[0]
$effects = [ordered]@{}
foreach ($factor in @('factor_full_weak_prime','factor_long_prime')) {
    $effects[$factor] = [ordered]@{
        moving_flash_reduction=(Factor-Effect $candidates $factor 'moving_flash_reduction')
        perceptual_min_reduction=(Factor-Effect $candidates $factor 'perceptual_min_reduction')
        phase0_5hz_min_reduction=(Factor-Effect $candidates $factor 'phase0_5hz_min_reduction')
        phase0_5hz_mean_reduction=(Factor-Effect $candidates $factor 'phase0_5hz_mean_reduction')
        phase0_5hz_half_min_reduction=(Factor-Effect $candidates $factor 'phase0_5hz_half_min_reduction')
        phase_gap_mean=(Factor-Effect $candidates $factor 'phase_gap_mean')
        phase_gap_max=(Factor-Effect $candidates $factor 'phase_gap_max')
        pan_mae=(Factor-Effect $candidates $factor 'pan_mae')
        fast_pan_mae=(Factor-Effect $candidates $factor 'fast_pan_mae')
        extreme_pan_mae=(Factor-Effect $candidates $factor 'extreme_pan_mae')
    }
}

$c00 = @($candidates | Where-Object { -not $_.factor_full_weak_prime -and -not $_.factor_long_prime })[0]
$c10 = @($candidates | Where-Object { $_.factor_full_weak_prime -and -not $_.factor_long_prime })[0]
$c01 = @($candidates | Where-Object { -not $_.factor_full_weak_prime -and $_.factor_long_prime })[0]
$c11 = @($candidates | Where-Object { $_.factor_full_weak_prime -and $_.factor_long_prime })[0]
$interaction = [ordered]@{}
foreach ($metric in @(
    'moving_flash_reduction','perceptual_min_reduction',
    'phase0_5hz_min_reduction','phase0_5hz_mean_reduction',
    'phase0_5hz_half_min_reduction','phase_gap_mean','phase_gap_max',
    'pan_mae','fast_pan_mae','extreme_pan_mae')) {
    $interaction[$metric] =
        ([double]$c11.$metric - [double]$c01.$metric) -
        ([double]$c10.$metric - [double]$c00.$metric)
}

# Treat <.005 perceptual changes as replay/GPU noise and require a real phase-0
# gain before replacing the control. This avoids the Matrix-42 tie-break artifact.
$eligible = @($candidates | Where-Object {
    $_.replay_status -eq 'SUCCESS' -and
    [double]$_.pan_mae -le [double]$control.pan_mae + 0.0005 -and
    [double]$_.fast_pan_mae -le [double]$control.fast_pan_mae + 0.0005 -and
    [double]$_.extreme_pan_mae -le [double]$control.extreme_pan_mae + 0.0010 -and
    [double]$_.moving_flash_reduction -ge [double]$control.moving_flash_reduction - 0.005 -and
    [double]$_.phase0_5hz_half_min_reduction -ge
        [double]$control.phase0_5hz_half_min_reduction - 0.005 -and
    [double]$_.phase0_5hz_min_reduction -ge
        [double]$control.phase0_5hz_min_reduction + 0.010
})
$selected = if ($eligible.Count -gt 0) {
    $eligible | Sort-Object `
        @{Expression='phase0_5hz_min_reduction';Descending=$true}, `
        @{Expression='phase_gap_max';Descending=$false}, `
        @{Expression='phase0_5hz_mean_reduction';Descending=$true} |
        Select-Object -First 1
} else { $control }

$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/43'
    purpose='isolate signed-PRIME establishment amplitude versus signed-PRIME survival as the cause of the remaining 5 Hz phase-0 weakness on the clean R+L architecture'
    screening_resolution='320x180'
    screening_fps=30
    candidate_count=$candidates.Count
    phase_timing=[ordered]@{
        phase0_first_opposite_transition_ms=100.0
        phase0_5_first_opposite_transition_ms=66.666667
        interpretation='at 30 fps and 5 Hz, the two replay phase alignments differ primarily in how long the first signed PRIME must survive before the opposite transition arrives'
    }
    invariant='all candidates keep Matrix-41 R restoration ownership and Matrix-42 L 0.40 s qualified surface-risk lifetime ON; D/G/S remain OFF; all tuning and full replay cases are identical; only A weak-PRIME write amplitude and P stationary signed-PRIME lifetime vary'
    factor_definition=[ordered]@{
        A='once the existing weak-prime qualifier is nonzero, write a full signed PRIME; qualification thresholds and display authority are unchanged'
        P='extend stationary signed-PRIME lifetime to 0.40 s using stationarySequenceStateGate; surface-risk lifetime is already fixed at 0.40 s independently'
    }
    control=$control
    main_effects=$effects
    interaction_AxP=$interaction
    selection_gate=[ordered]@{
        meaningful_phase0_min_gain=0.010
        phase0_5_half_min_allowed_loss=0.005
        pan_mae_vs_control_max_add=0.0005
        fast_pan_mae_vs_control_max_add=0.0005
        extreme_pan_mae_vs_control_max_add=0.0010
        moving_flash_vs_control_max_loss=0.005
        objective='maximize 5 Hz phase-0 minimum reduction, then minimize maximum phase asymmetry, then maximize phase-0 mean reduction; keep control if no candidate gains at least .010'
    }
    selected=$selected
    screen_candidates=$candidates
}
$report | ConvertTo-Json -Depth 14 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Matrix 43 PRIME establishment/survival factorial complete in $($sw.ElapsedMilliseconds) ms"
exit 0
