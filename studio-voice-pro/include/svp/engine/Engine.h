// Studio Voice Pro — façade moteur : la porte d'entrée d'un callback audio.
//
// Objet de ce fichier : rendre le moteur appelable depuis un callback de
// pilote (WASAPI en mode exclusif, ASIO ensuite) sans qu'aucun appel n'alloue,
// ne bloque, n'accède au disque ou ne laisse sortir une valeur invalide.
#pragma once

#include <span>
#include <vector>

#include "svp/audio/Buffer.h"
#include "svp/dsp/Mixer.h"
#include "svp/metering/Meter.h"

namespace svp::engine {

/** Moteur de rendu appelable depuis le thread audio.

    CYCLE DE VIE
      1. `prepare()` — HORS THREAD AUDIO (création du flux, changement de carte
         son, changement de taille de bloc) : alloue les buffers de travail,
         la table de pointeurs de canaux du scratch, le tableau de `ChannelStrip`
         et le mesureur. C'est le SEUL endroit du moteur qui alloue.
      2. Configuration des sources depuis le thread de contrôle (voir
         `setSource()`), puis
      3. `process()` à chaque bloc, depuis le callback du pilote.
      4. `reset()` à l'arrêt du transport.

    CONTRAT DE `process()` — à respecter et à vérifier :
      * n'alloue JAMAIS (aucun `new`, aucun `std::vector` redimensionné, aucun
        `std::string`) : vérifié par un test qui instrumente `operator new` et
        exécute 10 000 blocs après `prepare()` (suite de tests `engine`) ;
      * ne bloque JAMAIS (aucun verrou, aucune attente) et ne touche JAMAIS le
        disque ;
      * n'écrit que dans la mémoire du pilote, à travers l'`AudioBufferView`
        fournie par l'appelant, et jamais au-delà de ses bornes (les canaux ou
        trames en excès de la vue sont laissés à zéro, pas à l'abandon) ;
      * ne laisse JAMAIS sortir un NaN, un Inf ou un dénormal : le bloc
        mesuré, assaini, puis copié vers le pilote est toujours fini et normal ;
      * est `noexcept` : aucune exception ne peut traverser le callback (une
        exception qui s'échappe d'un callback audio traverse du code C et fait
        planter le processus).
      * travaille sur `destination.frames()` trames au maximum : au-delà de la
        taille de bloc annoncée à `prepare()`, le reste de la vue est mis à
        silence (violation de précondition traitée sans écriture hors bornes,
        jamais par un dépassement de tampon).

    ÉTAT ACTUEL, SANS EMBELLISSEMENT
      * `setSource()` n'est PAS thread-safe : elle écrit dans la table lue par
        `process()`. Aujourd'hui elle doit être appelée sur le même thread que
        `process()` ou pendant que le flux audio est arrêté. Le canal de
        communication IHM ↔ audio (paramètres atomiques, lissage de gain,
        file SPSC) est l'étape suivante et n'est PAS faite ici — de même que
        `values()` du mesureur, qui n'est pas encore publiable entre threads.
      * Le transport et la position de lecture ne sont pas encore branchés :
        les sources sont lues à partir de leur trame 0, sur la longueur du bloc.
        Le suivi `Transport` → position d'échantillon appartient à l'étape
        suivante (aucun glissement d'index ici, aucun lissage de gain). */
class Engine {
public:
    /** Nombre de sources configurables par défaut. */
    static constexpr int kDefaultMaxSources = 64;

    Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;
    ~Engine() = default;

    /** [HORS THREAD AUDIO] Prépare le moteur pour un format de flux donné.
        Alloue et dimensionne TOUT ce dont `process()` a besoin :
          * le tampon de travail `maxChannels × maxBlockSize` et sa table de
            pointeurs de canaux (aucune dérivation de pointeur sur le chemin
            temps réel, aucune allocation) ;
          * la table des pointeurs de sources et le tableau de `ChannelStrip`
            (contexte de mixage pré-alloué) ;
          * le mesureur crête/RMS.
        Peut être appelée plusieurs fois (changement de carte son) : tout est
        réalloué et l'état remis à zéro. `sampleRate`/`maxBlockSize` invalides
        (<= 0) sont ramenés à 48 000 Hz / 512 trames. */
    void prepare(double sampleRate, int maxBlockSize, int maxChannels,
                 int maxSources = kDefaultMaxSources);

