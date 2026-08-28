param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'apply-matrix37-source.ps1') -SourcePath $SourcePath

$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('matrix41DisocclusionFactor')) {
    Write-Host 'Matrix 41 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Matrix 41 source transform missing anchor: $($Old.Substring(0, [Math]::Min(100, $Old.Length)))"
    }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 41 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

# Matrix 41 decomposes Matrix 36 mode 35's state-semantics factor at the
# established base 0.22 s qualified-risk lifetime. Existing controls are:
#   mode 20 = 000 (none of the state-semantics changes)
#   mode 35 = 111 (all three state-semantics changes)
# New modes 39-44 cover the six intermediate combinations:
#   D = state-disocclusion ownership uses the mode-21 formula
#   R = qualified-state restoration uses stationarySequenceStateGate
#   G = qualified-risk lifetime extension is gated by stationarySequenceStateGate
# No Matrix-35 repetition guard, 0.40 s risk lifetime, 0.40 s prime lifetime,
# or Matrix-37 direct weak-risk seed is added to these modes.
Replace-Once 'P16.x > 15.5 && P16.x < 38.5;' 'P16.x > 15.5 && P16.x < 44.5;'
Replace-Once 'P16.x > 16.5 && P16.x < 38.5;' 'P16.x > 16.5 && P16.x < 44.5;'
Replace-Once 'P16.x > 19.5 && P16.x < 38.5;' 'P16.x > 19.5 && P16.x < 44.5;'

Replace-Once @'
        const bool matrix37ExtendedRiskLifetimeFactor =
            P16.x > 37.5 && P16.x < 38.5;
        const float matrix35RepetitionMotionGuardGate =
'@ @'
        const bool matrix37ExtendedRiskLifetimeFactor =
            P16.x > 37.5 && P16.x < 38.5;
        // Matrix 41: 2^3 decomposition of mode 35 state semantics.
        // 39=D, 40=R, 41=G, 42=D+R, 43=D+G, 44=R+G.
        const bool matrix41DisocclusionFactor =
            (P16.x > 38.5 && P16.x < 39.5) ||
            (P16.x > 41.5 && P16.x < 43.5);
        const bool matrix41RestoreFactor =
            (P16.x > 39.5 && P16.x < 40.5) ||
            (P16.x > 41.5 && P16.x < 42.5) ||
            (P16.x > 43.5 && P16.x < 44.5);
        const bool matrix41RiskGateFactor =
            (P16.x > 40.5 && P16.x < 41.5) ||
            (P16.x > 42.5 && P16.x < 44.5);
        const float matrix35RepetitionMotionGuardGate =
'@

Replace-Once @'
        const float texturelessStateDisocclusionGate =
            (matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                matrix37DirectWeakRiskSeedFactor) ?
                matrix35RiskStateDisocclusionGate :
'@ @'
        const float texturelessStateDisocclusionGate =
            (matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                matrix37DirectWeakRiskSeedFactor ||
                matrix41DisocclusionFactor) ?
                matrix35RiskStateDisocclusionGate :
'@

Replace-Once @'
                (((qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                    matrix37DirectWeakRiskSeedFactor) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@ @'
                (((qualifiedStationaryCameraGuardArchitecture ||
                    matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                    matrix37DirectWeakRiskSeedFactor ||
                    matrix41RestoreFactor) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@

Replace-Once @'
        const float stationaryRiskStateGate =
            (qualifiedStationaryCameraGuardArchitecture ||
                matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                matrix37DirectWeakRiskSeedFactor) ?
                stationarySequenceStateGate : stationaryRepetitionGate;
'@ @'
        const float stationaryRiskStateGate =
            (qualifiedStationaryCameraGuardArchitecture ||
                matrix35RiskStateFactor || matrix36StateSemanticsFactor ||
                matrix37DirectWeakRiskSeedFactor ||
                matrix41RiskGateFactor) ?
                stationarySequenceStateGate : stationaryRepetitionGate;
'@

Replace-Once 'std::clamp(tuning.architectureMode, 0, 38);' 'std::clamp(tuning.architectureMode, 0, 44);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 38);' 'std::clamp(requestedArchitecture, 0, 44);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 41 state-semantics decomposition transform.'
