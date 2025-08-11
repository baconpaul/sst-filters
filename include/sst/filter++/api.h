/*
 * sst-filters - A header-only collection of SIMD filter
 * implementations by the Surge Synth Team
 *
 * Copyright 2019-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * sst-filters is released under the Gnu General Public Licens
 * version 3 or later. Some of the filters in this package
 * originated in the version of Surge open sourced in 2018.
 *
 * All source in sst-filters available at
 * https://github.com/surge-synthesizer/sst-filters
 */

#ifndef INCLUDE_SST_FILTERPLUSPLUS_API_H
#define INCLUDE_SST_FILTERPLUSPLUS_API_H

#include "sst/basic-blocks/simd/setup.h"

namespace sst::filterplusplus
{

static constexpr int nVoices{4}; // evertyhing is 4 way simd

/*
 * Filter++ refactors the surge filter API into more useful objects and does
 * it without virtual functions etc.... by re-grouping as a set of objects by
 * filter type. To add a filter type, add it to this enum, set up a set of properties
 * in the etc...
 */
enum FilterModels
{
    VemberClassic,
    Vintage,
    OBXf
};

template <FilterModels ft, size_t blockSize>
struct FilterConfig // needs block size so it can firend filter
{
};
} // namespace sst::filterplusplus

#include "features.h"

namespace sst::filterplusplus
{

template <FilterModels ft, size_t blockSize> struct FilterPreparation;
template <FilterModels ft, size_t blockSize> struct FilterSampleProcessor;

template <FilterModels ft, size_t bs>
struct Filter : features::AddSlope<ft, bs>,
                features::AddPassTypes<ft, bs>,
                features::AddDrive<ft, bs>,
                features::AddBasicQuadFilterAPI<ft, bs>
{
    static constexpr size_t blockSize{bs};
    using config_t = FilterConfig<ft, blockSize>;
    // The minimum api is
    void setSampleRate(double sr)
    {
        sampleRate = sr;
        sampleRateInv = 1.0 / sr;
    }

    [[nodiscard]] bool prepareInstance();

    // At head of block call this and any other relevant setters from superclasses
    void setVoiceActive(int voice, bool isActive) { active[voice] = isActive ? 0xFFFFFFFF : 0; }

    void setCutoffAndResonance(int voice, float co, float res)
    {
        cutoff[voice] = co;
        resonance[voice] = res;
    }

    void reset();

    void prepareBlock();

    // Then to calculate the block call one of these
    void processBlock(float *in, float *out)
    {
        for (int i = 0; i < blockSize; i++)
        {
            // processSingle(SIMD_M128::load(in + i * 2), SIMD_M128::load(out + i * 2))
        }
    }
    void processBlock(float *inL, float *inR, float *outL, float *outR)
    {
        for (int i = 0; i < blockSize; i++)
        {
            // processSingle(SIMD_M128::load(in + i * 2), SIMD_M128::load(out + i * 2))
        }
    }
    void processBlock(float *in1, float *in2, float *in3, float *in4, float *out1, float *out2,
                      float *out3, float *out4)
    {
        for (int i = 0; i < blockSize; i++)
        {
            // processSingle(SIMD_M128::load(in + i * 2), SIMD_M128::load(out + i * 2))
        }
    }
    void processBlock(SIMD_M128 *in, SIMD_M128 *out)
    {
        for (int i = 0; i < blockSize; i++)
        {
            processSingle(in[i], out[i]);
        }
    }

    /**
     * It is the callers responsibility to call this exactly blockSize times between
     * the call to prepareBlock and completeBlock
     *
     * @param in
     * @param out
     */
    void processSingle(const SIMD_M128 in, SIMD_M128 &out);

    void completeBlock();

    friend class FilterPreparation<ft, blockSize>;
    friend class FilterSampleProcessor<ft, blockSize>;

  protected:
    config_t config;
    double sampleRate{1}, sampleRateInv{1};
    std::array<float, nVoices> cutoff, resonance;
    std::array<uint32_t, nVoices> active;
};

template <FilterModels ft, size_t blockSize> struct FilterPreparation
{
    [[nodiscard]] static bool prepareInstance(Filter<ft, blockSize> &)
    {
        throw std::logic_error("Not implemented FP");
    }
    static void prepareBlock(Filter<ft, blockSize> &)
    {
        throw std::logic_error("Not implemented FP");
    }
    static void completeBlock(Filter<ft, blockSize> &)
    {
        throw std::logic_error("Not implemented FP");
    }
    static void reset(Filter<ft, blockSize> &) { throw std::logic_error("Not implemented FP"); }
};

template <FilterModels ft, size_t blockSize> struct FilterSampleProcessor
{
    static void processSingle(Filter<ft, blockSize> &, const SIMD_M128 in, SIMD_M128 &out)
    {
        throw std::logic_error("Not implemented FP Process");
    }
};

template <FilterModels ft, size_t blockSize> bool Filter<ft, blockSize>::prepareInstance()
{
    return FilterPreparation<ft, blockSize>::prepareInstance(*this);
}

template <FilterModels ft, size_t blockSize> void Filter<ft, blockSize>::prepareBlock()
{
    FilterPreparation<ft, blockSize>::prepareBlock(*this);
}

template <FilterModels ft, size_t blockSize> void Filter<ft, blockSize>::reset()
{
    FilterPreparation<ft, blockSize>::reset(*this);
}

template <FilterModels ft, size_t blockSize> void Filter<ft, blockSize>::completeBlock()
{
    FilterPreparation<ft, blockSize>::completeBlock(*this);
}

template <FilterModels ft, size_t blockSize>
void Filter<ft, blockSize>::processSingle(SIMD_M128 in, SIMD_M128 &out)
{
    FilterSampleProcessor<ft, blockSize>::processSingle(*this, in, out);
}

}; // namespace sst::filterplusplus

#include "config/VemberClassic.h"

#endif // API_H
