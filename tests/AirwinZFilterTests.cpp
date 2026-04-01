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
 * AirwinZFilterTests.cpp — two-tier tests for the AirwinZ filter family:
 *
 *   1. RMS frequency-response tests (via TestUtils::runTest) for all four
 *      passmodes at clean and driven settings, classic API and filters++ API.
 *
 *   2. Golden-value sample-exact tests driven by LCG noise.  These capture
 *      the exact output waveform and detect any unintentional DSP change.
 *
 * GOLDEN PRINT MODE
 *   Run with AIRWINZ_GOLDEN=1 to print new expected arrays to stdout:
 *     AIRWINZ_GOLDEN=1 ./sst-filters-tests "[AirwinZ][golden]"
 *   Copy the printed arrays back into the expected{} initialisers below,
 *   then verify in normal mode:
 *     ./sst-filters-tests "[AirwinZ][golden]"
 */

#include "TestUtils.h"

// ── Golden test constants ──────────────────────────────────────────────────────
static constexpr int GOLDEN_WARMUP = 100;
static constexpr int GOLDEN_RECORD = 50;
static constexpr float GOLDEN_SR = 48000.f;
static constexpr float GOLDEN_TOL = 1e-5f;

static bool goldenPrintMode() { return std::getenv("AIRWINZ_GOLDEN") != nullptr; }

static float lcgSample(uint32_t &rng)
{
    rng = rng * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int32_t>(rng)) / 2147483648.f;
}

static void goldenCheckOrPrint(const char *label, const std::array<float, GOLDEN_RECORD> &got,
                               const std::array<float, GOLDEN_RECORD> &expected)
{
    if (goldenPrintMode())
    {
        std::printf("        // %s\n        {", label);
        for (int i = 0; i < GOLDEN_RECORD; ++i)
        {
            if (i > 0 && i % 5 == 0)
                std::printf("\n         ");
            std::printf("%.9ff%s", got[i], i + 1 < GOLDEN_RECORD ? ", " : "");
        }
        std::printf("};\n");
    }
    else
    {
        for (int i = 0; i < GOLDEN_RECORD; ++i)
        {
            INFO(label << " sample[" << i << "]: got=" << got[i] << " expected=" << expected[i]);
            REQUIRE(got[i] == Approx(expected[i]).margin(GOLDEN_TOL));
        }
    }
}

// Run one golden test via the classic API.
// extra: drive value forwarded to MakeCoeffs (used for filters++ subtypes 16–19)
static std::array<float, GOLDEN_RECORD> runGolden(sst::filters::FilterType ft,
                                                  sst::filters::FilterSubType st, float cutoff,
                                                  float reso, float extra = 0.f)
{
    using FCM = sst::filters::FilterCoefficientMaker<sst::filters::detail::BasicTuningProvider>;

    FCM cm;
    cm.setSampleRateAndBlockSize(GOLDEN_SR, 32);

    sst::filters::QuadFilterUnitState qfu{};
    qfu.sampleRate = GOLDEN_SR;
    qfu.sampleRateInv = 1.f / GOLDEN_SR;
    for (int i = 0; i < 4; ++i)
        qfu.active[i] = 0xffffffff;

    auto fn = sst::filters::GetQFPtrFilterUnit(ft, st);
    REQUIRE(fn != nullptr);

    uint32_t rng = 0xdeadbeef;

    // Warmup
    for (int i = 0; i < GOLDEN_WARMUP; ++i)
    {
        cm.MakeCoeffs(cutoff, reso, ft, st, nullptr, false, extra, 0.f, 0.f);
        cm.updateState(qfu, 0);
        auto x = SIMD_MM(set1_ps)(lcgSample(rng));
        fn(&qfu, x);
    }

    // Record
    std::array<float, GOLDEN_RECORD> got;
    for (int i = 0; i < GOLDEN_RECORD; ++i)
    {
        cm.MakeCoeffs(cutoff, reso, ft, st, nullptr, false, extra, 0.f, 0.f);
        cm.updateState(qfu, 0);
        auto x = SIMD_MM(set1_ps)(lcgSample(rng));
        auto y = fn(&qfu, x);
        got[i] = SIMD_MM(cvtss_f32)(y);
    }
    return got;
}

