// Studio Voice Pro — tests : gain, panoramique, mute et solo.
#include <cmath>
#include <vector>

#include "svp/audio/Convert.h"
#include "svp/dsp/GainPan.h"
#include "svp/dsp/Mixer.h"
#include "svp_test.hpp"

using namespace svp;

namespace {

constexpr float kHalfPower = 0.70710678f; // -3,01 dB

audio::AudioBuffer constant(float value, int channels, int frames)
{
    audio::AudioBuffer buffer(channels, frames);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < frames; ++i)
            buffer.setSample(ch, i, value);
    return buffer;
}

float worstDifference(const audio::AudioBuffer& a, const audio::AudioBuffer& b)
{
    float worst = 0.0f;
    for (int ch = 0; ch < a.channels(); ++ch)
        for (int i = 0; i < a.frames(); ++i)
            worst = std::max(worst, std::fabs(a.sample(ch, i) - b.sample(ch, i)));
    return worst;
}

} // namespace

SVP_TEST(dsp, gain_linear_and_db)
{
    audio::AudioBuffer buffer = constant(0.5f, 2, 64);
    dsp::applyGain(buffer, 0.5f);
    SVP_CHECK_NEAR(buffer.sample(0, 0), 0.25, 1e-7);

    buffer = constant(0.5f, 2, 64);
    dsp::applyGainDb(buffer, -6.020599913279624);
    SVP_CHECK_NEAR(buffer.sample(1, 63), 0.25, 1e-6);

    buffer = constant(0.25f, 1, 8);
    dsp::applyGainDb(buffer, 0.0);
    SVP_CHECK_NEAR(buffer.sample(0, 4), 0.25, 1e-7);

    // Gain nul : la piste devient silencieuse.
    dsp::applyGain(buffer, 0.0f);
    SVP_CHECK_NEAR(buffer.sample(0, 4), 0.0, 1e-12);
}

SVP_TEST(dsp, gain_ramp_is_linear_from_start_to_end)
{
    audio::AudioBuffer buffer = constant(1.0f, 1, 5);
    dsp::applyGainRamp(buffer, 0.0f, 1.0f);
    SVP_CHECK_NEAR(buffer.sample(0, 0), 0.0, 1e-7);
    SVP_CHECK_NEAR(buffer.sample(0, 2), 0.5, 1e-7);
    SVP_CHECK_NEAR(buffer.sample(0, 4), 1.0, 1e-7);
}

SVP_TEST(dsp, pan_law_is_constant_power)
{
    float left = 0.0f;
    float right = 0.0f;

    dsp::panGains(0.0f, left, right);
    SVP_CHECK_NEAR(left, kHalfPower, 1e-6);
    SVP_CHECK_NEAR(right, kHalfPower, 1e-6);
    SVP_CHECK_NEAR(left * left + right * right, 1.0, 1e-6);

    dsp::panGains(-1.0f, left, right);
    SVP_CHECK_NEAR(left, 1.0, 1e-6);
    SVP_CHECK_NEAR(right, 0.0, 1e-7);

    dsp::panGains(1.0f, left, right);
    SVP_CHECK_NEAR(left, 0.0, 1e-7);
    SVP_CHECK_NEAR(right, 1.0, 1e-6);

    // Valeurs hors bornes : ramenées dans [-1, +1].
    dsp::panGains(-3.0f, left, right);
    SVP_CHECK_NEAR(left, 1.0, 1e-6);
    dsp::panGains(+3.0f, left, right);
    SVP_CHECK_NEAR(right, 1.0, 1e-6);
}

SVP_TEST(dsp, pan_applies_to_stereo_buffer)
{
    audio::AudioBuffer buffer = constant(1.0f, 2, 32);
    dsp::applyPan(buffer, -1.0f);
    SVP_CHECK_NEAR(buffer.sample(0, 0), 1.0, 1e-6);
    SVP_CHECK_NEAR(buffer.sample(1, 0), 0.0, 1e-7);

    buffer = constant(1.0f, 2, 32);
    dsp::applyPan(buffer, 0.0f);
    SVP_CHECK_NEAR(buffer.sample(0, 0), kHalfPower, 1e-6);
    SVP_CHECK_NEAR(buffer.sample(1, 0), kHalfPower, 1e-6);
}

SVP_TEST(dsp, pan_leaves_mono_amplitude_unchanged)
{
    // Loi constante en puissance : un canal unique garde son énergie ; le
    // placement se fait au mixage vers le bus stéréo.
    audio::AudioBuffer buffer = constant(1.0f, 1, 16);
    dsp::applyPan(buffer, 0.75f);
    SVP_CHECK_NEAR(buffer.sample(0, 0), 1.0, 1e-6);
}

SVP_TEST(dsp, channel_audibility_rules)
{
    dsp::ChannelStrip plain;
    SVP_CHECK(dsp::isChannelAudible(plain, false));
    SVP_CHECK(!dsp::isChannelAudible(plain, true)); // une autre piste est en solo

    dsp::ChannelStrip muted;
    muted.mute = true;
    SVP_CHECK(!dsp::isChannelAudible(muted, false));

    dsp::ChannelStrip soloed;
    soloed.solo = true;
    SVP_CHECK(dsp::isChannelAudible(soloed, true));

    dsp::ChannelStrip mutedSoloed;
    mutedSoloed.mute = true;
    mutedSoloed.solo = true;
    SVP_CHECK(!dsp::isChannelAudible(mutedSoloed, true)); // mute gagne sur solo

    std::vector<dsp::ChannelStrip> strips(3);
    strips[1].solo = true;
    SVP_CHECK(dsp::anySoloActive(strips));
    SVP_CHECK(!dsp::isChannelAudible(strips[0], true));
    SVP_CHECK(dsp::isChannelAudible(strips[1], true));

    std::vector<dsp::ChannelStrip> none(2);
    SVP_CHECK(!dsp::anySoloActive(none));
}

