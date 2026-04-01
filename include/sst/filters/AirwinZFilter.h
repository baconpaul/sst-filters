/*
 * sst-filters - A header-only collection of SIMD filter
 * implementations by the Surge Synth Team
 *
 * Copyright 2019-2025, various authors, as described in the GitHub
 * transaction log.
 *
 * sst-filters is released under the Gnu General Public Licens
 * version 3 or later. Some of the filters in this package
 * originated in the version of Surge open sourced in 2018.
 *
 * All source in sst-filters available at
 * https://github.com/surge-synthesizer/sst-filters
 */

/*
 * AirwinZFilter.h — SIMD (4-wide float) implementation of the Airwindows Z-series
 * biquad filter, ported from new_mono::ZFilter.
 *
 * Signal chain per sample:
 *   in → × C[5](inTrim) → clamp(±1) →
 *   [Stage A: DF2T biquad, ±1 output clamp]
 *   [Stage B..D: same, blended in by C[7](poles) ∈ [0..4]] →
 *   IIR DC-blocker → × C[6](outScale, unity-gain normalised) → out
 *
 * Differences from new_mono::ZFilter:
 *  - Float (not double) arithmetic.
 *  - No per-stage clipFactor divide (cf=1 throughout).
 *  - No fixed 15.5 kHz opamp LP biquad (saves 5 coefficients + 4 state registers).
 *  - No soft-saturation output stage.
 *  - No pre-stage trim scaling; outScale absorbs the compensation.
 *  - Poles is a runtime float (0–4) stored in C[7]; stages blend progressively.
 *
 * Coefficient layout (C[0..7]):
 *   0: a0   1: a1   2: a2   3: b1   4: b2   (DF2T biquad)
 *   5: inTrim  = (A·10)^4      pre-clamp input gain
 *   6: outScale = 1/(inTrim·G1^poles)   unity-gain output trim
 *   7: poles   ∈ [0..4]        runtime stage-blend parameter
 *
 * State layout (R[0..8]):
 *   R[2k], R[2k+1] = s1, s2 for stage k  (k = 0..3)
 *   R[8]           = iirSample (DC-blocker state)
 *
 * Subtype encoding:
 *   0–15  classic API:  poles = (subtype/4)+1, drive = {0, 0.33, 0.66, 1}[subtype%4]
 *   16–19 filters++ API: poles = subtype−15,   drive = extra1 (0..1 continuous)
 */

#ifndef INCLUDE_SST_FILTERS_AIRWINZFILTER_H
#define INCLUDE_SST_FILTERS_AIRWINZFILTER_H

#include <algorithm>
#include <cmath>

#include "QuadFilterUnit.h"
#include "FilterCoefficientMaker.h"
#include "sst/basic-blocks/dsp/FastMath.h"

