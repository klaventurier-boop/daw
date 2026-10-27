// Studio Voice Pro — tests : façade moteur appelable depuis un callback audio.
//
// Ce que ces tests prouvent, exécution à l'appui :
//   1. `Engine::process()` n'alloue RIEN : `operator new`/`operator delete`
//      sont remplacés au niveau global et comptés pendant 10 000 blocs de
//      512 trames ;
//   2. le mixage ne modifie JAMAIS un média : une piste muette est intacte
//      après une passe de mixage ;
//   3. aucune valeur invalide (NaN, ±Inf) ne sort de `process()`, et le
//      témoin d'écrêtage s'allume réellement sur une sortie invalide ;
//   4. aucun dénormal ne sort de `process()` après un fondu vers zéro.
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>
#include <vector>

#include "svp/audio/Buffer.h"
#include "svp/dsp/Denormals.h"
#include "svp/dsp/GainPan.h"
#include "svp/dsp/Mixer.h"
#include "svp/dsp/Sanitize.h"
#include "svp/engine/Engine.h"
#include "svp/metering/Meter.h"
#include "svp_test.hpp"

using namespace svp;

// ---------------------------------------------------------------------------
// Instrumentation de l'allocateur global.
//
// `operator new` / `operator delete` sont REMPLACÉS ([replacement.functions]) :
// tout appel à l'allocateur du programme passe désormais par ces fonctions.
// Le compteur n'incrémente que lorsque `g_armed` est vrai, c'est-à-dire
// uniquement autour de la boucle de `process()` : les allocations du cadre de
// test lui-même (std::string, std::function, std::vector) sont donc exclues de
// la mesure, et le résultat « 0 » ne peut pas être un artefact de comptage.
// ---------------------------------------------------------------------------
namespace {
std::size_t g_newCalls = 0;
std::size_t g_deleteCalls = 0;
bool g_armed = false;
} // namespace

void* operator new(std::size_t size)
{
    if (g_armed)
        ++g_newCalls;
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* p) noexcept
{
    if (g_armed)
        ++g_deleteCalls;
    std::free(p);
}

void operator delete[](void* p) noexcept
{
    ::operator delete(p);
}

void operator delete(void* p, std::size_t) noexcept
{
    ::operator delete(p);
}

void operator delete[](void* p, std::size_t) noexcept
{
    ::operator delete(p);
}

// Allocations sur-alignées (C++17) : elles aussi comptées, avec un en-tête
// maison qui mémorise le pointeur brut à libérer.
void* operator new(std::size_t size, std::align_val_t alignment)
{
    if (g_armed)
        ++g_newCalls;
    const std::size_t align = static_cast<std::size_t>(alignment);
    if (align <= alignof(std::max_align_t)) {
        void* p = std::malloc(size == 0 ? 1 : size);
        if (p == nullptr)
            throw std::bad_alloc();
        return p;
    }
    void* raw = std::malloc(size + align + sizeof(void*));
    if (raw == nullptr)
        throw std::bad_alloc();
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(raw) + sizeof(void*);
    void* aligned = reinterpret_cast<void*>((base + align - 1) & ~(align - 1));
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return aligned;
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return ::operator new(size, alignment);
}

void operator delete(void* p, std::align_val_t alignment) noexcept
{
    if (g_armed)
        ++g_deleteCalls;
    if (p == nullptr)
        return;
    const std::size_t align = static_cast<std::size_t>(alignment);
    if (align <= alignof(std::max_align_t)) {
        std::free(p);
        return;
    }
    std::free(reinterpret_cast<void**>(p)[-1]);
}

void operator delete[](void* p, std::align_val_t alignment) noexcept
{
    ::operator delete(p, alignment);
}

namespace {

/// Mémoire de sortie « pilote » : deux canaux contigus + table de pointeurs,
/// exactement la forme qu'un backend ASIO/WASAPI fournit au moteur.
struct DriverBuffer {
    std::vector<float> storage; // canaux × trames
    std::vector<float*> pointers;

    DriverBuffer(int channels, int frames)
        : storage(static_cast<std::size_t>(channels) * static_cast<std::size_t>(frames), 0.0f),
          pointers(static_cast<std::size_t>(channels), nullptr)
    {
        for (int ch = 0; ch < channels; ++ch)
            pointers[static_cast<std::size_t>(ch)] =
                storage.data() + static_cast<std::size_t>(ch) * static_cast<std::size_t>(frames);
    }

    [[nodiscard]] audio::AudioBufferView view(int frames) noexcept
    {
        return audio::AudioBuffer::wrapExternal(pointers.data(), static_cast<int>(pointers.size()), frames);
    }
};

audio::AudioBuffer constant(float value, int channels, int frames)
{
    audio::AudioBuffer buffer(channels, frames);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < frames; ++i)
            buffer.setSample(ch, i, value);
    return buffer;
}

