param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'apply-matrix36-source.ps1') -SourcePath $SourcePath

$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('matrix37QualifiedWeakRiskSeed')) {
    Write-Host 'Matrix 37 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Matrix 37 source transform missing anchor: $($Old.Substring(0, [Math]::Min(100, $Old.Length)))"
    }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 37 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

# Modes 37/38 inherit Matrix 36's state-semantics ownership fix.
# Their only new factor is a surface-validated direct seed from an already
# opposition-qualified weak reversal. Mode 38 also keeps the 0.40 s risk lifetime.
Replace-Once 'P16.x > 15.5 && P16.x < 36.5;' 'P16.x > 15.5 && P16.x < 38.5;'
Replace-Once 'P16.x > 16.5 && P16.x < 36.5;' 'P16.x > 16.5 && P16.x < 38.5;'
Replace-Once 'P16.x > 19.5 && P16.x < 36.5;' 'P16.x > 19.5 && P16.x < 38.5;'

Replace-Once @'
        const bool matrix36RiskLifetimeFactor =
            P16.x > 35.5 && P16.x < 36.5;
        const float matrix35RepetitionMotionGuardGate =
'@ @'
        const bool matrix36RiskLifetimeFactor =
            P16.x > 35.5 && P16.x < 36.5;
        const bool matrix37DirectWeakRiskSeedFactor =
            P16.x > 36.5 && P16.x < 38.5;
        const bool matrix37ExtendedRiskLifetimeFactor =
            P16.x > 37.5 && P16.x < 38.5;
        const float matrix35RepetitionMotionGuardGate =
'@

Replace-Once @'
        const float texturelessStateDisocclusionGate =
            (matrix35RiskStateFactor || matrix36StateSemanticsFactor) ?
                matrix35RiskStateDisocclusionGate :
'@ @'
        const float texturelessStateDisocclusionGate =
            (matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                matrix37DirectWeakRiskSeedFactor) ?
                matrix35RiskStateDisocclusionGate :
'@

Replace-Once @'
                (((qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor || matrix36StateSemanticsFactor) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@ @'
                (((qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                    matrix37DirectWeakRiskSeedFactor) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@

Replace-Once @'
        const float stationaryRiskStateGate =
            (qualifiedStationaryCameraGuardArchitecture ||
                matrix35RiskStateFactor || matrix36StateSemanticsFactor) ?
                stationarySequenceStateGate : stationaryRepetitionGate;
'@ @'
        const float stationaryRiskStateGate =
            (qualifiedStationaryCameraGuardArchitecture ||
                matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                matrix37DirectWeakRiskSeedFactor) ?
                stationarySequenceStateGate : stationaryRepetitionGate;
'@

Replace-Once @'
            const float stationaryRiskTau = max(baseRiskTau,
                (qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor || matrix36RiskLifetimeFactor) ?
                    0.40 : 0.22);
'@ @'
            const float stationaryRiskTau = max(baseRiskTau,
                (qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor || matrix36RiskLifetimeFactor ||
                    matrix37ExtendedRiskLifetimeFactor) ?
                    0.40 : 0.22);
'@

# weakOpposingTransitionGate is already the conjunction of an opposing signed
# prime and a tiny stationary reversal. The old generic seed multiplies that
# authority by effectiveOpposingTransitionGate again, effectively squaring weak
# evidence. Preserve the conservative generic path, but for Matrix 37 allow that
# already-qualified weak reversal to seed NEXT-frame surface-local risk directly.
# It still requires both sequence-state permission and validated surface
# continuity, and it never directly authorizes display output or old RGB.
Replace-Once @'
            const float persistentSeedAuthority = repetitionGatedArchitecture ?
                repeatedMemoryGate : (oppositionGatedArchitecture ?
                    effectiveOpposingTransitionGate : 1.0);
            surfaceRiskStateSeed = eventOnlyArchitecture ? 0.0 :
                qualifiedIntrinsicEvent * persistentSeedAuthority;
'@ @'
            const float persistentSeedAuthority = repetitionGatedArchitecture ?
                repeatedMemoryGate : (oppositionGatedArchitecture ?
                    effectiveOpposingTransitionGate : 1.0);
            const float matrix37QualifiedWeakRiskSeed =
                matrix37DirectWeakRiskSeedFactor ?
                    weakOpposingTransitionGate *
                    stationarySequenceStateGate * surfaceContinuity : 0.0;
            surfaceRiskStateSeed = eventOnlyArchitecture ? 0.0 :
                max(qualifiedIntrinsicEvent * persistentSeedAuthority,
                    matrix37QualifiedWeakRiskSeed);
'@

Replace-Once 'std::clamp(tuning.architectureMode, 0, 36);' 'std::clamp(tuning.architectureMode, 0, 38);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 36);' 'std::clamp(requestedArchitecture, 0, 38);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 37 qualified weak-risk seed transform.'
