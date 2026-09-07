param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$inv = [Globalization.CultureInfo]::InvariantCulture

# Matrix 42 starts from Matrix 41's R-only architecture, which retained mode-20
# weak-flash protection while reproducing essentially all of mode-35's pan repair.
# It is a 2^2 factorial over the remaining weak-protection ideas:
# S = direct qualified weak-risk seed, L = 0.40 s qualified-risk lifetime.
# D and G remain OFF in every candidate.
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
    [bool]$WeakSeed,
    [bool]$LongRisk,
    [hashtable]$Tune = @{}) {
    $r = [ordered]@{
        name=$Name; profile=1; full=1; small=1; fps=30; motion_scale=1.0
        architecture_mode=$Architecture
        factor_weak_seed=$WeakSeed
        factor_long_risk=$LongRisk
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

# mode 40 is Matrix 41's R-only control. Modes 45-47 keep that exact ownership
# rule and add only S, L, or S+L respectively.
$specs = @(
    (New-Spec 'r00_restore_only' 40 $false $false $mergedTune),
    (New-Spec 'r10_restore_seed' 45 $true $false $mergedTune),
    (New-Spec 'r01_restore_longrisk' 46 $false $true $mergedTune),
    (New-Spec 'r11_restore_seed_longrisk' 47 $true $true $mergedTune)
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
Write-Host "FlashBench Matrix 42: $($specs.Count) R-baseline weak-protection combinations"
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
$sw.Stop()
if ($p.ExitCode -ne 0) {
    throw "Matrix 42 replay batch failed with exit code $($p.ExitCode)"
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

$targetCases = [ordered]@{
    delta4_5hz_phase0='4|5|0'
    delta4_5hz_phase0_5='4|5|0.5'
    delta12_5hz_phase0_5='12|5|0.5'
    delta32_5hz_phase0_5='32|5|0.5'
    delta4_10hz_phase0_5='4|10|0.5'
    delta4_15hz_phase0_5='4|15|0.5'
}

$candidates = @()
foreach ($spec in $specs) {
    $dir = Join-Path $OutputDir $spec.name
    $replay = Get-Content -Raw (Join-Path $dir 'synthetic-replay.json') |
        ConvertFrom-Json
    $trail = Get-Content -Raw (Join-Path $dir 'trail-metrics.json') |
        ConvertFrom-Json
    $perceptualData = Read-PerceptualMap $dir
    $targets = [ordered]@{}
    foreach ($target in $targetCases.GetEnumerator()) {
        if (-not $perceptualData.map.ContainsKey($target.Value)) {
            throw "Missing target perceptual case $($target.Value) for $($spec.name)"
        }
        $targets[$target.Key] = [double]$perceptualData.map[$target.Value]
    }
    $fiveHz = @($perceptualData.report.cases |
        Where-Object { [double]$_.frequency_hz -eq 5.0 })
    $delta4 = @($perceptualData.report.cases |
        Where-Object { [int]$_.delta_code -eq 4 })
    $candidates += [pscustomobject][ordered]@{
        name=[string]$spec.name
        architecture_mode=[int]$spec.architecture_mode
        factor_weak_seed=[bool]$spec.factor_weak_seed
        factor_long_risk=[bool]$spec.factor_long_risk
        replay_status=[string]$replay.status
        moving_clear_05_ms=[double]$trail.moving_square_clear_to_0_05_ms
        moving_flash_reduction=[double]$replay.moving_flash_reduction
        perceptual_min_reduction=[double](
            ($perceptualData.report.cases | Measure-Object reduction -Minimum).Minimum)
        perceptual_5hz_min_reduction=[double](
            ($fiveHz | Measure-Object reduction -Minimum).Minimum)
        perceptual_delta4_min_reduction=[double](
            ($delta4 | Measure-Object reduction -Minimum).Minimum)
        pan_mae=[double]$replay.pan_mae
        fast_pan_mae=[double]$replay.fast_pan_mae
        extreme_pan_mae=[double]$replay.extreme_pan_mae
        target_cases=$targets
    }
}

$control = @($candidates | Where-Object { $_.architecture_mode -eq 40 })[0]
$effects = [ordered]@{}
foreach ($factor in @('factor_weak_seed','factor_long_risk')) {
    $effects[$factor] = [ordered]@{
        moving_flash_reduction=(Factor-Effect $candidates $factor 'moving_flash_reduction')
        perceptual_min_reduction=(Factor-Effect $candidates $factor 'perceptual_min_reduction')
        perceptual_5hz_min_reduction=(Factor-Effect $candidates $factor 'perceptual_5hz_min_reduction')
        perceptual_delta4_min_reduction=(Factor-Effect $candidates $factor 'perceptual_delta4_min_reduction')
        pan_mae=(Factor-Effect $candidates $factor 'pan_mae')
        fast_pan_mae=(Factor-Effect $candidates $factor 'fast_pan_mae')
        extreme_pan_mae=(Factor-Effect $candidates $factor 'extreme_pan_mae')
    }
}

# Explicit SxL interaction: (11-01) - (10-00). Positive protection values mean
# synergy; positive MAE means the combination costs fidelity beyond additive effects.
$c00 = @($candidates | Where-Object { -not $_.factor_weak_seed -and -not $_.factor_long_risk })[0]
$c10 = @($candidates | Where-Object { $_.factor_weak_seed -and -not $_.factor_long_risk })[0]
$c01 = @($candidates | Where-Object { -not $_.factor_weak_seed -and $_.factor_long_risk })[0]
$c11 = @($candidates | Where-Object { $_.factor_weak_seed -and $_.factor_long_risk })[0]
$interaction = [ordered]@{}
foreach ($metric in @(
    'moving_flash_reduction','perceptual_min_reduction','perceptual_5hz_min_reduction',
    'perceptual_delta4_min_reduction','pan_mae','fast_pan_mae','extreme_pan_mae')) {
    $interaction[$metric] =
        ([double]$c11.$metric - [double]$c01.$metric) -
        ([double]$c10.$metric - [double]$c00.$metric)
}

# Prefer the strongest perceptual candidate that preserves the R-only motion envelope.
$eligible = @($candidates | Where-Object {
    $_.replay_status -eq 'SUCCESS' -and
    [double]$_.pan_mae -le [double]$control.pan_mae + 0.0005 -and
    [double]$_.fast_pan_mae -le [double]$control.fast_pan_mae + 0.0005 -and
    [double]$_.extreme_pan_mae -le [double]$control.extreme_pan_mae + 0.0010 -and
    [double]$_.moving_flash_reduction -ge 0.75
})
$selected = if ($eligible.Count -gt 0) {
    $eligible | Sort-Object `
        @{Expression='perceptual_min_reduction';Descending=$true}, `
        @{Expression='perceptual_5hz_min_reduction';Descending=$true}, `
        @{Expression='extreme_pan_mae';Descending=$false} |
        Select-Object -First 1
} else { $null }

$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/42'
    purpose='2^2 retest of direct weak-risk seeding and 0.40 s qualified-risk lifetime on the Matrix-41 R-only restoration baseline'
    screening_resolution='320x180'
    screening_fps=30
    candidate_count=$candidates.Count
    invariant='all candidates keep Matrix-41 R restoration ownership ON and D/G OFF; tuning, full replay corpus, signed-prime lifetime, and all other architecture semantics are unchanged; only S direct qualified weak-risk seed and L 0.40 s qualified-risk lifetime vary'
    factor_definition=[ordered]@{
        S='surface-validated direct NEXT-frame risk seed from an already opposition-qualified weak reversal'
        L='qualified surface-risk lifetime extends from 0.22 s to 0.40 s'
    }
    control=$control
    main_effects=$effects
    interaction_SxL=$interaction
    selection_gate=[ordered]@{
        pan_mae_vs_control_max_add=0.0005
        fast_pan_mae_vs_control_max_add=0.0005
        extreme_pan_mae_vs_control_max_add=0.0010
        moving_flash_reduction_min=0.75
        objective='maximize perceptual minimum reduction, then 5 Hz minimum reduction, then minimize extreme-pan MAE'
    }
    selected=$selected
    screen_candidates=$candidates
}
$report | ConvertTo-Json -Depth 12 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Matrix 42 R-baseline weak-protection factorial complete in $($sw.ElapsedMilliseconds) ms"
exit 0
