param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$inv = [Globalization.CultureInfo]::InvariantCulture

# Matrix 40 isolates persistent-batch candidate-order contamination after
# Matrix 39 showed same-architecture mode-29 repeats are exactly deterministic.
# Two fresh processes use normal NVOFA temporal hints and identical replay/tuning:
# one runs BBBBBB (mode 29 only), the other reproduces Matrix 38's ABABBA order
# with A=mode 35 and B=mode 29.
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
function New-Spec([string]$Name, [int]$Architecture, [string]$Role, [int]$Position,
                  [hashtable]$Tune = @{}) {
    $r = [ordered]@{
        name=$Name; profile=1; full=1; small=1; fps=30; motion_scale=1.0
        architecture_mode=$Architecture; role=$Role; position=$Position
        risk_neutral=$null; risk_gain=$null
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

$base = @{
    event_delta_low=.003; event_delta_high=.018; intrinsic_residual_low=.004; intrinsic_residual_high=.035
    hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018; event_seed_low=.010; event_seed_high=.075
    repeated_memory_low=.20; repeated_memory_high=.45
    risk_neutral=.12
}
$tune = @{surface_risk_tau=.100; risk_gain=2.0}
$mergedTune = Merge-Tune $base $tune

function New-BOnlySpecs {
    @(
        (New-Spec 'bonly_b1_state400_tau100_gain200' 29 'B' 1 $mergedTune),
        (New-Spec 'bonly_b2_state400_tau100_gain200' 29 'B' 2 $mergedTune),
        (New-Spec 'bonly_b3_state400_tau100_gain200' 29 'B' 3 $mergedTune),
        (New-Spec 'bonly_b4_state400_tau100_gain200' 29 'B' 4 $mergedTune),
        (New-Spec 'bonly_b5_state400_tau100_gain200' 29 'B' 5 $mergedTune),
        (New-Spec 'bonly_b6_state400_tau100_gain200' 29 'B' 6 $mergedTune)
    )
}
function New-MixedSpecs {
    @(
        (New-Spec 'mixed_a1_state220_tau100_gain200' 35 'A' 1 $mergedTune),
        (New-Spec 'mixed_b1_state400_tau100_gain200' 29 'B' 2 $mergedTune),
        (New-Spec 'mixed_a2_state220_tau100_gain200' 35 'A' 3 $mergedTune),
        (New-Spec 'mixed_b2_state400_tau100_gain200' 29 'B' 4 $mergedTune),
        (New-Spec 'mixed_b3_state400_tau100_gain200' 29 'B' 5 $mergedTune),
        (New-Spec 'mixed_a3_state220_tau100_gain200' 35 'A' 6 $mergedTune)
    )
}

function Invoke-Condition([string]$Condition, [object[]]$Specs) {
    $dir = Join-Path $OutputDir $Condition
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $plan = Join-Path $dir 'plan.tsv'
    $header = @('name','profile','full','small','fps','motion_scale') +
        $allKeys + @('architecture_mode') + $riskKeys
    $lines = @('# ' + ($header -join '<TAB>'))
    foreach ($s in $Specs) {
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
        '--synthetic-replay-batch-output', ('"' + $dir + '"'),
        '--replay-width', '320', '--replay-height', '180'
    )
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
    $sw.Stop()
    if ($p.ExitCode -ne 0) {
        throw "Matrix 40 condition '$Condition' failed with exit code $($p.ExitCode)"
    }
    [pscustomobject]@{
        condition=$Condition
        directory=$dir
        elapsed_ms=$sw.ElapsedMilliseconds
        specs=$Specs
    }
}

function Perceptual-Key([object]$Case) {
    '{0}|{1}|{2}' -f [int]$Case.delta_code,
        ([double]$Case.frequency_hz).ToString('0.###', $inv),
        ([double]$Case.phase_frames).ToString('0.###', $inv)
}
function Spread([object[]]$Values) {
    $numbers = @($Values | ForEach-Object { [double]$_ })
    if ($numbers.Count -lt 1) { return 0.0 }
    $minimum = [double](($numbers | Measure-Object -Minimum).Minimum)
    $maximum = [double](($numbers | Measure-Object -Maximum).Maximum)
    [double]($maximum - $minimum)
}
function Mean([object[]]$Values) {
    $numbers = @($Values | ForEach-Object { [double]$_ })
    if ($numbers.Count -lt 1) { return 0.0 }
    [double](($numbers | Measure-Object -Average).Average)
}
function Read-PerceptualMap([string]$CandidateDir) {
    $per = Get-Content -Raw (Join-Path $CandidateDir 'perceptual-sweep.json') |
        ConvertFrom-Json
    $map = @{}
    foreach ($case in @($per.cases)) {
        $map[(Perceptual-Key $case)] = [double]$case.reduction
    }
    $map
}

$targetCases = [ordered]@{
    delta4_5hz_phase0='4|5|0'
    delta4_5hz_phase0_5='4|5|0.5'
    delta12_5hz_phase0_5='12|5|0.5'
    delta32_5hz_phase0_5='32|5|0.5'
    delta4_10hz_phase0_5='4|10|0.5'
    delta4_15hz_phase0_5='4|15|0.5'
}

function Read-Condition([object]$Run) {
    $publicCandidates = @()
    $internalCandidates = @()
    foreach ($spec in @($Run.specs)) {
        $candidateDir = Join-Path $Run.directory $spec.name
        $replay = Get-Content -Raw (Join-Path $candidateDir 'synthetic-replay.json') |
            ConvertFrom-Json
        $map = Read-PerceptualMap $candidateDir
        $public = [pscustomobject][ordered]@{
            condition=[string]$Run.condition
            name=[string]$spec.name
            role=[string]$spec.role
            position=[int]$spec.position
            architecture_mode=[int]$spec.architecture_mode
            replay_status=[string]$replay.status
            moving_flash_reduction=[double]$replay.moving_flash_reduction
            pan_mae=[double]$replay.pan_mae
            fast_pan_mae=[double]$replay.fast_pan_mae
            extreme_pan_mae=[double]$replay.extreme_pan_mae
            perceptual_min_reduction=[double](($map.Values | Measure-Object -Minimum).Minimum)
        }
        $publicCandidates += $public
        $internalCandidates += [pscustomobject]@{
            public=$public
            map=$map
        }
    }
    [pscustomobject]@{
        condition=[string]$Run.condition
        elapsed_ms=[long]$Run.elapsed_ms
        public_candidates=$publicCandidates
        internal_candidates=$internalCandidates
    }
}

function Analyze-Role([object]$Condition, [string]$Role) {
    $internal = @($Condition.internal_candidates |
        Where-Object { $_.public.role -eq $Role })
    if ($internal.Count -lt 1) { return $null }

    $keys = @($internal[0].map.Keys | Sort-Object)
    $allCaseMaxSpread = 0.0
    foreach ($key in $keys) {
        $values = @($internal | ForEach-Object {
            if (-not $_.map.ContainsKey($key)) {
                throw "Missing perceptual case $key in $($Condition.condition) role $Role"
            }
            [double]$_.map[$key]
        })
        $allCaseMaxSpread = [Math]::Max($allCaseMaxSpread, (Spread $values))
    }

    $targets = [ordered]@{}
    $targetMaxSpread = 0.0
    foreach ($target in $targetCases.GetEnumerator()) {
        $values = @($internal | ForEach-Object {
            if (-not $_.map.ContainsKey($target.Value)) {
                throw "Missing target case $($target.Value) in $($Condition.condition) role $Role"
            }
            [double]$_.map[$target.Value]
        })
        $spread = Spread $values
        $targetMaxSpread = [Math]::Max($targetMaxSpread, $spread)
        $targets[$target.Key] = [ordered]@{values=$values; spread=$spread}
    }

    $public = @($internal | ForEach-Object { $_.public })
    $panValues = @($public | ForEach-Object { [double]$_.pan_mae })
    $fastPanValues = @($public | ForEach-Object { [double]$_.fast_pan_mae })
    $extremePanValues = @($public | ForEach-Object { [double]$_.extreme_pan_mae })
    $movingFlashValues = @($public | ForEach-Object { [double]$_.moving_flash_reduction })
    $panFamilyMaxSpread = [Math]::Max(
        (Spread $panValues),
        [Math]::Max((Spread $fastPanValues), (Spread $extremePanValues)))

    [pscustomobject][ordered]@{
        role=$Role
        architecture_mode=[int]$public[0].architecture_mode
        candidate_names=@($public | ForEach-Object { $_.name })
        positions=@($public | ForEach-Object { $_.position })
        all_perceptual_max_spread=$allCaseMaxSpread
        target_perceptual_max_spread=$targetMaxSpread
        target_cases=$targets
        pan_family_max_spread=$panFamilyMaxSpread
        moving_flash_reduction_spread=(Spread $movingFlashValues)
        perceptual_determinism_pass=($allCaseMaxSpread -le 0.005)
    }
}

function Compare-BAcrossConditions([object]$BOnlyCondition, [object]$MixedCondition) {
    $baseline = @($BOnlyCondition.internal_candidates |
        Where-Object { $_.public.role -eq 'B' })
    $mixed = @($MixedCondition.internal_candidates |
        Where-Object { $_.public.role -eq 'B' })
    if ($baseline.Count -lt 1 -or $mixed.Count -lt 1) {
        throw 'Matrix 40 requires B candidates in both conditions'
    }

    $keys = @($baseline[0].map.Keys | Sort-Object)
    $maxAbsDelta = 0.0
    foreach ($key in $keys) {
        $baselineValues = @($baseline | ForEach-Object { [double]$_.map[$key] })
        $baselineMean = Mean $baselineValues
        foreach ($candidate in $mixed) {
            $delta = [Math]::Abs([double]$candidate.map[$key] - $baselineMean)
            $maxAbsDelta = [Math]::Max($maxAbsDelta, $delta)
        }
    }

    $targets = [ordered]@{}
    $targetMaxAbsDelta = 0.0
    foreach ($target in $targetCases.GetEnumerator()) {
        $baselineValues = @($baseline | ForEach-Object {
            [double]$_.map[$target.Value]
        })
        $baselineMean = Mean $baselineValues
        $mixedValues = @($mixed | ForEach-Object {
            [double]$_.map[$target.Value]
        })
        $absDeltas = @($mixedValues | ForEach-Object {
            [Math]::Abs([double]$_ - $baselineMean)
        })
        $caseMax = [double](($absDeltas | Measure-Object -Maximum).Maximum)
        $targetMaxAbsDelta = [Math]::Max($targetMaxAbsDelta, $caseMax)
        $targets[$target.Key] = [ordered]@{
            bonly_values=$baselineValues
            bonly_mean=$baselineMean
            mixed_b_values=$mixedValues
            mixed_b_abs_deltas=$absDeltas
            max_abs_delta=$caseMax
        }
    }

    [pscustomobject][ordered]@{
        all_perceptual_max_abs_delta_from_bonly_mean=$maxAbsDelta
        target_perceptual_max_abs_delta_from_bonly_mean=$targetMaxAbsDelta
        target_cases=$targets
        matches_bonly_within_gate=($maxAbsDelta -le 0.005)
    }
}

Write-Host 'FlashBench Matrix 40: BBBBBB fresh-process control'
$bonlyRun = Invoke-Condition 'bbbbbb-control' (New-BOnlySpecs)
Write-Host 'FlashBench Matrix 40: ABABBA fresh-process order'
$mixedRun = Invoke-Condition 'ababba-mixed' (New-MixedSpecs)

$bonly = Read-Condition $bonlyRun
$mixed = Read-Condition $mixedRun
$bonlyB = Analyze-Role $bonly 'B'
$mixedB = Analyze-Role $mixed 'B'
$mixedA = Analyze-Role $mixed 'A'
$crossB = Compare-BAcrossConditions $bonly $mixed

$bonlyDrift = [double]$bonlyB.all_perceptual_max_spread -gt 0.005
$mixedBDrift = [double]$mixedB.all_perceptual_max_spread -gt 0.005
$crossBShift = [double]$crossB.all_perceptual_max_abs_delta_from_bonly_mean -gt 0.005
$orderEffect = (-not $bonlyDrift) -and ($mixedBDrift -or $crossBShift)

$combinedCandidates = @($bonly.public_candidates) + @($mixed.public_candidates)
$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/40'
    purpose='causal isolation of persistent-batch candidate-order contamination after Matrix 39 showed exact same-architecture repeatability with normal NVOFA temporal hints'
    screening_resolution='320x180'
    screening_fps=30
    candidate_count=$combinedCandidates.Count
    invariant='two fresh processes use normal NVOFA temporal hints, identical full replay corpus, identical tuning, and the same transformed benchmark source; BBBBBB uses six mode-29 candidates while ABABBA reproduces Matrix 38 order with A=mode35 and B=mode29'
    determinism_gate=[ordered]@{perceptual_spread_max=0.005; cross_condition_abs_delta_max=0.005}
    bbbbbb=[ordered]@{
        elapsed_ms=$bonly.elapsed_ms
        b_mode29=$bonlyB
        candidates=$bonly.public_candidates
    }
    ababba=[ordered]@{
        elapsed_ms=$mixed.elapsed_ms
        b_mode29=$mixedB
        a_mode35=$mixedA
        candidates=$mixed.public_candidates
    }
    b_cross_condition=$crossB
    inference=[ordered]@{
        bbbbbb_session_drift=$bonlyDrift
        ababba_b_internal_drift=$mixedBDrift
        ababba_b_shift_from_bonly=$crossBShift
        matrix38_style_instability_reproduced=($mixedBDrift -or $crossBShift)
        architecture_order_contamination_supported=$orderEffect
        process_session_lifetime_leak_supported=$bonlyDrift
    }
    selected=$null
    screen_candidates=$combinedCandidates
}
$report | ConvertTo-Json -Depth 12 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host 'Matrix 40 candidate-order isolation complete'
exit 0
