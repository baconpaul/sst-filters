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

#ifndef INCLUDE_SST_FILTERPLUSPLUS_DETAILS_FEATURES_IMPL_H
#define INCLUDE_SST_FILTERPLUSPLUS_DETAILS_FEATURES_IMPL_H
#include <TestUtils.h>

namespace sst::filterplusplus::features
{
template <FilterModels ft, size_t blockSize>
void WithBasicQuadFilterAPI<ft, blockSize, true>::setupCoefficients(int voice, float cu, float res)
{
    // makers[voice].MakeCoeffs()
}
} // namespace sst::filterplusplus::features

#endif // FEATURES_IMPL_H
