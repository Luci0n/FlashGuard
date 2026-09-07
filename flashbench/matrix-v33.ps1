param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 33 reuses Matrix 26's canonical full-replay reader and hard gates. It
# isolates mode 21's current-event camera veto from every sequence-state rewrite.
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
    (New-Spec 'current_event_camera_guard_tau100_gain200' 24 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace('Full stationary-repetition matrix failed','Full current-event-camera-guard matrix failed')
$source = $source.Replace('FlashBench stationary-repetition matrix:','FlashBench current-event-camera-guard matrix:')
$source = $source.Replace("schema='FLASHGUARD_MATRIX/26'","schema='FLASHGUARD_MATRIX/33'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='canonical full-replay causal isolation of mode 21 unmasked camera motion applied only to current-event disocclusion while retaining mode 20 sequence-state behavior'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; mode 24 is mode 20 except wholeFrameMotionEventVeto uses max(corroboratedMotionGate, unmaskedCpuCameraMotionGate); textureless fallback, state disocclusion, risk/prime lifetimes, stable intrinsic authority and hold authorization remain mode-20 behavior'")
$source = $source.Replace('Stationary-repetition matrix complete','Current-event-camera-guard matrix complete')

if ($source -eq $original -or $source -notmatch 'FLASHGUARD_MATRIX/33' -or
    $source -notmatch 'current_event_camera_guard_tau100_gain200') {
    throw 'Matrix 33 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) ('flashguard-matrix-v33-' + [Guid]::NewGuid().ToString('N') + '.ps1')
try {
    [IO.File]::WriteAllText($temp, $source, [Text.UTF8Encoding]::new($false))
    if ($ScreenOnly) { & $temp -Executable $Executable -OutputDir $OutputDir -ScreenOnly }
    else { & $temp -Executable $Executable -OutputDir $OutputDir }
    exit $LASTEXITCODE
}
finally { Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue }
