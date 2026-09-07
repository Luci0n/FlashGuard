                // Translation matching suppresses coherent pans without suppressing
                // large exposure changes or confirmed future reversals. First find
                // the best whole-cell shift, then refine it to sub-cell precision.
                // This is important for slow scrolling: at 1080p one analysis cell
                // is about 15 screen pixels, so a few pixels of motion is fractional.
                constexpr int motionMarginX = 6;
                constexpr int motionMarginY = 10;
                const int analysisW = static_cast<int>(kAnalysisWidth);
                const int analysisH = static_cast<int>(kAnalysisHeight);

                const auto samplePrevious = [&](float x, float y) {
                    const int x0 = static_cast<int>(std::floor(x));
                    const int y0 = static_cast<int>(std::floor(y));
                    const int x1 = std::min(x0 + 1, analysisW - 1);
                    const int y1 = std::min(y0 + 1, analysisH - 1);
                    const float tx = x - static_cast<float>(x0);
                    const float ty = y - static_cast<float>(y0);
                    const auto at = [&](int sx, int sy) {
                        return m_prevAnalysis[static_cast<size_t>(sy) * kAnalysisWidth + sx];
                    };
                    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
                    const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
                    return top + (bottom - top) * ty;
                };

                const auto integerShiftError = [&](int shiftX, int shiftY) {
                    float error = 0.0f;
                    uint32_t samples = 0;
                    for (int y = motionMarginY; y < analysisH - motionMarginY; ++y)
                    for (int x = motionMarginX; x < analysisW - motionMarginX; ++x)
                    {
                        const size_t cur = static_cast<size_t>(y) * kAnalysisWidth + x;
                        const size_t prev = static_cast<size_t>(y + shiftY) * kAnalysisWidth +
                            static_cast<size_t>(x + shiftX);
                        error += std::fabs(m_currentAnalysis[cur] - m_prevAnalysis[prev]);
                        ++samples;
                    }
                    return samples ? error / static_cast<float>(samples) : 0.0f;
                };

                const auto fractionalShiftError = [&](float shiftX, float shiftY) {
                    float error = 0.0f;
                    uint32_t samples = 0;
                    for (int y = motionMarginY; y < analysisH - motionMarginY; ++y)
                    for (int x = motionMarginX; x < analysisW - motionMarginX; ++x)
                    {
                        const size_t cur = static_cast<size_t>(y) * kAnalysisWidth + x;
                        const float previous = samplePrevious(
                            static_cast<float>(x) + shiftX,
                            static_cast<float>(y) + shiftY);
                        error += std::fabs(m_currentAnalysis[cur] - previous);
                        ++samples;
                    }
                    return samples ? error / static_cast<float>(samples) : 0.0f;
                };

                const float sameError = integerShiftError(0, 0);
                float bestShiftError = sameError;
                float bestShiftX = 0.0f;
                float bestShiftY = 0.0f;

                // Preserve the original search range for large motion. Keep this
                // stage on direct texel loads; only the small refinement is bilinear.
                for (int sy = -8; sy <= 8; ++sy)
                for (int sx = -4; sx <= 4; ++sx)
                {
                    if (sx == 0 && sy == 0) continue;
                    const float error = integerShiftError(sx, sy);
                    if (error < bestShiftError)
                    {
                        bestShiftError = error;
                        bestShiftX = static_cast<float>(sx);
                        bestShiftY = static_cast<float>(sy);
                    }
                }

                // Hierarchical fractional refinement adds only 24 candidates while
                // resolving motion down to 1/8 of an analysis cell.
                constexpr float refineSteps[] = { 0.5f, 0.25f, 0.125f };
                for (float step : refineSteps)
                {
                    float refinedError = bestShiftError;
                    float refinedX = bestShiftX;
                    float refinedY = bestShiftY;
                    for (int oy = -1; oy <= 1; ++oy)
                    for (int ox = -1; ox <= 1; ++ox)
                    {
                        if (ox == 0 && oy == 0) continue;
                        const float candidateX = bestShiftX + static_cast<float>(ox) * step;
                        const float candidateY = bestShiftY + static_cast<float>(oy) * step;
                        if (std::fabs(candidateX) > 4.875f || std::fabs(candidateY) > 8.875f)
                            continue;
                        const float error = fractionalShiftError(candidateX, candidateY);
                        if (error < refinedError)
                        {
                            refinedError = error;
                            refinedX = candidateX;
                            refinedY = candidateY;
                        }
                    }
                    bestShiftError = refinedError;
                    bestShiftX = refinedX;
                    bestShiftY = refinedY;
                }

                if (sameError > 0.001f)
                {
                    const float rawCameraMotionScore = std::clamp(
                        1.0f - bestShiftError / sameError, 0.0f, 1.0f);
                    stats.cameraMotionScore = rawCameraMotionScore;

                    // Validate the luminance-derived shift against spatial
                    // structure. Gradient magnitude ignores contrast polarity
                    // and additive brightness changes, so a stationary flash
                    // cannot earn camera authority merely because shifting its
                    // bright/dark boundary reduces raw photometric error.
                    const auto structuralShiftError =
                        [&](float shiftX, float shiftY) {
                        float error = 0.0f;
                        uint32_t samples = 0;
                        for (int y = motionMarginY + 1;
                             y < analysisH - motionMarginY - 1; ++y)
                        for (int x = motionMarginX + 1;
                             x < analysisW - motionMarginX - 1; ++x)
                        {
                            const auto currentAt = [&](int sx, int sy) {
                                return m_currentAnalysis[
                                    static_cast<size_t>(sy) * kAnalysisWidth + sx];
                            };
                            const float currentGx =
                                currentAt(x + 1, y) - currentAt(x - 1, y);
                            const float currentGy =
                                currentAt(x, y + 1) - currentAt(x, y - 1);
                            const float previousGx =
                                samplePrevious(x + shiftX + 1.0f, y + shiftY) -
                                samplePrevious(x + shiftX - 1.0f, y + shiftY);
                            const float previousGy =
                                samplePrevious(x + shiftX, y + shiftY + 1.0f) -
                                samplePrevious(x + shiftX, y + shiftY - 1.0f);
                            const float currentMagnitude = std::sqrt(
                                currentGx * currentGx + currentGy * currentGy);
                            const float previousMagnitude = std::sqrt(
                                previousGx * previousGx + previousGy * previousGy);
                            error += std::fabs(currentMagnitude - previousMagnitude);
                            ++samples;
                        }
                        return samples ? error / static_cast<float>(samples) : 0.0f;
                    };
                    const float sameStructuralError = structuralShiftError(0.0f, 0.0f);
                    if (sameStructuralError > 0.0005f)
                    {
                        const float shiftedStructuralError =
                            structuralShiftError(bestShiftX, bestShiftY);
                        const float structuralImprovement = std::clamp(
                            1.0f - shiftedStructuralError / sameStructuralError,
                            0.0f, 1.0f);
                        stats.structuralCameraMotionScore =
                            rawCameraMotionScore * structuralImprovement;
                    }
                }
