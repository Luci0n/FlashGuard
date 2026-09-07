param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'apply-matrix44-source.ps1') -SourcePath $SourcePath

$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('matrix45PrimeContinuityTraceArchitecture')) {
    Write-Host 'Matrix 45 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Matrix 45 source transform missing anchor: $($Old.Substring(0, [Math]::Min(100, $Old.Length)))"
    }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 45 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

# Matrix 44 proved that phase-0 loses the signed PRIME before opposition/risk
# activation. Matrix 45 remains behavior-identical to R+L and traces only the
# four constituents of signedPrimeContinuity plus PRIME immediately before the
# continuity multiplication. Mode 52 changes diagnostics only.
Replace-Once 'P16.x > 15.5 && P16.x < 51.5;' 'P16.x > 15.5 && P16.x < 52.5;'
Replace-Once 'P16.x > 16.5 && P16.x < 51.5;' 'P16.x > 16.5 && P16.x < 52.5;'
Replace-Once 'P16.x > 19.5 && P16.x < 51.5;' 'P16.x > 19.5 && P16.x < 52.5;'

Replace-Once @'
        const bool matrix44ActivationTraceArchitecture =
            P16.x > 50.5 && P16.x < 51.5;
        const float matrix35RepetitionMotionGuardGate =
'@ @'
        const bool matrix44ActivationTraceArchitecture =
            P16.x > 50.5 && P16.x < 51.5;
        // Matrix 45 keeps the exact Matrix-44 R+L behavior. The new mode only
        // exposes the PRIME-continuity veto inputs through replay diagnostic MRTs.
        const bool matrix45PrimeContinuityTraceArchitecture =
            P16.x > 51.5 && P16.x < 52.5;
        const float matrix35RepetitionMotionGuardGate =
'@

# Preserve R restoration ownership for mode 52.
Replace-Once @'
                    matrix43RestoreLongRiskBaseline ||
                    matrix44ActivationTraceArchitecture) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@ @'
                    matrix43RestoreLongRiskBaseline ||
                    matrix44ActivationTraceArchitecture ||
                    matrix45PrimeContinuityTraceArchitecture) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@

# Preserve the Matrix-42 L = 0.40 s qualified surface-risk lifetime for mode 52.
Replace-Once @'
                    matrix43RestoreLongRiskBaseline ||
                    matrix44ActivationTraceArchitecture) ?
                    0.40 : 0.22);
'@ @'
                    matrix43RestoreLongRiskBaseline ||
                    matrix44ActivationTraceArchitecture ||
                    matrix45PrimeContinuityTraceArchitecture) ?
                    0.40 : 0.22);
'@

