R"HLSL(        // First compare raw source at the same screen coordinate. Bright moving
        // objects create a huge same-coordinate delta even though their appearance
        // is unchanged; using that delta directly is what produced v7's trails.
        float sourceDelta = 0.0;
        float localMotionGate = 0.0;
        float hardwareMotionGate = 0.0;
        // P8.x encodes NVOFA state: 0=fallback/unavailable, 0.5=anchor-only, 1=fresh flow.
        // A skipped execute still keeps the immediate previous frame as the next anchor,
        // but must NOT trigger the expensive portable matcher.
        const bool hardwareFlowAvailable = P8.x > 0.25;
        const bool hardwareFlowValid = P8.x > 0.75 && P7.z > 0.5;
        if (P7.z > 0.5)
        {
            const float3 previousSourceSame = PreviousSource.SampleLevel(
                LinearClamp, i.uv, 0.0).rgb;
            sourceDelta = SourceMatchError(rawSourceColor, previousSourceSame);

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

                // NVIDIA's global-flow vector is a strong camera/background prior.
                // Validate it against the raw source before using it so a spatially
                // uniform flash cannot masquerade as camera motion.
                const float2 globalPixels = LoadGlobalOpticalFlow();
                const float globalMagnitude = length(globalPixels);
                float globalMotionGate = 0.0;
                if (globalMagnitude > 0.35 && sourceDelta > 0.004)
                {
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
                        const float globalImprovement = saturate(
                            1.0 - globalError / max(sourceDelta, 0.003));
                        const float globalAbsoluteMatch =
                            1.0 - smoothstep(0.035, 0.135, globalError);
                        const float globalEvidence = globalAbsoluteMatch *
                            smoothstep(0.18, 0.55, globalImprovement);
                        globalMotionGate = globalEvidence > 0.48 ? 1.0 :
                            smoothstep(0.26, 0.48, globalEvidence);
                        hardwareMotionGate = max(
                            hardwareMotionGate, globalMotionGate);
                        localMotionGate = max(localMotionGate, globalMotionGate);
                    }
                }

                // --- A. Current surface -> its previous position -----------------
                const float2 forwardPixels = LoadOpticalFlow(ForwardOpticalFlow, i.uv);
                const float2 previousUv = i.uv + forwardPixels / outputSize;
                const float flowMagnitude = length(forwardPixels);
                const bool insidePrevious = all(previousUv >= float2(0.0, 0.0)) &&
                    all(previousUv <= float2(1.0, 1.0));

                if (insidePrevious && flowMagnitude > 0.10)
                {
                    const float2 backwardPixels = LoadOpticalFlow(
                        BackwardOpticalFlow, previousUv);
                    const float roundTripError = length(forwardPixels + backwardPixels);

                    // Verify transport with a small CROSS patch, not one pixel.
                    // This makes a good flow match decisive while rejecting a flash
                    // that merely happens to produce a plausible vector.
                    float samePatchError = 0.0;
                    float warpedPatchError = 0.0;
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
                                const float2 currentPatchUv = i.uv + patchOffset;
                                const float3 currentPatch = CurrentFrame.SampleLevel(
                                    LinearClamp, currentPatchUv, 0.0).rgb;
                                const float3 previousSamePatch = PreviousSource.SampleLevel(
                                    LinearClamp, currentPatchUv, 0.0).rgb;

                                const float2 patchForward = LoadOpticalFlow(
                                    ForwardOpticalFlow, currentPatchUv);
                                const float2 patchPreviousUv =
                                    currentPatchUv + patchForward / outputSize;
                                const float3 previousWarpedPatch = PreviousSource.SampleLevel(
                                    LinearClamp, patchPreviousUv, 0.0).rgb;

                                samePatchError += SourceMatchError(
                                    currentPatch, previousSamePatch);
                                warpedPatchError += SourceMatchError(
                                    currentPatch, previousWarpedPatch);
                            }
                        }
                    }
                    samePatchError /= 5.0;
                    warpedPatchError /= 5.0;

                    const float patchImprovement = samePatchError > 0.003 ?
                        saturate(1.0 - warpedPatchError / samePatchError) : 0.0;
                    const float absoluteMatch =
                        1.0 - smoothstep(0.030, 0.125, warpedPatchError);
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
                    const float transportEvidence =
                        fbConfidence * absoluteMatch *
                        smoothstep(0.10, 0.34, patchImprovement) *
                        lerp(0.35, 1.0, costConfidence);

                    // Once the warped raw patch genuinely explains the new image,
                    // make motion nearly binary. Partial gates are precisely what
                    // left 30-70% of old history visible in v9.
                    const float transportGate =
                        transportEvidence > 0.34 ? 1.0 :
                        smoothstep(0.18, 0.34, transportEvidence);

                    hardwareMotionGate = max(hardwareMotionGate, transportGate);
                    if (transportGate > localMotionGate)
                    {
                        // Flow is classification evidence only. Never spatially warp
                        // displayed history: one bad vector should not bend geometry.
                        localMotionGate = transportGate;
                    }
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

                    const float3 previousHere = PreviousSource.SampleLevel(
                        LinearClamp, i.uv, 0.0).rgb;
                    const float3 currentAtDestination = CurrentFrame.SampleLevel(
                        LinearClamp, movedToUv, 0.0).rgb;

                    const float movedObjectError =
                        SourceMatchError(previousHere, currentAtDestination);
                    const float stayedHereError =
                        SourceMatchError(previousHere, rawSourceColor);
                    const float movedImprovement = stayedHereError > 0.003 ?
                        saturate(1.0 - movedObjectError / stayedHereError) : 0.0;

                    const float vacatedAllowedRoundTrip =
                        max(1.25, 0.65 + vacatedMagnitude * 0.22);
                    const float vacatedFb =
                        1.0 - smoothstep(vacatedAllowedRoundTrip,
                                       vacatedAllowedRoundTrip + 2.0,
                                       vacatedRoundTrip);
                    const float vacatedMatch =
                        1.0 - smoothstep(0.030, 0.125, movedObjectError);
                    float vacatedCostConfidence = 1.0;
                    if (P9.w > 0.5)
                    {
                        const float flowCost = LoadOpticalCost(
                            BackwardOpticalCost, i.uv);
                        vacatedCostConfidence = 1.0 - smoothstep(0.28, 0.78, flowCost);
                    }
                    const float vacatedEvidence =
)HLSL" R"HLSL(                        vacatedFb * vacatedMatch *
                        smoothstep(0.12, 0.38, movedImprovement) *
                        lerp(0.35, 1.0, vacatedCostConfidence);

                    const float vacatedGate =
                        vacatedEvidence > 0.34 ? 1.0 :
                        smoothstep(0.18, 0.34, vacatedEvidence);

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
                // a nearby backward vector survives round-trip, raw-image and cost
                // validation. Strong accumulated flash memory disables this local
                // infill; the independently validated global-pan gate above remains.
                if (hardwareMotionGate < 0.80 && sourceDelta > 0.010 &&
                    temporalRisk < 0.70)
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

                                            const float3 previousNeighbor =
                                                PreviousSource.SampleLevel(
                                                    LinearClamp, neighborUv,
                                                    0.0).rgb;
                                            const float3 currentDestination =
                                                CurrentFrame.SampleLevel(
                                                    LinearClamp,
                                                    neighborDestination,
                                                    0.0).rgb;
                                            const float3 currentNeighbor =
                                                CurrentFrame.SampleLevel(
                                                    LinearClamp, neighborUv,
                                                    0.0).rgb;
                                            const float movedError =
                                                SourceMatchError(
                                                    previousNeighbor,
                                                    currentDestination);
                                            const float stayedError =
                                                SourceMatchError(
                                                    previousNeighbor,
                                                    currentNeighbor);
                                            const float movedImprovement =
                                                stayedError > 0.003 ?
                                                saturate(1.0 -
                                                    movedError / stayedError) :
                                                0.0;
                                            const float absoluteMatch =
                                                1.0 - smoothstep(
                                                    0.030, 0.120, movedError);
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
                                            const float evidence =
                                                fbConfidence * absoluteMatch *
                                                smoothstep(
                                                    0.32, 0.72,
                                                    movedImprovement) *
                                                lerp(0.25, 1.0,
                                                    costConfidence) *
                                                sweptSupport;
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
                        hardwareMotionGate = max(
                            hardwareMotionGate, infillGate);
                        localMotionGate = max(
                            localMotionGate, infillGate);
                    }
                }
            }

            // Fresh NVOFA gets first chance, but bright/flat surfaces can leave
            // weak edge/disocclusion evidence. Let the raw-image matcher verify
            // low-confidence pixels instead of accepting a partial motion gate.
            if ((!hardwareFlowValid || localMotionGate < 0.80) &&
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
                        localMotionGate = 1.0;
                    }
                    else
                    {
                        const float denseAbsoluteMatch = 1.0 - smoothstep(
                            0.035, 0.120, denseMeanError);
                        const float denseGate = smoothstep(
                            0.24, 0.60, denseImprovement) * denseAbsoluteMatch;
                        localMotionGate = max(localMotionGate, denseGate);
                    }
                }
            }
        }
)HLSL"
