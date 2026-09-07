param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$inv = [Globalization.CultureInfo]::InvariantCulture

# Matrix 39 isolates NVOFA temporal hints as the remaining source of the weak
# 5 Hz nondeterminism exposed by Matrix 38. All six candidates are identical
# mode 29. Three run in a normal-hints process and three run in a fresh process
# with only --replay-disable-nvof-temporal-hints added.
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

$base = @{
    event_delta_low=.003; event_delta_high=.018; intrinsic_residual_low=.004; intrinsic_residual_high=.035
    hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018; event_seed_low=.010; event_seed_high=.075
    repeated_memory_low=.20; repeated_memory_high=.45
    risk_neutral=.12
}
$tune = @{surface_risk_tau=.100; risk_gain=2.0}

function Invoke-Condition([string]$Condition, [string]$Prefix, [bool]$DisableHints) {
    $dir = Join-Path $OutputDir $Condition
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $plan = Join-Path $dir 'plan.tsv'
    $header = @('name','profile','full','small','fps','motion_scale') +
        $allKeys + @('architecture_mode') + $riskKeys
    $lines = @('# ' + ($header -join '<TAB>'))
    $specs = @(
        (New-Spec ($Prefix + 'repeat1_state400_tau100_gain200') 29 (Merge-Tune $base $tune)),
        (New-Spec ($Prefix + 'repeat2_state400_tau100_gain200') 29 (Merge-Tune $base $tune)),
        (New-Spec ($Prefix + 'repeat3_state400_tau100_gain200') 29 (Merge-Tune $base $tune))
    )
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
        '--synthetic-replay-batch-output', ('"' + $dir + '"'),
        '--replay-width', '320', '--replay-height', '180'
    )
    if ($DisableHints) { $args += '--replay-disable-nvof-temporal-hints' }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
    $sw.Stop()
    if ($p.ExitCode -ne 0) {
        throw "Matrix 39 condition '$Condition' failed with exit code $($p.ExitCode)"
    }
    [pscustomobject]@{
        condition=$Condition
        directory=$dir
        elapsed_ms=$sw.ElapsedMilliseconds
        specs=$specs
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
    $candidateReports = @()
    $maps = @()
    foreach ($spec in @($Run.specs)) {
        $candidateDir = Join-Path $Run.directory $spec.name
        $replay = Get-Content -Raw (Join-Path $candidateDir 'synthetic-replay.json') |
            ConvertFrom-Json
        $map = Read-PerceptualMap $candidateDir
        $maps += ,$map
        $candidateReports += [pscustomobject][ordered]@{
            condition=[string]$Run.condition
            name=[string]$spec.name
            architecture_mode=29
            replay_status=[string]$replay.status
            moving_flash_reduction=[double]$replay.moving_flash_reduction
            pan_mae=[double]$replay.pan_mae
            fast_pan_mae=[double]$replay.fast_pan_mae
            extreme_pan_mae=[double]$replay.extreme_pan_mae
            perceptual_min_reduction=[double](($map.Values | Measure-Object -Minimum).Minimum)
        }
    }

    $keys = @($maps[0].Keys | Sort-Object)
    $allCaseMaxSpread = 0.0
    foreach ($key in $keys) {
        $values = @($maps | ForEach-Object {
            if (-not $_.ContainsKey($key)) {
                throw "Missing perceptual case $key in condition $($Run.condition)"
            }
            [double]$_[$key]
        })
        $allCaseMaxSpread = [Math]::Max($allCaseMaxSpread, (Spread $values))
    }

    $targetReport = [ordered]@{}
    $targetMaxSpread = 0.0
    foreach ($target in $targetCases.GetEnumerator()) {
        $values = @($maps | ForEach-Object {
            if (-not $_.ContainsKey($target.Value)) {
                throw "Missing target case $($target.Value) in condition $($Run.condition)"
            }
            [double]$_[$target.Value]
        })
        $spread = Spread $values
        $targetMaxSpread = [Math]::Max($targetMaxSpread, $spread)
        $targetReport[$target.Key] = [ordered]@{values=$values; spread=$spread}
    }

    $panValues = @($candidateReports | ForEach-Object { [double]$_.pan_mae })
    $fastPanValues = @($candidateReports | ForEach-Object { [double]$_.fast_pan_mae })
    $extremePanValues = @($candidateReports | ForEach-Object { [double]$_.extreme_pan_mae })
    $movingFlashValues = @($candidateReports | ForEach-Object { [double]$_.moving_flash_reduction })
    $panFamilyMaxSpread = [Math]::Max(
        (Spread $panValues),
        [Math]::Max((Spread $fastPanValues), (Spread $extremePanValues)))

    [pscustomobject][ordered]@{
        condition=[string]$Run.condition
        elapsed_ms=[long]$Run.elapsed_ms
        all_perceptual_max_spread=$allCaseMaxSpread
        target_perceptual_max_spread=$targetMaxSpread
        target_cases=$targetReport
        pan_mae_values=$panValues
        pan_mae_spread=(Spread $panValues)
        fast_pan_mae_values=$fastPanValues
        fast_pan_mae_spread=(Spread $fastPanValues)
        extreme_pan_mae_values=$extremePanValues
        extreme_pan_mae_spread=(Spread $extremePanValues)
        pan_family_max_spread=$panFamilyMaxSpread
        moving_flash_reduction_values=$movingFlashValues
        moving_flash_reduction_spread=(Spread $movingFlashValues)
        perceptual_determinism_pass=($allCaseMaxSpread -le 0.005)
        candidates=$candidateReports
    }
}

