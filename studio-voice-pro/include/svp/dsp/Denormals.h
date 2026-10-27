// Studio Voice Pro — hygiène numérique : dénormaux (FTZ/DAZ) et assainissement.
// Temps réel : aucune allocation, aucun verrou, aucun accès disque.
#pragma once

namespace svp::dsp {

/** Garde RAII « pas de dénormaux » : active FTZ (Flush-To-Zero) et DAZ
    (Denormals-Are-Zero) dans le registre MXCSR du FIL COURANT, et restaure
    la valeur précédente à la destruction.

    Pourquoi : un dénormal (ex. 1e-40) est un flottant 10 à 100 fois plus
    lent à traiter sur x86 ; un fondu, un filtre récursif ou une réverbération
    qui décroît vers le silence en produit des millions. Sur un thread audio
    à budget de temps strict, c'est une cause classique de craquements.
    L'usage normal est donc : `ScopedNoDenormals guard;` en PREMIÈRE ligne de
    `Engine::process()` — l'état est restauré en sortie de bloc.

    Portabilité :
      * MSVC x86/x64 (`_M_IX86`/`_M_X64`) et GCC/Clang x86/x64 (`__SSE2__`) :
        intrinsèques SSE `_mm_getcsr`/`_mm_setcsr`, bits FTZ (15) et DAZ (6).
      * Autres architectures (ARM64, où le comportement des dénormaux se règle
        au niveau du registre FPCR) : la garde est un NO-OP explicite, et
        `supported()` retourne faux. Ce n'est pas un oubli : le code reste
        correct sans elle, car `sanitizeBlock()` élimine de toute façon les
        dénormaux en sortie de bloc. FTZ/DAZ n'est qu'une optimisation.

    Non copiable : une garde ne se duplique pas. [RT] */
class ScopedNoDenormals {
public:
    ScopedNoDenormals() noexcept;
    ~ScopedNoDenormals() noexcept;

    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

    /** Vrai si FTZ/DAZ est réellement disponible et actif sur cette
        plateforme (voir `supported()`), faux si la garde est un no-op. */
    [[nodiscard]] static bool supported() noexcept;

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#    define SVP_HAS_MXCSR 1
#elif defined(__SSE2__)
#    define SVP_HAS_MXCSR 1
#else
#    define SVP_HAS_MXCSR 0
#endif

private:
#if SVP_HAS_MXCSR
    unsigned int previousState_ = 0;
#endif
};

} // namespace svp::dsp
