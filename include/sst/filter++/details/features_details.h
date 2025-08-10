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

#ifndef INCLUDE_SST_FILTERPLUSPLUS_DETAILS_FEATURES_DETAILS_H
#define INCLUDE_SST_FILTERPLUSPLUS_DETAILS_FEATURES_DETAILS_H

#define DeclareFeature(X)                                                                          \
    template <FilterModels ft, size_t blockSize, typename = void> struct supports##X               \
    {                                                                                              \
        static constexpr bool value = false;                                                       \
    };                                                                                             \
    template <FilterModels ft, size_t blockSize>                                                   \
    struct supports##X<ft, blockSize,                                                              \
                       std::void_t<decltype(std::declval<FilterConfig<ft, blockSize>>().has##X)>>  \
    {                                                                                              \
        static constexpr bool value = FilterConfig<ft, blockSize>::has##X;                         \
    };                                                                                             \
    template <FilterModels ft, size_t blockSize>                                                   \
    static constexpr bool supports##X##_v = supports##X<ft, blockSize>::value;                     \
    template <FilterModels ft, size_t blockSize, bool b> struct With##X                            \
    {                                                                                              \
        static constexpr bool supports##X{false};                                                  \
    };                                                                                             \
                                                                                                   \
    template <FilterModels ft, size_t blockSize>                                                   \
    using Add##X = With##X<ft, blockSize, supports##X##_v<ft, blockSize>>;                         \
    template <FilterModels ft, size_t blockSize> struct With##X<ft, blockSize, true>

#endif // FEATURES_DETAILS_H
