param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 28 reuses Matrix 26's canonical reader and hard gates. It changes only
# the candidate list and report metadata so prior matrix evidence stays immutable.
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
    (New-Spec 'stationary_qualified_state_tau100_gain200' 18 (Merge-Tune $base $tune)),
    (New-Spec 'stationary_motion_only_state_tau100_gain200' 19 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace(
    'Full stationary-repetition matrix failed',
    'Full stationary-motion-state matrix failed')
$source = $source.Replace(
    'FlashBench stationary-repetition matrix:',
    'FlashBench stationary-motion-state matrix:')
$source = $source.Replace(
    "schema='FLASHGUARD_MATRIX/26'",
    "schema='FLASHGUARD_MATRIX/28'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='canonical full-replay accept/reject of stationary sequence-state continuity without requiring optical-flow current-surface confidence after Matrix 27 isolated uniform low-frequency failures'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; mode 19 inherits mode 18 but for risk/prime only treats absence of scene-level and local motion as stationary correspondence even when NVOFA has no current-surface texture; displayed history and moving/camera paths remain conservative raw-disocclusion'")
$source = $source.Replace(
    'Stationary-repetition matrix complete',
    'Stationary-motion-state matrix complete')

if ($source -eq $original -or $source -notmatch 'FLASHGUARD_MATRIX/28' -or
    $source -notmatch 'stationary_motion_only_state_tau100_gain200') {
    throw 'Matrix 28 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) (
    'flashguard-matrix-v28-' + [Guid]::NewGuid().ToString('N') + '.ps1')
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
