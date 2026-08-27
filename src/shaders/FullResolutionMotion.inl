R"HLSL(        // First compare raw source at the same screen coordinate. Bright moving
        // objects create a huge same-coordinate delta even though their appearance
        // is unchanged; using that delta directly is what produced v7's trails.
        float sourceDelta = 0.0;
        float motionCompensatedSourceDelta = 0.0;
        float localMotionGate = 0.0;
        float hardwareMotionGate = 0.0;
        float2 protectionStatePreviousUv = i.uv;
        float protectionStateTransportConfidence = 0.0;
        float diagGlobalFlowGate = 0.0;
        float diagCurrentSurfaceGate = 0.0;
        float diagVacatedGate = 0.0;
        float diagInfillGate = 0.0;
        float diagPortableGate = 0.0;
        // P8.x encodes NVOFA state: 0=fallback/unavailable, 0.5=anchor-only, 1=fresh flow.
        // A skipped execute still keeps the immediate previous frame as the next anchor,
        // but must NOT trigger the expensive portable matcher.
        const bool hardwareFlowAvailable = P8.x > 0.25;
        const bool hardwareFlowValid = P8.x > 0.75 && P7.z > 0.5;
        if (P7.z > 0.5)
        {
            const float4 previousSourceSameState = PreviousSource.SampleLevel(
                LinearClamp, i.uv, 0.0);
            const float3 previousSourceSame = previousSourceSameState.rgb;
            sourceDelta = SourceMatchError(rawSourceColor, previousSourceSame);
            motionCompensatedSourceDelta = sourceDelta;

            // NVIDIA Optical Flow SDK path.
            //
            // There are TWO motion cases that matter for ghosting:
            //   1) current-surface transport: where a moving object is NOW;
            //   2) vacated/disoccluded pixels: where that object USED TO BE.
            //
            // v9 only handled (1). A bright object therefore left its filtered
            // luminance behind on trailing edges even with valid grid-2 flow.
            // Fresh hardware flow is independent evidence and must be evaluated
            // before flash/CPU state decides whether temporal history is allowed.
            // The raw patch, forward/backward and NVOFA-cost checks below are the
            // authority for rejecting noisy/static vectors.
            if (hardwareFlowValid)
            {
                const float2 outputSize = max(float2(P2.z, P2.w), float2(1.0, 1.0));
                const float2 outputTexel = 1.0 / outputSize;
                const float2 forwardPixels =
                    LoadOpticalFlow(ForwardOpticalFlow, i.uv);
                const float flowMagnitude = length(forwardPixels);

                // Global-flow confidence is geometric: require dominant/local
                // vector agreement and NVOFA cost. The warped source is sampled
                // only after that decision to measure independent appearance
                // residual, so a moving flash can retain valid geometry.
                const float2 globalPixels = LoadGlobalOpticalFlow();
                const float globalMagnitude = length(globalPixels);
                float globalMotionGate = 0.0;
                if (globalMagnitude > 0.35 && flowMagnitude > 0.10)
                {
                    const float directionAgreement =
                        dot(globalPixels, forwardPixels) /
                        max(globalMagnitude * flowMagnitude, 0.001);
                    const float magnitudeAgreement =
                        min(globalMagnitude, flowMagnitude) /
                        max(globalMagnitude, flowMagnitude);
                    float localCostConfidence = 1.0;
                    if (P9.w > 0.5)
                    {
                        const float flowCost = LoadOpticalCost(
                            ForwardOpticalCost, i.uv);
                        localCostConfidence =
                            1.0 - smoothstep(0.28, 0.78, flowCost);
                    }
                    const float globalEvidence =
                        smoothstep(0.72, 0.94, directionAgreement) *
                        smoothstep(0.30, 0.72, magnitudeAgreement) *
                        lerp(0.35, 1.0, localCostConfidence);
                    globalMotionGate = globalEvidence > 0.52 ? 1.0 :
                        smoothstep(0.28, 0.52, globalEvidence);

                    const float2 globalPreviousUv = i.uv + globalPixels / outputSize;
                    const bool insideGlobalPrevious =
                        all(globalPreviousUv >= float2(0.0, 0.0)) &&
                        all(globalPreviousUv <= float2(1.0, 1.0));
                    if (insideGlobalPrevious)
                    {
                        const float3 previousGlobal = PreviousSource.SampleLevel(
                            LinearClamp, globalPreviousUv, 0.0).rgb;
                        const float globalError =
                            SourceMatchError(rawSourceColor, previousGlobal);
                        diagGlobalFlowGate = max(diagGlobalFlowGate, globalMotionGate);
                        hardwareMotionGate = max(
                            hardwareMotionGate, globalMotionGate);
                        localMotionGate = max(localMotionGate, globalMotionGate);
                        motionCompensatedSourceDelta = lerp(
                            motionCompensatedSourceDelta,
                            min(motionCompensatedSourceDelta, globalError),
                            globalMotionGate);
                        if (globalMotionGate > protectionStateTransportConfidence)
                        {
                            protectionStateTransportConfidence = globalMotionGate;
                            protectionStatePreviousUv = globalPreviousUv;
                        }
                    }
                }

                // --- A. Current surface -> its previous position -----------------
                const float2 previousUv = i.uv + forwardPixels / outputSize;
                const bool insidePrevious = all(previousUv >= float2(0.0, 0.0)) &&
                    all(previousUv <= float2(1.0, 1.0));

                if (insidePrevious && flowMagnitude > 0.10)
                {
                    const float4 previousWarpedState = PreviousSource.SampleLevel(
                        LinearClamp, previousUv, 0.0);
                    const float3 previousWarped = previousWarpedState.rgb;
                    const float previousGeometryConfidence =
                        saturate(previousWarpedState.a);
                    const float currentSurfaceResidual =
                        SourceMatchError(rawSourceColor, previousWarped);
                    const float2 backwardPixels = LoadOpticalFlow(
                        BackwardOpticalFlow, previousUv);
                    const float roundTripError = length(forwardPixels + backwardPixels);

                    // Measure current-frame spatial observability only. Do not use
                    // current-vs-previous appearance similarity to establish
                    // correspondence: intrinsic brightness/chroma may legitimately
                    // change on a moving surface.
                    float currentStructure = 0.0;
                    float2 currentGradient = float2(0.0, 0.0);
                    const float centerSourceLuma = Luma(rawSourceColor);
                    [unroll]
                    for (int py = -1; py <= 1; ++py)
                    {
                        [unroll]
                        for (int px = -1; px <= 1; ++px)
                        {
                            if (abs(px) + abs(py) <= 1)
                            {
                                const float2 patchOffset =
                                    float2((float)px, (float)py) * outputTexel * 2.0;
                                const float3 currentPatch = CurrentFrame.SampleLevel(
                                    LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                const float patchLuma = Luma(currentPatch);
                                currentStructure +=
                                    abs(patchLuma - centerSourceLuma);
                                currentGradient +=
                                    float2((float)px, (float)py) * patchLuma;
                            }
                        }
                    }
                    currentStructure /= 5.0;

                    const float allowedRoundTrip =
                        max(1.25, 0.65 + flowMagnitude * 0.22);
                    const float fbConfidence =
                        1.0 - smoothstep(allowedRoundTrip,
                                       allowedRoundTrip + 2.0,
                                       roundTripError);

                    float costConfidence = 1.0;
                    if (P9.w > 0.5)
                    {
                        const float flowCost = LoadOpticalCost(
                            ForwardOpticalCost, i.uv);
                        costConfidence = 1.0 - smoothstep(0.28, 0.78, flowCost);
                    }

                    float flowCoherence = 0.0;
                    [unroll]
                    for (int ni = 0; ni < 4; ++ni)
                    {
                        const float2 neighborDirection = ni == 0 ? float2(-1.0, 0.0) :
                            (ni == 1 ? float2(1.0, 0.0) :
                            (ni == 2 ? float2(0.0, -1.0) : float2(0.0, 1.0)));
                        const float2 neighborUv =
                            i.uv + neighborDirection * outputTexel * 4.0;
                        const float2 neighborFlow =
                            LoadOpticalFlow(ForwardOpticalFlow, neighborUv);
                        const float neighborMagnitude = length(neighborFlow);
                        if (neighborMagnitude > 0.10)
                        {
                            const float directionAgreement =
                                dot(forwardPixels, neighborFlow) /
                                max(flowMagnitude * neighborMagnitude, 0.001);
                            const float magnitudeAgreement =
                                min(flowMagnitude, neighborMagnitude) /
                                max(flowMagnitude, neighborMagnitude);
                            flowCoherence +=
                                smoothstep(0.70, 0.94, directionAgreement) *
                                smoothstep(0.30, 0.75, magnitudeAgreement);
                        }
                    }
                    flowCoherence *= 0.25;

                    const float gradientMagnitude = length(currentGradient);
                    const float normalAlignment = gradientMagnitude > 0.0005 ?
                        abs(dot(forwardPixels, currentGradient) /
                            max(flowMagnitude * gradientMagnitude, 0.001)) : 0.0;
                    const float edgeObservability =
                        smoothstep(0.004, 0.035, currentStructure) *
                        smoothstep(0.20, 0.72, normalAlignment);
                    const float coherentObservability =
                        smoothstep(0.40, 0.78, flowCoherence);
                    const float geometryObservability =
                        max(edgeObservability, coherentObservability);
                    const float geometryBase =
                        fbConfidence * geometryObservability *
                        lerp(0.35, 1.0, costConfidence);
                    const float continuitySupport =
                        smoothstep(0.25, 0.70, previousGeometryConfidence);
                    const float continuityGeometry =
                        fbConfidence * coherentObservability * continuitySupport *
                        lerp(0.35, 1.0, costConfidence);
                    const float transportEvidence =
                        max(geometryBase, continuityGeometry * 0.92);

                    const float transportGate =
                        transportEvidence > 0.34 ? 1.0 :
                        smoothstep(0.18, 0.34, transportEvidence);
                    diagCurrentSurfaceGate = max(
                        diagCurrentSurfaceGate, transportGate);
                    if (transportGate > protectionStateTransportConfidence)
                    {
                        protectionStateTransportConfidence = transportGate;
                        protectionStatePreviousUv = previousUv;
                    }

                    // The compensated residual is measured only after geometry is
                    // accepted. It remains independent evidence of intrinsic change.
                    motionCompensatedSourceDelta = lerp(
                        motionCompensatedSourceDelta,
                        min(motionCompensatedSourceDelta, currentSurfaceResidual),
                        transportGate);
                    hardwareMotionGate = max(hardwareMotionGate, transportGate);
                    localMotionGate = max(localMotionGate, transportGate);
                }

                // --- B. Previous surface moved AWAY from this screen pixel -------
                // At a trailing edge there is no valid current->previous
                // correspondence for the newly exposed background. Use backward
                // flow from the PREVIOUS pixel instead: if that old pixel is found
                // at its new current destination, this location was vacated by
                // motion and old filtered history must be dropped immediately.
                const float2 backwardFromHere =
                    LoadOpticalFlow(BackwardOpticalFlow, i.uv);
                const float vacatedMagnitude = length(backwardFromHere);
                const float2 movedToUv = i.uv + backwardFromHere / outputSize;
                const bool insideMovedTo = all(movedToUv >= float2(0.0, 0.0)) &&
                    all(movedToUv <= float2(1.0, 1.0));

                if (insideMovedTo && vacatedMagnitude > 0.10)
                {
                    const float2 forwardAtDestination =
                        LoadOpticalFlow(ForwardOpticalFlow, movedToUv);
                    const float vacatedRoundTrip =
                        length(backwardFromHere + forwardAtDestination);

                    const float vacatedAllowedRoundTrip =
                        max(1.25, 0.65 + vacatedMagnitude * 0.22);
                    const float vacatedFb =
                        1.0 - smoothstep(vacatedAllowedRoundTrip,
                                       vacatedAllowedRoundTrip + 2.0,
                                       vacatedRoundTrip);
                    float vacatedCostConfidence = 1.0;
                    if (P9.w > 0.5)
                    {
                        const float flowCost = LoadOpticalCost(
                            BackwardOpticalCost, i.uv);
                        vacatedCostConfidence = 1.0 - smoothstep(0.28, 0.78, flowCost);
                    }
                    const float vacatedContinuity =
                        smoothstep(0.25, 0.70, previousSourceSameState.a);
                    // A vacated-surface decision is geometric. Appearance at this
                    // screen coordinate now belongs to a newly revealed surface.
                    const float vacatedEvidence =
)HLSL" R"HLSL(                        vacatedFb *
                        lerp(0.35, 1.0, vacatedCostConfidence) *
                        lerp(0.55, 1.0, vacatedContinuity);

                    const float rawVacatedGate =
                        vacatedEvidence > 0.34 ? 1.0 :
                        smoothstep(0.18, 0.34, vacatedEvidence);
                    // Mode 13 fixes the semantic ambiguity exposed by Matrix 22.
                    // The old material point at this screen coordinate may move
                    // away while another point of the SAME surface moves into the
                    // coordinate. A valid current->previous correspondence proves
                    // that this current pixel has a predecessor, so it cannot also
                    // be treated as a disocclusion merely because the old point
                    // moved elsewhere. True trailing-edge holes retain no current
                    // surface coverage and therefore keep the raw vacated result.
                    const float vacatedGate = P16.x > 12.5 ?
                        rawVacatedGate *
                            (1.0 - saturate(diagCurrentSurfaceGate)) :
                        rawVacatedGate;
                    diagVacatedGate = max(diagVacatedGate, vacatedGate);

                    // Only corrected current-pixel disocclusion may erase the
                    // intrinsic residual. This preserves a real brightness change
                    // on overlapping translating surfaces while true newly revealed
                    // background still discards the departed surface's history.
                    motionCompensatedSourceDelta *= (1.0 - vacatedGate);

                    // Do NOT transport history for a vacated pixel: the previous
                    // history here belongs to the object that left. We only need the
                    // motion bypass so the freshly revealed background appears now.
                    hardwareMotionGate = max(hardwareMotionGate, vacatedGate);
                    localMotionGate = max(localMotionGate, vacatedGate);
                }

                // --- C. Conservative disocclusion infill --------------------------
                // A fast object can expose a strip whose exact boundary vector is
                // noisy even though nearby previous->current vectors are excellent.
                // Search only changing pixels and accept an infill bypass only when
                // a nearby backward vector survives round-trip, cost, and prior
                // surface-continuity validation. Legacy modes retain the high-risk
                // guard; benchmark mode 5 tests whether that guard creates a
                // self-locking stale-risk trail by suppressing valid geometry.
                if (hardwareMotionGate < 0.80 && sourceDelta > 0.010 &&
                    (P16.x > 4.5 || temporalRisk < 0.70))
                {
                    float bestInfillEvidence = 0.0;
                    float2 bestInfillFlow = float2(0.0, 0.0);
                    [unroll]
                    for (int iri = 0; iri < 4; ++iri)
                    {
                        const float radius = exp2((float)(iri + 1)); // 2,4,8,16 px
                        [unroll]
                        for (int iy = -1; iy <= 1; ++iy)
                        {
                            [unroll]
                            for (int ix = -1; ix <= 1; ++ix)
                            {
                                if (abs(ix) + abs(iy) == 1)
                                {
                                    const float2 neighborUv = i.uv +
                                        float2((float)ix, (float)iy) *
                                        outputTexel * radius;
                                    const bool insideNeighbor =
                                        all(neighborUv >= float2(0.0, 0.0)) &&
                                        all(neighborUv <= float2(1.0, 1.0));
                                    if (insideNeighbor)
                                    {
                                        const float2 neighborBackward =
                                            LoadOpticalFlow(
                                                BackwardOpticalFlow, neighborUv);
                                        const float neighborMagnitude =
                                            length(neighborBackward);
                                        const float2 neighborDestination =
                                            neighborUv +
                                            neighborBackward / outputSize;
                                        const bool insideDestination =
                                            all(neighborDestination >=
                                                float2(0.0, 0.0)) &&
                                            all(neighborDestination <=
                                                float2(1.0, 1.0));
                                        if (insideDestination &&
                                            neighborMagnitude > 0.50)
                                        {
                                            const float2 forwardAtDestination =
                                                LoadOpticalFlow(
                                                    ForwardOpticalFlow,
                                                    neighborDestination);
                                            const float roundTrip = length(
                                                neighborBackward +
                                                forwardAtDestination);
                                            const float allowedRoundTrip =
                                                max(1.25, 0.65 +
                                                    neighborMagnitude * 0.22);
                                            const float fbConfidence =
                                                1.0 - smoothstep(
                                                    allowedRoundTrip,
                                                    allowedRoundTrip + 2.0,
                                                    roundTrip);

                                            const float4 previousNeighborState =
                                                PreviousSource.SampleLevel(
                                                    LinearClamp, neighborUv,
                                                    0.0);
                                            float costConfidence = 1.0;
                                            if (P9.w > 0.5)
                                            {
                                                const float flowCost =
                                                    LoadOpticalCost(
                                                        BackwardOpticalCost,
                                                        neighborUv);
                                                costConfidence =
                                                    1.0 - smoothstep(
                                                        0.24, 0.70, flowCost);
                                            }
                                            // Only vectors whose displacement can
                                            // plausibly sweep over this radius may
                                            // fill the newly exposed strip.
                                            const float sweptSupport =
                                                1.0 - smoothstep(
                                                    neighborMagnitude + 2.0,
                                                    neighborMagnitude + 7.0,
                                                    radius);
                                            const float neighborContinuity =
                                                smoothstep(0.25, 0.70,
                                                    previousNeighborState.a);
                                            const float evidence =
                                                fbConfidence *
                                                lerp(0.25, 1.0,
                                                    costConfidence) *
                                                sweptSupport *
                                                lerp(0.45, 1.0,
                                                    neighborContinuity);
                                            if (evidence > bestInfillEvidence)
                                            {
                                                bestInfillEvidence = evidence;
                                                bestInfillFlow =
                                                    neighborBackward;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Require at least two nearby vectors agreeing with the best
                    // verified motion before dropping history at the disocclusion.
                    if (bestInfillEvidence > 0.48)
                    {
                        int agreeingVectors = 0;
                        const float bestMagnitude =
                            max(length(bestInfillFlow), 0.001);
                        [unroll]
                        for (int si = 0; si < 4; ++si)
                        {
                            const float radius = exp2((float)(si + 1));
                            [unroll]
                            for (int syi = -1; syi <= 1; ++syi)
                            {
                                [unroll]
                                for (int sxi = -1; sxi <= 1; ++sxi)
                                {
                                    if (abs(sxi) + abs(syi) == 1)
                                    {
                                        const float2 sampleUv = i.uv +
                                            float2((float)sxi, (float)syi) *
                                            outputTexel * radius;
                                        if (all(sampleUv >= float2(0.0, 0.0)) &&
                                            all(sampleUv <= float2(1.0, 1.0)))
                                        {
                                            const float2 sampleFlow =
                                                LoadOpticalFlow(
                                                    BackwardOpticalFlow,
                                                    sampleUv);
                                            const float sampleMagnitude =
                                                length(sampleFlow);
                                            const float directionAgreement =
                                                dot(sampleFlow, bestInfillFlow) /
                                                max(sampleMagnitude *
                                                    bestMagnitude, 0.001);
                                            const float magnitudeRatio =
                                                min(sampleMagnitude,
                                                    bestMagnitude) /
                                                max(sampleMagnitude,
                                                    bestMagnitude);
                                            if (sampleMagnitude > 0.50 &&
                                                directionAgreement > 0.90 &&
                                                magnitudeRatio > 0.62)
)HLSL" R"HLSL(                                                agreeingVectors++;
                                        }
                                    }
                                }
                            }
                        }
                        const float supportGate =
                            smoothstep(1.0, 3.0,
                                (float)agreeingVectors);
                        const float infillGate = supportGate *
                            smoothstep(0.48, 0.72,
                                bestInfillEvidence);
                        diagInfillGate = max(diagInfillGate, infillGate);
                        hardwareMotionGate = max(
                            hardwareMotionGate, infillGate);
                        localMotionGate = max(
                            localMotionGate, infillGate);
                    }
                }
            }

            // The raw-image matcher is a fallback only. When fresh NVOFA exists,
            // appearance similarity must never be mixed back into geometric
            // correspondence confidence.
            if (!hardwareFlowValid &&
                P6.y < max(0.10, P5.x * 0.75) &&
                sourceDelta > 0.010 && coarseMotion < 0.30 &&
                max(coarseEvent, coarseRisk) > 0.010)
            {
                const float2 outputTexel = 1.0 / max(
                    float2(P2.z, P2.w), float2(1.0, 1.0));

                // First determine the most plausible CARDINAL transport direction
                // from inexpensive center-pixel matches. Cardinal preference avoids
                // the ambiguity of a flat bright object matching many diagonal)HLSL" R"HLSL(
                // interior pixels.
                float bestCardinalError = sourceDelta;
                float2 bestCardinalDir = float2(0.0, 0.0);
                [unroll]
                for (int ri = 0; ri < 5; ++ri)
                {
                    const float radius = exp2((float)ri); // 1,2,4,8,16 pixels
                    [unroll]
                    for (int my = -1; my <= 1; ++my)
                    {
                        [unroll]
                        for (int mx = -1; mx <= 1; ++mx)
                        {
                            if (abs(mx) + abs(my) == 1)
                            {
                                const float2 dir = float2((float)mx, (float)my);
                                const float2 offset = dir * outputTexel * radius;
                                const float3 previousMoved = PreviousSource.SampleLevel(
                                    LinearClamp, i.uv + offset, 0.0).rgb;
                                const float error = SourceMatchError(
                                    rawSourceColor, previousMoved);
                                if (error < bestCardinalError)
                                {
                                    bestCardinalError = error;
                                    bestCardinalDir = dir;
                                }
                            }
                        }
                    }
                }

                // Current/source patch error at the unshifted coordinate. The
                // verification patch is center + four cardinal neighbors, 2 px
                // apart, so a moving edge/shape has spatial structure to align.
                float samePatchError = 0.0;
                const float verifyRadius = 2.0;
                [unroll]
                for (int py = -1; py <= 1; ++py)
                {
                    [unroll]
                    for (int px = -1; px <= 1; ++px)
                    {
                        if (abs(px) + abs(py) <= 1)
                        {
                            const float2 patchOffset = float2((float)px, (float)py) *
                                outputTexel * verifyRadius;
                            const float3 currentPatch = CurrentFrame.SampleLevel(
                                LinearClamp, i.uv + patchOffset, 0.0).rgb;
                            const float3 previousSamePatch = PreviousSource.SampleLevel(
                                LinearClamp, i.uv + patchOffset, 0.0).rgb;
                            samePatchError += SourceMatchError(
                                currentPatch, previousSamePatch);
                        }
                    }
                }

                // Once a direction is known, choose its radius by PATCH error, not
                // center color. This is what lets a translating bright rectangle
                // resolve to its true previous edge instead of an arbitrary bright
                // interior sample.
                if (dot(bestCardinalDir, bestCardinalDir) > 0.0 &&
                    bestCardinalError < sourceDelta * 0.80 && samePatchError > 0.012)
                {
                    float bestPatchError = samePatchError;
                    float2 bestPatchOffset = float2(0.0, 0.0);
                    [unroll]
                    for (int ri = 0; ri < 5; ++ri)
                    {
                        const float radius = exp2((float)ri);
                        const float2 offset = bestCardinalDir * outputTexel * radius;
                        float shiftedPatchError = 0.0;
                        [unroll]
                        for (int py = -1; py <= 1; ++py)
                        {
                            [unroll]
                            for (int px = -1; px <= 1; ++px)
                            {
                                if (abs(px) + abs(py) <= 1)
                                {
                                    const float2 patchOffset = float2((float)px, (float)py) *
                                        outputTexel * verifyRadius;
                                    const float3 currentPatch = CurrentFrame.SampleLevel(
                                        LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                    const float3 previousShiftPatch = PreviousSource.SampleLevel(
                                        LinearClamp, i.uv + patchOffset + offset, 0.0).rgb;
                                    shiftedPatchError += SourceMatchError(
                                        currentPatch, previousShiftPatch);
                                }
                            }
                        }
                        if (shiftedPatchError < bestPatchError)
                        {
                            bestPatchError = shiftedPatchError;
                            bestPatchOffset = offset;
                        }
                    }

                    const float patchImprovement = saturate(
                        1.0 - bestPatchError / samePatchError);
                    const float absoluteMatch = 1.0 - smoothstep(0.040, 0.145,
                        bestPatchError / 5.0);
                    const float portableGate =
                        smoothstep(0.32, 0.68, patchImprovement) * absoluteMatch;
                    diagPortableGate = max(diagPortableGate,
                        portableGate > 0.60 ? 1.0 : portableGate);
                    localMotionGate = max(localMotionGate,
                        portableGate > 0.60 ? 1.0 : portableGate);
                }

                // Diagonal fallback only when cardinal transport was inconclusive.
                // It is intentionally conditional so ordinary gameplay does not
                // pay for two full local searches on every changing pixel.
                if (localMotionGate < 0.45 && samePatchError > 0.012)
                {
                    float bestDiagonalError = sourceDelta;
                    float2 bestDiagonalDir = float2(0.0, 0.0);
                    [unroll]
                    for (int ri = 1; ri < 5; ++ri) // 2,4,8,16 pixels
                    {
                        const float radius = exp2((float)ri);
                        [unroll]
                        for (int my = -1; my <= 1; my += 2)
                        {
                            [unroll]
                            for (int mx = -1; mx <= 1; mx += 2)
                            {
                                const float2 dir = float2((float)mx, (float)my);
                                const float2 offset = dir * outputTexel * radius;
                                const float3 previousMoved = PreviousSource.SampleLevel(
                                    LinearClamp, i.uv + offset, 0.0).rgb;
                                const float error = SourceMatchError(
                                    rawSourceColor, previousMoved);
                                if (error < bestDiagonalError)
                                {
                                    bestDiagonalError = error;
                                    bestDiagonalDir = dir;
                                }
                            }
                        }
                    }

                    if (dot(bestDiagonalDir, bestDiagonalDir) > 0.0 &&
                        bestDiagonalError < sourceDelta * 0.80)
                    {
                        float bestPatchError = samePatchError;
                        float2 bestPatchOffset = float2(0.0, 0.0);
                        [unroll]
                        for (int ri = 1; ri < 5; ++ri)
                        {
                            const float radius = exp2((float)ri);
                            const float2 offset = bestDiagonalDir * outputTexel * radius;
                            float shiftedPatchError = 0.0;
                            [unroll]
                            for (int py = -1; py <= 1; ++py)
                            {
                                [unroll]
                                for (int px = -1; px <= 1; ++px)
                                {
                                    if (abs(px) + abs(py) <= 1)
                                    {
                                        const float2 patchOffset = float2((float)px, (float)py) *
                                            outputTexel * verifyRadius;
                                        const float3 currentPatch = CurrentFrame.SampleLevel(
                                            LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                        const float3 previousShiftPatch = PreviousSource.SampleLevel(
                                            LinearClamp, i.uv + patchOffset + offset, 0.0).rgb;
                                        shiftedPatchError += SourceMatchError(
                                            currentPatch, previousShiftPatch);
                                    }
                                }
                            }
                            if (shiftedPatchError < bestPatchError)
                            {
                                bestPatchError = shiftedPatchError;
                                bestPatchOffset = offset;
                            }
                        }

                        const float patchImprovement = saturate(
                            1.0 - bestPatchError / samePatchError);
                        const float absoluteMatch = 1.0 - smoothstep(0.040, 0.145,
                            bestPatchError / 5.0);
                        const float diagonalGate = smoothstep(0.32, 0.68,
                            patchImprovement) * absoluteMatch;
                        diagPortableGate = max(diagPortableGate,
                            diagonalGate > 0.60 ? 1.0 : diagonalGate);
                        if (diagonalGate > localMotionGate)
                        {
                            localMotionGate = diagonalGate > 0.60 ? 1.0 : diagonalGate;
                        }
                    }
                }

                // Arbitrary shallow/steep slopes are common for projectiles,
                // particles and model edges. Cardinal + 45-degree searches can
                // miss a 3:1 or 3:2 translation even when the raw source clearly
                // proves motion. Refine any non-decisive cheaper match: a partial
                // cardinal gate must not prevent a better exact oblique candidate.
                if (localMotionGate < 0.90 && samePatchError > 0.012)
                {
                    float bestObliqueError = sourceDelta;
                    float2 bestObliqueOffset = float2(0.0, 0.0);
                    [unroll]
                    for (int major = 2; major <= 3; ++major)
                    {
                        [unroll]
                        for (int minor = 1; minor < major; ++minor)
                        {
                            [unroll]
                            for (int swapAxes = 0; swapAxes < 2; ++swapAxes)
                            {
                                [unroll]
                                for (int sy = -1; sy <= 1; sy += 2)
                                {
                                    [unroll]
                                    for (int sx = -1; sx <= 1; sx += 2)
                                    {
                                        const float2 offsetPixels = swapAxes == 0 ?
                                            float2((float)(sx * major), (float)(sy * minor)) :
                                            float2((float)(sx * minor), (float)(sy * major));
                                        const float2 offset = offsetPixels * outputTexel;
                                        const float3 previousMoved = PreviousSource.SampleLevel(
                                            LinearClamp, i.uv + offset, 0.0).rgb;
                                        const float error = SourceMatchError(
                                            rawSourceColor, previousMoved);
                                        if (error < bestObliqueError)
                                        {
)HLSL" R"HLSL(                                            bestObliqueError = error;
                                            bestObliqueOffset = offset;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (dot(bestObliqueOffset, bestObliqueOffset) > 0.0 &&
                        bestObliqueError < sourceDelta * 0.82)
                    {
                        float shiftedPatchError = 0.0;
                        [unroll]
                        for (int py = -1; py <= 1; ++py)
                        {
                            [unroll]
                            for (int px = -1; px <= 1; ++px)
                            {
                                if (abs(px) + abs(py) <= 1)
                                {
                                    const float2 patchOffset = float2((float)px, (float)py) *
                                        outputTexel * verifyRadius;
                                    const float3 currentPatch = CurrentFrame.SampleLevel(
                                        LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                    const float3 previousShiftPatch = PreviousSource.SampleLevel(
                                        LinearClamp, i.uv + patchOffset + bestObliqueOffset, 0.0).rgb;
                                    shiftedPatchError += SourceMatchError(
                                        currentPatch, previousShiftPatch);
                                }
                            }
                        }

                        const float patchImprovement = saturate(
                            1.0 - shiftedPatchError / samePatchError);
                        const float absoluteMatch = 1.0 - smoothstep(
                            0.040, 0.145, shiftedPatchError / 5.0);
                        const float obliqueGate = smoothstep(
                            0.28, 0.62, patchImprovement) * absoluteMatch;
                        diagPortableGate = max(diagPortableGate,
                            obliqueGate > 0.56 ? 1.0 : obliqueGate);
                        localMotionGate = max(localMotionGate,
                            obliqueGate > 0.56 ? 1.0 : obliqueGate);
                    }
                }

                // Dense patch-space refinement resolves the ambiguity of flat
                // bright objects: many offsets can match the center color, so the)HLSL" R"HLSL(
                // earlier directional searches may pick the wrong transport. Test
                // every small local translation by PATCH error and make a strong
                // structural match decisive. Keep this off global/broad protection
                // so a true flash never pays the dense search or gets bypassed.
                if (localMotionGate < 0.98 && samePatchError > 0.012 &&
                    abs(protectionGate) < 0.5 && overloadGate < 0.5 && P6.x < 0.5)
                {
                    float bestDensePatchError = samePatchError;
                    [loop]
                    for (int denseY = -4; denseY <= 4; ++denseY)
                    {
                        [loop]
                        for (int denseX = -4; denseX <= 4; ++denseX)
                        {
                            if (denseX == 0 && denseY == 0) continue;
                            const float2 denseOffset = float2(
                                (float)denseX, (float)denseY) * outputTexel;
                            float densePatchError = 0.0;
                            [unroll]
                            for (int py = -1; py <= 1; ++py)
                            {
                                [unroll]
                                for (int px = -1; px <= 1; ++px)
                                {
                                    if (abs(px) + abs(py) <= 1)
                                    {
                                        const float2 patchOffset =
                                            float2((float)px, (float)py) *
                                            outputTexel * verifyRadius;
                                        const float3 currentPatch =
                                            CurrentFrame.SampleLevel(
                                                LinearClamp, i.uv + patchOffset, 0.0).rgb;
                                        const float3 previousPatch =
                                            PreviousSource.SampleLevel(
                                                LinearClamp,
                                                i.uv + patchOffset + denseOffset,
                                                0.0).rgb;
                                        densePatchError += SourceMatchError(
                                            currentPatch, previousPatch);
                                    }
                                }
                            }
                            bestDensePatchError = min(
                                bestDensePatchError, densePatchError);
                        }
                    }

                    const float denseImprovement = saturate(
                        1.0 - bestDensePatchError / samePatchError);
                    const float denseMeanError = bestDensePatchError / 5.0;
                    if (denseImprovement > 0.24 && denseMeanError < 0.085)
                    {
                        diagPortableGate = 1.0;
                        localMotionGate = 1.0;
                    }
                    else
                    {
                        const float denseAbsoluteMatch = 1.0 - smoothstep(
                            0.035, 0.120, denseMeanError);
                        const float denseGate = smoothstep(
                            0.24, 0.60, denseImprovement) * denseAbsoluteMatch;
                        diagPortableGate = max(diagPortableGate, denseGate);
                        localMotionGate = max(localMotionGate, denseGate);
                    }
                }
            }

            // Transport one-frame geometry confidence with raw source RGB.
            // Newly revealed pixels deliberately start untrusted so the departed
            // surface's confidence cannot leak onto a disocclusion.
            sourceHistoryGeometryConfidence =
                saturate(max(diagGlobalFlowGate, diagCurrentSurfaceGate));
            sourceHistoryGeometryConfidence *=
                1.0 - saturate(max(diagVacatedGate, diagInfillGate));
        }
)HLSL"
