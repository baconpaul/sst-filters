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

#ifndef INCLUDE_SST_FILTERPLUSPLUS_FEATURES_H
#define INCLUDE_SST_FILTERPLUSPLUS_FEATURES_H

#include "details/features_details.h"
#include "sst/filters.h"

namespace sst::filterplusplus::features
{

DeclareFeature(PassTypes)
{
    static constexpr bool supportsPassTypes{true};

    using passTypes_t = typename FilterConfig<ft, blockSize>::PassTypes;
    void setPassType(const passTypes_t &p) { passType = p; }

  protected:
    passTypes_t passType;
};

DeclareFeature(Slope)
{
    static constexpr bool supportsSlope{true};
    using slopes_t = typename FilterConfig<ft, blockSize>::Slopes;
    void setSlope(const slopes_t &s) { slope = s; }

  protected:
    slopes_t slope;
};

DeclareFeature(Drive)
{
    static constexpr bool supportsDrive{true};
    using drives_t = typename FilterConfig<ft, blockSize>::Drives;
    void setDrive(const drives_t &d) { drive = d; }

  protected:
    drives_t drive;
};

DeclareFeature(BasicQuadFilterAPI)
{
    static constexpr bool supportsBasicQuadFilterAPI{true};

    void setQFType(sst::filters::FilterType t) { qfType = t; }
    void setQFSubType(sst::filters::FilterSubType t) { qfSubType = t; }
    void initQF()
    {
        memset(&quadFilterUnitState, 0, sizeof(quadFilterUnitState));
        filterFunc = nullptr;
    }
    [[nodiscard]] bool getQFPtr()
    {
        filterFunc = sst::filters::GetCompensatedQFPtrFilterUnit<true>(qfType, qfSubType);
        return filterFunc != nullptr;
    }

    void setupCoefficients(int voice, float cu, float res);

  protected:
    sst::filters::QuadFilterUnitState quadFilterUnitState{};
    sst::filters::FilterType qfType{};
    sst::filters::FilterSubType qfSubType{};

    sst::filters::FilterUnitQFPtr filterFunc{nullptr};
};

} // namespace sst::filterplusplus::features

#undef DeclareFeature

#include "details/features_impl.h"

#endif // FEATURES_H
