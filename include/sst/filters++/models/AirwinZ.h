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
 * AirwinZ.h — filters++ model configuration for the AirwinZ filter family.
 *
 * The AirwinZ model exposes the four Airwindows Z-series passmodes
 * (LP / HP / BP / Notch) with a Standard drive mode.  Both drive and poles
 * are continuous parameters supplied at coefficient time:
 *   extra1 = drive  (0..1)
 *   extra2 = poles  (0..4)
 *
 * coefficientsExtraCount() returns 2 for this model.
 */

#ifndef INCLUDE_SST_FILTERS_PLUS_PLUS_MODELS_AIRWINZ_H
#define INCLUDE_SST_FILTERS_PLUS_PLUS_MODELS_AIRWINZ_H

#include "sst/filters.h"

namespace sst::filtersplusplus::models::airwinz
{
inline const details::FilterPayload::configMap_t &getModelConfigurations()
{
    namespace sft = sst::filters;

    static details::FilterPayload::configMap_t configs{
        {{Passband::LP, DriveMode::Standard},
         {sft::FilterType::fut_airwinz_lp, sft::st_airwinz_pp_continuous}},
        {{Passband::HP, DriveMode::Standard},
         {sft::FilterType::fut_airwinz_hp, sft::st_airwinz_pp_continuous}},
        {{Passband::BP, DriveMode::Standard},
         {sft::FilterType::fut_airwinz_bp, sft::st_airwinz_pp_continuous}},
        {{Passband::Notch, DriveMode::Standard},
         {sft::FilterType::fut_airwinz_notch, sft::st_airwinz_pp_continuous}},
    };
    return configs;
}
} // namespace sst::filtersplusplus::models::airwinz

#endif // INCLUDE_SST_FILTERS_PLUS_PLUS_MODELS_AIRWINZ_H
