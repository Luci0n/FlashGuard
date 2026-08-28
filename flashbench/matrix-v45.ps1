param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$inv = [Globalization.CultureInfo]::InvariantCulture

# Matrix 45 keeps Matrix 44's behavior-identical R+L diagnostic architecture and
# traces the four constituents of signedPrimeContinuity. It also records PRIME
# immediately before the continuity multiplication so a true veto can be proven.
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
        architecture_mode=$Architecture
        risk_neutral=$null; risk_gain=$null
    }
    foreach ($k in $allKeys) { $r[$k] = $null }
    foreach ($k in $Tune.Keys) {
        if (-not $r.Contains($k)) { throw "Unknown tuning key '$k'" }
        $r[$k] = [double]$Tune[$k]
    }
    [pscustomobject]$r
}

$tune = @{
    event_delta_low=.003; event_delta_high=.018
    intrinsic_residual_low=.004; intrinsic_residual_high=.035
    hold_gate_low=.08; hold_gate_high=.38
    direct_intrinsic_low=.003; direct_intrinsic_high=.018
    event_seed_low=.010; event_seed_high=.075
    repeated_memory_low=.20; repeated_memory_high=.45
    surface_risk_tau=.100
    risk_neutral=.12; risk_gain=2.0
}
$spec = New-Spec 'rl_prime_continuity_trace' 52 $tune
$specs = @($spec)

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
Write-Host 'FlashBench Matrix 45: one R+L PRIME-continuity trace replay'
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
$sw.Stop()
if ($p.ExitCode -ne 0) {
    throw "Matrix 45 replay batch failed with exit code $($p.ExitCode)"
}

$candidateDir = Join-Path $OutputDir $spec.name
$replay = Get-Content -Raw (Join-Path $candidateDir 'synthetic-replay.json') |
    ConvertFrom-Json
$trail = Get-Content -Raw (Join-Path $candidateDir 'trail-metrics.json') |
    ConvertFrom-Json
$perceptual = Get-Content -Raw (Join-Path $candidateDir 'perceptual-sweep.json') |
    ConvertFrom-Json
$tracePath = Join-Path $candidateDir 'matrix45-prime-continuity-trace.json'
if (-not (Test-Path $tracePath)) {
    throw 'Matrix 45 PRIME-continuity trace was not produced.'
}
$trace = Get-Content -Raw $tracePath | ConvertFrom-Json
if ($trace.schema -ne 'FLASHGUARD_MATRIX45_PRIME_CONTINUITY_TRACE/1') {
    throw "Unexpected Matrix 45 trace schema '$($trace.schema)'"
}

function Perceptual-Reduction([int]$Delta, [double]$Phase) {
    $case = @($perceptual.cases | Where-Object {
        [int]$_.delta_code -eq $Delta -and
        [Math]::Abs([double]$_.frequency_hz - 5.0) -lt 0.001 -and
        [Math]::Abs([double]$_.phase_frames - $Phase) -lt 0.001
    })[0]
    if ($null -eq $case) {
        throw "Missing 5 Hz perceptual case delta=$Delta phase=$Phase"
    }
    [double]$case.reduction
}
function First-Matching([object[]]$Rows, [scriptblock]$Predicate) {
    foreach ($row in @($Rows | Sort-Object frame)) {
        if (& $Predicate $row) { return $row }
    }
    $null
}
function Describe-Row([object]$Row) {
    if ($null -eq $Row) { return $null }
    [ordered]@{
        frame=[int]$Row.frame
        high=[bool]$Row.high
        stationary_prime_continuity=[double]$Row.stationary_prime_continuity
        verified_current_surface_transport=[double]$Row.verified_current_surface_transport
        textureless_stationary_fallback_gate=[double]$Row.textureless_stationary_fallback_gate
        hard_state_disocclusion=[double]$Row.hard_state_disocclusion
        signed_prime_continuity_evidence=[double]$Row.signed_prime_continuity_evidence
        signed_prime_continuity=[double]$Row.signed_prime_continuity
        prime_before_continuity=[double]$Row.prime_before_continuity
        prime_after_continuity=[double]$Row.prime_after_continuity
        would_be_opposition_strength=[double]$Row.would_be_opposition_strength
        actual_opposition_strength=[double]$Row.actual_opposition_strength
        architecture_luma_delta=[double]$Row.architecture_luma_delta
        surface_memory_strength=[double]$Row.surface_memory_strength
    }
}
function Min-Metric([object[]]$Rows, [string]$Property) {
    if ($Rows.Count -eq 0) { return 0.0 }
    [double](($Rows | Measure-Object -Property $Property -Minimum).Minimum)
}
function Max-Metric([object[]]$Rows, [string]$Property) {
    if ($Rows.Count -eq 0) { return 0.0 }
    [double](($Rows | Measure-Object -Property $Property -Maximum).Maximum)
}
function Phase-Summary([int]$Delta, [double]$Phase) {
    $rows = @($trace.frames | Where-Object {
        [int]$_.delta_code -eq $Delta -and
        [Math]::Abs([double]$_.phase_frames - $Phase) -lt 0.001
    } | Sort-Object frame)
    if ($rows.Count -eq 0) {
        throw "Missing PRIME-continuity trace delta=$Delta phase=$Phase"
    }

    # A continuity kill means useful PRIME existed immediately before the
    # multiplication but essentially vanished after it.
    $kill = First-Matching $rows {
        param($r)
        [Math]::Abs([double]$r.prime_before_continuity) -gt 0.01 -and
        [Math]::Abs([double]$r.prime_after_continuity) -le 0.001
    }
    # Stronger causal probe: on this frame an opposite transition could have
    # consumed PRIME before continuity, but post-veto opposition is essentially 0.
    $blockedOpposition = First-Matching $rows {
        param($r)
        [double]$r.would_be_opposition_strength -gt 0.01 -and
        [double]$r.actual_opposition_strength -le 0.001
    }
    $partialSuppression = First-Matching $rows {
        param($r)
        [double]$r.would_be_opposition_strength -gt 0.01 -and
        [double]$r.signed_prime_continuity -gt 0.001 -and
        [double]$r.signed_prime_continuity -lt 0.999
    }

    [ordered]@{
        perceptual_reduction=(Perceptual-Reduction $Delta $Phase)
        first_prime_kill=(Describe-Row $kill)
        first_blocked_would_be_opposition=(Describe-Row $blockedOpposition)
        first_partial_continuity_suppression=(Describe-Row $partialSuppression)
        min_stationary_prime_continuity=(Min-Metric $rows 'stationary_prime_continuity')
        min_verified_current_surface_transport=(Min-Metric $rows 'verified_current_surface_transport')
        min_textureless_stationary_fallback_gate=(Min-Metric $rows 'textureless_stationary_fallback_gate')
        max_hard_state_disocclusion=(Max-Metric $rows 'hard_state_disocclusion')
        min_signed_prime_continuity=(Min-Metric $rows 'signed_prime_continuity')
        max_would_be_opposition_strength=(Max-Metric $rows 'would_be_opposition_strength')
        max_actual_opposition_strength=(Max-Metric $rows 'actual_opposition_strength')
    }
}

