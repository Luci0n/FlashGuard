param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 32 reuses Matrix 26's canonical full-replay reader and hard gates. It
# isolates the minimum camera veto from the larger mode-21 state rewrite.
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
    (New-Spec 'minimal_camera_veto_stationary_state_tau100_gain200' 23 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace(
    'Full stationary-repetition matrix failed',
    'Full minimal-camera-veto matrix failed')
$source = $source.Replace(
    'FlashBench stationary-repetition matrix:',
    'FlashBench minimal-camera-veto matrix:')
$source = $source.Replace(
    "schema='FLASHGUARD_MATRIX/26'",
    "schema='FLASHGUARD_MATRIX/32'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='canonical full-replay accept/reject of a minimal unmasked-camera veto applied only to mode 20 textureless fallback and state disocclusion after Matrix 31 disproved camera-score substitution'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; mode 23 is mode 20 everywhere except an unmasked CPU camera gate suppresses the textureless stationary fallback and restores raw state disocclusion during camera translation; risk lifetime, signed-prime lifetime, stable intrinsic authority, hold authorization, and current-event camera semantics remain mode-20 behavior'")
$source = $source.Replace(
    'Stationary-repetition matrix complete',
    'Minimal-camera-veto matrix complete')

if ($source -eq $original -or $source -notmatch 'FLASHGUARD_MATRIX/32' -or
    $source -notmatch 'minimal_camera_veto_stationary_state_tau100_gain200') {
    throw 'Matrix 32 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) (
    'flashguard-matrix-v32-' + [Guid]::NewGuid().ToString('N') + '.ps1')
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
