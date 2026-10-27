// Studio Voice Pro — tests : transport (position, boucle, temps musical).
#include "svp/transport/Transport.h"
#include "svp_test.hpp"

using namespace svp::transport;

SVP_TEST(transport, starts_stopped_and_does_not_advance)
{
    Transport transport;
    transport.prepare(48000.0, 120.0);
    SVP_CHECK(transport.state() == State::Stopped);
    SVP_CHECK(!transport.isPlaying());

    transport.advance(1000);
    SVP_CHECK(transport.positionFrames() == 0);

    transport.play();
    SVP_CHECK(transport.isPlaying());
    transport.advance(1000);
    SVP_CHECK(transport.positionFrames() == 1000);

    transport.stop();
    transport.advance(1000);
    SVP_CHECK(transport.positionFrames() == 1000); // figé à l'arrêt
}

SVP_TEST(transport, time_and_sample_conversions)
{
    Transport transport;
    transport.prepare(48000.0, 120.0);

    SVP_CHECK(transport.secondsToFrames(1.0) == 48000);
    SVP_CHECK_NEAR(transport.framesToSeconds(24000), 0.5, 1e-12);
    // À 120 BPM, une noire dure 0,5 s soit 24000 trames.
    SVP_CHECK(transport.beatsToFrames(1.0) == 24000);
    SVP_CHECK(transport.beatsToFrames(4.0) == 96000);
    SVP_CHECK_NEAR(transport.framesToBeats(24000), 1.0, 1e-12);

    transport.setPositionSeconds(2.5);
    SVP_CHECK(transport.positionFrames() == 120000);
    SVP_CHECK_NEAR(transport.positionSeconds(), 2.5, 1e-12);
    SVP_CHECK_NEAR(transport.positionBeats(), 5.0, 1e-12);

    transport.setTempo(60.0);
    SVP_CHECK(transport.beatsToFrames(1.0) == 48000);

    transport.setPositionFrames(-500);
    SVP_CHECK(transport.positionFrames() == 0); // jamais de position négative
}

SVP_TEST(transport, simple_loop_wraps_around)
{
    Transport transport;
    transport.prepare(48000.0, 120.0, 48000);
    transport.setLoopRange(1000, 5800); // boucle de 4800 trames
    transport.setLoopEnabled(true);
    transport.setPositionFrames(1000); // au début de la boucle
    transport.play();

    transport.advance(4799);
    SVP_CHECK(transport.positionFrames() == 5799);
    SVP_CHECK(transport.isPlaying());

    transport.advance(1); // franchit exactement la fin de boucle
    SVP_CHECK(transport.positionFrames() == 1000);

    transport.advance(4800); // un tour complet
    SVP_CHECK(transport.positionFrames() == 1000);

    transport.advance(9600); // deux tours complets
    SVP_CHECK(transport.positionFrames() == 1000);

    transport.advance(500);
    SVP_CHECK(transport.positionFrames() == 1500);

    transport.advance(5000); // plus d'un tour en un seul appel
    SVP_CHECK(transport.positionFrames() == 1700);
    SVP_CHECK(transport.isPlaying());
}

SVP_TEST(transport, playback_entering_the_loop_region_wraps_at_loop_end)
{
    // Une position située avant la boucle traverse la zone puis reboucle.
    Transport transport;
    transport.prepare(48000.0, 120.0, 48000);
    transport.setLoopRange(1000, 5800);
    transport.setLoopEnabled(true);
    transport.play();

    transport.advance(1000);
    SVP_CHECK(transport.positionFrames() == 1000);
    transport.advance(4799);
    SVP_CHECK(transport.positionFrames() == 5799);
    transport.advance(1);
    SVP_CHECK(transport.positionFrames() == 1000); // rebouclage
}

SVP_TEST(transport, loop_has_priority_over_end_of_content)
{
    // Même avec une durée de contenu connue, la boucle continue de tourner.
    Transport transport;
    transport.prepare(48000.0, 120.0, 20000);
    transport.setLoopRange(0, 5000);
    transport.setLoopEnabled(true);
    transport.play();

    transport.advance(60000); // bien au-delà du contenu
    SVP_CHECK(transport.positionFrames() == 0);
    SVP_CHECK(transport.isPlaying());
}

SVP_TEST(transport, loop_disabled_stops_at_end_of_content)
{
    Transport transport;
    transport.prepare(48000.0, 120.0, 24000);
    transport.play();
    transport.advance(23900);
    SVP_CHECK(transport.positionFrames() == 23900);
    SVP_CHECK(transport.isPlaying());

    transport.advance(500); // dépasse la fin
    SVP_CHECK(transport.positionFrames() == 24000);
    SVP_CHECK(!transport.isPlaying()); // le transport s'arrête de lui-même
}

SVP_TEST(transport, rewind_and_position_setters)
{
    Transport transport;
    transport.prepare(48000.0, 120.0, 48000);
    transport.play();
    transport.advance(10000);
    transport.rewind();
    SVP_CHECK(transport.positionFrames() == 0);
    SVP_CHECK(transport.isPlaying()); // rewind ne change pas l'état lecture/arrêt

    transport.togglePlayStop();
    SVP_CHECK(!transport.isPlaying());
    transport.togglePlayStop();
    SVP_CHECK(transport.isPlaying());

    transport.setPositionFrames(12345);
    SVP_CHECK(transport.positionFrames() == 12345);
}

SVP_TEST(transport, loop_range_is_normalised)
{
    Transport transport;
    transport.prepare(48000.0, 120.0);
    transport.setLoopRange(-100, -50); // bornes invalides
    SVP_CHECK(transport.loopStart() == 0);
    SVP_CHECK(transport.loopEnd() == 0);

    transport.setLoopRange(500, 100); // fin avant début : ramenée au début
    SVP_CHECK(transport.loopStart() == 500);
    SVP_CHECK(transport.loopEnd() == 500);
}
