param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'apply-matrix41-source.ps1') -SourcePath $SourcePath

$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('matrix42WeakSeedFactor')) {
    Write-Host 'Matrix 42 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Matrix 42 source transform missing anchor: $($Old.Substring(0, [Math]::Min(100, $Old.Length)))"
    }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 42 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

# Matrix 41 isolated qualified-state restoration (R) as the pan-repair owner.
# Matrix 42 fixes R ON and retests two unresolved weak-flash improvements:
#   S = direct surface-validated seed from an already opposition-qualified weak reversal
#   L = extend qualified surface-risk lifetime from 0.22 s to 0.40 s
# Modes 45-47 are R+S, R+L, and R+S+L. They deliberately do NOT enable
# Matrix-41 D (state-disocclusion ownership) or G (risk-state sequence gating).
Replace-Once 'P16.x > 15.5 && P16.x < 44.5;' 'P16.x > 15.5 && P16.x < 47.5;'
Replace-Once 'P16.x > 16.5 && P16.x < 44.5;' 'P16.x > 16.5 && P16.x < 47.5;'
Replace-Once 'P16.x > 19.5 && P16.x < 44.5;' 'P16.x > 19.5 && P16.x < 47.5;'

Replace-Once @'
        const bool matrix41RiskGateFactor =
            (P16.x > 40.5 && P16.x < 41.5) ||
            (P16.x > 42.5 && P16.x < 44.5);
        const float matrix35RepetitionMotionGuardGate =
'@ @'
        const bool matrix41RiskGateFactor =
            (P16.x > 40.5 && P16.x < 41.5) ||
            (P16.x > 42.5 && P16.x < 44.5);
        // Matrix 42 keeps only Matrix 41's restoration ownership fix, then
        // independently varies weak-risk seeding and qualified-risk lifetime.
        const bool matrix42RestoreBaseline =
            P16.x > 44.5 && P16.x < 47.5;
        const bool matrix42WeakSeedFactor =
            (P16.x > 44.5 && P16.x < 45.5) ||
            (P16.x > 46.5 && P16.x < 47.5);
        const bool matrix42ExtendedRiskLifetimeFactor =
            P16.x > 45.5 && P16.x < 47.5;
        const float matrix35RepetitionMotionGuardGate =
'@

# R-only ownership: new Matrix-42 modes use stationarySequenceStateGate only
# when deciding whether an already-qualified state may be restored. Do not add
# them to texturelessStateDisocclusionGate or stationaryRiskStateGate.
Replace-Once @'
                    matrix37DirectWeakRiskSeedFactor ||
                    matrix41RestoreFactor) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@ @'
                    matrix37DirectWeakRiskSeedFactor ||
                    matrix41RestoreFactor || matrix42RestoreBaseline) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@

Replace-Once @'
                    matrix35RiskStateFactor || matrix36RiskLifetimeFactor ||
                    matrix37ExtendedRiskLifetimeFactor) ?
                    0.40 : 0.22);
'@ @'
                    matrix35RiskStateFactor || matrix36RiskLifetimeFactor ||
                    matrix37ExtendedRiskLifetimeFactor ||
                    matrix42ExtendedRiskLifetimeFactor) ?
                    0.40 : 0.22);
'@

# Matrix 37's seed factor also inherited the broader state-semantics bundle.
# Keep that historical control intact. Matrix 42 adds an independent seed that
# is valid only after opposition qualification, stationary sequence permission,
# and current-surface continuity. It seeds NEXT-frame risk only and never writes
# old RGB or directly authorizes the current display.
Replace-Once @'
            const float matrix37QualifiedWeakRiskSeed =
                matrix37DirectWeakRiskSeedFactor ?
                    weakOpposingTransitionGate *
                    stationarySequenceStateGate * surfaceContinuity : 0.0;
            surfaceRiskStateSeed = eventOnlyArchitecture ? 0.0 :
                max(qualifiedIntrinsicEvent * persistentSeedAuthority,
                    matrix37QualifiedWeakRiskSeed);
'@ @'
            const float matrix37QualifiedWeakRiskSeed =
                matrix37DirectWeakRiskSeedFactor ?
                    weakOpposingTransitionGate *
                    stationarySequenceStateGate * surfaceContinuity : 0.0;
            const float matrix42QualifiedWeakRiskSeed =
                matrix42WeakSeedFactor ?
                    weakOpposingTransitionGate *
                    stationarySequenceStateGate * surfaceContinuity : 0.0;
            surfaceRiskStateSeed = eventOnlyArchitecture ? 0.0 :
                max(qualifiedIntrinsicEvent * persistentSeedAuthority,
                    max(matrix37QualifiedWeakRiskSeed,
                        matrix42QualifiedWeakRiskSeed));
'@

Replace-Once 'std::clamp(tuning.architectureMode, 0, 44);' 'std::clamp(tuning.architectureMode, 0, 47);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 44);' 'std::clamp(requestedArchitecture, 0, 47);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 42 R-baseline weak-protection transform.'
