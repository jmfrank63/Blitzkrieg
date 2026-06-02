/*
 * Copyright (c) 1999
 * Silicon Graphics
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef _STLP_RANGE_ERRORS_H
#define _STLP_RANGE_ERRORS_H


#if defined(_STLP_CAN_THROW_RANGE_ERRORS) && defined(_STLP_USE_EXCEPTIONS) \
    && !defined(_STLP_DONT_THROW_RANGE_ERRORS)
# define _STLP_THROW_RANGE_ERRORS
#endif


#if defined ( _STLP_OWN_IOSTREAMS  ) && ! defined (_STLP_EXTERN_RANGE_ERRORS) 
#  define _STLP_EXTERN_RANGE_ERRORS
# endif

#if defined (_STLP_EXTERN_RANGE_ERRORS)
_STLP_BEGIN_NAMESPACE
void  _STLP_DECLSPEC _STLP_CALL __stl_throw_range_error(const char* __msg);
void  _STLP_DECLSPEC _STLP_CALL __stl_throw_out_of_range(const char* __msg);
void  _STLP_DECLSPEC _STLP_CALL __stl_throw_length_error(const char* __msg);
void  _STLP_DECLSPEC _STLP_CALL __stl_throw_invalid_argument(const char* __msg);
void  _STLP_DECLSPEC _STLP_CALL __stl_throw_overflow_error(const char* __msg);
_STLP_END_NAMESPACE
#else

#if defined(_STLP_THROW_RANGE_ERRORS)
# ifndef _STLP_STDEXCEPT
#  include <stdexcept>
# endif
# ifndef _STLP_STRING
#  include <string>
# endif
# define _STLP_THROW_MSG(ex,msg)  throw ex(string(msg))
#else
# if defined (_STLP_WINCE)
#  define _STLP_THROW_MSG(ex,msg)  TerminateProcess(GetCurrentProcess(), 0)
# else
#  include <cstdlib>
#  include <cstdio>
#  define _STLP_THROW_MSG(ex,msg)  puts(msg),_STLP_ABORT()
# endif
#endif


_STLP_BEGIN_NAMESPACE
inline void _STLP_DECLSPEC _STLP_CALL __stl_throw_range_error(const char* __msg) { 
  _STLP_THROW_MSG(range_error, __msg); 
}

inline void _STLP_DECLSPEC _STLP_CALL __stl_throw_out_of_range(const char* __msg) { 
  _STLP_THROW_MSG(out_of_range, __msg); 
}

inline void _STLP_DECLSPEC _STLP_CALL __stl_throw_length_error(const char* __msg) { 
  _STLP_THROW_MSG(length_error, __msg); 
}

inline void _STLP_DECLSPEC _STLP_CALL __stl_throw_invalid_argument(const char* __msg) { 
  _STLP_THROW_MSG(invalid_argument, __msg); 
}

inline void _STLP_DECLSPEC _STLP_CALL __stl_throw_overflow_error(const char* __msg) { 
  _STLP_THROW_MSG(overflow_error, __msg); 
}
_STLP_END_NAMESPACE

# undef _STLP_THROW_MSG

# endif /* EXTERN_RANGE_ERRORS */


#endif /* _STLP_RANGE_ERRORS_H */