namespace sst::filters::AirwinZFilter
{

enum class AZPassmode : uint32_t
{
    Low,
    High,
    Band,
    Notch
};

// ── Coefficient indices ────────────────────────────────────────────────────────
enum az_coeffs : int
{
    az_a0 = 0,
    az_a1,
    az_a2,
    az_b1,
    az_b2,
    az_inTrim,
    az_outScale,
    az_poles,
    n_az_coeff // == 8 == n_cm_coeffs
};

// ── State register indices ─────────────────────────────────────────────────────
enum az_state : int
{
    az_s0A = 0,
    az_s0B,
    az_s1A,
    az_s1B,
    az_s2A,
    az_s2B,
    az_s3A,
    az_s3B,
    az_iir,
    n_az_state // 9 ≤ 16
};

static_assert(n_az_coeff == sst::filters::n_cm_coeffs,
              "AirwinZFilter uses all 8 coefficient slots");
static_assert(n_az_state <= 16, "State registers fit in QuadFilterUnitState");

// ── Coefficient maker (called at control rate) ─────────────────────────────────
//
// poles ∈ [0..4]  — number of biquad stages (may be fractional for continuous blend)
// drive ∈ [0..1]  — saturation drive level
template <typename TuningProvider, AZPassmode mode>
inline void makeCoefficients(FilterCoefficientMaker<TuningProvider> *cm, float pitch, float reso,
                             float poles, float drive, float sampleRate, float sampleRateInv,
                             TuningProvider *provider)
{
    using FCM = FilterCoefficientMaker<TuningProvider>;

    float freqHz = 440.f * FCM::provider_note_to_pitch(provider, pitch);
    freqHz = std::clamp(freqHz, 5.f, sampleRate * 0.49f);
    float freq = freqHz * sampleRateInv; // normalised [0..0.5]

    poles = std::clamp(poles, 0.f, 4.f);
    drive = std::clamp(drive, 0.f, 1.f);

    // Q from resonance
    float r = std::clamp(reso, 0.f, 1.f);
    float Q = std::max(4.f * r * r, 0.01f);

    // Biquad via bilinear transform: K = tan(π·freq)
    const float K = sst::basic_blocks::dsp::fasttan((float)M_PI * freq);
    const float norm = 1.f / (1.f + K / Q + K * K);

    float a0, a1, a2, b1, b2;
    if constexpr (mode == AZPassmode::Low)
    {
        a0 = K * K * norm;
        a1 = 2.f * a0;
        a2 = a0;
    }
    else if constexpr (mode == AZPassmode::High)
    {
        a0 = norm;
        a1 = -2.f * norm;
        a2 = norm;
    }
    else if constexpr (mode == AZPassmode::Band)
    {
        a0 = K / Q * norm;
        a1 = 0.f;
        a2 = -a0;
    }
    else // Notch
    {
        a0 = (1.f + K * K) * norm;
        a1 = 2.f * (K * K - 1.f) * norm;
        a2 = a0;
    }
    b1 = 2.f * (K * K - 1.f) * norm;
    b2 = (1.f - K / Q + K * K) * norm;
    if constexpr (mode == AZPassmode::Notch)
    {
        b1 = a1;
    }

    // Drive → A → inTrim = (A·10)^4
    const float A = 0.1f + 0.9f * drive;
    const float it = A * 10.f;
    const float inTrimV = it * it * it * it;

    // Single-stage gain magnitude at reference frequency for unity-gain normalisation.
    // outScale = 1 / (inTrim · G1^poles)
    float refHz;
    if constexpr (mode == AZPassmode::Low)
    {
        refHz = 10.f;
    }
    else if constexpr (mode == AZPassmode::High)
    {
        refHz = std::min(18000.f, sampleRate * 0.45f);
    }
    else if constexpr (mode == AZPassmode::Band)
    {
        refHz = freqHz;
    }
    else // Notch
    {
        refHz = 10.f;
    }

    const float omega = 2.f * (float)M_PI * refHz * sampleRateInv;
    const float cosw = std::cos(omega), cos2w = std::cos(2.f * omega);
    const float sinw = std::sin(omega), sin2w = std::sin(2.f * omega);
    const float nr = a0 + a1 * cosw + a2 * cos2w;
    const float ni = -(a1 * sinw + a2 * sin2w);
    const float dr = 1.f + b1 * cosw + b2 * cos2w;
    const float di = -(b1 * sinw + b2 * sin2w);
    const float den2 = dr * dr + di * di;
    const float G1 = (den2 > 1e-20f) ? std::sqrt((nr * nr + ni * ni) / den2) : 1.f;

    // G1^poles: use exp(poles·ln(G1)) to handle fractional poles cleanly.
    const float GN = (G1 > 1e-20f) ? std::exp(poles * std::log(G1)) : 1.f;
    const float gp = inTrimV * GN;
    float outScaleV = (gp > 1e-12f) ? 1.f / gp : 0.5f;
    outScaleV = std::clamp(outScaleV, 0.f, 100.f);

    float lC[sst::filters::n_cm_coeffs]{};
    lC[az_a0] = a0;
    lC[az_a1] = a1;
    lC[az_a2] = a2;
    lC[az_b1] = b1;
    lC[az_b2] = b2;
    lC[az_inTrim] = inTrimV;
    lC[az_outScale] = outScaleV;
    lC[az_poles] = poles;

    cm->FromDirect(lC);
}

// ── Per-sample process function, templated on passmode only ───────────────────
//
// Poles are read at runtime from C[az_poles].  Stages blend progressively:
//   aWet = clamp(poles,   0,1),  bWet = clamp(poles-1, 0,1),
//   cWet = clamp(poles-2, 0,1),  dWet = clamp(poles-3, 0,1).
// All four stages run every sample so state stays warm for smooth sweeping.
template <AZPassmode mode> inline SIMD_M128 process(QuadFilterUnitState *__restrict f, SIMD_M128 in)
{
    // Advance all coefficients by their per-sample interpolation delta
    for (int i = 0; i < n_az_coeff; ++i)
        f->C[i] = SIMD_MM(add_ps)(f->C[i], f->dC[i]);

    const auto a0 = f->C[az_a0];
    const auto a1 = f->C[az_a1];
    const auto a2 = f->C[az_a2];
    const auto b1 = f->C[az_b1];
    const auto b2 = f->C[az_b2];
    const auto inTrim = f->C[az_inTrim];
    const auto outScale = f->C[az_outScale];

    const auto m1 = SIMD_MM(set1_ps)(1.f);
    const auto mn1 = SIMD_MM(set1_ps)(-1.f);
    const auto mz = SIMD_MM(setzero_ps)();

    // Poles blend weights from lane 0 of C[az_poles] (same across SIMD lanes)
    const float polesF = SIMD_MM(cvtss_f32)(f->C[az_poles]);
    const auto aW = SIMD_MM(set1_ps)(std::clamp(polesF, 0.f, 1.f));
    const auto bW = SIMD_MM(set1_ps)(std::clamp(polesF - 1.f, 0.f, 1.f));
    const auto cW = SIMD_MM(set1_ps)(std::clamp(polesF - 2.f, 0.f, 1.f));
    const auto dW = SIMD_MM(set1_ps)(std::clamp(polesF - 3.f, 0.f, 1.f));

    // iirAmount ≈ 30.429 / sampleRate
    const auto iirAmt = SIMD_MM(set1_ps)(30.429f * f->sampleRateInv);
    const auto iirComp = SIMD_MM(sub_ps)(m1, iirAmt);

    // ── Input gain and hard clip ──────────────────────────────────────────────
    in = SIMD_MM(mul_ps)(in, inTrim);
    in = SIMD_MM(max_ps)(mn1, SIMD_MM(min_ps)(m1, in));

    // ── DF2T biquad helper: clips output to ±1 ───────────────────────────────
    auto runStage = [&](SIMD_M128 x, int k) -> SIMD_M128 {
        auto &s1 = f->R[k * 2];
        auto &s2 = f->R[k * 2 + 1];
        auto out = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(x, a0), s1);
        out = SIMD_MM(max_ps)(mn1, SIMD_MM(min_ps)(m1, out));
        s1 = SIMD_MM(add_ps)(SIMD_MM(sub_ps)(SIMD_MM(mul_ps)(x, a1), SIMD_MM(mul_ps)(out, b1)), s2);
        s2 = SIMD_MM(sub_ps)(SIMD_MM(mul_ps)(x, a2), SIMD_MM(mul_ps)(out, b2));
        return out;
    };