    /** [RT] Remet le mesureur et les buffers de travail à zéro (silence).
        La configuration des sources est conservée : elle relève du thread de
        contrôle. N'alloue pas. */
    void reset() noexcept;

    /** Vrai si `prepare()` a été appelée. */
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }

    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] int maxBlockSize() const noexcept { return maxBlockSize_; }
    [[nodiscard]] int maxChannels() const noexcept { return maxChannels_; }
    [[nodiscard]] int maxSources() const noexcept { return static_cast<int>(strips_.size()); }

    /** Configure une source : le média à lire (jamais modifié : `const`) et ses
        réglages de tranche. Écriture d'un pointeur et d'une structure dans des
        tableaux pré-alloués : aucune allocation, mais NON thread-safe tant que
        le canal IHM ↔ audio n'existe pas (voir l'avertissement de classe).
        Un index hors bornes ou un pointeur `nullptr` sont acceptés sans
        erreur : une source nulle est simplement ignorée au mixage. */
    void setSource(int index, const audio::AudioBuffer* source,
                   const dsp::ChannelStrip& strip) noexcept;

    /** Nombre de sources prises en compte par `process(destination)` (borné au
        maximum de `prepare()`). */
    void setSourceCount(int count) noexcept;

    [[nodiscard]] int sourceCount() const noexcept { return sourceCount_; }

    /** [RT] Rend un bloc à partir des sources configurées, dans la mémoire du
        pilote. `process(destination)` équivaut à
        `process(destination, sources configurées, tranches configurées)`. */
    void process(audio::AudioBufferView destination) noexcept;

    /** [RT] Rend un bloc en mixant `sources` avec `strips`.
        Variante explicite : c'est celle qu'utilisent les outils et les tests,
        et celle qui n'exige aucune configuration préalable. Les tableaux sont
        fournis par l'appelant — en pratique des `std::span` sur sa propre
        mémoire, donc sans allocation. `sources` et `strips` sont appariés
        index par index ; les médias pointés ne sont jamais écrits. */
    void process(audio::AudioBufferView destination,
                 std::span<const audio::AudioBuffer* const> sources,
                 std::span<const dsp::ChannelStrip> strips) noexcept;

    /** Mesures du dernier bloc rendu (crête, RMS, maintien, écrêtage).
        Mêmes réserves que `PeakRmsMeter::values()` : à lire depuis le thread
        audio tant que la publication atomique n'est pas en place. */
    [[nodiscard]] metering::MeterValues meters() const noexcept { return meter_.values(); }

    /** Éteint le témoin d'écrêtage sans toucher aux autres mesures. [RT] */
    void clearClip() noexcept { meter_.clearClip(); }

    /** Le mesureur interne (préparé par `prepare()`). [RT] */
    [[nodiscard]] metering::PeakRmsMeter& meter() noexcept { return meter_; }

    /** Tampon de travail du moteur : c'est là que le mixage est calculé avant
        d'être copié dans la mémoire du pilote. Ne pas le redimensionner après
        `prepare()` (voir la règle de propriété d'`AudioBuffer`). */
    [[nodiscard]] audio::AudioBuffer& workBuffer() noexcept { return work_; }

private:
    double sampleRate_ = 48000.0;
    int maxBlockSize_ = 0;
    int maxChannels_ = 0;
    int sourceCount_ = 0;
    bool prepared_ = false;

    audio::AudioBuffer work_;              // tampon de travail (alloué dans prepare())
    std::vector<float*> workChannelPtrs_;  // table de pointeurs de canaux du scratch
    std::vector<const audio::AudioBuffer*> sourcePtrs_; // contexte de mixage pré-alloué
    std::vector<dsp::ChannelStrip> strips_;
    metering::PeakRmsMeter meter_;
};

} // namespace svp::engine