bool hasSubnormal(const audio::AudioBuffer& buffer)
{
    for (int ch = 0; ch < buffer.channels(); ++ch)
        for (int i = 0; i < buffer.frames(); ++i)
            if (std::fpclassify(buffer.sample(ch, i)) == FP_SUBNORMAL)
                return true;
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Aucune allocation sur le chemin temps réel.
// ---------------------------------------------------------------------------
SVP_TEST(engine, ten_thousand_blocks_allocate_nothing)
{
    constexpr int kBlockFrames = 512;
    constexpr int kBlockCount = 10000;
    constexpr double kSampleRate = 48000.0;

    engine::Engine engine;
    engine.prepare(kSampleRate, kBlockFrames, 2, 4);

    // Deux pistes de média, remplies HORS de la fenêtre mesurée.
    audio::AudioBuffer trackA = constant(0.25f, 1, kBlockFrames);
    audio::AudioBuffer trackB(1, kBlockFrames);
    for (int i = 0; i < kBlockFrames; ++i)
        trackB.setSample(0, i, 0.1f * static_cast<float>(i % 7) / 7.0f);

    dsp::ChannelStrip stripA;
    stripA.gain = 0.8f;
    stripA.pan = -0.5f;
    dsp::ChannelStrip stripB;
    stripB.gain = 0.6f;
    stripB.pan = 0.5f;
    engine.setSource(0, &trackA, stripA);
    engine.setSource(1, &trackB, stripB);
    SVP_CHECK(engine.sourceCount() == 2);

    // Sortie « pilote » et sonde de mixage explicite : tout est alloué avant
    // l'armement du compteur.
    DriverBuffer driver(2, kBlockFrames);
    const audio::AudioBufferView driverView = driver.view(kBlockFrames);
    const audio::AudioBuffer* explicitSources[] = {&trackA, &trackB};
    std::vector<dsp::ChannelStrip> explicitStrips{stripA, stripB};

    // Pré-échauffement (hors mesure) : premier bloc sur les deux chemins.
    engine.process(driverView);
    engine.process(driverView, explicitSources, explicitStrips);

    g_newCalls = 0;
    g_deleteCalls = 0;
    g_armed = true;
    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < kBlockCount; ++block)
        engine.process(driverView);
    const auto stop = std::chrono::steady_clock::now();
    g_armed = false;

    const std::size_t allocations = g_newCalls;
    const std::size_t frees = g_deleteCalls;
    const double milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
    const double audioSeconds = static_cast<double>(kBlockCount) * kBlockFrames / kSampleRate;

    // Sortie explicite exigée par la preuve.
    std::printf("      %d blocs de %d trames via Engine::process() :\n", kBlockCount, kBlockFrames);
    std::printf("        %zu allocation(s) de tas, %zu libération(s)\n", allocations, frees);
    std::printf("        temps : %.1f ms pour %.1f s d'audio a %.0f Hz "
                "(%.2f us/bloc, x%.0f le temps reel)\n",
                milliseconds, audioSeconds, kSampleRate,
                1000.0 * milliseconds / kBlockCount, audioSeconds * 1000.0 / milliseconds);

    SVP_CHECK(allocations == 0);
    SVP_CHECK(frees == 0);

    // La sortie reste par ailleurs correcte après 10 000 blocs.
    // Trame 0 : piste A (mono, 0.25, gain 0.8) sur le canal gauche au pan -0.5,
    // soit cos((1 - 0.5) * pi/4) = 0.92388 ; la piste B vaut 0 à la trame 0
    // (0 mod 7 = 0). Attendu : 0.25 * 0.8 * 0.92388 = 0.184776.
    SVP_CHECK_NEAR(driver.storage[0], 0.184776, 1e-5);
    SVP_CHECK(std::isfinite(driver.storage[0]));
    SVP_CHECK(std::isfinite(driver.storage[kBlockFrames]));
}

