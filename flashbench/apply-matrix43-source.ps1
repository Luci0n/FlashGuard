param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'apply-matrix42-source.ps1') -SourcePath $SourcePath

$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('matrix43FullWeakPrimeWriteFactor')) {
    Write-Host 'Matrix 43 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Matrix 43 source transform missing anchor: $($Old.Substring(0, [Math]::Min(100, $Old.Length)))"
    }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 43 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

# Matrix 42 showed that L improves 5 Hz continuity while S is not useful.
# Matrix 43 therefore fixes the clean R+L architecture and isolates the two
# stages that can explain the remaining phase-0 weakness:
#   A = establish a full signed PRIME once the existing weak-prime qualifier fires
#   P = preserve that PRIME with a 0.40 s stationary signed-prime lifetime
# Modes 48-50 are R+L+A, R+L+P, and R+L+A+P. D/G/S remain OFF.
Replace-Once 'P16.x > 15.5 && P16.x < 47.5;' 'P16.x > 15.5 && P16.x < 50.5;'
Replace-Once 'P16.x > 16.5 && P16.x < 47.5;' 'P16.x > 16.5 && P16.x < 50.5;'
Replace-Once 'P16.x > 19.5 && P16.x < 47.5;' 'P16.x > 19.5 && P16.x < 50.5;'

Replace-Once @'
        const bool matrix42ExtendedRiskLifetimeFactor =
            P16.x > 45.5 && P16.x < 47.5;
        const float matrix35RepetitionMotionGuardGate =
'@ @'
        const bool matrix42ExtendedRiskLifetimeFactor =
            P16.x > 45.5 && P16.x < 47.5;
        // Matrix 43 keeps Matrix 42's R+L result fixed, then splits PRIME
        // establishment amplitude from PRIME survival duration.
        const bool matrix43RestoreLongRiskBaseline =
            P16.x > 47.5 && P16.x < 50.5;
        const bool matrix43FullWeakPrimeWriteFactor =
            (P16.x > 47.5 && P16.x < 48.5) ||
            (P16.x > 49.5 && P16.x < 50.5);
        const bool matrix43LongPrimeFactor =
            P16.x > 48.5 && P16.x < 50.5;
        const float matrix35RepetitionMotionGuardGate =
'@

# All Matrix-43 modes retain only R's restoration ownership change.
Replace-Once @'
                    matrix37DirectWeakRiskSeedFactor ||
                    matrix41RestoreFactor || matrix42RestoreBaseline) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@ @'
                    matrix37DirectWeakRiskSeedFactor ||
                    matrix41RestoreFactor || matrix42RestoreBaseline ||
                    matrix43RestoreLongRiskBaseline) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@

# L is fixed ON for Matrix 43 because Matrix 42 showed a real 5 Hz benefit
# with negligible motion cost. This remains the qualified SURFACE-RISK lifetime,
# independent from the signed PRIME lifetime tested below.
Replace-Once @'
                    matrix37ExtendedRiskLifetimeFactor ||
                    matrix42ExtendedRiskLifetimeFactor) ?
                    0.40 : 0.22);
'@ @'
                    matrix37ExtendedRiskLifetimeFactor ||
                    matrix42ExtendedRiskLifetimeFactor ||
                    matrix43RestoreLongRiskBaseline) ?
                    0.40 : 0.22);
'@

# P changes only signed-PRIME survival. It uses the already-existing stationary
# sequence gate, so coherent displacement still collapses back to the base tau.
Replace-Once @'
            const bool extendedPrimeLifetime =
                qualifiedStationaryCameraGuardArchitecture ||
                matrix35PrimeLifetimeFactor;
'@ @'
            const bool extendedPrimeLifetime =
                qualifiedStationaryCameraGuardArchitecture ||
                matrix35PrimeLifetimeFactor || matrix43LongPrimeFactor;
'@

# A changes only how strongly an already-qualified weak transition writes the
# signed PRIME. The qualifying threshold and stationary gate are unchanged; a
# lone PRIME remains display-inert and still needs a later opposite sign before
# it can create safety authority.
Replace-Once @'
                const float primeWrite = saturate(max(
                    2.0 * currentIntrinsicEvent * signedMagnitudeGate,
                    weakSignedMagnitudeGate));
'@ @'
                const float matrix43WeakPrimeWrite =
                    matrix43FullWeakPrimeWriteFactor && weakSignedMagnitudeGate > 0.0 ?
                        1.0 : weakSignedMagnitudeGate;
                const float primeWrite = saturate(max(
                    2.0 * currentIntrinsicEvent * signedMagnitudeGate,
                    matrix43WeakPrimeWrite));
'@

Replace-Once 'std::clamp(tuning.architectureMode, 0, 47);' 'std::clamp(tuning.architectureMode, 0, 50);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 47);' 'std::clamp(requestedArchitecture, 0, 50);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 43 PRIME establishment/survival transform.'
