/*
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */



#ifndef _STLP_DEFALLOC_H
#define _STLP_DEFALLOC_H

# ifndef _STLP_OUTERMOST_HEADER_ID
#  define _STLP_OUTERMOST_HEADER_ID 0xa005
#  include <stl/_prolog.h>
# endif

# if defined (_STLP_DEBUG) && ! defined ( _STLP_DEBUG_H )
#  include <stl/debug/_debug.h>
# endif

#if defined (_STLP_USE_NEW_STYLE_HEADERS)
# include <cstddef>
# include <cstdlib>
# include <cstring>
# include <cassert>
#else
# include <stddef.h>
# include <stdlib.h>
# include <string.h>
# include <assert.h>
#endif

# include <new>

#ifdef _STLP_THREADS
# include <stl/_threads.h>
#endif

# if !defined (__THROW_BAD_ALLOC) && !defined(_STLP_USE_EXCEPTIONS)
#   if defined (_STLP_USE_NEW_STYLE_HEADERS)
#    include <cstdio>
#   else
#    include <stdio.h>
#   endif
# endif
#  include <stl/_alloc.h>

#ifdef _STLP_USE_NAMESPACES
# ifdef _STLP_BROKEN_USING_DIRECTIVE
using namespace STLPORT;
# else
using STLPORT::allocator;
# endif /* _STLP_BROKEN_USING_DIRECTIVE */
#endif /*  _STLP_USE_NAMESPACES */

# if (_STLP_OUTERMOST_HEADER_ID == 0xa005)
#  include <stl/_epilog.h>
#  undef _STLP_OUTERMOST_HEADER_ID
# endif

#endif /* _STLP_DEFALLOC_H */