$deltas = @(4, 8, 12, 16, 24, 32)
$phasePairs = [ordered]@{}
foreach ($delta in $deltas) {
    $phase0 = Phase-Summary $delta 0.0
    $phase05 = Phase-Summary $delta 0.5
    $phasePairs["delta$delta"] = [ordered]@{
        phase0=$phase0
        phase0_5=$phase05
        comparison=[ordered]@{
            reduction_phase0_5_minus_phase0=
                [double]$phase05.perceptual_reduction -
                [double]$phase0.perceptual_reduction
            phase0_has_blocked_would_be_opposition=
                $null -ne $phase0.first_blocked_would_be_opposition
            phase0_5_has_blocked_would_be_opposition=
                $null -ne $phase05.first_blocked_would_be_opposition
        }
    }
}

# Aggregate all blocked-opposition rows to identify which constituent is actually
# responsible. hardStateDisocclusion is a veto; the other three are positive
# continuity supports combined before that veto.
$blockedRows = @($trace.frames | Where-Object {
    [double]$_.would_be_opposition_strength -gt 0.01 -and
    [double]$_.actual_opposition_strength -le 0.001
})
$blockedAtHardDisocclusion = @($blockedRows | Where-Object {
    [double]$_.hard_state_disocclusion -ge 0.999
}).Count
$blockedWithNoPositiveSupport = @($blockedRows | Where-Object {
    [double]$_.hard_state_disocclusion -lt 0.999 -and
    [double]$_.stationary_prime_continuity -le 0.001 -and
    [double]$_.verified_current_surface_transport -le 0.001 -and
    [double]$_.textureless_stationary_fallback_gate -le 0.001
}).Count
$blockedOther = $blockedRows.Count -
    $blockedAtHardDisocclusion - $blockedWithNoPositiveSupport

$perceptualMin = [double](($perceptual.cases |
    Measure-Object reduction -Minimum).Minimum)
$candidate = [ordered]@{
    name=$spec.name
    architecture_mode=52
    replay_status=[string]$replay.status
    moving_clear_05_ms=[double]$trail.moving_square_clear_to_0_05_ms
    moving_flash_reduction=[double]$replay.moving_flash_reduction
    perceptual_min_reduction=$perceptualMin
    pan_mae=[double]$replay.pan_mae
    fast_pan_mae=[double]$replay.fast_pan_mae
    extreme_pan_mae=[double]$replay.extreme_pan_mae
}

$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/45'
    purpose='identify which signedPrimeContinuity constituent destroys otherwise usable stationary PRIME before the phase-0 opposite transition'
    screening_resolution='320x180'
    screening_fps=30
    candidate_count=1
    invariant='mode 52 is behavior-identical to Matrix-44 mode 51 and Matrix-42 R+L: R restoration ownership and 0.40 s qualified surface-risk lifetime ON; D/G/S/A/P OFF; all new writes are replay-only diagnostics'
    trace_schema=[string]$trace.schema
    diagnostic_channels=[ordered]@{
        mrt0=@('stationary_prime_continuity','verified_current_surface_transport','textureless_stationary_fallback_gate','hard_state_disocclusion')
        mrt1=@('signed_prime_continuity_evidence','signed_prime_continuity','prime_before_continuity_encoded','would_be_opposition_strength')
        mrt2=@('preprocess_luma_delta','architecture_luma_delta','authority_current_event','surface_memory_strength')
    }
    attribution_rule='if would_be_opposition_strength is positive but actual_opposition_strength is zero, continuity destroyed usable PRIME on that frame; hard_state_disocclusion near 1 identifies the explicit veto, otherwise all three positive support gates near 0 identifies continuity-evidence loss'
    blocked_opposition_attribution=[ordered]@{
        total_blocked_rows=$blockedRows.Count
        hard_disocclusion_veto_rows=$blockedAtHardDisocclusion
        no_positive_continuity_support_rows=$blockedWithNoPositiveSupport
        other_rows=$blockedOther
    }
    phase_pairs=$phasePairs
    selected=$candidate
    screen_candidates=@($candidate)
    elapsed_ms=$sw.ElapsedMilliseconds
}
$report | ConvertTo-Json -Depth 18 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Matrix 45 PRIME-continuity trace complete in $($sw.ElapsedMilliseconds) ms"
exit 0
