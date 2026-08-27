param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 30 reuses Matrix 26's canonical full-replay reader and hard gates. It
# compares mode 20 against the camera-guarded stationary sequence correction.
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
    (New-Spec 'camera_guarded_stationary_state_tau100_gain200' 21 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace(
    'Full stationary-repetition matrix failed',
    'Full camera-guarded-stationary matrix failed')
$source = $source.Replace(
    'FlashBench stationary-repetition matrix:',
    'FlashBench camera-guarded-stationary matrix:')
$source = $source.Replace(
    "schema='FLASHGUARD_MATRIX/26'",
    "schema='FLASHGUARD_MATRIX/30'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='canonical full-replay accept/reject of unmasked camera-motion veto plus stationary-only qualified prime/risk lifetime after Matrix 29 improved weak flashes but regressed extreme pan'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; mode 21 inherits mode 20 textureless stationary prime/risk continuity, but an unmasked whole-frame translation score vetoes stationary state even during active protection; raw disocclusion is restored when that veto fires, and only textureless-stationary sequence state receives the longer 0.40 s prime/risk lifetime'")
$source = $source.Replace(
    'Stationary-repetition matrix complete',
    'Camera-guarded-stationary matrix complete')

if ($source -eq $original -or $source -notmatch 'FLASHGUARD_MATRIX/30' -or
    $source -notmatch 'camera_guarded_stationary_state_tau100_gain200') {
    throw 'Matrix 30 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) (
    'flashguard-matrix-v30-' + [Guid]::NewGuid().ToString('N') + '.ps1')
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
