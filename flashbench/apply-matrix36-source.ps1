param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'apply-matrix35-source.ps1') -SourcePath $SourcePath

$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('matrix36StateSemanticsFactor')) {
    Write-Host 'Matrix 36 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Matrix 36 source transform missing anchor: $($Old.Substring(0, [Math]::Min(100, $Old.Length)))"
    }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 36 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

# Extend the mode-20 benchmark family through the two B-decomposition modes.
Replace-Once 'P16.x > 15.5 && P16.x < 34.5;' 'P16.x > 15.5 && P16.x < 36.5;'
Replace-Once 'P16.x > 16.5 && P16.x < 34.5;' 'P16.x > 16.5 && P16.x < 36.5;'
Replace-Once 'P16.x > 19.5 && P16.x < 34.5;' 'P16.x > 19.5 && P16.x < 36.5;'

Replace-Once @'
        const bool matrix35PrimeLifetimeFactor =
            (P16.x > 29.5 && P16.x < 30.5) ||
            (P16.x > 31.5 && P16.x < 34.5);
        const float matrix35RepetitionMotionGuardGate =
'@ @'
        const bool matrix35PrimeLifetimeFactor =
            (P16.x > 29.5 && P16.x < 30.5) ||
            (P16.x > 31.5 && P16.x < 34.5);
        // Matrix 36 splits Matrix 35 factor B into its two conceptual pieces.
        // Mode 35: mode-21 sequence-conditioned risk/state semantics, base 0.22 s lifetime.
        // Mode 36: mode-20 state semantics, but the qualified risk lifetime is 0.40 s.
        const bool matrix36StateSemanticsFactor =
            P16.x > 34.5 && P16.x < 35.5;
        const bool matrix36RiskLifetimeFactor =
            P16.x > 35.5 && P16.x < 36.5;
        const float matrix35RepetitionMotionGuardGate =
'@

Replace-Once @'
        const float texturelessStateDisocclusionGate =
            matrix35RiskStateFactor ?
                matrix35RiskStateDisocclusionGate :
'@ @'
        const float texturelessStateDisocclusionGate =
            (matrix35RiskStateFactor || matrix36StateSemanticsFactor) ?
                matrix35RiskStateDisocclusionGate :
'@

Replace-Once @'
                (((qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@ @'
                (((qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor || matrix36StateSemanticsFactor) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@

Replace-Once @'
        const float stationaryRiskStateGate =
            (qualifiedStationaryCameraGuardArchitecture ||
                matrix35RiskStateFactor) ?
                stationarySequenceStateGate : stationaryRepetitionGate;
'@ @'
        const float stationaryRiskStateGate =
            (qualifiedStationaryCameraGuardArchitecture ||
                matrix35RiskStateFactor || matrix36StateSemanticsFactor) ?
                stationarySequenceStateGate : stationaryRepetitionGate;
'@

Replace-Once @'
            const float stationaryRiskTau = max(baseRiskTau,
                (qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor) ? 0.40 : 0.22);
'@ @'
            const float stationaryRiskTau = max(baseRiskTau,
                (qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor || matrix36RiskLifetimeFactor) ?
                    0.40 : 0.22);
'@

Replace-Once 'std::clamp(tuning.architectureMode, 0, 34);' 'std::clamp(tuning.architectureMode, 0, 36);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 34);' 'std::clamp(requestedArchitecture, 0, 36);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 36 B-decomposition source transform.'
