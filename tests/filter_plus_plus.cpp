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

#include <array>
#include <iostream>
#include <numeric>
#include <vector>

#include <catch2/catch2.hpp>

#include <sst/filter++/api.h>

TEST_CASE("Filter++ on Classic")
{
    using vcf_t = sst::filterplusplus::Filter<sst::filterplusplus::VemberClassic, 16>;

    auto filter = vcf_t();
    filter.setPassType(vcf_t::config_t::PassTypes::LP);
    filter.setSlope(vcf_t::config_t::Slopes::Slope_24db);
    filter.setDrive(vcf_t::config_t::Drives::Driven);
    filter.setSampleRate(48000.0f);

    REQUIRE(filter.prepareInstance());

    filter.setVoiceActive(0, true);
    filter.setVoiceActive(1, false);
    filter.setVoiceActive(2, false);
    filter.setVoiceActive(3, false);
    filter.setCutoffAndResonance(0, 0, 0.7); // 440hz
    filter.prepareBlock();
}