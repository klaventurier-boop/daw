// Studio Voice Pro — tests : conversion dBFS <-> linéaire et PCM <-> flottant.
#include <cmath>

#include "svp/audio/Convert.h"
#include "svp_test.hpp"

using namespace svp::audio;

SVP_TEST(convert, decibels_to_gain_reference_values)
{
    SVP_CHECK_NEAR(decibelsToGain(0.0), 1.0, 1e-12);
    SVP_CHECK_NEAR(decibelsToGain(-6.020599913279624), 0.5, 1e-9);
    SVP_CHECK_NEAR(decibelsToGain(6.020599913279624), 2.0, 1e-9);
    SVP_CHECK_NEAR(decibelsToGain(-20.0), 0.1, 1e-12);
}

SVP_TEST(convert, gain_to_decibels_reference_values)
{
    SVP_CHECK_NEAR(gainToDecibels(1.0), 0.0, 1e-12);
    SVP_CHECK_NEAR(gainToDecibels(0.5), -6.020599913279624, 1e-9);
    SVP_CHECK_NEAR(gainToDecibels(0.1), -20.0, 1e-12);
}

SVP_TEST(convert, gain_to_decibels_silence_is_floored)
{
    SVP_CHECK_NEAR(gainToDecibels(0.0), kSilenceDb, 1e-12);
    SVP_CHECK_NEAR(gainToDecibels(-1.0), kSilenceDb, 1e-12);
}

SVP_TEST(convert, decibels_gain_roundtrip)
{
    for (double dB = -120.0; dB <= 12.0; dB += 3.0)
        SVP_CHECK_NEAR(gainToDecibels(decibelsToGain(dB)), dB, 1e-9);
}

SVP_TEST(convert, clamp_sample_bounds)
{
    SVP_CHECK_NEAR(clampSample(2.5f), 1.0f, 0.0);
    SVP_CHECK_NEAR(clampSample(-2.5f), -1.0f, 0.0);
    SVP_CHECK_NEAR(clampSample(0.25f), 0.25f, 0.0);
}

SVP_TEST(convert, pcm_conversions_use_full_scale)
{
    SVP_CHECK_NEAR(pcm16ToFloat(floatToPcm16(1.0f)), 1.0, 1e-6);
    SVP_CHECK_NEAR(pcm16ToFloat(floatToPcm16(-1.0f)), -1.0, 1e-6);
    SVP_CHECK_NEAR(pcm24ToFloat(floatToPcm24(1.0f)), 1.0, 1e-7);
    SVP_CHECK_NEAR(pcm24ToFloat(floatToPcm24(-1.0f)), -1.0, 1e-7);
    SVP_CHECK_NEAR(pcm16ToFloat(floatToPcm16(0.5f)), 0.5, 1e-4);
    SVP_CHECK_NEAR(pcm24ToFloat(floatToPcm24(0.5f)), 0.5, 1e-6);
    // Les dépassements sont bornés, jamais repliés.
    SVP_CHECK(floatToPcm16(1.7f) == floatToPcm16(1.0f));
    SVP_CHECK(floatToPcm24(-1.7f) == floatToPcm24(-1.0f));
}