// ---------------------------------------------------------------------------
// 2. Non-destructivité : un mute n'efface jamais un média.
// ---------------------------------------------------------------------------
SVP_TEST(engine, a_muted_track_keeps_its_media_intact)
{
    constexpr int kFrames = 256;
    audio::AudioBuffer muted = constant(0.9f, 2, kFrames);   // média muet
    audio::AudioBuffer audible = constant(0.1f, 2, kFrames); // média audible

    dsp::ChannelStrip mutedStrip;
    mutedStrip.mute = true;
    dsp::ChannelStrip audibleStrip;

    const audio::AudioBuffer* sources[] = {&muted, &audible};
    std::vector<dsp::ChannelStrip> strips{mutedStrip, audibleStrip};

    engine::Engine engine;
    engine.prepare(48000.0, kFrames, 2, 4);
    DriverBuffer driver(2, kFrames);
    engine.process(driver.view(kFrames), sources, strips);

    // Le média muet est INTACT, échantillon par échantillon.
    bool untouched = true;
    for (int ch = 0; ch < muted.channels(); ++ch)
        for (int i = 0; i < muted.frames(); ++i)
            untouched = untouched && (muted.sample(ch, i) == 0.9f);
    SVP_CHECK(untouched);

    // Le média audible est intact lui aussi.
    SVP_CHECK(audible.sample(0, 0) == 0.1f);
    SVP_CHECK(audible.sample(1, kFrames - 1) == 0.1f);

    // Et la destination ne contient QUE la piste audible (au centre : -3 dB).
    SVP_CHECK_NEAR(driver.storage[0], 0.1f * 0.70710678f, 1e-6);
    SVP_CHECK_NEAR(driver.storage[kFrames], 0.1f * 0.70710678f, 1e-6);
}

// ---------------------------------------------------------------------------
// 3. NaN et ±Inf ne sortent pas de process(), et le témoin s'allume.
// ---------------------------------------------------------------------------
SVP_TEST(engine, nan_and_inf_never_reach_the_driver_and_light_the_clip_meter)
{
    constexpr int kFrames = 128;
    const float kQuiet = 0.25f;

    audio::AudioBuffer corrupted = constant(kQuiet, 1, kFrames);
    corrupted.setSample(0, 10, std::nanf(""));
    corrupted.setSample(0, 60, std::numeric_limits<float>::infinity());
    corrupted.setSample(0, 90, -std::numeric_limits<float>::infinity());

    const audio::AudioBuffer* sources[] = {&corrupted};
    std::vector<dsp::ChannelStrip> strips(1);

    engine::Engine engine;
    engine.prepare(48000.0, kFrames, 2, 4);
    DriverBuffer driver(2, kFrames);
    engine.process(driver.view(kFrames), sources, strips);

    // 1. Ce qui sort est fini, normal, et jamais NaN/Inf.
    const audio::AudioBufferView out = driver.view(kFrames);
    for (int ch = 0; ch < out.channels(); ++ch) {
        for (int i = 0; i < kFrames; ++i) {
            const float v = out.sample(ch, i);
            SVP_CHECK(std::isfinite(v));
            if (!std::isfinite(v))
                break; // éviter 128 messages d'échec identiques
        }
    }
    SVP_CHECK(dsp::isBlockClean(out.toConst()));

    // 2. Le témoin d'écrêtage s'allume sur une sortie INVALIDE, alors même
    //    qu'aucun échantillon ne dépasse 1.0 en amplitude.
    const metering::MeterValues valuesWithGarbage = engine.meters();
    SVP_CHECK(valuesWithGarbage.clipped);
    SVP_CHECK(valuesWithGarbage.nonFinite);
    SVP_CHECK(valuesWithGarbage.peak < 1.0f); // la crête reste exploitable
    SVP_CHECK(std::isfinite(valuesWithGarbage.peak));
    SVP_CHECK(std::isfinite(valuesWithGarbage.rms));

    // 3. clearClip() éteint le témoin sans toucher aux autres mesures (C5).
    const float peakBefore = valuesWithGarbage.peak;
    engine.clearClip();
    SVP_CHECK(!engine.meters().clipped);
    SVP_CHECK(engine.meters().nonFinite);
    SVP_CHECK_NEAR(engine.meters().peak, peakBefore, 0.0);

    // 4. Un bloc sain ensuite ne rallume pas le témoin.
    const audio::AudioBuffer clean = constant(0.5f, 1, kFrames);
    const audio::AudioBuffer* cleanSources[] = {&clean};
    engine.process(driver.view(kFrames), cleanSources, strips);
    SVP_CHECK(!engine.meters().clipped);
    SVP_CHECK(engine.isPrepared());
}

