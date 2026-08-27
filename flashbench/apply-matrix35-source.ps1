param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'apply-matrix34-source.ps1') -SourcePath $SourcePath

$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('matrix35RepetitionGuardFactor')) {
    Write-Host 'Matrix 35 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Matrix 35 source transform missing anchor: $($Old.Substring(0, [Math]::Min(100, $Old.Length)))"
    }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 35 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

# Extend the mode-20 family through the Matrix 35 factorial modes.
Replace-Once @'
        const bool cameraAwareEventDisocclusionArchitecture =
            P16.x > 15.5 && P16.x < 27.5;
'@ @'
        const bool cameraAwareEventDisocclusionArchitecture =
            P16.x > 15.5 && P16.x < 34.5;
'@
Replace-Once @'
        const bool stationaryWeakRepetitionArchitecture =
            P16.x > 16.5 && P16.x < 27.5;
'@ @'
        const bool stationaryWeakRepetitionArchitecture =
            P16.x > 16.5 && P16.x < 34.5;
'@
Replace-Once @'
        const bool texturelessStationaryPrimeStateArchitecture =
            P16.x > 19.5 && P16.x < 27.5;
'@ @'
        const bool texturelessStationaryPrimeStateArchitecture =
            P16.x > 19.5 && P16.x < 34.5;
'@

# Matrix 35 is a 2^3 factorial around mode 20:
# A = unmasked camera motion participates only in repetition qualification.
# B = mode-21 risk/state restoration semantics plus the 0.40 s qualified risk lifetime.
# C = mode-21 0.40 s signed-prime lifetime.
Replace-Once @'
        const float holdAuthorizationMotionGuardGate =
            holdAuthorizationCameraGuardArchitecture ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                stationaryMotionGuardGate;
'@ @'
        const float holdAuthorizationMotionGuardGate =
            holdAuthorizationCameraGuardArchitecture ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                stationaryMotionGuardGate;
        const bool matrix35RepetitionGuardFactor =
            (P16.x > 27.5 && P16.x < 28.5) ||
            (P16.x > 30.5 && P16.x < 32.5) ||
            (P16.x > 33.5 && P16.x < 34.5);
        const bool matrix35RiskStateFactor =
            (P16.x > 28.5 && P16.x < 29.5) ||
            (P16.x > 30.5 && P16.x < 31.5) ||
            (P16.x > 32.5 && P16.x < 34.5);
        const bool matrix35PrimeLifetimeFactor =
            (P16.x > 29.5 && P16.x < 30.5) ||
            (P16.x > 31.5 && P16.x < 34.5);
        const float matrix35RepetitionMotionGuardGate =
            matrix35RepetitionGuardFactor ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                stationaryMotionGuardGate;
'@

Replace-Once @'
        const float stationaryRepetitionGate =
            stationaryWeakRepetitionArchitecture ?
                (1.0 - smoothstep(0.08, 0.30, stationaryMotionGuardGate)) : 0.0;
'@ @'
        const float stationaryRepetitionGate =
            stationaryWeakRepetitionArchitecture ?
                (1.0 - smoothstep(0.08, 0.30,
                    matrix35RepetitionMotionGuardGate)) : 0.0;
'@

Replace-Once @'
        const float texturelessStateDisocclusionGate =
            minimalTexturelessCameraVetoArchitecture ?
                lerp(texturelessStateDisocclusionGateBase,
                    explicitDisocclusionGate, unmaskedCpuCameraMotionGate) :
                texturelessStateDisocclusionGateBase;
'@ @'
        const float matrix35RiskStateDisocclusionGate =
            lerp(explicitDisocclusionGate,
                correctedCurrentPixelDisocclusionGate,
                stationaryRepetitionGate) *
                (1.0 - texturelessStationaryFallbackGate);
        const float texturelessStateDisocclusionGate =
            matrix35RiskStateFactor ?
                matrix35RiskStateDisocclusionGate :
            (minimalTexturelessCameraVetoArchitecture ?
                lerp(texturelessStateDisocclusionGateBase,
                    explicitDisocclusionGate, unmaskedCpuCameraMotionGate) :
                texturelessStateDisocclusionGateBase);
'@

Replace-Once @'
        const bool restoreQualifiedState =
            texturelessStationaryPrimeStateArchitecture ?
                ((qualifiedStationaryCameraGuardArchitecture ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
                 texturelessStateDisocclusionGate <= P13.z) :
'@ @'
        const bool restoreQualifiedState =
            texturelessStationaryPrimeStateArchitecture ?
                (((qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
                 texturelessStateDisocclusionGate <= P13.z) :
'@

Replace-Once @'
        const float stationaryRiskStateGate =
            qualifiedStationaryCameraGuardArchitecture ?
                stationarySequenceStateGate : stationaryRepetitionGate;
'@ @'
        const float stationaryRiskStateGate =
            (qualifiedStationaryCameraGuardArchitecture ||
                matrix35RiskStateFactor) ?
                stationarySequenceStateGate : stationaryRepetitionGate;
'@

Replace-Once @'
            const float stationaryRiskTau = max(baseRiskTau,
                qualifiedStationaryCameraGuardArchitecture ? 0.40 : 0.22);
'@ @'
            const float stationaryRiskTau = max(baseRiskTau,
                (qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor) ? 0.40 : 0.22);
'@

Replace-Once @'
            const float stationarySignedPrimeTau =
                qualifiedStationaryCameraGuardArchitecture ?
                    max(baseSignedPrimeTau, 0.40) : baseSignedPrimeTau;
            const float effectiveSignedPrimeTau =
                qualifiedStationaryCameraGuardArchitecture ?
                    lerp(baseSignedPrimeTau, stationarySignedPrimeTau,
                        stationarySequenceStateGate) : baseSignedPrimeTau;
'@ @'
            const bool extendedPrimeLifetime =
                qualifiedStationaryCameraGuardArchitecture ||
                matrix35PrimeLifetimeFactor;
            const float stationarySignedPrimeTau =
                extendedPrimeLifetime ?
                    max(baseSignedPrimeTau, 0.40) : baseSignedPrimeTau;
            const float effectiveSignedPrimeTau =
                extendedPrimeLifetime ?
                    lerp(baseSignedPrimeTau, stationarySignedPrimeTau,
                        stationarySequenceStateGate) : baseSignedPrimeTau;
'@

Replace-Once 'std::clamp(tuning.architectureMode, 0, 27);' 'std::clamp(tuning.architectureMode, 0, 34);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 27);' 'std::clamp(requestedArchitecture, 0, 34);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 35 factorial source transform.'
