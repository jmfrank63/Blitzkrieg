


# define _STLP_UINT32_T unsigned int

# define _STLP_HAS_NO_NEW_C_HEADERS




#ifdef __linux__

#   define _STLP_NO_NATIVE_MBSTATE_T      1
#   define _STLP_NO_NATIVE_WIDE_FUNCTIONS 1
#   define _STLP_NO_NATIVE_WIDE_STREAMS   1
#   define _STLP_NO_LONG_DOUBLE   1

# define __wcstoull_internal_defined	1
# define __wcstoll_internal_defined	1

#endif /* __COMO__ under __linux__ */

#ifdef __USING_x86SVR3x_WITH_COMO /* SCO et al */
/* UNIX 386+ SVR3 mods made with __USING_x86SVR3x_WITH_COMO
   in other sources, not here */
#    define atan2l atan2
#    define cosl cos
#    define sinl sin
#    define sqrtl sqrt
#    include <math.h>
     inline long double expl(long double arg) { return exp(arg); }
     inline long double logl(long double arg) { return log(arg); }
#    define log10l log10

#    define sinhl sinh
#    define coshl cosh
#    define fabsl fabs
namespace std {
 inline int min(int a, int b) { return a>b ? b : a; }
}
#endif

#ifdef sun

#ifdef solarissparc
#define __USING_SOLARIS_SPARC_WITH_COMO /* show this in the source when grep'ing for COMO */

#include <math.h>
#    define sinf sin
#    define sinl sin
#    define sinhf sinh
#    define sinhl sinh
#    define cosf cos
#    define cosl cos
#    define coshf cosh
#    define coshl cosh
#    define atan2l atan2
#    define atan2f atan2
     inline float logf(float arg) { return log(arg); }
     inline long double logl(long double arg) { return log(arg); }
#    define log10f log10
#    define log10l log10
#    define expf exp
     inline long double expl(long double arg) { return exp(arg); }
#    define sqrtf sqrt
#    define sqrtl sqrt
#    define fabsf fabs
#    define fabsl fabs
#else
#define __USING_SUNOS_WITH_COMO

#define __unix 1
#define __EXTENSIONS__ /* This might create undue noise somewhere */
#endif
#endif /* sun */

#if defined(__NetBSD__)
#undef _STLP_NO_FUNCTION_PTR_IN_CLASS_TEMPLATE
#define __unix 1

#include <sys/cdefs.h>
#undef __END_DECLS
#define __END_DECLS }

#include <sys/cdefs.h>
#undef __RENAME
#define __RENAME(x)

#define wchar_t __COMO_WCHAR_T
#include <stddef.h>
#undef wchar_t

#include <math.h>
# ifdef BORIS_DISABLED
#    define atan2l atan2
#    define cosl cos
#    define sinl sin
#    define sqrtl sqrt
     inline long double expl(long double arg) { return exp(arg); }
     inline long double logl(long double arg) { return log(arg); }
#    define log10l log10
#    define sinhl sinh
#    define coshl cosh
#    define fabsl fabs
# endif
#endif /* __NetBSD__ under __COMO__ */


#define _STLP_NO_DRAND48

#define _STLP_PARTIAL_SPECIALIZATION_SYNTAX
#define _STLP_NO_USING_CLAUSE_IN_CLASS


#if __COMO_VERSION__ >= 4245
#define _STLP_NO_EXCEPTION_HEADER /**/
#endif
#define _STLP_NO_BAD_ALLOC /**/
#define _STLP_USE_AUTO_PTR_CONVERSIONS /**/

#if __COMO_VERSION__ >= 4245
#include <stdexcept.stdh>
#endif

# if defined (_MSC_VER)
#  define _STLP_WCHAR_T_IS_USHORT 1
#  if _MSC_VER <= 1200
#   define _STLP_VENDOR_GLOBAL_CSTD
#  endif
#  if _MSC_VER < 1100
#   define _STLP_NO_BAD_ALLOC 1
#   define _STLP_NO_EXCEPTION_HEADER 1
#   define _STLP_NO_NEW_NEW_HEADER 1
#   define _STLP_NO_NEW_IOSTREAMS 1
#  endif
# endif



