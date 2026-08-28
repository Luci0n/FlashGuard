param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results'
)

$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$inv = [Globalization.CultureInfo]::InvariantCulture

# Matrix 44 is diagnostic rather than a new protection candidate. Mode 51 is
# behavior-identical to Matrix 42's R+L control and exposes the center-pixel
# PRIME -> opposition -> risk seed -> transported risk -> display-authority chain
# during every 5 Hz perceptual phase case.
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
$spec = New-Spec 'rl_activation_trace' 51 $tune
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
Write-Host 'FlashBench Matrix 44: one R+L activation-trace replay'
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru
$sw.Stop()
if ($p.ExitCode -ne 0) {
    throw "Matrix 44 replay batch failed with exit code $($p.ExitCode)"
}

$candidateDir = Join-Path $OutputDir $spec.name
$replay = Get-Content -Raw (Join-Path $candidateDir 'synthetic-replay.json') |
    ConvertFrom-Json
$trail = Get-Content -Raw (Join-Path $candidateDir 'trail-metrics.json') |
    ConvertFrom-Json
$perceptual = Get-Content -Raw (Join-Path $candidateDir 'perceptual-sweep.json') |
    ConvertFrom-Json
$tracePath = Join-Path $candidateDir 'matrix44-activation-trace.json'
if (-not (Test-Path $tracePath)) {
    throw 'Matrix 44 activation trace was not produced.'
}
$trace = Get-Content -Raw $tracePath | ConvertFrom-Json
if ($trace.schema -ne 'FLASHGUARD_MATRIX44_ACTIVATION_TRACE/1') {
    throw "Unexpected Matrix 44 trace schema '$($trace.schema)'"
}

function Perceptual-Reduction([int]$Delta, [double]$Phase) {
    $case = @($perceptual.cases | Where-Object {
        [int]$_.delta_code -eq $Delta -and
        [Math]::Abs([double]$_.frequency_hz - 5.0) -lt 0.001 -and
        [Math]::Abs([double]$_.phase_frames - $Phase) -lt 0.001
    })[0]
    if ($null -eq $case) { throw "Missing 5 Hz perceptual case delta=$Delta phase=$Phase" }
    [double]$case.reduction
}
function First-Frame([object[]]$Rows, [string]$Property, [double]$Threshold = 0.001) {
    foreach ($row in @($Rows | Sort-Object frame)) {
        if ([double]$row.$Property -gt $Threshold) { return [int]$row.frame }
    }
    $null
}
function Peak([object[]]$Rows, [string]$Property) {
    if ($Rows.Count -eq 0) { return 0.0 }
    [double](($Rows | Measure-Object -Property $Property -Maximum).Maximum)
}
function Sum-Metric([object[]]$Rows, [string]$Property) {
    if ($Rows.Count -eq 0) { return 0.0 }
    [double](($Rows | Measure-Object -Property $Property -Sum).Sum)
}
function Phase-Summary([int]$Delta, [double]$Phase) {
    $rows = @($trace.frames | Where-Object {
        [int]$_.delta_code -eq $Delta -and
        [Math]::Abs([double]$_.phase_frames - $Phase) -lt 0.001
    } | Sort-Object frame)
    if ($rows.Count -eq 0) {
        throw "Missing activation trace delta=$Delta phase=$Phase"
    }
    $opposition = First-Frame $rows 'weak_opposing_transition_gate'
    $seed = First-Frame $rows 'surface_risk_seed'
    $risk = First-Frame $rows 'transported_surface_risk'
    $memory = First-Frame $rows 'surface_memory_strength'
    $authority = First-Frame $rows 'current_frame_strength'
    $display = First-Frame $rows 'architecture_luma_delta' 0.00001
    [ordered]@{
        perceptual_reduction=(Perceptual-Reduction $Delta $Phase)
        first_weak_opposition_frame=$opposition
        first_surface_risk_seed_frame=$seed
        first_transported_surface_risk_frame=$risk
        first_surface_memory_frame=$memory
        first_current_frame_strength_frame=$authority
        first_architecture_luma_delta_frame=$display
        seed_minus_opposition_frames=if ($null -ne $seed -and $null -ne $opposition) {
            $seed - $opposition
        } else { $null }
        transported_risk_minus_seed_frames=if ($null -ne $risk -and $null -ne $seed) {
            $risk - $seed
        } else { $null }
        memory_minus_seed_frames=if ($null -ne $memory -and $null -ne $seed) {
            $memory - $seed
        } else { $null }
        display_minus_opposition_frames=if ($null -ne $display -and $null -ne $opposition) {
            $display - $opposition
        } else { $null }
        peak_weak_signed_magnitude_gate=(Peak $rows 'weak_signed_magnitude_gate')
        peak_opposition_strength=(Peak $rows 'opposition_strength')
        peak_weak_opposing_transition_gate=(Peak $rows 'weak_opposing_transition_gate')
        peak_qualified_intrinsic_event=(Peak $rows 'qualified_intrinsic_event')
        peak_surface_risk_seed=(Peak $rows 'surface_risk_seed')
        peak_transported_surface_risk=(Peak $rows 'transported_surface_risk')
        peak_surface_memory_strength=(Peak $rows 'surface_memory_strength')
        peak_current_frame_strength=(Peak $rows 'current_frame_strength')
        architecture_luma_delta_sum=(Sum-Metric $rows 'architecture_luma_delta')
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
            first_opposition_frame_delta=if (
                $null -ne $phase05.first_weak_opposition_frame -and
                $null -ne $phase0.first_weak_opposition_frame) {
                [int]$phase05.first_weak_opposition_frame -
                [int]$phase0.first_weak_opposition_frame
            } else { $null }
            first_seed_frame_delta=if (
                $null -ne $phase05.first_surface_risk_seed_frame -and
                $null -ne $phase0.first_surface_risk_seed_frame) {
                [int]$phase05.first_surface_risk_seed_frame -
                [int]$phase0.first_surface_risk_seed_frame
            } else { $null }
            first_memory_frame_delta=if (
                $null -ne $phase05.first_surface_memory_frame -and
                $null -ne $phase0.first_surface_memory_frame) {
                [int]$phase05.first_surface_memory_frame -
                [int]$phase0.first_surface_memory_frame
            } else { $null }
            architecture_luma_delta_sum_difference=
                [double]$phase05.architecture_luma_delta_sum -
                [double]$phase0.architecture_luma_delta_sum
        }
    }
}