// Run a sine-sweep RMS test through the filters++ API with explicit extra params.
// drive = extra1, poles = extra2.
static float runSinePlusPlus(sst::filtersplusplus::Passband pb, float cutoff, float res,
                              float drive, float poles, float testFreq, int numSamples)
{
    namespace sfpp = sst::filtersplusplus;
    auto filter = sfpp::Filter();
    filter.setFilterModel(sfpp::FilterModel::AirwinZ);
    filter.setModelConfiguration({pb, sfpp::DriveMode::Standard});
    filter.setSampleRateAndBlockSize(TestUtils::sampleRate, TestUtils::blockSize);
    for (int i = 0; i < 4; ++i)
        filter.setActive(i, true);
    REQUIRE(filter.prepareInstance());

    filter.reset();
    auto bs = filter.getBlockSize();
    std::vector<float> y(numSamples, 0.f);
    for (int i = 0; i < numSamples; ++i)
    {
        if (i % bs == 0)
        {
            if (i != 0)
                filter.concludeBlock();
            filter.makeCoefficients(0, cutoff, res, drive, poles);
            filter.prepareBlock();
        }
        auto x = (float)std::sin(2.0 * M_PI * (double)i * testFreq / TestUtils::sampleRate);
        auto yv = filter.processSample(SIMD_MM(set1_ps)(x));
        float ya alignas(16)[4];
        SIMD_MM(store_ps)(ya, yv);
        y[i] = ya[0];
    }
    auto ss = std::inner_product(y.begin(), y.end(), y.begin(), 0.f);
    return 20.f * std::log10(std::sqrt(ss / (float)numSamples));
}

// ── RMS frequency-response tests ──────────────────────────────────────────────
TEST_CASE("AirwinZ Filter")
{
    using namespace TestUtils;
    namespace sfpp = sst::filtersplusplus;

    SECTION("Lowpass - 4 pole clean")
    {
        // subtype 12 = poles 4, drive index 0 (0.0)
        runTest({FilterType::fut_airwinz_lp,
                 static_cast<FilterSubType>(12),
                 {-2.7876f, -2.75723f, -3.33801f, -28.9105f, -51.1037f}});
    }

    SECTION("Lowpass - 4 pole driven")
    {
        // subtype 14 = poles 4, drive index 2 (0.66)
        runTest({FilterType::fut_airwinz_lp,
                 static_cast<FilterSubType>(14),
                 {-68.1089f, -69.1615f, -70.6298f, -92.7744f, -111.072f}});
    }

    SECTION("Highpass - 4 pole clean")
    {
        runTest({FilterType::fut_airwinz_hp,
                 static_cast<FilterSubType>(12),
                 {-40.8394f, -31.6701f, -3.31528f, -2.78381f, -2.165f}});
    }

    SECTION("Bandpass - 4 pole clean")
    {
        runTest({FilterType::fut_airwinz_bp,
                 static_cast<FilterSubType>(12),
                 {-42.2912f, -26.0833f, -3.4618f, -27.3982f, -57.2744f}});
    }

    SECTION("Notch - 4 pole clean")
    {
        runTest({FilterType::fut_airwinz_notch,
                 static_cast<FilterSubType>(12),
                 {-3.74486f, -8.07695f, -28.9359f, -7.56735f, -2.30218f}});
    }

    // filters++ continuous API: extra1=drive, extra2=poles
    // (poles=4, drive=0 should match classic subtype 12)
    SECTION("filters++ LP continuous (poles=4, drive=0)")
    {
        static const std::array<float, numTestFreqs> expected{
            -2.7876f, -2.75723f, -3.33801f, -28.9105f, -51.1037f};
        for (int i = 0; i < numTestFreqs; ++i)
        {
            auto rms = runSinePlusPlus(sfpp::Passband::LP, 0.f, 0.5f, 0.f, 4.f, testFreqs[i],
                                       blockSize);
            if constexpr (!printRMSs)
            {
                INFO("freq " << testFreqs[i] << " Hz: got=" << rms
                             << " expected=" << expected[i]);
                REQUIRE(rms == Approx(expected[i]).margin(1e-2f));
            }
            else
            {
                std::cout << rms << "f, ";
            }
        }
    }

    SECTION("filters++ HP continuous (poles=4, drive=0)")
    {
        static const std::array<float, numTestFreqs> expected{
            -40.8394f, -31.6701f, -3.31528f, -2.78381f, -2.165f};
        for (int i = 0; i < numTestFreqs; ++i)
        {
            auto rms = runSinePlusPlus(sfpp::Passband::HP, 0.f, 0.5f, 0.f, 4.f, testFreqs[i],
                                       blockSize);
            if constexpr (!printRMSs)
            {
                INFO("freq " << testFreqs[i] << " Hz: got=" << rms
                             << " expected=" << expected[i]);
                REQUIRE(rms == Approx(expected[i]).margin(1e-2f));
            }
            else
            {
                std::cout << rms << "f, ";
            }
        }
    }
}

