param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 37 reuses Matrix 26's full-replay reader and hard gates. Matrix 36
# established that the state/disocclusion/restoration semantics cause the pan
# repair and that a longer risk lifetime does not. This matrix asks a narrower
# question: can an already opposition-qualified weak stationary transition seed
# surface-local risk without being multiplied by the same opposition gate twice?
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
    (New-Spec 'control_state_semantics_risklife220_tau100_gain200' 35 (Merge-Tune $base $tune)),
    (New-Spec 'control_state_semantics_risklife400_tau100_gain200' 29 (Merge-Tune $base $tune)),
    (New-Spec 'qualified_weak_seed_risklife220_tau100_gain200' 37 (Merge-Tune $base $tune)),
    (New-Spec 'qualified_weak_seed_risklife400_tau100_gain200' 38 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace(
    'Full stationary-repetition matrix failed',
    'Full qualified-weak-seed matrix failed')
$source = $source.Replace(
    'FlashBench stationary-repetition matrix:',
    'FlashBench qualified-weak-seed matrix:')
$source = $source.Replace(
    "schema='FLASHGUARD_MATRIX/26'",
    "schema='FLASHGUARD_MATRIX/37'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='canonical full-replay test of direct next-frame surface-risk seeding from an already opposition-qualified weak stationary reversal while preserving Matrix 36 surface ownership semantics'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; modes 35 and 29 are Matrix 36 controls; modes 37 and 38 inherit mode-35 surface ownership semantics and differ only by allowing weakOpposingTransitionGate to seed next-frame surface-local risk after stationarySequenceStateGate and surfaceContinuity validation; mode 38 additionally uses the already-tested 0.40 s qualified risk lifetime'")
$source = $source.Replace(
    'Stationary-repetition matrix complete',
    'Qualified-weak-seed matrix complete')

if ($source -eq $original -or
    $source -notmatch 'FLASHGUARD_MATRIX/37' -or
    $source -notmatch 'qualified_weak_seed_risklife220_tau100_gain200' -or
    $source -notmatch 'qualified_weak_seed_risklife400_tau100_gain200') {
    throw 'Matrix 37 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) (
    'flashguard-matrix-v37-' + [Guid]::NewGuid().ToString('N') + '.ps1')
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
