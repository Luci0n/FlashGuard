param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 38 verifies replay determinism after protection-state reset repair.
# Keep the established Matrix 26 full replay corpus unchanged so the only
# experimental variable is candidate order/repetition. Modes 35 and 29 are
# repeated three times each in mixed order; the report measures per-case spread.
$sourcePath = Join-Path $PSScriptRoot 'matrix-v26.ps1'
$source = Get-Content -Raw $sourcePath
$original = $source

$oldSpecs = @'
$specs = @(
    (New-Spec 'camera_aware_event_disocclusion_tau100_gain200' 16 (Merge-Tune $base $tune)),
    (New-Spec 'stationary_reversal_hold_tau100_gain200' 17 (Merge-Tune $base $tune))
)
'@
$newSpecs = @'
$specs = @(
    (New-Spec 'repeat_a1_state220_tau100_gain200' 35 (Merge-Tune $base $tune)),
    (New-Spec 'repeat_b1_state400_tau100_gain200' 29 (Merge-Tune $base $tune)),
    (New-Spec 'repeat_a2_state220_tau100_gain200' 35 (Merge-Tune $base $tune)),
    (New-Spec 'repeat_b2_state400_tau100_gain200' 29 (Merge-Tune $base $tune)),
    (New-Spec 'repeat_b3_state400_tau100_gain200' 29 (Merge-Tune $base $tune)),
    (New-Spec 'repeat_a3_state220_tau100_gain200' 35 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace(
    'Full stationary-repetition matrix failed',
    'Full determinism-verification matrix failed')
$source = $source.Replace(
    'FlashBench stationary-repetition matrix:',
    'FlashBench replay-determinism matrix:')
$source = $source.Replace(
    "schema='FLASHGUARD_MATRIX/26'",
    "schema='FLASHGUARD_MATRIX/38'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='same-binary replay determinism verification after clearing persistent protection-state memory at every replay reset; repeat identical mode-35 and mode-29 candidates in mixed order and measure per-case spread'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; all A candidates are identical mode 35 and all B candidates are identical mode 29; the established full replay corpus and tuning are unchanged; only candidate identity/order differs'")
$source = $source.Replace(
    'Stationary-repetition matrix complete',
    'Replay-determinism matrix complete')

$oldCandidateBlock = @'
$candidates = @($specs | ForEach-Object { Read-Candidate $_ $screen.directory })
$passing = @($candidates | Where-Object { $_.primary_pass })
'@
$newCandidateBlock = @'
$candidates = @($specs | ForEach-Object { Read-Candidate $_ $screen.directory })

function Perceptual-Key([object]$Case) {
    '{0}|{1}|{2}' -f [int]$Case.delta_code,
        ([double]$Case.frequency_hz).ToString('0.###', $inv),
        ([double]$Case.phase_frames).ToString('0.###', $inv)
}
function Read-PerceptualMap([string]$Name, [string]$BatchDir) {
    $path = Join-Path (Join-Path $BatchDir $Name) 'perceptual-sweep.json'
    $per = Get-Content -Raw $path | ConvertFrom-Json
    $map = @{}
    foreach ($case in @($per.cases)) {
        $map[(Perceptual-Key $case)] = [double]$case.reduction
    }
    $map
}
function Spread([object[]]$Values) {
    $numbers = @($Values | ForEach-Object { [double]$_ })
    if ($numbers.Count -lt 1) { return 0.0 }
    $minimum = [double](($numbers | Measure-Object -Minimum).Minimum)
    $maximum = [double](($numbers | Measure-Object -Maximum).Maximum)
    [double]($maximum - $minimum)
}

$repeatGroups = [ordered]@{
    state220 = @(
        'repeat_a1_state220_tau100_gain200',
        'repeat_a2_state220_tau100_gain200',
        'repeat_a3_state220_tau100_gain200'
    )
    state400 = @(
        'repeat_b1_state400_tau100_gain200',
        'repeat_b2_state400_tau100_gain200',
        'repeat_b3_state400_tau100_gain200'
    )
}
$targetCases = [ordered]@{
    delta4_5hz_phase0='4|5|0'
    delta4_5hz_phase0_5='4|5|0.5'
    delta12_5hz_phase0_5='12|5|0.5'
    delta32_5hz_phase0_5='32|5|0.5'
    delta4_10hz_phase0_5='4|10|0.5'
    delta4_15hz_phase0_5='4|15|0.5'
}

$groupReports = @()
$globalPerceptualSpread = 0.0
$globalTargetSpread = 0.0
$globalPanSpread = 0.0
$globalMovingFlashSpread = 0.0
foreach ($entry in $repeatGroups.GetEnumerator()) {
    $names = @($entry.Value)
    $maps = @($names | ForEach-Object { Read-PerceptualMap $_ $screen.directory })
    $keys = @($maps[0].Keys | Sort-Object)
    $allCaseMaxSpread = 0.0
    foreach ($key in $keys) {
        $values = @($maps | ForEach-Object {
            if (-not $_.ContainsKey($key)) { throw "Missing perceptual case $key in repeat group $($entry.Key)" }
            [double]$_[$key]
        })
        $allCaseMaxSpread = [Math]::Max($allCaseMaxSpread, (Spread $values))
    }

    $targetReport = [ordered]@{}
    $targetMaxSpread = 0.0
    foreach ($target in $targetCases.GetEnumerator()) {
        $values = @($maps | ForEach-Object {
            if (-not $_.ContainsKey($target.Value)) {
                throw "Missing target perceptual case $($target.Value) in repeat group $($entry.Key)"
            }
            [double]$_[$target.Value]
        })
        $spread = Spread $values
        $targetMaxSpread = [Math]::Max($targetMaxSpread, $spread)
        $targetReport[$target.Key] = [ordered]@{
            values=$values
            spread=$spread
        }
    }

    $groupCandidates = @($candidates | Where-Object { $names -contains $_.name })
    if ($groupCandidates.Count -ne 3) {
        throw "Expected three candidates in repeat group $($entry.Key), got $($groupCandidates.Count)"
    }
    $panValues = @($groupCandidates | ForEach-Object { [double]$_.pan_mae })
    $fastPanValues = @($groupCandidates | ForEach-Object { [double]$_.fast_pan_mae })
    $extremePanValues = @($groupCandidates | ForEach-Object { [double]$_.extreme_pan_mae })
    $movingFlashValues = @($groupCandidates | ForEach-Object { [double]$_.moving_flash_reduction })
    $panSpread = Spread $panValues
    $fastPanSpread = Spread $fastPanValues
    $extremePanSpread = Spread $extremePanValues
    $movingFlashSpread = Spread $movingFlashValues

    $globalPerceptualSpread = [Math]::Max($globalPerceptualSpread, $allCaseMaxSpread)
    $globalTargetSpread = [Math]::Max($globalTargetSpread, $targetMaxSpread)
    $globalPanSpread = [Math]::Max($globalPanSpread,
        [Math]::Max($panSpread, [Math]::Max($fastPanSpread, $extremePanSpread)))
    $globalMovingFlashSpread = [Math]::Max($globalMovingFlashSpread, $movingFlashSpread)

    $groupReports += [pscustomobject][ordered]@{
        group=[string]$entry.Key
        candidate_names=$names
        all_perceptual_max_spread=$allCaseMaxSpread
        target_perceptual_max_spread=$targetMaxSpread
        target_cases=$targetReport
        pan_mae_values=$panValues
        pan_mae_spread=$panSpread
        fast_pan_mae_values=$fastPanValues
        fast_pan_mae_spread=$fastPanSpread
        extreme_pan_mae_values=$extremePanValues
        extreme_pan_mae_spread=$extremePanSpread
        moving_flash_reduction_values=$movingFlashValues
        moving_flash_reduction_spread=$movingFlashSpread
    }
}

$determinism = [ordered]@{
    perceptual_spread_max=0.005
    pan_spread_max=0.0002
    moving_flash_spread_max=0.005
    observed_all_perceptual_max_spread=$globalPerceptualSpread
    observed_target_perceptual_max_spread=$globalTargetSpread
    observed_pan_family_max_spread=$globalPanSpread
    observed_moving_flash_max_spread=$globalMovingFlashSpread
    pass=($globalPerceptualSpread -le 0.005 -and
        $globalPanSpread -le 0.0002 -and
        $globalMovingFlashSpread -le 0.005)
    groups=$groupReports
}

$passing = @($candidates | Where-Object { $_.primary_pass })
'@
$source = $source.Replace($oldCandidateBlock, $newCandidateBlock)

$oldReportTail = @'
    selected=$selected
    screen_candidates=$candidates
'@
$newReportTail = @'
    determinism=$determinism
    selected=$selected
    screen_candidates=$candidates
'@
$source = $source.Replace($oldReportTail, $newReportTail)

if ($source -eq $original -or
    $source -notmatch 'FLASHGUARD_MATRIX/38' -or
    $source -notmatch 'repeat_a3_state220_tau100_gain200' -or
    $source -notmatch 'repeat_b3_state400_tau100_gain200' -or
    $source -notmatch 'observed_all_perceptual_max_spread' -or
    $source -notmatch 'determinism=\$determinism') {
    throw 'Matrix 38 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) (
    'flashguard-matrix-v38-' + [Guid]::NewGuid().ToString('N') + '.ps1')
try {
    [IO.File]::WriteAllText($temp, $source, [Text.UTF8Encoding]::new($false))
    if ($ScreenOnly) {
        & $temp -Executable $Executable -OutputDir $OutputDir -ScreenOnly
    } else {
        & $temp -Executable $Executable -OutputDir $OutputDir
    }
    exit $LASTEXITCODE
}
finally {
    Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
}
