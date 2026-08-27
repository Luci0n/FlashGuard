param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 34 reuses Matrix 26's canonical full-replay reader and hard gates. It
# isolates the two remaining mode-21 display-authority camera guards.
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
    (New-Spec 'textureless_stationary_prime_state_tau100_gain200' 20 (Merge-Tune $base $tune)),
    (New-Spec 'stable_authority_camera_guard_tau100_gain200' 25 (Merge-Tune $base $tune)),
    (New-Spec 'hold_authorization_camera_guard_tau100_gain200' 26 (Merge-Tune $base $tune)),
    (New-Spec 'display_authority_camera_guard_tau100_gain200' 27 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace(
    'Full stationary-repetition matrix failed',
    'Full display-authority camera-guard matrix failed')
$source = $source.Replace(
    'FlashBench stationary-repetition matrix:',
    'FlashBench display-authority camera-guard matrix:')
$source = $source.Replace(
    "schema='FLASHGUARD_MATRIX/26'",
    "schema='FLASHGUARD_MATRIX/34'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='canonical full-replay causal isolation of the two remaining mode-21 display-authority camera guards after Matrices 32 and 33 ruled out sequence-state persistence and current-event disocclusion'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; modes 25-27 inherit mode 20 state, lifetimes and current-event semantics; mode 25 changes only stableMotionConflict to unmasked camera evidence, mode 26 changes only stationaryCurrentHoldAuthorization, and mode 27 changes both'")
$source = $source.Replace(
    'Stationary-repetition matrix complete',
    'Display-authority camera-guard matrix complete')

if ($source -eq $original -or $source -notmatch 'FLASHGUARD_MATRIX/34' -or
    $source -notmatch 'stable_authority_camera_guard_tau100_gain200' -or
    $source -notmatch 'hold_authorization_camera_guard_tau100_gain200' -or
    $source -notmatch 'display_authority_camera_guard_tau100_gain200') {
    throw 'Matrix 34 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) (
    'flashguard-matrix-v34-' + [Guid]::NewGuid().ToString('N') + '.ps1')
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
