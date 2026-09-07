param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 36 reuses Matrix 26's full-replay reader and hard gates. It repeats
# identical mode-20 and full-B controls in the same binary, then splits Matrix 35
# factor B into state semantics only versus the 0.40 s risk lifetime only.
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
    (New-Spec 'repeat_mode20_a_tau100_gain200' 20 (Merge-Tune $base $tune)),
    (New-Spec 'repeat_mode20_b_tau100_gain200' 20 (Merge-Tune $base $tune)),
    (New-Spec 'repeat_full_B_a_tau100_gain200' 29 (Merge-Tune $base $tune)),
    (New-Spec 'repeat_full_B_b_tau100_gain200' 29 (Merge-Tune $base $tune)),
    (New-Spec 'state_semantics_only_tau100_gain200' 35 (Merge-Tune $base $tune)),
    (New-Spec 'risk_lifetime_only_tau100_gain200' 36 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace(
    'Full stationary-repetition matrix failed',
    'Full B-decomposition/repeatability matrix failed')
$source = $source.Replace(
    'FlashBench stationary-repetition matrix:',
    'FlashBench B-decomposition/repeatability matrix:')
$source = $source.Replace(
    "schema='FLASHGUARD_MATRIX/26'",
    "schema='FLASHGUARD_MATRIX/36'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='canonical full-replay same-binary repeatability check plus decomposition of Matrix 35 factor B into sequence-conditioned risk/state semantics versus qualified risk lifetime'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; duplicate mode-20 and mode-29 controls quantify same-binary repeatability; mode 35 changes only factor-B state/disocclusion/restoration gating while retaining the 0.22s risk lifetime, and mode 36 changes only the qualified risk lifetime to 0.40s while retaining mode-20 state semantics'")
$source = $source.Replace(
    'Stationary-repetition matrix complete',
    'B-decomposition/repeatability matrix complete')

if ($source -eq $original -or
    $source -notmatch 'FLASHGUARD_MATRIX/36' -or
    $source -notmatch 'state_semantics_only_tau100_gain200' -or
    $source -notmatch 'risk_lifetime_only_tau100_gain200') {
    throw 'Matrix 36 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) (
    'flashguard-matrix-v36-' + [Guid]::NewGuid().ToString('N') + '.ps1')
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
