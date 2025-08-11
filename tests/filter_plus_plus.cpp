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

template <typename F> void bruteForceResponseCurve(std::function<void(F &)> config)
{
    int step = 1;
    auto nVoices{sst::filterplusplus::nVoices};
    for (int i = 0; i < 100; i += nVoices * step)
    {
        auto filter = F();
        config(filter);

        filter.setSampleRate(48000.0f);

        for (int j = 0; j < nVoices; ++j)
        {
            filter.setVoiceActive(j, true);
            filter.setCutoffAndResonance(j, 0, 0.7); // 440hz
        }
        filter.prepareInstance();

        float fr[nVoices], inf[nVoices], phase[nVoices];
        for (int j = 0; j < nVoices; ++j)
        {
            fr[j] = 440 * pow(2.0, (i - 69 + j * step) / 12.);
            inf[j] = fr[j] * filter.sampleRateInv;
            ;
            phase[0] = 0.f;
        }

        // Now loop
        size_t blockPos = 0;
        double irms[nVoices], orms[nVoices];
        std::fill(irms, irms + nVoices, 0.0);
        std::fill(orms, orms + nVoices, 0.0);

        for (int j = 0; j < 2500; ++j)
        {
            if (blockPos == 0)
            {
                filter.prepareBlock();
            }
            float sinv alignas(16)[nVoices], filtv alignas(16)[nVoices];
            for (int k = 0; k < nVoices; ++k)
            {
                sinv[k] = sinf(2.0 * M_PI * phase[k]);
                irms[k] += sinv[k] * sinv[k];
                phase[k] += inf[k];
                if (phase[k] > 1.f)
                    phase[k] -= 1.f;
            }

            auto in = SIMD_MM(load_ps(sinv));
            SIMD_M128 out;
            filter.processSingle(in, out);

            SIMD_MM(store_ps(filtv, out));
            for (int k = 0; k < nVoices; ++k)
            {
                orms[k] += filtv[k] * filtv[k];
            }

            blockPos++;
            if (blockPos == F::blockSize)
            {
                filter.completeBlock();
                blockPos = 0;
            }
        }

        for (int j = 0; j < nVoices; ++j)
        {
            irms[j] = sqrt(irms[j] / 1500.0);
            orms[j] = sqrt(orms[j] / 1500.0);
            std::cout << "RMS " << i + j * step << ": " << irms[j] << " " << orms[j] << std::endl;
        }
    }
}

TEST_CASE("Filter++ on Classic")
{

    SECTION("Compiles")
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

        for (int i = 0; i < vcf_t::blockSize; ++i)
        {
            auto in = SIMD_MM(setzero_ps());
            SIMD_M128 out;
            filter.processSingle(in, out);
        }
        filter.completeBlock();
    }

    SECTION("Response")
    {
        using vcf_t = sst::filterplusplus::Filter<sst::filterplusplus::VemberClassic, 16>;
        bruteForceResponseCurve<vcf_t>([](auto &filter) {
            filter.setPassType(vcf_t::config_t::PassTypes::LP);
            filter.setSlope(vcf_t::config_t::Slopes::Slope_24db);
            filter.setDrive(vcf_t::config_t::Drives::Standard);
        });
    }
}