SVP_TEST(dsp, apply_strip_writes_silence_without_touching_the_source)
{
    // Règle non destructive : la piste muette ne s'écrit PAS elle-même à zéro,
    // c'est la destination qui reçoit le silence. C'est ce qui empêche un mute
    // d'effacer un média chargé.
    const audio::AudioBuffer media = constant(0.8f, 1, 32);
    audio::AudioBuffer destination(1, 32);

    dsp::ChannelStrip strip;
    strip.mute = true;
    dsp::applyStripTo(media.constView(), destination.view(), strip, false);
    SVP_CHECK_NEAR(destination.sample(0, 5), 0.0, 1e-12);
    SVP_CHECK_NEAR(media.sample(0, 5), 0.8, 1e-12); // média intact

    dsp::ChannelStrip soloed;
    dsp::applyStripTo(media.constView(), destination.view(), soloed, true); // solo ailleurs
    SVP_CHECK_NEAR(destination.sample(0, 5), 0.0, 1e-12);
    SVP_CHECK_NEAR(media.sample(0, 5), 0.8, 1e-12); // média toujours intact

    // Piste audible : la destination reçoit la copie pondérée, la source reste intacte.
    dsp::ChannelStrip audible;
    audible.gain = 0.5f;
    dsp::applyStripTo(media.constView(), destination.view(), audible, false);
    SVP_CHECK_NEAR(destination.sample(0, 5), 0.4, 1e-6);
    SVP_CHECK_NEAR(media.sample(0, 5), 0.8, 1e-12);
}

SVP_TEST(dsp, mix_strips_sums_audible_tracks_only)
{
    const audio::AudioBuffer trackA = constant(0.25f, 1, 64);
    const audio::AudioBuffer trackB = constant(0.25f, 1, 64);
    const audio::AudioBuffer trackC = constant(0.25f, 1, 64);

    std::vector<dsp::ChannelStrip> strips(3);
    strips[0].gain = 1.0f;
    strips[1].gain = 1.0f;
    strips[1].mute = true;
    strips[2].gain = 1.0f;
    strips[2].solo = true;

    // Sources dans un tableau de pile : aucun std::vector, donc aucune
    // allocation par bloc (c'est exactement ce que fait Engine::process).
    const audio::AudioBuffer* sources[] = {&trackA, &trackB, &trackC};

    audio::AudioBuffer mix(2, 64);
    dsp::mixStrips(mix, sources, strips);

    // A est exclu par le solo de C : le mix ne contient que C (au centre).
    const float expected = 0.25f * kHalfPower;
    SVP_CHECK_NEAR(mix.sample(0, 10), expected, 1e-6);
    SVP_CHECK_NEAR(mix.sample(1, 10), expected, 1e-6);

    // Piste muette : exclue même sans solo.
    std::vector<dsp::ChannelStrip> muteOnly(2);
    muteOnly[1].mute = true;
    audio::AudioBuffer mix2(2, 64);
    dsp::mixStrips(mix2, sources, muteOnly);
    SVP_CHECK_NEAR(mix2.sample(0, 10), 0.25f * kHalfPower, 1e-6);

    // Rien d'audible : silence total.
    std::vector<dsp::ChannelStrip> allMuted(2);
    allMuted[0].mute = true;
    allMuted[1].mute = true;
    audio::AudioBuffer mix3(2, 64);
    dsp::mixStrips(mix3, sources, allMuted);
    SVP_CHECK_NEAR(mix3.sample(0, 10), 0.0, 1e-12);
    SVP_CHECK_NEAR(mix3.sample(1, 10), 0.0, 1e-12);
}

SVP_TEST(dsp, mix_respects_pan_position_of_each_track)
{
    const audio::AudioBuffer track = constant(1.0f, 1, 32);
    std::vector<dsp::ChannelStrip> strips(1);
    strips[0].gain = 1.0f;
    strips[0].pan = -1.0f; // tout à gauche

    const audio::AudioBuffer* sources[] = {&track};
    audio::AudioBuffer mix(2, 32);
    dsp::mixStrips(mix, sources, strips);
    SVP_CHECK_NEAR(mix.sample(0, 0), 1.0, 1e-6);
    SVP_CHECK_NEAR(mix.sample(1, 0), 0.0, 1e-7);
}

SVP_TEST(dsp, mix_clears_previous_content)
{
    const audio::AudioBuffer track = constant(0.1f, 1, 16);
    std::vector<dsp::ChannelStrip> strips(1);
    const audio::AudioBuffer* sources[] = {&track};
    audio::AudioBuffer mix(2, 16);
    for (int i = 0; i < 16; ++i) {
        mix.setSample(0, i, 5.0f);
        mix.setSample(1, i, 5.0f);
    }
    dsp::mixStrips(mix, sources, strips);
    SVP_CHECK(mix.sample(0, 0) < 1.0f);
    SVP_CHECK(mix.sample(1, 0) < 1.0f);
    SVP_CHECK_NEAR(worstDifference(mix, mix), 0.0, 0.0);
}
