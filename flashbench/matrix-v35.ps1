param(
    [string]$Executable = '.\FlashGuard.exe',
    [string]$OutputDir = 'flashbench/matrix-results',
    [switch]$ScreenOnly
)

$ErrorActionPreference = 'Stop'

# Matrix 35 reuses Matrix 26's canonical full-replay reader and hard gates.
# It runs a 2^3 factorial over the three still-unisolated mode-21 changes,
# with mode 20 as baseline and mode 21 as a positive control.
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
    (New-Spec 'camera_guarded_stationary_state_tau100_gain200' 21 (Merge-Tune $base $tune)),
    (New-Spec 'factor_A_repetition_guard_tau100_gain200' 28 (Merge-Tune $base $tune)),
    (New-Spec 'factor_B_risk_state_tau100_gain200' 29 (Merge-Tune $base $tune)),
    (New-Spec 'factor_C_prime_lifetime_tau100_gain200' 30 (Merge-Tune $base $tune)),
    (New-Spec 'factor_AB_repetition_risk_state_tau100_gain200' 31 (Merge-Tune $base $tune)),
    (New-Spec 'factor_AC_repetition_prime_tau100_gain200' 32 (Merge-Tune $base $tune)),
    (New-Spec 'factor_BC_risk_state_prime_tau100_gain200' 33 (Merge-Tune $base $tune)),
    (New-Spec 'factor_ABC_residual_mode21_delta_tau100_gain200' 34 (Merge-Tune $base $tune))
)
'@
$source = $source.Replace($oldSpecs, $newSpecs)
$source = $source.Replace(
    'Full stationary-repetition matrix failed',
    'Full residual mode-21 factorial matrix failed')
$source = $source.Replace(
    'FlashBench stationary-repetition matrix:',
    'FlashBench residual mode-21 factorial matrix:')
$source = $source.Replace(
    "schema='FLASHGUARD_MATRIX/26'",
    "schema='FLASHGUARD_MATRIX/35'")
$source = $source.Replace(
    "purpose='canonical full-replay accept/reject of sequence-qualified stationary weak-reversal authority and stationary-only opposition-risk hold after Matrix 25 exposed low-frequency and 4-code gaps'",
    "purpose='canonical full-replay 2^3 causal factorial of the remaining mode-20 to mode-21 delta after Matrices 32-34 ruled out textureless fallback veto, current-event disocclusion, stable repeated authority and stationary current-hold authorization'")
$source = $source.Replace(
    "invariant='production architecture 0 is unchanged; mode 17 inherits mode 16 camera-aware event disocclusion, extends opposition-qualified risk only when scene-level motion is absent, and admits tiny changes only after an opposing signed reversal'",
    "invariant='production architecture 0 is unchanged; modes 28-34 inherit mode 20 and toggle only A=unmasked camera motion in repetition qualification, B=mode-21 risk/state restoration semantics plus 0.40s qualified risk lifetime, and C=mode-21 0.40s signed-prime lifetime; mode 21 is the positive control'")
$source = $source.Replace(
    'Stationary-repetition matrix complete',
    'Residual mode-21 factorial matrix complete')

if ($source -eq $original -or
    $source -notmatch 'FLASHGUARD_MATRIX/35' -or
    $source -notmatch 'factor_A_repetition_guard_tau100_gain200' -or
    $source -notmatch 'factor_ABC_residual_mode21_delta_tau100_gain200') {
    throw 'Matrix 35 transform did not match Matrix 26 source'
}

$temp = Join-Path ([IO.Path]::GetTempPath()) (
    'flashguard-matrix-v35-' + [Guid]::NewGuid().ToString('N') + '.ps1')
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