// ---------------------------------------------------------------------------
// 4. Aucun dénormal ne sort d'un fondu vers zéro.
// ---------------------------------------------------------------------------
SVP_TEST(engine, a_fade_to_zero_leaves_no_denormals_in_the_output)
{
    constexpr int kFrames = 512;

    // A. La garde FTZ/DAZ est-elle réellement active ? (x86/x64 : oui ;
    //    ailleurs la garde est un no-op documenté et l'assainissement de
    //    sortie reste le filet de sécurité.)
    if (dsp::ScopedNoDenormals::supported()) {
        const dsp::ScopedNoDenormals guard;
        const float underflowed = 1e-30f * 1e-30f; // ~1e-60 : dénormal sans FTZ
        SVP_CHECK(underflowed == 0.0f);
        std::printf("      FTZ/DAZ actif : 1e-30 * 1e-30 ramené à %.1e\n",
                    static_cast<double>(underflowed));
    } else {
        std::printf("      FTZ/DAZ indisponible sur cette plateforme (garde no-op)\n");
    }

    // B. Fondu d'un signal minuscule mais NORMAL : la queue tombe dans les
    //    dénormaux si rien ne les arrête.
    const float smallestNormal = std::ldexp(1.0f, -126); // ~1.18e-38
    audio::AudioBuffer source(1, kFrames);
    for (int i = 0; i < kFrames; ++i)
        source.setSample(0, i, smallestNormal);
    audio::AudioBuffer destination(1, kFrames);
    {
        const dsp::ScopedNoDenormals guard;
        dsp::applyGainRampTo(source.constView(), destination.view(), 1.0f, 0.0f);
    }

    // Avec FTZ, la queue sous-normale est ramenée à zéro exactement par le
    // matériel ; sans FTZ disponible, seul l'assainissement de sortie du
    // moteur (partie D) garantit la propreté du bloc.
    if (dsp::ScopedNoDenormals::supported())
        SVP_CHECK(dsp::isBlockClean(destination.constView()));
    SVP_CHECK(destination.sample(0, kFrames - 1) == 0.0f);
    SVP_CHECK(source.sample(0, 0) == smallestNormal); // source intacte

    // C. Sans hygiène (aucune garde), le même fondu laisse bel et bien des
    //    dénormaux : le test précédent n'est donc pas vide.
    audio::AudioBuffer unguarded(1, kFrames);
    dsp::applyGainRampTo(source.constView(), unguarded.view(), 1.0f, 0.0f);
    if (dsp::ScopedNoDenormals::supported()) {
        SVP_CHECK(hasSubnormal(unguarded));
        SVP_CHECK(!dsp::isBlockClean(unguarded.constView()));
    }

    // D. Bout en bout dans le moteur : un média qui contient déjà des
    //    dénormaux ne peut pas en faire sortir.
    audio::AudioBuffer mediaWithDenormals(1, kFrames);
    for (int i = 0; i < kFrames; ++i)
        mediaWithDenormals.setSample(0, i, smallestNormal * (1.0f - static_cast<float>(i) / static_cast<float>(kFrames)));
    SVP_CHECK(hasSubnormal(mediaWithDenormals)); // entrée réellement sale

    const audio::AudioBuffer* sources[] = {&mediaWithDenormals};
    std::vector<dsp::ChannelStrip> strips(1);
    engine::Engine engine;
    engine.prepare(48000.0, kFrames, 2, 2);
    DriverBuffer driver(2, kFrames);
    engine.process(driver.view(kFrames), sources, strips);
    SVP_CHECK(dsp::isBlockClean(driver.view(kFrames).toConst()));
    for (int i = 0; i < kFrames; ++i) {
        SVP_CHECK(std::fpclassify(driver.storage[i]) != FP_SUBNORMAL);
        if (std::fpclassify(driver.storage[i]) == FP_SUBNORMAL)
            break;
    }
}

// ---------------------------------------------------------------------------
// 5. Bornes : la vue du pilote plus grande que la taille de bloc préparée.
// ---------------------------------------------------------------------------
SVP_TEST(engine, an_oversized_driver_view_is_silenced_not_overwritten)
{
    constexpr int kPreparedFrames = 64;
    constexpr int kDriverFrames = 256;

    audio::AudioBuffer track = constant(1.0f, 1, kDriverFrames);
    const audio::AudioBuffer* sources[] = {&track};
    std::vector<dsp::ChannelStrip> strips(1);

    engine::Engine engine;
    engine.prepare(48000.0, kPreparedFrames, 2, 2);
    DriverBuffer driver(2, kDriverFrames);
    for (float& v : driver.storage)
        v = 999.0f; // mémoire « sale » au départ
    engine.process(driver.view(kDriverFrames), sources, strips);

    // Les 64 premières trames sont rendues...
    SVP_CHECK(driver.storage[0] > 0.0f);
    // ...et les suivantes sont mises à silence, pas laissées en l'état.
    for (int i = kPreparedFrames; i < kDriverFrames; ++i) {
        SVP_CHECK(driver.storage[i] == 0.0f);
        SVP_CHECK(driver.storage[kDriverFrames + i] == 0.0f);
    }

    // Un moteur non préparé rend le silence, jamais de la mémoire non initialisée.
    engine::Engine fresh;
    DriverBuffer other(2, 32);
    for (float& v : other.storage)
        v = 42.0f;
    fresh.process(other.view(32), sources, strips);
    SVP_CHECK(other.storage[0] == 0.0f);
    SVP_CHECK(!fresh.isPrepared());
}
