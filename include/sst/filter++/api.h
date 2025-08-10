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

namespace sst::filterplusplus
{

static constexpr int nVoices{4}; // evertyhing is 4 way simd

/*
 * Filter++ refactors the surge filter API into more useful objects and does
 * it without virtual functions etc.... by re-grouping as a set of objects by
 * filter type. To add a filter type, add it to this enum, set up a set of properties
 * in the etc...
 */
enum FilterTypes
{
    VemberClassic,
    Vintage,
    OBXf
};

template <FilterTypes ft> struct FilterConfig
{
};
} // namespace sst::filterplusplus

#include "features.h"

namespace sst::filterplusplus
{

template <FilterTypes ft>
struct Filter : features::AddSlope<ft>,
                features::AddPassTypes<ft>,
                features::AddDrive<ft>,
                features::AddBasicQuadFilterAPI<ft>
{
    using config_t = FilterConfig<ft>;

    // The minimum api is
    void setSampleRate(double sr)
    {
        sampleRate = sr;
        sampleRateInv = 1.0 / sr;
    }
    void setBlockSize(size_t bs)
    {
        blockSize = bs;
    } // do i want this tempalte arg and if so how to do the specialization

    [[nodiscard]] bool prepareInstance() { throw std::logic_error("Not implemented"); }

    // At head of block call this and any other relevant setters from superclasses
    void setVoiceActive(int voice, bool isActive) {}

    void setCutoffAndResonance(int voice, float co, float res)
    {
        cutoff[voice] = co;
        resonance[voice] = res;
    }

    void prepareBlock() { throw std::logic_error("Not implemented"); }
    void reset() { throw std::logic_error("Not implemented"); }

    // Then to calculate the block call one of these
    void processBlock(float *in, float *out);
    void processBlock(float *inL, float *inR, float *outL, float *outR);
    // void processBlock(__m128 *in, __m128 *out);

  protected:
    config_t config;
    double sampleRate{1}, sampleRateInv{1};
    size_t blockSize{1};
    std::array<float, 4> cutoff, resonance;
};
}; // namespace sst::filterplusplus

#include "config/VemberClassic.h"

#endif // API_H