// ── Golden sample-exact tests ──────────────────────────────────────────────────
TEST_CASE("AirwinZ Filter Golden", "[AirwinZ][golden]")
{
    // cutoff = 0.0 (A440), resonance = 0.5, LCG noise seed = 0xdeadbeef

    SECTION("LP 4-pole clean (subtype 12)")
    {
        auto got = runGolden(sst::filters::fut_airwinz_lp,
                             static_cast<sst::filters::FilterSubType>(12), 0.f, 0.5f);
        static const std::array<float, GOLDEN_RECORD> expected{
            // LP 4-pole clean
            0.127889022f, 0.130300134f, 0.132583693f, 0.134730339f, 0.136730790f, 0.138575941f,
            0.140256792f, 0.141764611f, 0.143090874f, 0.144227386f, 0.145166337f, 0.145900205f,
            0.146421969f, 0.146725029f, 0.146803305f, 0.146651283f, 0.146264061f, 0.145637333f,
            0.144767478f, 0.143651560f, 0.142287388f, 0.140673459f, 0.138809070f, 0.136694342f,
            0.134330064f, 0.131717905f, 0.128860235f, 0.125760272f, 0.122421950f, 0.118849978f,
            0.115049787f, 0.111027449f, 0.106789775f, 0.102344103f, 0.097698420f, 0.092861228f,
            0.087841548f, 0.082648866f, 0.077293038f, 0.071784317f, 0.066133283f, 0.060350727f,
            0.054447703f, 0.048435386f, 0.042325150f, 0.036128394f, 0.029856594f, 0.023521228f,
            0.017133765f, 0.010705588f};
        goldenCheckOrPrint("LP 4-pole clean", got, expected);
    }

    SECTION("LP 4-pole driven (subtype 14, drive=0.66)")
    {
        auto got = runGolden(sst::filters::fut_airwinz_lp,
                             static_cast<sst::filters::FilterSubType>(14), 0.f, 0.5f);
        static const std::array<float, GOLDEN_RECORD> expected{
            // LP 4-pole driven
            0.000106163f,  0.000108372f, 0.000110454f, 0.000112400f, 0.000114199f,  0.000115842f,
            0.000117318f,  0.000118619f, 0.000119736f, 0.000120660f, 0.000121382f,  0.000121896f,
            0.000122193f,  0.000122268f, 0.000122114f, 0.000121725f, 0.000121096f,  0.000120225f,
            0.000119107f,  0.000117739f, 0.000116120f, 0.000114248f, 0.000112124f,  0.000109748f,
            0.000107121f,  0.000104245f, 0.000101124f, 0.000097761f, 0.000094162f,  0.000090330f,
            0.000086274f,  0.000081999f, 0.000077514f, 0.000072827f, 0.000067946f,  0.000062883f,
            0.000057647f,  0.000052248f, 0.000046699f, 0.000041011f, 0.000035197f,  0.000029268f,
            0.000023239f,  0.000017122f, 0.000010931f, 0.000004679f, -0.000001620f, -0.000007951f,
            -0.000014302f, -0.000020658f};
        goldenCheckOrPrint("LP 4-pole driven", got, expected);
    }

    SECTION("HP 4-pole clean (subtype 12)")
    {
        auto got = runGolden(sst::filters::fut_airwinz_hp,
                             static_cast<sst::filters::FilterSubType>(12), 0.f, 0.5f);
        static const std::array<float, GOLDEN_RECORD> expected{
            // HP 4-pole clean
            -1.003374338f, -1.002738237f, -0.176639020f, 0.996172726f,  0.995541215f,
            -1.003253222f, -0.423108697f, -1.002348900f, -0.068910055f, 0.996493399f,
            0.780102432f,  0.981788635f,  0.934125066f,  0.994152606f,  0.399892867f,
            -0.808588386f, -1.004381776f, 0.437741280f,  0.729365110f,  0.581187665f,
            0.477215767f,  0.647009075f,  0.992597163f,  0.991967857f,  0.991339087f,
            -0.129031971f, -1.007370830f, -1.006732225f, -1.006093979f, -1.005456209f,
            -0.968618989f, -0.689235985f, -0.114928506f, 0.994468272f,  0.993837833f,
            0.406942397f,  -1.005213380f, -1.004576206f, -0.230183125f, 0.501150608f,
            0.994052112f,  0.993421972f,  0.060725719f,  -1.005409598f, -1.004772186f,
            0.994028032f,  0.993397892f,  0.992768109f,  0.992138743f,  0.887249708f};
        goldenCheckOrPrint("HP 4-pole clean", got, expected);
    }

    SECTION("BP 4-pole clean (subtype 12)")
    {
        auto got = runGolden(sst::filters::fut_airwinz_bp,
                             static_cast<sst::filters::FilterSubType>(12), 0.f, 0.5f);
        static const std::array<float, GOLDEN_RECORD> expected{
            // BP 4-pole clean
            0.002360105f,  0.005475751f, 0.008733083f, 0.012127745f, 0.015653152f,  0.019299828f,
            0.023055585f,  0.026905131f, 0.030828688f, 0.034799460f, 0.038782429f,  0.042736340f,
            0.046616625f,  0.050377015f, 0.053969249f, 0.057343811f, 0.060453445f,  0.063255943f,
            0.065713122f,  0.067788713f, 0.069447629f, 0.070657156f, 0.071390085f,  0.071628019f,
            0.071363539f,  0.070600964f, 0.069355607f, 0.067651376f, 0.065517589f,  0.062986061f,
            0.060089454f,  0.056861501f, 0.053338610f, 0.049560320f, 0.045568883f,  0.041408755f,
            0.037126090f,  0.032769319f, 0.028389875f, 0.024039892f, 0.019767117f,  0.015610849f,
            0.011602092f,  0.007766940f, 0.004129468f, 0.000711080f, -0.002472223f, -0.005411388f,
            -0.008104157f, -0.010552976f};
        goldenCheckOrPrint("BP 4-pole clean", got, expected);
    }

    SECTION("Notch 4-pole clean (subtype 12)")
    {
        auto got = runGolden(sst::filters::fut_airwinz_notch,
                             static_cast<sst::filters::FilterSubType>(12), 0.f, 0.5f);
        static const std::array<float, GOLDEN_RECORD> expected{
            // Notch 4-pole clean
            -1.002952933f, -1.002317190f, -0.205101594f, 0.998948336f,  0.998315096f,
            -1.002817750f, -0.449368060f, -1.001897335f, -0.008689098f, 0.999243498f,
            0.719188631f,  0.811891675f,  0.611719787f,  0.997251630f,  0.855144978f,
            0.000939773f,  -1.004423380f, -0.009358454f, 0.362437814f,  0.033697542f,
            -0.238446161f, 0.002659850f,  0.313634545f,  0.996418953f,  0.995787203f,
            -0.111037098f, -1.005273700f, -1.004636407f, -1.003999472f, -1.003363013f,
            -0.967151523f, -1.002113819f, -0.460703194f, 0.999313533f,  0.998679996f,
            0.414911658f,  -1.002716184f, -1.002080560f, -0.224697649f, 0.506226718f,
            0.998876333f,  0.998243093f,  0.066232152f,  -1.002931714f, -1.002295971f,
            0.998839498f,  0.998206258f,  0.997573495f,  0.996941090f,  0.834196985f};
        goldenCheckOrPrint("Notch 4-pole clean", got, expected);
    }

    SECTION("LP 1-pole clean (subtype 0)")
    {
        auto got = runGolden(sst::filters::fut_airwinz_lp,
                             static_cast<sst::filters::FilterSubType>(0), 0.f, 0.5f);
        static const std::array<float, GOLDEN_RECORD> expected{
            // LP 1-pole clean
            -0.086951435f, -0.081504904f, -0.074396998f, -0.066838980f, -0.058819558f,
            -0.050165445f, -0.041340437f, -0.032510005f, -0.025202274f, -0.020503744f,
            -0.017929053f, -0.016935475f, -0.017137948f, -0.019248242f, -0.023802295f,
            -0.029513814f, -0.035134591f, -0.041477039f, -0.049536705f, -0.059604753f,
            -0.071736656f, -0.084894016f, -0.097759858f, -0.109549776f, -0.119543709f,
            -0.127454668f, -0.133633316f, -0.138854504f, -0.143768340f, -0.148884550f,
            -0.154169649f, -0.158517882f, -0.161700621f, -0.163809016f, -0.164442018f,
            -0.163895205f, -0.161622629f, -0.156602293f, -0.149220616f, -0.141190216f,
            -0.134239480f, -0.128435433f, -0.122647889f, -0.115622282f, -0.107587300f,
            -0.099970080f, -0.093225859f, -0.087639906f, -0.083041728f, -0.077904373f};
        goldenCheckOrPrint("LP 1-pole clean", got, expected);
    }
}
