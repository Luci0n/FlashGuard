param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$inv = [Globalization.CultureInfo]::InvariantCulture

# Matrix 41 resumes causal architecture work after replay determinism checks.
# It is a complete 2^3 factorial decomposition of Matrix 36 mode 35's
# state-semantics factor, with mode 20 as 000 and mode 35 as 111.
# D = state-disocclusion ownership, R = restoration sequence gate,
# G = qualified-risk sequence gate. All candidates keep the base 0.22 s
# qualified-risk lifetime and base signed-prime lifetime.
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
    [bool]$Disocclusion,
    [bool]$Restore,
    [bool]$RiskGate,
    [hashtable]$Tune = @{}) {
    $r = [ordered]@{
        name=$Name; profile=1; full=1; small=1; fps=30; motion_scale=1.0
        architecture_mode=$Architecture
        factor_disocclusion=$Disocclusion
        factor_restore=$Restore
        factor_risk_gate=$RiskGate
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

$specs = @(
    (New-Spec 's000_mode20' 20 $false $false $false $mergedTune),
    (New-Spec 's100_disocclusion' 39 $true $false $false $mergedTune),
    (New-Spec 's010_restore' 40 $false $true $false $mergedTune),
    (New-Spec 's001_risk_gate' 41 $false $false $true $mergedTune),
    (New-Spec 's110_disocclusion_restore' 42 $true $true $false $mergedTune),
    (New-Spec 's101_disocclusion_risk_gate' 43 $true $false $true $mergedTune),
    (New-Spec 's011_restore_risk_gate' 44 $false $true $true $mergedTune),
    (New-Spec 's111_mode35' 35 $true $true $true $mergedTune)
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
Write-Host "FlashBench Matrix 41: $($specs.Count) state-semantics combinations"
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
$sw.Stop()
if ($p.ExitCode -ne 0) {
    throw "Matrix 41 replay batch failed with exit code $($p.ExitCode)"
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
    $map = $perceptualData.map
    $targets = [ordered]@{}
    foreach ($target in $targetCases.GetEnumerator()) {
        if (-not $map.ContainsKey($target.Value)) {
            throw "Missing target perceptual case $($target.Value) for $($spec.name)"
        }
        $targets[$target.Key] = [double]$map[$target.Value]
    }
    $fiveHz = @($perceptualData.report.cases |
        Where-Object { [double]$_.frequency_hz -eq 5.0 })
    $delta4 = @($perceptualData.report.cases |
        Where-Object { [int]$_.delta_code -eq 4 })
    $candidates += [pscustomobject][ordered]@{
        name=[string]$spec.name
        architecture_mode=[int]$spec.architecture_mode
        factor_disocclusion=[bool]$spec.factor_disocclusion
        factor_restore=[bool]$spec.factor_restore
        factor_risk_gate=[bool]$spec.factor_risk_gate
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
        moving_flash_target_cases=$targets
    }
}

$baseline = @($candidates | Where-Object { $_.architecture_mode -eq 20 })[0]
$full = @($candidates | Where-Object { $_.architecture_mode -eq 35 })[0]
$extremeRepairSpan = [double]$baseline.extreme_pan_mae - [double]$full.extreme_pan_mae
$factorNames = @('factor_disocclusion','factor_restore','factor_risk_gate')
$effects = [ordered]@{}
foreach ($factor in $factorNames) {
    $effects[$factor] = [ordered]@{
        # Positive reduction effects are beneficial; negative MAE effects are beneficial.
        moving_flash_reduction=(Factor-Effect $candidates $factor 'moving_flash_reduction')
        perceptual_min_reduction=(Factor-Effect $candidates $factor 'perceptual_min_reduction')
        perceptual_5hz_min_reduction=(Factor-Effect $candidates $factor 'perceptual_5hz_min_reduction')
        pan_mae=(Factor-Effect $candidates $factor 'pan_mae')
        fast_pan_mae=(Factor-Effect $candidates $factor 'fast_pan_mae')
        extreme_pan_mae=(Factor-Effect $candidates $factor 'extreme_pan_mae')
    }
}

$candidateComparisons = @()
foreach ($candidate in $candidates) {
    $repairFraction = if ([Math]::Abs($extremeRepairSpan) -gt 1e-12) {
        ([double]$baseline.extreme_pan_mae - [double]$candidate.extreme_pan_mae) /
            $extremeRepairSpan
    } else { 0.0 }
    $candidateComparisons += [pscustomobject][ordered]@{
        name=[string]$candidate.name
        architecture_mode=[int]$candidate.architecture_mode
        extreme_pan_repair_fraction_vs_mode20_to_mode35=[double]$repairFraction
        perceptual_gain_vs_mode20=[double]$candidate.perceptual_min_reduction -
            [double]$baseline.perceptual_min_reduction
        moving_flash_gain_vs_mode20=[double]$candidate.moving_flash_reduction -
            [double]$baseline.moving_flash_reduction
    }
}

$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/41'
    purpose='2^3 causal decomposition of mode-35 state semantics after Matrix 40 restored confidence in replay repeatability'
    screening_resolution='320x180'
    screening_fps=30
    candidate_count=$candidates.Count
    invariant='all candidates inherit mode-20 sequence architecture and identical tuning; only D state-disocclusion ownership, R qualified-state restoration sequence gating, and G qualified-risk sequence gating vary; qualified-risk lifetime remains 0.22 s and signed-prime lifetime remains at the mode-20 base'
    factor_definition=[ordered]@{
        D='state-disocclusion ownership uses the mode-21 formula'
        R='restoreQualifiedState uses stationarySequenceStateGate rather than stationaryRepetitionGate'
        G='stationaryRiskStateGate uses stationarySequenceStateGate rather than stationaryRepetitionGate'
    }
    controls=[ordered]@{
        s000_mode20=$baseline
        s111_mode35=$full
        extreme_pan_repair_span=$extremeRepairSpan
    }
    main_effects=$effects
    candidate_comparisons=$candidateComparisons
    selected=$null
    screen_candidates=$candidates
}
$report | ConvertTo-Json -Depth 12 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Matrix 41 state-semantics decomposition complete in $($sw.ElapsedMilliseconds) ms"
exit 0
