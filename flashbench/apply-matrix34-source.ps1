param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('stableAuthorityCameraGuardArchitecture')) {
    Write-Host 'Matrix 34 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) { throw "Matrix 34 source transform missing anchor: $($Old.Substring(0, [Math]::Min(80, $Old.Length)))" }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 34 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

Replace-Once @'
        const bool cameraAwareEventDisocclusionArchitecture =
            P16.x > 15.5 && P16.x < 24.5;
'@ @'
        const bool cameraAwareEventDisocclusionArchitecture =
            P16.x > 15.5 && P16.x < 27.5;
'@
Replace-Once @'
        const bool stationaryWeakRepetitionArchitecture =
            P16.x > 16.5 && P16.x < 24.5;
'@ @'
        const bool stationaryWeakRepetitionArchitecture =
            P16.x > 16.5 && P16.x < 27.5;
'@
Replace-Once @'
        const bool texturelessStationaryPrimeStateArchitecture =
            P16.x > 19.5 && P16.x < 24.5;
'@ @'
        const bool texturelessStationaryPrimeStateArchitecture =
            P16.x > 19.5 && P16.x < 27.5;
'@
Replace-Once @'
        const bool currentEventOnlyCameraGuardArchitecture =
            P16.x > 23.5 && P16.x < 24.5;
'@ @'
        const bool currentEventOnlyCameraGuardArchitecture =
            P16.x > 23.5 && P16.x < 24.5;
        // Matrix 34 isolates mode 21's two remaining display-authority changes.
        // Modes 25/26 apply them separately and mode 27 applies both; sequence
        // state and current-event disocclusion remain mode-20 behavior.
        const bool stableAuthorityCameraGuardArchitecture =
            (P16.x > 24.5 && P16.x < 25.5) ||
            (P16.x > 26.5 && P16.x < 27.5);
        const bool holdAuthorizationCameraGuardArchitecture =
            P16.x > 25.5 && P16.x < 27.5;
'@
Replace-Once @'
        const float stationaryMotionGuardGate =
            qualifiedStationaryCameraGuardArchitecture ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                corroboratedMotionGate;
'@ @'
        const float stationaryMotionGuardGate =
            qualifiedStationaryCameraGuardArchitecture ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                corroboratedMotionGate;
        const float stableAuthorityMotionGuardGate =
            stableAuthorityCameraGuardArchitecture ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                stationaryMotionGuardGate;
        const float holdAuthorizationMotionGuardGate =
            holdAuthorizationCameraGuardArchitecture ?
                max(corroboratedMotionGate, unmaskedCpuCameraMotionGate) :
                stationaryMotionGuardGate;
'@
Replace-Once @'
        const float stableMotionConflict =
            smoothstep(0.10, 0.45, stationaryMotionGuardGate);
'@ @'
        const float stableMotionConflict =
            smoothstep(0.10, 0.45, stableAuthorityMotionGuardGate);
'@
Replace-Once @'
        const float stationaryCurrentHoldAuthorization =
            eventMask *
            (1.0 - smoothstep(0.02, 0.12, stationaryMotionGuardGate)) *
            (1.0 - verifiedLocalTransportGate);
'@ @'
        const float stationaryCurrentHoldAuthorization =
            eventMask *
            (1.0 - smoothstep(0.02, 0.12, holdAuthorizationMotionGuardGate)) *
            (1.0 - verifiedLocalTransportGate);
'@
Replace-Once 'std::clamp(tuning.architectureMode, 0, 24);' 'std::clamp(tuning.architectureMode, 0, 27);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 24);' 'std::clamp(requestedArchitecture, 0, 27);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 34 source transform.'
