param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'apply-matrix43-source.ps1') -SourcePath $SourcePath

$text = [IO.File]::ReadAllText($SourcePath).Replace("`r`n", "`n")
if ($text.Contains('matrix44ActivationTraceArchitecture')) {
    Write-Host 'Matrix 44 source transform already present.'
    exit 0
}

function Replace-Once([string]$Old, [string]$New) {
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $first = $script:text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Matrix 44 source transform missing anchor: $($Old.Substring(0, [Math]::Min(100, $Old.Length)))"
    }
    $second = $script:text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) { throw 'Matrix 44 source transform anchor is not unique.' }
    $script:text = $script:text.Substring(0, $first) + $New +
        $script:text.Substring($first + $Old.Length)
}

# Matrix 43 falsified signed-PRIME amplitude/lifetime as the general cause of the
# 5 Hz phase-0 weakness. Matrix 44 therefore makes no new protection decision.
# Mode 51 is behavior-identical to Matrix 42's clean R+L control (mode 46), but
# exposes the post-PRIME activation chain through replay-only diagnostic MRTs.
Replace-Once 'P16.x > 15.5 && P16.x < 50.5;' 'P16.x > 15.5 && P16.x < 51.5;'
Replace-Once 'P16.x > 16.5 && P16.x < 50.5;' 'P16.x > 16.5 && P16.x < 51.5;'
Replace-Once 'P16.x > 19.5 && P16.x < 50.5;' 'P16.x > 19.5 && P16.x < 51.5;'

Replace-Once @'
        const bool matrix43LongPrimeFactor =
            P16.x > 48.5 && P16.x < 50.5;
        const float matrix35RepetitionMotionGuardGate =
'@ @'
        const bool matrix43LongPrimeFactor =
            P16.x > 48.5 && P16.x < 50.5;
        // Matrix 44 changes diagnostics only. Its behavior is the R+L control:
        // R restoration ownership ON, 0.40 s qualified surface-risk lifetime ON,
        // D/G/S/A/P OFF.
        const bool matrix44ActivationTraceArchitecture =
            P16.x > 50.5 && P16.x < 51.5;
        const float matrix35RepetitionMotionGuardGate =
'@

# Preserve R exactly for the diagnostic architecture.
Replace-Once @'
                    matrix41RestoreFactor || matrix42RestoreBaseline ||
                    matrix43RestoreLongRiskBaseline) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@ @'
                    matrix41RestoreFactor || matrix42RestoreBaseline ||
                    matrix43RestoreLongRiskBaseline ||
                    matrix44ActivationTraceArchitecture) ?
                    stationarySequenceStateGate : stationaryRepetitionGate) > 0.0 &&
'@

# Preserve L exactly for the diagnostic architecture. Signed-PRIME lifetime stays
# at the R+L control value; Matrix-43 P is deliberately OFF.
Replace-Once @'
                    matrix42ExtendedRiskLifetimeFactor ||
                    matrix43RestoreLongRiskBaseline) ?
                    0.40 : 0.22);
'@ @'
                    matrix42ExtendedRiskLifetimeFactor ||
                    matrix43RestoreLongRiskBaseline ||
                    matrix44ActivationTraceArchitecture) ?
                    0.40 : 0.22);
'@

# Once the normal R+L chain has computed every relevant quantity, expose the
# exact intermediate state in diagnostic MRT 0/1. These targets exist only in
# synthetic replay; production does not bind them. No value below feeds back.
Replace-Once @'
            const float currentFrameStrength = saturate(max(max(
                qualifiedIntrinsicEvent, surfaceMemoryMitigation),
                phaseHoldMitigation));
            if (phaseHoldArchitecture)
'@ @'
            const float currentFrameStrength = saturate(max(max(
                qualifiedIntrinsicEvent, surfaceMemoryMitigation),
                phaseHoldMitigation));
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
'@

# Add a cheap center-pixel reader for the three replay-only diagnostic MRTs.
Replace-Once @'
                    m_context->Unmap(m_replayReadback.get(), 0);
                    return std::pair<double, double>{ sourceLuma, outputLuma };
                };
                const auto perceptualPath =
'@ @'
                    m_context->Unmap(m_replayReadback.get(), 0);
                    return std::pair<double, double>{ sourceLuma, outputLuma };
                };
                const auto readMatrix44CenterDiagnostics = [&](size_t group) {
                    std::array<double, 4> values{};
                    if (group >= m_motionDiagnosticTextures.size() ||
                        !m_motionDiagnosticTextures[group] ||
                        !m_motionDiagnosticReadbacks[group])
                        return values;
                    m_context->CopyResource(
                        m_motionDiagnosticReadbacks[group].get(),
                        m_motionDiagnosticTextures[group].get());
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    ThrowIfFailed(m_context->Map(
                        m_motionDiagnosticReadbacks[group].get(), 0,
                        D3D11_MAP_READ, 0, &mapped));
                    const UINT x = width / 2u;
                    const UINT y = height / 2u;
                    const auto* row = reinterpret_cast<const uint16_t*>(
                        static_cast<const uint8_t*>(mapped.pData) +
                        static_cast<size_t>(y) * mapped.RowPitch);
                    for (size_t channel = 0; channel < 4; ++channel)
                        values[channel] = std::clamp(
                            static_cast<double>(halfToFloat(
                                row[static_cast<size_t>(x) * 4 + channel])),
                            0.0, 1.0);
                    m_context->Unmap(
                        m_motionDiagnosticReadbacks[group].get(), 0);
                    return values;
                };
                const auto perceptualPath =
