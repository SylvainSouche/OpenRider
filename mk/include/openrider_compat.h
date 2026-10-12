/* Force-included in every C and C++ translation unit built with clang
 * (see mk/pre.llvm.mk).
 *
 * MacPorts clang 19/22 with the current macOS SDK: the SDK's <math.h> does
 * a partial include of <float.h> (#define __need_infinity_nan), but libc++'s
 * <float.h> wrapper sets its include guard on that partial include, so a
 * later full #include <float.h> is skipped and FLT_MIN/FLT_MAX vanish; in
 * the other order INFINITY/NAN vanish. Including the full <float.h> first,
 * then supplying INFINITY/NAN from builtins, restores both. Apple clang and
 * gcc are unaffected.
 * @impl 0f87-6acd-2e44-0deb */
#ifndef OPENRIDER_COMPAT_H
#define OPENRIDER_COMPAT_H
#include <float.h>
#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif
#ifndef NAN
#define NAN (__builtin_nanf(""))
#endif
#endif
