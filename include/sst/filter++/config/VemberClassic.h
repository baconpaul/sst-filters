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

#ifndef INCLUDE_SST_FILTERPLUSPLUS_CONFIG_VEMBERCLASSIC_H
#define INCLUDE_SST_FILTERPLUSPLUS_CONFIG_VEMBERCLASSIC_H

#include "sst/filter++/features.h"
#include "sst/filters.h"

#include <cassert>
#include <sst/filter++/api.h>

namespace sst::filterplusplus
{
template <size_t blockSize> struct FilterConfig<FilterModels::VemberClassic, blockSize>
{
    static constexpr bool hasPassTypes{true};
    enum PassTypes
    {
        LP,
        HP,
        BP,
        Notch
    };

    static constexpr bool hasSlope{true};
    enum Slopes
    {
        Slope_12db,
        Slope_24db
    };

    static constexpr bool hasDrive{true};
    enum Drives
    {
        Standard,
        Driven, // only in bp lp hp
        Clean,  // only in bp lp hp

        Mild, // only in notch mode
    };

    static constexpr bool hasBasicQuadFilterAPI{true};

    friend class Filter<FilterModels::VemberClassic, blockSize>;
};

template <size_t blockSize> struct FilterPreparation<FilterModels::VemberClassic, blockSize>
{
    static constexpr FilterModels ft = FilterModels::VemberClassic;
    using filter_t = Filter<ft, blockSize>;
    [[nodiscard]] static bool prepareInstance(filter_t &f)
    {
        f.initQF();
        if (f.passType == filter_t::passTypes_t::Notch &&
            !(f.drive == filter_t::drives_t::Mild || f.drive == filter_t::drives_t::Standard))
        {
            return false;
        }

        bool is12 = f.slope == filter_t::config_t::Slope_12db;
        switch (f.passType)
        {
        case filter_t::config_t::LP:
            f.setQFType(is12 ? sst::filters::FilterType::fut_lp12
                             : sst::filters::FilterType::fut_lp24);
            break;
        case filter_t::config_t::HP:
            f.setQFType(is12 ? sst::filters::FilterType::fut_hp12
                             : sst::filters::FilterType::fut_hp24);
            break;
        case filter_t::config_t::BP:
            f.setQFType(is12 ? sst::filters::FilterType::fut_bp12
                             : sst::filters::FilterType::fut_bp24);
            break;
        case filter_t::config_t::Notch:
            f.setQFType(is12 ? sst::filters::FilterType::fut_notch12
                             : sst::filters::FilterType::fut_notch24);
            break;
        }

        switch (f.drive)
        {
        case filter_t::drives_t::Standard:
            f.setQFSubType(sst::filters::FilterSubType::st_Standard); // which is same val as notch
            break;
        case filter_t::drives_t::Driven:
            f.setQFSubType(sst::filters::FilterSubType::st_Driven);
            break;
        case filter_t::drives_t::Clean:
            f.setQFSubType(sst::filters::FilterSubType::st_Clean);
            break;
        case filter_t::drives_t::Mild:
            f.setQFSubType(sst::filters::FilterSubType::st_NotchMild);
            break;
        }

        for (auto &maker : f.makers)
        {
            maker.setSampleRateAndBlockSize(f.sampleRate, blockSize);
        }

        return f.resolveQFPtr();
    }
    static void prepareBlock(filter_t &f)
    {
        f.quadFilterUnitState.sampleRate = f.sampleRate;
        f.quadFilterUnitState.sampleRateInv = 1.0 / f.sampleRate; // TODO CACHE
        for (int i = 0; i < nVoices; ++i)
        {
            f.quadFilterUnitState.active[i] = f.active[i];
            f.quadFilterUnitState.DB[i] = nullptr;

            if (f.active[i])
                f.setupCoefficients(i, f.cutoff[i], f.resonance[i]);
        }
    }

    static void completeBlock(filter_t &f)
    {
        // bring the state back
        for (int i = 0; i < nVoices; ++i)
        {
            if (f.active[i])
                f.makers[i].template updateState(f.quadFilterUnitState, i);
        }
    }
    // we need to split reset and init
    static void reset(filter_t &f) { f.resetQF(); }
};

template <size_t blockSize> struct FilterSampleProcessor<FilterModels::VemberClassic, blockSize>
{
    static void processSingle(Filter<FilterModels::VemberClassic, blockSize> &f, const SIMD_M128 in,
                              SIMD_M128 &out)
    {
        assert(f.filterFunc);
        out = f.filterFunc(&f.quadFilterUnitState, in);
    }
};

} // namespace sst::filterplusplus
#endif // VEMBERCLASSIC_H