'@

# Retain all 5 Hz perceptual frames for mode 51. The trace records the source and
# displayed luma plus each stage of the PRIME -> opposition -> risk -> mitigation
# chain. It is intentionally diagnostic-only and does not alter the replay cases.
Replace-Once @'
                bool firstPerceptual = true;
                perceptualMinReduction = 1.0;
                for (int deltaCode : deltas)
'@ @'
                struct Matrix44ActivationTraceFrame
                {
                    int deltaCode = 0;
                    double phaseFrames = 0.0;
                    int frame = 0;
                    bool high = false;
                    double sourceLuma = 0.0;
                    double outputLuma = 0.0;
                    std::array<double, 4> diagnostics0{};
                    std::array<double, 4> diagnostics1{};
                    std::array<double, 4> diagnostics2{};
                };
                std::vector<Matrix44ActivationTraceFrame>
                    matrix44ActivationTrace;
                bool firstPerceptual = true;
                perceptualMinReduction = 1.0;
                for (int deltaCode : deltas)
'@

Replace-Once @'
                                fillGray(static_cast<uint8_t>(
                                    phase < 0.5 ? highCode : lowCode));
                                const auto currentCenter = renderCenterLuma();
                                sourceVariation += std::fabs(
'@ @'
                                fillGray(static_cast<uint8_t>(
                                    phase < 0.5 ? highCode : lowCode));
                                const auto currentCenter = renderCenterLuma();
                                if (m_benchmarkArchitectureMode == 51 &&
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
                                    matrix44ActivationTrace.push_back(trace);
                                }
                                sourceVariation += std::fabs(
'@

Replace-Once @'
                std::fputs("\n  ]\n}\n", perceptualReport);
                std::fclose(perceptualReport);

                if (replayScreening)
'@ @'
                std::fputs("\n  ]\n}\n", perceptualReport);
                std::fclose(perceptualReport);

                if (m_benchmarkArchitectureMode == 51 &&
                    !matrix44ActivationTrace.empty())
                {
                    const auto tracePath =
                        std::filesystem::path(reportPath).parent_path() /
                        L"matrix44-activation-trace.json";
                    FILE* traceReport = nullptr;
                    if (_wfopen_s(&traceReport, tracePath.c_str(), L"wb") != 0 ||
                        !traceReport)
                        return false;
                    std::fprintf(traceReport,
                        "{\n"
                        "  \"schema\": \"FLASHGUARD_MATRIX44_ACTIVATION_TRACE/1\",\n"
                        "  \"fps\": %d,\n"
                        "  \"architecture_mode\": 51,\n"
                        "  \"diagnostic_only\": true,\n"
                        "  \"frames\": [\n",
                        replayFps);
                    for (size_t traceIndex = 0;
                         traceIndex < matrix44ActivationTrace.size(); ++traceIndex)
                    {
                        const auto& trace = matrix44ActivationTrace[traceIndex];
                        std::fprintf(traceReport,
                            "    {\"delta_code\":%d,\"phase_frames\":%.1f,"
                            "\"frame\":%d,\"high\":%s,"
                            "\"source_luma\":%.8f,\"output_luma\":%.8f,"
                            "\"weak_signed_magnitude_gate\":%.8f,"
                            "\"transported_prime_encoded\":%.8f,"
                            "\"transported_prime_signed\":%.8f,"
                            "\"opposition_strength\":%.8f,"
                            "\"weak_opposing_transition_gate\":%.8f,"
                            "\"qualified_intrinsic_event\":%.8f,"
                            "\"surface_risk_seed\":%.8f,"
                            "\"transported_surface_risk\":%.8f,"
                            "\"current_frame_strength\":%.8f,"
                            "\"preprocess_luma_delta\":%.8f,"
                            "\"architecture_luma_delta\":%.8f,"
                            "\"authority_current_event\":%.8f,"
                            "\"surface_memory_strength\":%.8f}%s\n",
                            trace.deltaCode, trace.phaseFrames, trace.frame,
                            trace.high ? "true" : "false",
                            trace.sourceLuma, trace.outputLuma,
                            trace.diagnostics0[0], trace.diagnostics0[1],
                            trace.diagnostics0[1] * 2.0 - 1.0,
                            trace.diagnostics0[2], trace.diagnostics0[3],
                            trace.diagnostics1[0], trace.diagnostics1[1],
                            trace.diagnostics1[2], trace.diagnostics1[3],
                            trace.diagnostics2[0], trace.diagnostics2[1],
                            trace.diagnostics2[2], trace.diagnostics2[3],
                            traceIndex + 1 < matrix44ActivationTrace.size() ?
                                "," : "");
                    }
                    std::fputs("  ]\n}\n", traceReport);
                    std::fclose(traceReport);
                }

                if (replayScreening)
'@

Replace-Once 'std::clamp(tuning.architectureMode, 0, 50);' 'std::clamp(tuning.architectureMode, 0, 51);'
Replace-Once 'std::clamp(requestedArchitecture, 0, 50);' 'std::clamp(requestedArchitecture, 0, 51);'

[IO.File]::WriteAllText($SourcePath, $text, [Text.UTF8Encoding]::new($false))
Write-Host 'Applied benchmark-only Matrix 44 activation-trace transform.'