    // lerp(a, b, t) = a + t*(b-a)
    auto lerp = [&](SIMD_M128 a, SIMD_M128 b, SIMD_M128 t) -> SIMD_M128 {
        return SIMD_MM(add_ps)(a, SIMD_MM(mul_ps)(t, SIMD_MM(sub_ps)(b, a)));
    };

    (void)mz;

    // ── Progressive stage blending ────────────────────────────────────────────
    // All 4 stages always run; wet weights blend each stage's contribution in.
    auto yA = runStage(in, 0);
    auto x1 = lerp(in, yA, aW); // after A blend

    auto yB = runStage(x1, 1);
    auto x2 = lerp(x1, yB, bW); // after B blend

    auto yC = runStage(x2, 2);
    auto x3 = lerp(x2, yC, cW); // after C blend

    auto yD = runStage(x3, 3);
    auto x4 = lerp(x3, yD, dW); // after D blend

    // ── IIR DC blocker: iir = iir·(1−α) + x·α;  out = x − iir ──────────────
    auto &iir = f->R[az_iir];
    iir = SIMD_MM(add_ps)(SIMD_MM(mul_ps)(iir, iirComp), SIMD_MM(mul_ps)(x4, iirAmt));
    x4 = SIMD_MM(sub_ps)(x4, iir);

    // ── Output gain (unity-gain normalised) ──────────────────────────────────
    return SIMD_MM(mul_ps)(x4, outScale);
}

// ── Convenience wrappers matching FilterUnitQFPtr signature ───────────────────
#define AZ_PROCESS_FN(M)                                                                           \
    inline SIMD_M128 process_##M(QuadFilterUnitState *__restrict f, SIMD_M128 in)                  \
    {                                                                                              \
        return process<AZPassmode::M>(f, in);                                                      \
    }

AZ_PROCESS_FN(Low)
AZ_PROCESS_FN(High)
AZ_PROCESS_FN(Band)
AZ_PROCESS_FN(Notch)
#undef AZ_PROCESS_FN

} // namespace sst::filters::AirwinZFilter

#endif // INCLUDE_SST_FILTERS_AIRWINZFILTER_H