$perceptualMin = [double](($perceptual.cases |
    Measure-Object reduction -Minimum).Minimum)
$candidate = [ordered]@{
    name=$spec.name
    architecture_mode=51
    replay_status=[string]$replay.status
    moving_clear_05_ms=[double]$trail.moving_square_clear_to_0_05_ms
    moving_flash_reduction=[double]$replay.moving_flash_reduction
    perceptual_min_reduction=$perceptualMin
    pan_mae=[double]$replay.pan_mae
    fast_pan_mae=[double]$replay.fast_pan_mae
    extreme_pan_mae=[double]$replay.extreme_pan_mae
}

$report = [ordered]@{
    schema='FLASHGUARD_MATRIX/44'
    purpose='trace the exact post-PRIME activation chain that causes the remaining 5 Hz phase-0 asymmetry without changing R+L behavior'
    screening_resolution='320x180'
    screening_fps=30
    candidate_count=1
    invariant='mode 51 is behavior-identical to Matrix-42 mode 46 R+L: R restoration ownership and 0.40 s qualified surface-risk lifetime ON; D/G/S and Matrix-43 A/P OFF; diagnostic MRT writes do not feed display or persistent state'
    trace_schema=[string]$trace.schema
    diagnostic_channels=[ordered]@{
        mrt0=@('weak_signed_magnitude_gate','transported_prime_encoded','opposition_strength','weak_opposing_transition_gate')
        mrt1=@('qualified_intrinsic_event','surface_risk_seed','transported_surface_risk','current_frame_strength')
        mrt2=@('preprocess_luma_delta','architecture_luma_delta','authority_current_event','surface_memory_strength')
    }
    interpretation_rule='compare phase0 and phase0.5 frame-by-frame: divergence before weak_opposing_transition_gate implicates opposition qualification; equal opposition but delayed/weak surface_risk_seed implicates seeding; equal seed with one-frame-later transported risk/memory implicates state activation latency; equal authority but different architecture_luma_delta implicates final display conversion'
    phase_pairs=$phasePairs
    selected=$candidate
    screen_candidates=@($candidate)
    elapsed_ms=$sw.ElapsedMilliseconds
}
$report | ConvertTo-Json -Depth 16 |
    Set-Content -Encoding utf8 (Join-Path $OutputDir 'matrix.json')
Write-Host "Matrix 44 activation trace complete in $($sw.ElapsedMilliseconds) ms"
exit 0
