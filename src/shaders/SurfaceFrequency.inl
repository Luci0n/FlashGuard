R"SURFACE(
// Experimental surface-owned frequency detector. Linear RGB features, bounded
// Census correspondence, discrete (never interpolated) transported state, and
// current-source-only attenuation. No optical flow API or displayed RGB history.
Texture2D<float4> Source : register(t0);
Texture2D<float4> Features : register(t1);
Texture2D<float4> OldFeatures : register(t2);
Texture2D<float4> Phase : register(t3);
Texture2D<float4> Envelope : register(t4);
Texture2D<float4> Track : register(t5);
Texture2D<float4> Overlay : register(t6);
Texture2D<uint> Activity : register(t7);
Texture2D<float4> ColorFloor : register(t8);
RWTexture2D<uint> ActivityOut : register(u0);
SamplerState ClampSampler : register(s0);
cbuffer Parameters : register(b0)
{
    float4 Grid; // grid width/height, dt seconds, history valid
    float4 Image; // source width/height, static tone map, idle (no observation)
    float4 Limits; // static linear floor/ceiling, frequency min/max
    float4 Display; // debug overlay enabled, overlay width/height, unused
};
struct Vertex { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
Vertex VS(uint id : SV_VertexID)
{
    Vertex v;
    v.uv = float2((id << 1) & 2, id & 2);
    v.position = float4(v.uv.x * 2 - 1, 1 - v.uv.y * 2, 0, 1);
    return v;
}
float3 Linear(float3 c)
{
    return lerp(c / 12.92, pow((c + .055) / 1.055, 2.4), step(.04045, c));
}
float3 Srgb(float3 c)
{
    c = max(c, 0);
    return lerp(c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - .055, step(.0031308, c));
}
float Y(float3 c) { return dot(c, float3(.2126, .7152, .0722)); }
groupshared uint changed;
[numthreads(8, 8, 1)]
void ActivityCS(uint3 id : SV_DispatchThreadID, uint local : SV_GroupIndex)
{
    if (local == 0) changed = 0;
    GroupMemoryBarrierWithGroupSync();
    if (all(id.xy < (uint2)Grid.xy))
    {
        float3 delta = abs(Srgb(Features.Load(int3(id.xy, 0)).rgb) -
            Srgb(OldFeatures.Load(int3(id.xy, 0)).rgb));
        if (max(delta.r, max(delta.g, delta.b)) > 2.5 / 255.0)
            InterlockedOr(changed, 1);
    }
    GroupMemoryBarrierWithGroupSync();
    if (local == 0 && changed != 0) InterlockedOr(ActivityOut[uint2(0, 0)], 1);
}
int2 Cell(int2 p) { return clamp(p, int2(0, 0), int2(Grid.xy) - 1); }
static const int2 Ring[8] = {
    int2(-1,-1),int2(0,-1),int2(1,-1),int2(-1,0),
    int2(1,0),int2(-1,1),int2(0,1),int2(1,1)
};
float4 FeaturePS(Vertex v) : SV_TARGET
{
    float3 rgb = Linear(Source.SampleLevel(ClampSampler, v.uv, 0).rgb);
    float center = Y(rgb);
    uint descriptor = 0;
    // Wider rings retain silhouette evidence for otherwise textureless objects.
    [unroll] for (int r = 0; r < 3; ++r)
    {
        float radius = r == 0 ? 1 : (r == 1 ? 4 : 12);
        [unroll] for (int k = 0; k < 4; ++k)
        {
            float other = Y(Linear(Source.SampleLevel(ClampSampler,
                v.uv + Ring[k * 2] * radius / Grid.xy, 0).rgb));
            uint relation = other > center + .001 ? 1u : (other < center - .001 ? 2u : 0u);
            descriptor |= relation << ((r * 4 + k) * 2);
        }
    }
    // Store integer bits numerically: descriptor fits exactly in float32.
    return float4(rgb, (float)descriptor);
}
float MatchCost(float4 now, int2 candidate, int2 origin)
{
    float4 old = OldFeatures.Load(int3(Cell(candidate), 0));
    uint descriptor = (uint)old.a;
    uint inverted = ((descriptor & 0x555555u) << 1) | ((descriptor & 0xAAAAAAu) >> 1);
    uint hamming = min(countbits((uint)now.a ^ descriptor), countbits((uint)now.a ^ inverted));
    // Contrast inversion is permitted for geometry, never for flash amplitude.
    float structural = hamming / 24.0;
    float photo = min(length(now.rgb - old.rgb), .5);
    return structural + .16 * photo + .0015 * length(float2(candidate - origin));
}
struct StateOutput
{
    float4 phase : SV_TARGET0; // positive age, negative age, period, evidence
    float4 envelope : SV_TARGET1; // trough, peak, hold seconds, packed source amplitudes
    float4 track : SV_TARGET2; // backward displacement xy, edge age, last sign
    float4 surface : SV_TARGET3; // current RGB and visible/temporarily dormant descriptor
    float4 colorFloor : SV_TARGET4; // per-channel phase trough; never displayed history
};
StateOutput StatePS(Vertex v)
{
    int2 p = Cell(int2(v.position.xy));
    float4 now = Features.Load(int3(p, 0));
    float luma = Y(now.rgb);
    StateOutput o;
    o.phase = float4(2, 2, 0, 0);
    o.envelope = float4(luma, luma, 0, 0);
    o.track = float4(0, 0, 2, 0);
    o.surface = now;
    o.colorFloor = float4(now.rgb, 0);
    if (Grid.w < .5) return o;
    if (Image.w > .5)
    {
        // An idle redraw advances time only. In particular, a dormant moving
        // hypothesis must not move again when no new source frame was observed.
        o.phase = Phase.Load(int3(p, 0));
        o.envelope = Envelope.Load(int3(p, 0));
        o.track = Track.Load(int3(p, 0));
        o.surface = OldFeatures.Load(int3(p, 0));
        o.colorFloor = ColorFloor.Load(int3(p, 0));
        o.phase.xy = min(o.phase.xy + Grid.z, 2.0);
        o.track.z = min(o.track.z + Grid.z, 2.0);
        o.envelope.z = max(0, o.envelope.z - Grid.z);
        if (o.track.z > max(.25, o.phase.z * 1.8)) o.phase.zw = 0;
        return o;
    }

    float2 velocity = Track.Load(int3(p, 0)).xy;
    float3 previousColor = OldFeatures.Load(int3(p, 0)).rgb;
    float3 localCodeChange = abs(Srgb(now.rgb) - Srgb(previousColor));
    // Measure actual source changes, never changes invented by correspondence.
    // Independent medians of luma-code and RGB-code amplitude reject isolated
    // render noise while retaining coherent weak flashes and color alternation.
    float2 a0 = 2, a1 = 2, a2 = 2, a3 = 2, a4 = 2;
    [unroll] for (int n = 0; n < 9; ++n)
    {
        int2 q = n == 0 ? p : Cell(p + Ring[max(0, n - 1)] * 2);
        float3 current = Features.Load(int3(q, 0)).rgb;
        float3 previous = OldFeatures.Load(int3(q, 0)).rgb;
        float3 codeDelta = abs(Srgb(current) - Srgb(previous));
        float2 value = float2(abs(Srgb(Y(current).xxx).x - Srgb(Y(previous).xxx).x),
            max(codeDelta.r, max(codeDelta.g, codeDelta.b)));
        float2 next = max(a0, value); a0 = min(a0, value); value = next;
        next = max(a1, value); a1 = min(a1, value); value = next;
        next = max(a2, value); a2 = min(a2, value); value = next;
        next = max(a3, value); a3 = min(a3, value); value = next;
        a4 = min(a4, value);
    }
    bool unchanged = Activity.Load(int3(0, 0, 0)) == 0 ||
        max(localCodeChange.r, max(localCodeChange.g, localCodeChange.b)) <= 2.5 / 255.0;
    if (dot(velocity, velocity) < .5 && Image.w < .5)
    {
        // Carry boundary motion into a flat interior of the same appearance.
        // This transports state; it does not spatially warp displayed pixels.
        float2 sum = 0;
        float votes = 0;
        [unroll] for (int n = 0; n < 8; ++n)
        {
            int2 neighbor = Cell(p + Ring[n] * 12);
            float2 flow = Track.Load(int3(neighbor, 0)).xy;
            if (dot(flow, flow) >= 1 &&
                length(OldFeatures.Load(int3(neighbor, 0)).rgb - previousColor) < .012)
            {
                sum += flow;
                votes += 1;
            }
        }
        if (votes >= 2) velocity = round(sum / votes);
    }
    int2 predecessor = p;
    float sameCost = MatchCost(now, p, p);
    float bestCost = sameCost;
    if (sameCost > .003)
    {
        int2 prediction = Cell(p + int2(round(velocity)));
        float predictionCost = MatchCost(now, prediction, p);
        if (predictionCost < bestCost) { bestCost = predictionCost; predecessor = prediction; }
        // Bounded 4-level search: maximum 30 source pixels at grid scale 2.
        [loop] for (int stride = 8; stride >= 1; stride /= 2)
        {
            int2 anchor = predecessor;
            [unroll] for (int k = 0; k < 8; ++k)
            {
                int2 candidate = Cell(anchor + Ring[k] * stride);
                float cost = MatchCost(now, candidate, p);
                if (cost < bestCost) { bestCost = cost; predecessor = candidate; }
            }
        }
    }
    // A tiny improvement is ambiguous: do not spread memory to a random match.
    if (sameCost - bestCost < .008) predecessor = p;
    // A dark phase may become indistinguishable from the background. Keep a
    // bounded dormant hypothesis at the predicted surface location. It carries
    // frequency evidence, not permission to brighten or paint that background.
    int2 predicted = Cell(p + int2(round(velocity)));
    float4 predictedEnvelope = Envelope.Load(int3(predicted, 0));
    bool dormant = predictedEnvelope.z > 0 &&
        predictedEnvelope.y - predictedEnvelope.x > .01 &&
        abs(luma - predictedEnvelope.x) < .009;
    if (dormant) predecessor = predicted;
    else if (any(predicted != p) &&
        length(OldFeatures.Load(int3(predicted, 0)).rgb - now.rgb) < .006 &&
        MatchCost(now, predicted, p) < .04)
        predecessor = predicted;
    float4 old = OldFeatures.Load(int3(predecessor, 0));
    float oldLuma = Y(old.rgb);
    float4 phase = Phase.Load(int3(predecessor, 0));
    float4 envelope = Envelope.Load(int3(predecessor, 0));
    float localAmplitude = max(localCodeChange.r, max(localCodeChange.g, localCodeChange.b));
    // A moving boundary can have too few changed neighbors for a median.
    // Retain its transported budget, but clear that budget for small jitter.
    if (a4.y > 2.5 / 255.0 || (localAmplitude > 2.5 / 255.0 && localAmplitude < .06))
    {
        uint2 amplitude = (uint2)round(saturate(a4.y > 2.5 / 255.0 ? a4 : 0) * 4095);
        envelope.w = (float)(amplitude.x | (amplitude.y << 12));
    }
    float4 track = Track.Load(int3(predecessor, 0));
    float3 colorFloor = ColorFloor.Load(int3(predecessor, 0)).rgb;
    float dt = Grid.z;
    bool geometry = dormant || MatchCost(now, predecessor, p) < .22;
    bool moving = any(predecessor != p);
    float delta = luma - oldLuma;
    float chromaDelta = length((now.rgb - luma) - (old.rgb - oldLuma));
    // Qualified two-phase appearance rejects an unrelated exposed background.
    bool knownPhase = min(abs(luma - envelope.x), abs(luma - envelope.y)) <
        max(.018, (envelope.y - envelope.x) * .15);
    phase.xy = min(phase.xy + dt, 2.0);
    track.z = min(track.z + dt, 2.0);
    envelope.z = max(0, envelope.z - dt);
    float3 residualCode = abs(Srgb(now.rgb) - Srgb(old.rgb));
    bool significant = max(residualCode.r, max(residualCode.g, residualCode.b)) > 2.5 / 255.0;
    // Correspondence may continue through an invisible moving phase, but a
    // held source cannot manufacture new edges from transported colors.
    bool event = !unchanged && significant;
    if (event && !dormant)
    {
        float agreeing = 0;
        [unroll] for (int n = 0; n < 8; ++n)
        {
            float3 a = Features.Load(int3(Cell(p + Ring[n] * 2), 0)).rgb;
            float3 b = OldFeatures.Load(int3(Cell(predecessor + Ring[n] * 2), 0)).rgb;
            float residual = Y(a) - Y(b);
            bool luminanceAgrees = residual * delta > 0 && abs(residual) > .001;
            bool chromaAgrees = dot(Srgb(a) - Srgb(b),
                Srgb(now.rgb) - Srgb(old.rgb)) > .0001;
            agreeing += (luminanceAgrees || chromaAgrees) ? 1 : 0;
        }
        event = agreeing >= 6;
    }
    // A coherent intrinsic color change may introduce a new phase. Keep the
    // established frequency and extend its envelope instead of losing the guard.
    bool ownership = unchanged || (geometry &&
        (dormant || phase.w < 1 || moving || knownPhase || abs(delta) < .002 || event));
    if (!ownership) return o;
    // Use the strongest encoded channel, including blue and dark colors.
    // Reversing any RGB pair reverses this sign without a preferred hue axis.
    float3 colorDelta = Srgb(now.rgb) - Srgb(old.rgb);
    float3 magnitude = abs(colorDelta);
    float signedDelta = magnitude.r >= magnitude.g && magnitude.r >= magnitude.b ? colorDelta.r :
        (magnitude.g >= magnitude.b ? colorDelta.g : colorDelta.b);
    float direction = signedDelta >= 0 ? 1 : -1;
    bool edge = event && direction != track.w;
    if (event)
        colorFloor = envelope.z > 0 ? min(colorFloor, min(now.rgb, old.rgb)) : min(now.rgb, old.rgb);
    if (edge)
    {
        float period = direction > 0 ? phase.x : phase.y;
        bool consistent = phase.z <= 0 || abs(period - phase.z) <= max(2 * dt, phase.z * .22);
        // Average complete periods before applying the band. A full-frame
        // tolerance would incorrectly admit 40 Hz at 120 captured FPS.
        float candidatePeriod = phase.z > 0 && consistent ? lerp(phase.z, period, .25) : period;
        float tolerance = min(dt * .1, .0015);
        bool inBand = candidatePeriod >= 1.0 / Limits.w - tolerance &&
            candidatePeriod <= 1.0 / Limits.z + tolerance;
        if (inBand && consistent)
        {
            phase.z = candidatePeriod;
            phase.w = min(3, phase.w + 1);
        }
        else if (period < 1.0)
        {
            phase.z = candidatePeriod;
            phase.w = 0;
        }
        if (direction > 0) phase.x = 0; else phase.y = 0;
        track.z = 0;
        track.w = direction;
        envelope.x = min(envelope.x, min(luma, oldLuma));
        envelope.y = max(envelope.y, max(luma, oldLuma));
        if (phase.w >= 1)
            envelope.z = max(.10, phase.z * 1.6);
        else if (a4.y >= .025)
            envelope.z = max(envelope.z, .4); // retain a candidate across two 5 Hz periods
    }
    if (track.z > max(.25, phase.z * 1.8))
    {
        phase.zw = 0;
        // The envelope releases through the output gain, not old displayed RGB.
        if (envelope.z <= 0) envelope.xy = luma.xx;
    }
    o.phase = phase;
    o.envelope = envelope;
    o.colorFloor = float4(colorFloor, 0);
    o.track = float4(float2(predecessor - p), track.zw);
    if (dormant)
    {
        o.surface.a = old.a;
        o.track.xy = velocity;
    }
    return o;
}
float4 CompositePS(Vertex v) : SV_TARGET
{
    float3 rgb = Linear(Source.SampleLevel(ClampSampler, v.uv, 0).rgb);
    float luma = Y(rgb);
    int2 base = int2(floor(v.uv * Grid.xy - .5));
    int2 chosen = Cell(base);
    float best = 100;
    // Color-guided nearest owner, never bilinear state or a dilated risk mask.
    [unroll] for (int y = 0; y <= 1; ++y)
    [unroll] for (int x = 0; x <= 1; ++x)
    {
        int2 p = Cell(base + int2(x, y));
        float3 feature = Features.Load(int3(p, 0)).rgb;
        float cost = length(rgb - feature) +
            .0005 * length(v.uv * Grid.xy - (float2(p) + .5));
        if (cost < best) { best = cost; chosen = p; }
    }
    float4 envelope = Envelope.Load(int3(chosen, 0));
    uint packedAmplitude = (uint)round(envelope.w);
    float2 amplitude = float2(packedAmplitude & 4095, packedAmplitude >> 12) / 4095.0;
    float3 baseline = rgb;
    float membership = 1 - smoothstep(.01, .04, best);
    float strength = membership * smoothstep(0, .05, envelope.z);
    // Clip every RGB channel to its observed low phase. Grayscale projection
    // can leave hue/saturation modulation after the source-amplitude clamp.
    // This uses raw phase colors, never previously displayed output pixels.
    float3 floorColor = ColorFloor.Load(int3(chosen, 0)).rgb;
    rgb = lerp(rgb, min(rgb, floorColor), strength);
    if (Image.z > .5)
    {
        float y = Y(rgb);
        rgb = lerp(Limits.x, Limits.y, y) + (rgb - y) * (Limits.y - Limits.x);
        float baselineY = Y(baseline);
        baseline = lerp(Limits.x, Limits.y, baselineY) +
            (baseline - baselineY) * (Limits.y - Limits.x);
    }
    // Bound chroma correction too: tiny source jitter must never turn into
    // high-contrast speckles, even when correspondence chooses a wrong owner.
    baseline = saturate(Srgb(baseline));
    float3 result = clamp(saturate(Srgb(rgb)),
        max(0, baseline - amplitude.y), min(1, baseline + amplitude.y));
    if (Display.x > .5)
    {
        float2 uv = v.uv * Image.xy / Display.yz;
        if (all(uv <= 1))
        {
            float4 overlay = Overlay.SampleLevel(ClampSampler, uv, 0);
            result = lerp(result, overlay.rgb, overlay.a);
        }
    }
    return float4(result, 1);
}
)SURFACE"
