// Studio Voice Pro — garde RAII FTZ/DAZ (voir l'en-tête pour le pourquoi).
#include "svp/dsp/Denormals.h"

#if SVP_HAS_MXCSR
#    include <xmmintrin.h>
#endif

namespace svp::dsp {

namespace {
// Bits du registre MXCSR (Intel SDM, vol. 1, §10.2.3 « MXCSR ») :
//   bit 15 : FTZ — tout résultat dénormal est ramené à zéro ;
//   bit 6  : DAZ — tout opérande dénormal est lu comme zéro.
constexpr unsigned int kFlushToZero = 0x8000u;
constexpr unsigned int kDenormalsAreZero = 0x0040u;
} // namespace

bool ScopedNoDenormals::supported() noexcept
{
#if SVP_HAS_MXCSR
    return true;
#else
    return false;
#endif
}

ScopedNoDenormals::ScopedNoDenormals() noexcept
{
#if SVP_HAS_MXCSR
    previousState_ = _mm_getcsr();
    _mm_setcsr(previousState_ | kFlushToZero | kDenormalsAreZero);
#endif
}

ScopedNoDenormals::~ScopedNoDenormals() noexcept
{
#if SVP_HAS_MXCSR
    _mm_setcsr(previousState_);
#endif
}

} // namespace svp::dsp