# Capture the decayed/reprojected PRIME before continuity is applied. This is
# diagnostic-only state: neither value below changes the actual multiplication.
Replace-Once @'
            const float signedPrimeContinuity =
                saturate(signedPrimeContinuityEvidence) *
                (1.0 - hardStateDisocclusion);
            transportedSignedPrime *= signedPrimeContinuity;
            const float oppositionStrength = max(
'@ @'
            const float signedPrimeContinuity =
                saturate(signedPrimeContinuityEvidence) *
                (1.0 - hardStateDisocclusion);
            const float matrix45PrimeBeforeContinuity = transportedSignedPrime;
            const float matrix45WouldBeOppositionStrength = max(
                0.0, -matrix45PrimeBeforeContinuity * currentSignedDirection);
            transportedSignedPrime *= signedPrimeContinuity;
            const float oppositionStrength = max(
'@

# Matrix 44 writes activation diagnostics late enough that replay sees the final
# MRT values. Reuse that same point for mode 52, but expose continuity evidence:
#   MRT0 = stationary continuity, verified transport, textureless fallback,
#          hard disocclusion
#   MRT1 = combined continuity evidence, final continuity multiplier,
#          PRIME-before-continuity encoded, would-be opposition before the veto
# MRT2 remains the existing replay attribution output.
Replace-Once @'
            if (matrix44ActivationTraceArchitecture)
            {
                output.motionDiagnostics0 = float4(
                    saturate(weakSignedMagnitudeGate),
                    saturate(0.5 + 0.5 * transportedSignedPrime),
                    saturate(oppositionStrength),
                    saturate(weakOpposingTransitionGate));
                output.motionDiagnostics1 = float4(
                    saturate(qualifiedIntrinsicEvent),
                    saturate(surfaceRiskStateSeed),
                    saturate(transportedSurfaceRisk),
                    saturate(currentFrameStrength));
            }
            if (phaseHoldArchitecture)
'@ @'
            if (matrix44ActivationTraceArchitecture)
            {
                output.motionDiagnostics0 = float4(
                    saturate(weakSignedMagnitudeGate),
                    saturate(0.5 + 0.5 * transportedSignedPrime),
                    saturate(oppositionStrength),
                    saturate(weakOpposingTransitionGate));
                output.motionDiagnostics1 = float4(
                    saturate(qualifiedIntrinsicEvent),
                    saturate(surfaceRiskStateSeed),
                    saturate(transportedSurfaceRisk),
                    saturate(currentFrameStrength));
            }
            else if (matrix45PrimeContinuityTraceArchitecture)
            {
                output.motionDiagnostics0 = float4(
                    saturate(stationaryPrimeContinuity),
                    saturate(verifiedCurrentSurfaceTransport),
                    saturate(texturelessStationaryFallbackGate),
                    saturate(hardStateDisocclusion));
                output.motionDiagnostics1 = float4(
                    saturate(signedPrimeContinuityEvidence),
                    saturate(signedPrimeContinuity),
                    saturate(0.5 + 0.5 * matrix45PrimeBeforeContinuity),
                    saturate(matrix45WouldBeOppositionStrength));
            }
            if (phaseHoldArchitecture)
'@

# Matrix 44 already installed the replay center-pixel MRT reader and trace frame
# structure. Retain a second vector with identical storage for mode 52.
Replace-Once @'
                std::vector<Matrix44ActivationTraceFrame>
                    matrix44ActivationTrace;
                bool firstPerceptual = true;
'@ @'
                std::vector<Matrix44ActivationTraceFrame>
                    matrix44ActivationTrace;
                std::vector<Matrix44ActivationTraceFrame>
                    matrix45PrimeContinuityTrace;
                bool firstPerceptual = true;
'@

# Sample exactly the same 5 Hz frames as Matrix 44. Diagnostic textures are
# read after the real R+L render, never before it.
Replace-Once @'
                                    matrix44ActivationTrace.push_back(trace);
                                }
                                sourceVariation += std::fabs(
'@ @'
                                    matrix44ActivationTrace.push_back(trace);
                                }
                                if (m_benchmarkArchitectureMode == 52 &&
                                    std::fabs(frequencyHz - 5.0) < 0.001)
                                {
                                    Matrix44ActivationTraceFrame trace{};
                                    trace.deltaCode = deltaCode;
                                    trace.phaseFrames = phaseFrame;
                                    trace.frame = i;
                                    trace.high = phase < 0.5;
                                    trace.sourceLuma = currentCenter.first;
                                    trace.outputLuma = currentCenter.second;
                                    trace.diagnostics0 =
                                        readMatrix44CenterDiagnostics(0);
                                    trace.diagnostics1 =
                                        readMatrix44CenterDiagnostics(1);
                                    trace.diagnostics2 =
                                        readMatrix44CenterDiagnostics(2);
                                    matrix45PrimeContinuityTrace.push_back(trace);
                                }
                                sourceVariation += std::fabs(
'@

# Emit the raw continuity trace beside Matrix 44's activation trace. PRIME-after
# and actual post-veto opposition are derived from the recorded scalar multiplier;
# this avoids consuming another MRT channel and is algebraically exact here.
Replace-Once @'
                    std::fclose(traceReport);
                }

                if (replayScreening)
'@ @'
                    std::fclose(traceReport);
                }

                if (m_benchmarkArchitectureMode == 52 &&
                    !matrix45PrimeContinuityTrace.empty())
                {
                    const auto continuityPath =
                        std::filesystem::path(reportPath).parent_path() /
                        L"matrix45-prime-continuity-trace.json";
                    FILE* continuityReport = nullptr;
                    if (_wfopen_s(&continuityReport, continuityPath.c_str(), L"wb") != 0 ||
                        !continuityReport)
                        return false;
                    std::fprintf(continuityReport,
                        "{\n"
                        "  \"schema\": \"FLASHGUARD_MATRIX45_PRIME_CONTINUITY_TRACE/1\",\n"
                        "  \"fps\": %d,\n"
                        "  \"architecture_mode\": 52,\n"
                        "  \"diagnostic_only\": true,\n"
                        "  \"frames\": [\n",
                        replayFps);
                    for (size_t traceIndex = 0;
                         traceIndex < matrix45PrimeContinuityTrace.size(); ++traceIndex)
                    {
                        const auto& trace =
                            matrix45PrimeContinuityTrace[traceIndex];
                        const double primeBefore =
                            trace.diagnostics1[2] * 2.0 - 1.0;
                        const double primeAfter =
                            primeBefore * trace.diagnostics1[1];
                        const double actualOpposition =
                            trace.diagnostics1[3] * trace.diagnostics1[1];
                        std::fprintf(continuityReport,
                            "    {\"delta_code\":%d,\"phase_frames\":%.1f,"
                            "\"frame\":%d,\"high\":%s,"
                            "\"source_luma\":%.8f,\"output_luma\":%.8f,"
                            "\"stationary_prime_continuity\":%.8f,"
                            "\"verified_current_surface_transport\":%.8f,"
                            "\"textureless_stationary_fallback_gate\":%.8f,"
                            "\"hard_state_disocclusion\":%.8f,"
                            "\"signed_prime_continuity_evidence\":%.8f,"
                            "\"signed_prime_continuity\":%.8f,"
                            "\"prime_before_continuity\":%.8f,"
                            "\"prime_after_continuity\":%.8f,"
                            "\"would_be_opposition_strength\":%.8f,"
                            "\"actual_opposition_strength\":%.8f,"
                            "\"architecture_luma_delta\":%.8f,"
                            "\"surface_memory_strength\":%.8f}%s\n",
                            trace.deltaCode, trace.phaseFrames, trace.frame,
                            trace.high ? "true" : "false",
                            trace.sourceLuma, trace.outputLuma,
                            trace.diagnostics0[0], trace.diagnostics0[1],
                            trace.diagnostics0[2], trace.diagnostics0[3],
                            trace.diagnostics1[0], trace.diagnostics1[1],
                            primeBefore, primeAfter,
                            trace.diagnostics1[3], actualOpposition,
                            trace.diagnostics2[1], trace.diagnostics2[3],
                            traceIndex + 1 < matrix45PrimeContinuityTrace.size() ?
                                "," : "");
                    }
                    std::fputs("  ]\n}\n", continuityReport);
                    std::fclose(continuityReport);
                }

                if (replayScreening)
'@

Replace-Once 'std::clamp(tuning.architectureMode, 0, 51);' 'std::clamp(tuning.architectureMode, 0, 52);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 51);' 'std::clamp(requestedArchitecture, 0, 52);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 45 PRIME-continuity trace transform.'
