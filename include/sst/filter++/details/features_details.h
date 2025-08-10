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
    template <FilterTypes ft, typename = void> struct supports##X                                  \
    {                                                                                              \
        static constexpr bool value = false;                                                       \
    };                                                                                             \
    template <FilterTypes ft>                                                                      \
    struct supports##X<ft, std::void_t<decltype(std::declval<FilterConfig<ft>>().has##X)>>         \
    {                                                                                              \
        static constexpr bool value = FilterConfig<ft>::has##X;                                    \
    };                                                                                             \
    template <FilterTypes ft> static constexpr bool supports##X##_v = supports##X<ft>::value;      \
    template <FilterTypes ft, bool b> struct With##X                                               \
    {                                                                                              \
        static constexpr bool supports##X{false};                                                  \
    };                                                                                             \
                                                                                                   \
    template <FilterTypes ft> using Add##X = With##X<ft, supports##X##_v<ft>>;                     \
    template <FilterTypes ft> struct With##X<ft, true>

#endif // FEATURES_DETAILS_H
