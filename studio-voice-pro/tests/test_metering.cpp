// Studio Voice Pro — tests : crête, RMS, maintien du pic.
#include <cmath>

#include "svp/audio/Convert.h"
#include "svp/metering/Meter.h"
#include "svp_test.hpp"

using namespace svp;

namespace {

constexpr double kPi = 3.14159265358979323846;

audio::AudioBuffer sine(double amplitude, int sampleRate, int frames, double frequency)
{
    audio::AudioBuffer buffer(1, frames);
    for (int i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        buffer.setSample(0, i, static_cast<float>(amplitude * std::sin(2.0 * kPi * frequency * t)));
    }
    return buffer;
}

} // namespace

SVP_TEST(metering, full_scale_sine_rms_is_minus_3_dbfs)
{
    // Un sinus pleine échelle a une valeur efficace de 1/sqrt(2) = -3,0103 dBFS.
    const auto buffer = sine(1.0, 48000, 48000, 1000.0); // 1 s, 1000 cycles entiers
    const metering::MeterValues values = metering::measureBlock(buffer);

    SVP_CHECK_NEAR(audio::gainToDecibels(values.rms), -3.0103, 0.01);
    SVP_CHECK_NEAR(values.peak, 1.0, 1e-4);
    SVP_CHECK_NEAR(audio::gainToDecibels(values.peak), 0.0, 1e-4);
    SVP_CHECK(values.clipped);
}

SVP_TEST(metering, half_scale_sine_rms_is_minus_9_dbfs)
{
    // Amplitude divisée par 2 : -6,0206 dB de crête => RMS à -9,0309 dBFS.
    const auto buffer = sine(0.5, 48000, 48000, 1000.0);
    const metering::MeterValues values = metering::measureBlock(buffer);
    SVP_CHECK_NEAR(audio::gainToDecibels(values.rms), -9.0309, 0.01);
    SVP_CHECK_NEAR(audio::gainToDecibels(values.peak), -6.0206, 1e-3);
    SVP_CHECK(!values.clipped);
}

SVP_TEST(metering, silence_measures_floor_not_nan)
{
    const audio::AudioBuffer silent(2, 1024);
    const metering::MeterValues values = metering::measureBlock(silent);
    SVP_CHECK_NEAR(values.peak, 0.0, 0.0);
    SVP_CHECK_NEAR(values.rms, 0.0, 0.0);
    SVP_CHECK_NEAR(audio::gainToDecibels(values.rms), audio::kSilenceDb, 1e-12);
    SVP_CHECK(!values.clipped);
}

SVP_TEST(metering, rms_and_peak_over_multichannel_buffer)
{
    // Deux canaux identiques : la mesure globale reste celle d'un canal.
    audio::AudioBuffer buffer(2, 4800);
    for (int i = 0; i < 4800; ++i) {
        const double t = static_cast<double>(i) / 48000.0;
        const float value = static_cast<float>(std::sin(2.0 * kPi * 1000.0 * t));
        buffer.setSample(0, i, value);
        buffer.setSample(1, i, value);
    }
    const metering::MeterValues values = metering::measureBlock(buffer);
    SVP_CHECK_NEAR(audio::gainToDecibels(values.rms), -3.0103, 0.05);
}

SVP_TEST(metering, peak_hold_keeps_the_maximum_then_decays)
{
    metering::PeakRmsMeter meter;
    meter.prepare(48000.0, 480, 100.0f); // maintien court : 100 ms

    const auto loud = sine(1.0, 48000, 480, 1000.0);
    meter.process(loud);
    SVP_CHECK_NEAR(meter.values().peakHold, 1.0, 1e-4);

    const audio::AudioBuffer silent(1, 480);
    meter.process(silent);
    SVP_CHECK_NEAR(meter.values().peak, 0.0, 0.0);
    SVP_CHECK_NEAR(meter.values().peakHold, 1.0, 1e-6); // encore maintenu

    // Après expiration du maintien, le pic redescend sans jamais remonter.
    for (int i = 0; i < 40; ++i)
        meter.process(silent);
    SVP_CHECK(meter.values().peakHold < 1.0f);
    SVP_CHECK(meter.values().peakHold >= 0.0f);

    meter.reset();
    SVP_CHECK_NEAR(meter.values().peakHold, 0.0, 0.0);
}

SVP_TEST(metering, peak_hold_follows_a_new_louder_block)
{
    metering::PeakRmsMeter meter;
    meter.prepare(48000.0, 480, 500.0f);

    meter.process(sine(0.25, 48000, 480, 1000.0));
    SVP_CHECK_NEAR(meter.values().peakHold, 0.25, 1e-3);

    meter.process(sine(0.75, 48000, 480, 1000.0));
    SVP_CHECK_NEAR(meter.values().peakHold, 0.75, 1e-3);
}
