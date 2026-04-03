/* libxmp_compat.h — compatibility shim for baremetal openfpgaOS
 *
 * Force-included via -include libxmp_compat.h in the Makefile.
 * Provides errno constants and math wrappers that the SDK's
 * minimal libc doesn't expose.
 */
#ifndef LIBXMP_COMPAT_H
#define LIBXMP_COMPAT_H

#include <errno.h>

/* errno values that libxmp uses but baremetal musl may omit */
#ifndef EISDIR
#define EISDIR  21
#endif
#ifndef ENOTSUP
#define ENOTSUP 95
#endif

/* ── Math wrappers ───────────────────────────────────────────────
 *
 * The openfpgaOS SDK provides float-precision math via jump table
 * (powf, logf, ceilf, floorf, etc.) but NOT the double versions
 * (pow, log, ceil, floor, etc.) that libxmp calls.
 *
 * Since the VexRiscv only has single-precision FPU (rv32imafc),
 * double ops would be soft-emulated anyway — so casting to float
 * loses nothing in practice and avoids pulling in soft-float libm.
 */
#include <math.h>

static inline double pow(double b, double e)  { return (double)powf((float)b, (float)e); }
static inline double log(double x)            { return (double)logf((float)x); }
static inline double log2(double x)           { return (double)log2f((float)x); }
static inline double exp(double x)            { return (double)expf((float)x); }
static inline double ceil(double x)           { return (double)ceilf((float)x); }
static inline double floor(double x)          { return (double)floorf((float)x); }
static inline double round(double x)          { return (double)roundf((float)x); }
static inline double fabs(double x)           { return (double)fabsf((float)x); }
static inline double fmod(double x, double y) { return (double)fmodf((float)x, (float)y); }

#endif /* LIBXMP_COMPAT_H */
