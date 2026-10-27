// Studio Voice Pro — tests : buffers, taux d'échantillonnage, rééchantillonnage.
#include <cmath>

#include "svp/audio/Buffer.h"
#include "svp/audio/SampleRate.h"
#include "svp_test.hpp"

using namespace svp::audio;

SVP_TEST(audio, buffer_allocation_and_access)
{
    AudioBuffer buffer(2, 100);
    SVP_CHECK(buffer.channels() == 2);
    SVP_CHECK(buffer.frames() == 100);
    SVP_CHECK(buffer.totalSamples() == 200);
    SVP_CHECK(!buffer.empty());
    SVP_CHECK(buffer.sample(0, 0) == 0.0f); // alloué à zéro

    buffer.setSample(1, 99, 0.5f);
    SVP_CHECK_NEAR(buffer.sample(1, 99), 0.5, 0.0);

    // Les canaux sont bien séparés (disposition canal majeur).
    SVP_CHECK(buffer.sample(0, 99) == 0.0f);
    SVP_CHECK(buffer.channel(0) != buffer.channel(1));

    // Accès hors bornes : silencieux, jamais de plantage.
    SVP_CHECK(buffer.sample(5, 0) == 0.0f);
    SVP_CHECK(buffer.sample(0, 1000) == 0.0f);
    SVP_CHECK(buffer.channel(7) == nullptr);

    buffer.addSample(0, 0, 0.25f);
    SVP_CHECK_NEAR(buffer.sample(0, 0), 0.25, 0.0);

    buffer.clear();
    SVP_CHECK(buffer.sample(0, 0) == 0.0f);

    buffer.setSample(0, 10, 1.0f);
    buffer.setSample(1, 10, 1.0f);
    buffer.clearChannel(1);
    SVP_CHECK_NEAR(buffer.sample(0, 10), 1.0, 0.0);
    SVP_CHECK_NEAR(buffer.sample(1, 10), 0.0, 0.0);

    const AudioBuffer empty;
    SVP_CHECK(empty.empty());
    SVP_CHECK(empty.frames() == 0);
}

SVP_TEST(audio, sample_rate_validation)
{
    SVP_CHECK(isValidSampleRate(44100));
    SVP_CHECK(isValidSampleRate(48000));
    SVP_CHECK(isValidSampleRate(96000));
    SVP_CHECK(isValidSampleRate(192000));
    SVP_CHECK(!isValidSampleRate(0));
    SVP_CHECK(!isValidSampleRate(-48000));
    SVP_CHECK(!isValidSampleRate(1000));

    SVP_CHECK(sampleRatesCompatible(44100, 48000)); // conversion possible
    SVP_CHECK(!sampleRatesCompatible(44100, 1));
}

SVP_TEST(audio, resample_linear_changes_length_and_keeps_shape)
{
    AudioBuffer input(1, 1000);
    for (int i = 0; i < 1000; ++i)
        input.setSample(0, i, static_cast<float>(i) / 1000.0f); // rampe

    const AudioBuffer doubled = resampleLinear(input, 48000, 96000);
    SVP_CHECK(doubled.channels() == 1);
    SVP_CHECK(doubled.frames() == 2000);
    SVP_CHECK_NEAR(doubled.sample(0, 0), 0.0, 1e-6);
    // Point milieu conservé dans les deux cas.
    SVP_CHECK_NEAR(doubled.sample(0, 500), input.sample(0, 250), 1e-4);

    const AudioBuffer halved = resampleLinear(input, 96000, 48000);
    SVP_CHECK(halved.frames() == 500);

    const AudioBuffer identical = resampleLinear(input, 48000, 48000);
    SVP_CHECK(identical.frames() == input.frames());
    SVP_CHECK_NEAR(identical.sample(0, 999), input.sample(0, 999), 0.0);

    // Taux invalide : pas de conversion, buffer vide.
    const AudioBuffer refused = resampleLinear(input, 48000, 3);
    SVP_CHECK(refused.empty());
}
