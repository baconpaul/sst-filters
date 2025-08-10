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

namespace sst::filterplusplus
{
template <> struct FilterConfig<FilterTypes::VemberClassic>
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

    friend class Filter<FilterTypes::VemberClassic>;
};

template <> bool Filter<FilterTypes::VemberClassic>::prepareInstance()
{

    if (passType == passTypes_t::Notch && !(drive == drives_t::Mild || drive == drives_t::Standard))
    {
        return false;
    }

    bool is12 = slope == config_t::Slope_12db;
    switch (passType)
    {
    case config_t::LP:
        setQFType(is12 ? sst::filters::FilterType::fut_lp12 : sst::filters::FilterType::fut_lp24);
        break;
    case config_t::HP:
        setQFType(is12 ? sst::filters::FilterType::fut_hp12 : sst::filters::FilterType::fut_hp24);
        break;
    case config_t::BP:
        setQFType(is12 ? sst::filters::FilterType::fut_bp12 : sst::filters::FilterType::fut_bp24);
        break;
    case config_t::Notch:
        setQFType(is12 ? sst::filters::FilterType::fut_notch12
                       : sst::filters::FilterType::fut_notch24);
        break;
    }

    switch (drive)
    {
    case drives_t::Standard:
        setQFSubType(sst::filters::FilterSubType::st_Standard); // which is same val as notch
        break;
    case drives_t::Driven:
        setQFSubType(sst::filters::FilterSubType::st_Driven);
        break;
    case drives_t::Clean:
        setQFSubType(sst::filters::FilterSubType::st_Clean);
        break;
    case drives_t::Mild:
        setQFSubType(sst::filters::FilterSubType::st_NotchMild);
        break;
    }

    return getQFPtr();
}

template <> void Filter<FilterTypes::VemberClassic>::prepareBlock()
{
    for (int i = 0; i < nVoices; ++i)
    {
        if (true) // active[i]
            setupCoefficients(i, cutoff[i], resonance[i]);
    }
}

template <> void Filter<FilterTypes::VemberClassic>::reset() { initQF(); }
} // namespace sst::filterplusplus
#endif // VEMBERCLASSIC_H