Write-Host 'FlashBench Matrix 39: normal NVOFA temporal hints'
$normalRun = Invoke-Condition 'normal-hints' 'normal_' $false
Write-Host 'FlashBench Matrix 39: NVOFA temporal hints forced off'
$noHintsRun = Invoke-Condition 'temporal-hints-off' 'nohints_' $true
$normal = Read-Condition $normalRun
$noHints = Read-Condition $noHintsRun
$combinedCandidates = @($normal.candidates) + @($noHints.candidates)

$normalUnstable = [double]$normal.all_perceptual_max_spread -gt 0.005
$noHintsStable = [double]$noHints.all_perceptual_max_spread -le 0.005
$spreadReduction = [double]$normal.all_perceptual_max_spread -
    [double]$noHints.all_perceptual_max_spread
$spreadRatio = if ([double]$normal.all_perceptual_max_spread -gt 1e-12) {
    [double]$noHints.all_perceptual_max_spread /
        [double]$normal.all_perceptual_max_spread
} else { 0.0 }

$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/39'
    purpose='causal isolation of NVOFA temporal hints as a source of weak low-frequency replay nondeterminism after Matrix 38 measured up to 0.283 reduction spread among identical candidates'
    screening_resolution='320x180'
    screening_fps=30
    candidate_count=$combinedCandidates.Count
    invariant='all six candidates use identical mode-29 architecture and tuning with the established full replay corpus; three run with normal NVOFA temporal hints and three in a fresh process with only temporal hints forced off'
    determinism_gate=[ordered]@{perceptual_spread_max=0.005}
    normal_hints=$normal
    temporal_hints_off=$noHints
    inference=[ordered]@{
        normal_reproduced_instability=$normalUnstable
        hints_off_is_deterministic=$noHintsStable
        all_perceptual_spread_reduction=$spreadReduction
        hints_off_to_normal_spread_ratio=$spreadRatio
        temporal_hints_cause_supported=($normalUnstable -and $noHintsStable)
    }
    selected=$null
    screen_candidates=$combinedCandidates
}
$report | ConvertTo-Json -Depth 12 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host 'Matrix 39 temporal-hint isolation complete'
exit 0
