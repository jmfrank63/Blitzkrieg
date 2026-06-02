/*
 * Copyright (c) 1999
 * Silicon Graphics Computer Systems, Inc.
 *
 * Copyright (c) 1999 
 * Boris Fomitchev
 *
 * This material is provided "as is", with absolutely no warranty expressed
 * or implied. Any use is at your own risk.
 *
 * Permission to use or copy this software for any purpose is hereby granted 
 * without fee, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *
 */ 




#ifndef _STLP_STDIO_FILE_H
#define _STLP_STDIO_FILE_H




#ifndef _STLP_CSTDIO
# include <cstdio>
#endif
#ifndef _STLP_CSTDDEF
# include <cstddef>
#endif

#if defined(__MSL__)
# include <unix.h>	// get the definition of fileno
#endif

_STLP_BEGIN_NAMESPACE

#if !defined(_STLP_WINCE)
#if !defined(_STLP_USE_GLIBC) && \
    ( defined(__sgi) || \
      ( defined(__sun) && ! defined (_LP64) )  || \
      defined (__osf__) || defined(__DECCXX) || \
      defined (_STLP_MSVC) || defined (__ICL) || defined (__MINGW32__) || defined(__DJGPP) || defined (_AIX))

#if defined ( _MSC_VER ) || defined (__ICL) || defined (__MINGW32__) || defined(__DJGPP)
typedef  char* _File_ptr_type;
#else
typedef  unsigned char* _File_ptr_type;
#endif

inline int   _FILE_fd(const FILE *__f) { return __f->_file; }
inline char* _FILE_I_begin(const FILE *__f) { return (char*) __f->_base; }
inline char* _FILE_I_next(const FILE *__f) { return (char*) __f->_ptr; }  
inline char* _FILE_I_end(const FILE *__f)
  { return (char*) __f->_ptr + __f->_cnt; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return __f->_cnt; }

inline char& _FILE_I_preincr(FILE *__f)
  { --__f->_cnt; return *(char*) (++__f->_ptr); }
inline char& _FILE_I_postincr(FILE *__f)
  { --__f->_cnt; return *(char*) (__f->_ptr++); }
inline char& _FILE_I_predecr(FILE *__f)
  { ++__f->_cnt; return *(char*) (--__f->_ptr); }
inline char& _FILE_I_postdecr(FILE *__f)
  { ++__f->_cnt; return *(char*) (__f->_ptr--); }
inline void  _FILE_I_bump(FILE *__f, int __n)
  { __f->_ptr += __n; __f->_cnt -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char* __end) {
  __f->_base = (_File_ptr_type) __begin;
  __f->_ptr  = (_File_ptr_type) __next;
  __f->_cnt  = __end - __next;
}

# define _STLP_FILE_I_O_IDENTICAL 1

# elif defined(_STLP_SCO_OPENSERVER) || defined(__NCR_SVR)

typedef  unsigned char* _File_ptr_type;

inline int   _FILE_fd(const FILE *__f) { return __f->__file; }
inline char* _FILE_I_begin(const FILE *__f) { return (char*) __f->__base; }
inline char* _FILE_I_next(const FILE *__f) { return (char*) __f->__ptr; }
inline char* _FILE_I_end(const FILE *__f)
  { return (char*) __f->__ptr + __f->__cnt; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return __f->__cnt; }

inline char& _FILE_I_preincr(FILE *__f)
  { --__f->__cnt; return *(char*) (++__f->__ptr); }
inline char& _FILE_I_postincr(FILE *__f)
  { --__f->__cnt; return *(char*) (__f->__ptr++); }
inline char& _FILE_I_predecr(FILE *__f)
  { ++__f->__cnt; return *(char*) (--__f->__ptr); }
inline char& _FILE_I_postdecr(FILE *__f)
  { ++__f->__cnt; return *(char*) (__f->__ptr--); }
inline void  _FILE_I_bump(FILE *__f, int __n)
  { __f->__ptr += __n; __f->__cnt -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char* __end) {
  __f->__base = (_File_ptr_type) __begin;
  __f->__ptr  = (_File_ptr_type) __next;
  __f->__cnt  = __end - __next;
}

# define _STLP_FILE_I_O_IDENTICAL 1

# elif defined(__sun) && defined( _LP64)

typedef long _File_ptr_type;

inline int _FILE_fd(const FILE *__f) { return (int) __f->__pad[2]; }
inline char* _FILE_I_begin(const FILE *__f) { return (char*)
__f->__pad[1]; }
inline char* _FILE_I_next(const FILE *__f) { return (char*)
__f->__pad[0]; }
inline char* _FILE_I_end(const FILE *__f)
{ return (char*) __f->__pad[0] + __f->__pad[3]; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return __f->__pad[3]; }

inline char& _FILE_I_preincr(FILE *__f)
{ --__f->__pad[3]; return *(char*) (++__f->__pad[0]); }
inline char& _FILE_I_postincr(FILE *__f)
{ --__f->__pad[3]; return *(char*) (__f->__pad[0]++); }
inline char& _FILE_I_predecr(FILE *__f)
{ ++__f->__pad[3]; return *(char*) (--__f->__pad[0]); }
inline char& _FILE_I_postdecr(FILE *__f)
{ ++__f->__pad[3]; return *(char*) (__f->__pad[0]--); }
inline void _FILE_I_bump(FILE *__f, long __n)
{ __f->__pad[0] += __n; __f->__pad[3] -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char*
__end) {
__f->__pad[1] = (_File_ptr_type) __begin;
__f->__pad[0] = (_File_ptr_type) __next;
__f->__pad[3] = __end - __next;
}

# define _STLP_FILE_I_O_IDENTICAL

#elif defined (__CYGWIN__) || defined(__FreeBSD__)  || defined(__NetBSD__) \
  || defined(__amigaos__) || ( defined(__GNUC__) && defined(__APPLE__) )

inline int _FILE_fd(const FILE *__f) { return __f->_file; }
inline char* _FILE_I_begin(const FILE *__f) { return (char*)
						__f->_bf._base; }
inline char* _FILE_I_next(const FILE *__f) { return (char*) __f->_p; } 
inline char* _FILE_I_end(const FILE *__f)
{ return (char*) __f->_p + __f->_r; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return __f->_r; }

inline char& _FILE_I_preincr(FILE *__f)
{ --__f->_r; --__f->_bf._size; return *(char*) (++__f->_p); }
inline char& _FILE_I_postincr(FILE *__f)
{ --__f->_r; --__f->_bf._size; return *(char*) (__f->_p++); }
inline char& _FILE_I_predecr(FILE *__f)
{ ++__f->_r; ++ __f->_bf._size; return *(char*) (--__f->_p); }
inline char& _FILE_I_postdecr(FILE *__f)
{ ++__f->_r; ++__f->_bf._size; return *(char*) (__f->_p--); }
inline void _FILE_I_bump(FILE *__f, int __n)
{ __f->_p += __n; __f->_bf._size+=__n; __f->_r -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char*
			__end) {
  __f->_bf._base = (unsigned char*) __begin;
  __f->_p = (unsigned char*) __next;
  __f->_r = __f->_bf._size = __end - __next;
}
inline char* _FILE_O_begin(const FILE *__f) { return (char*)
						__f->_bf._base; }
inline char* _FILE_O_next(const FILE *__f) { return (char*) __f->_p; } 
inline char* _FILE_O_end(const FILE *__f)
{ return (char*) __f->_p + __f->_w; }

inline ptrdiff_t _FILE_O_avail(const FILE *__f) { return __f->_w; }

inline char& _FILE_O_preincr(FILE *__f)
{ --__f->_w; --__f->_bf._size; return *(char*) (++__f->_p); }
inline char& _FILE_O_postincr(FILE *__f)
{ --__f->_w; --__f->_bf._size; return *(char*) (__f->_p++); }
inline char& _FILE_O_predecr(FILE *__f)
{ ++__f->_w; ++__f->_bf._size; return *(char*) (--__f->_p); }
inline char& _FILE_O_postdecr(FILE *__f)
{ ++__f->_w; ++__f->_bf._size; return *(char*) (__f->_p--); }
inline void _FILE_O_bump(FILE *__f, int __n)
{ __f->_p += __n; __f->_bf._size+=__n; __f->_w -= __n; }

inline void _FILE_O_set(FILE *__f, char* __begin, char* __next, char*
			__end) {
  __f->_bf._base = (unsigned char*) __begin;
  __f->_p = (unsigned char*) __next;
  __f->_w = __f->_bf._size = __end - __next;
}

# undef _STLP_FILE_I_O_IDENTICAL

#elif defined(_STLP_USE_GLIBC)

inline int   _FILE_fd(const FILE *__f) { return __f->_fileno; }
inline char* _FILE_I_begin(const FILE *__f) { return __f->_IO_read_base; }
inline char* _FILE_I_next(const FILE *__f)  { return __f->_IO_read_ptr; }
inline char* _FILE_I_end(const FILE *__f)   { return __f->_IO_read_end; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) 
  { return __f->_IO_read_end - __f->_IO_read_ptr; }

inline char& _FILE_I_preincr(FILE *__f)  { return *++__f->_IO_read_ptr; }
inline char& _FILE_I_postincr(FILE *__f) { return *__f->_IO_read_ptr++; }
inline char& _FILE_I_predecr(FILE *__f)  { return *--__f->_IO_read_ptr; }
inline char& _FILE_I_postdecr(FILE *__f) { return *__f->_IO_read_ptr--; }
inline void  _FILE_I_bump(FILE *__f, int __n) { __f->_IO_read_ptr += __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char* __end) {
  __f->_IO_read_base = __begin; 
  __f->_IO_read_ptr  = __next; 
  __f->_IO_read_end  = __end; 
}

inline char* _FILE_O_begin(const FILE *__f) { return __f->_IO_write_base; }
inline char* _FILE_O_next(const FILE *__f)  { return __f->_IO_write_ptr; }
inline char* _FILE_O_end(const FILE *__f)   { return __f->_IO_write_end; }

inline ptrdiff_t _FILE_O_avail(const FILE *__f) 
  { return __f->_IO_write_end - __f->_IO_write_ptr; }

inline char& _FILE_O_preincr(FILE *__f)  { return *++__f->_IO_write_ptr; }
inline char& _FILE_O_postincr(FILE *__f) { return *__f->_IO_write_ptr++; }
inline char& _FILE_O_predecr(FILE *__f)  { return *--__f->_IO_write_ptr; }
inline char& _FILE_O_postdecr(FILE *__f) { return *__f->_IO_write_ptr--; }
inline void  _FILE_O_bump(FILE *__f, int __n) { __f->_IO_write_ptr += __n; }

inline void _FILE_O_set(FILE *__f, char* __begin, char* __next, char* __end) {
  __f->_IO_write_base = __begin; 
  __f->_IO_write_ptr  = __next; 
  __f->_IO_write_end  = __end; 

}

#elif defined(__hpux) /* && defined(__hppa) && defined(__HP_aCC)) */

#ifndef _INCLUDE_HPUX_SOURCE
extern "C" unsigned char *__bufendtab[];
#  undef  _bufend
#  define _bufend(__p) \
     (*(((__p)->__flag & _IOEXT)  ? &(((_FILEX *)(__p))->__bufendp)      \
 				    : &(__bufendtab[(__p) - __iob])))
 
#  define _bufsiz(__p)  (_bufend(__p) - (__p)->__base)
#endif /* _INCLUDE_HPUX_SOURCE */

#if defined(_STLP_HPACC_BROKEN_BUFEND)
#  undef  _bufend
#  define _bufend(__p) \
     (*(((__p)->__flag & _IOEXT)  ? &((__REINTERPRET_CAST(_FILEX*,(__p)))->__bufendp)  \
                               : &(__bufendtab[__REINTERPRET_CAST(FILE*,(__p)) - __iob])))
#endif

inline int   _FILE_fd(const FILE *__f) { return fileno(__CONST_CAST(FILE *,__f)); }
inline char* _FILE_I_begin(const FILE *__f) { return (__REINTERPRET_CAST(char*, __f->__base)); }
inline char* _FILE_I_next(const FILE *__f)  { return (__REINTERPRET_CAST(char*, __f->__ptr)); }
inline char* _FILE_I_end(const FILE *__f)   { return (__REINTERPRET_CAST(char*, __f->__ptr +__f->__cnt)); }

inline ptrdiff_t _FILE_I_avail(const FILE *__f)  { return __f->__cnt; }

inline char& _FILE_I_preincr(FILE *__f)  { --__f->__cnt; return *__REINTERPRET_CAST(char*, ++__f->__ptr); }
inline char& _FILE_I_postincr(FILE *__f) { --__f->__cnt; return *__REINTERPRET_CAST(char*, __f->__ptr++); }
inline char& _FILE_I_predecr(FILE *__f)  { ++__f->__cnt; return *__REINTERPRET_CAST(char*,--__f->__ptr); }
inline char& _FILE_I_postdecr(FILE *__f) { ++__f->__cnt; return *__REINTERPRET_CAST(char*,__f->__ptr--); }
inline void  _FILE_I_bump(FILE *__f, int __n) { __f->__cnt -= __n; __f->__ptr += __n; }
 
inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char* __end) {
# if defined(__hpux)
   if( (unsigned long) (__f - &__iob[0]) > _NFILE)
        __f->__flag |= _IOEXT;  // used by stdio's _bufend macro and goodness knows what else...
# endif
  __f->__cnt  = __end - __next; 
  __f->__base = __REINTERPRET_CAST(unsigned char*, __begin); 
  __f->__ptr  = __REINTERPRET_CAST(unsigned char*, __next);
  _bufend(__f) = __REINTERPRET_CAST(unsigned char*, __end); 
}


# define _STLP_FILE_I_O_IDENTICAL

#elif defined (__BORLANDC__)

typedef unsigned char* _File_ptr_type;

inline int _FILE_fd(const FILE *__f) { return __f->fd; }
inline char* _FILE_I_begin(const FILE *__f) { return (char*) __f->buffer;
}
inline char* _FILE_I_next(const FILE *__f) 
{ return (char*)__f->curp; } 
inline char* _FILE_I_end(const FILE *__f)
{ return (char*) __f->curp + __f->level; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return __f->level; }

inline char& _FILE_I_preincr(FILE *__f)
{ --__f->level; return *(char*) (++__f->curp); }
inline char& _FILE_I_postincr(FILE *__f)
{ --__f->level; return *(char*) (__f->curp++); }
inline char& _FILE_I_predecr(FILE *__f)
{ ++__f->level; return *(char*) (--__f->curp); }
inline char& _FILE_I_postdecr(FILE *__f)
{ ++__f->level; return *(char*) (__f->curp--); }
inline void _FILE_I_bump(FILE *__f, int __n)
{ __f->curp += __n; __f->level -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char*
			__end) {
  __f->buffer = (_File_ptr_type) __begin;
  __f->curp = (_File_ptr_type) __next;
  __f->level = __end - __next;
}

# define _STLP_FILE_I_O_IDENTICAL

#elif defined( __MWERKS__ )

# if __dest_os == __mac_os
inline int   _FILE_fd(const FILE *__f) { return ::fileno(__CONST_CAST(FILE*, __f)); }
# else
inline int   _FILE_fd(const FILE *__f) { return ::_fileno(__CONST_CAST(FILE*, __f)); }
# endif

inline char* _FILE_I_begin(const FILE *__f) { return __REINTERPRET_CAST(char*, __f->buffer); }
inline char* _FILE_I_next(const FILE *__f) { return __REINTERPRET_CAST(char*, __f->buffer_ptr); }

inline char* _FILE_I_end(const FILE *__f) { return __REINTERPRET_CAST(char*, __f->buffer_ptr + __f->buffer_len); }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return __f->buffer_len; }

inline char& _FILE_I_preincr(FILE *__f)
  { --__f->buffer_len; return *(char*) (++__f->buffer_ptr); }
inline char& _FILE_I_postincr(FILE *__f)
  { --__f->buffer_len; return *(char*) (__f->buffer_ptr++); }
inline char& _FILE_I_predecr(FILE *__f)
  { ++__f->buffer_len; return *(char*) (--__f->buffer_ptr); }
inline char& _FILE_I_postdecr(FILE *__f)
  { ++__f->buffer_len; return *(char*) (__f->buffer_ptr--); }
inline void  _FILE_I_bump(FILE *__f, int __n)
  { __f->buffer_ptr += __n; __f->buffer_len -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char* __end) {
  __f->buffer = __REINTERPRET_CAST(unsigned char*, __begin);
  __f->buffer_ptr   = __REINTERPRET_CAST(unsigned char*, __next);
  __f->buffer_len  = __end - __next;
  __f->buffer_size = __end - __begin;
}

# define _STLP_FILE_I_O_IDENTICAL

#elif defined(__MRC__) || defined(__SC__)		//*TY 02/24/2000 - added support for MPW

inline int   _FILE_fd(const FILE *__f) { return __f->_file; }

inline char* _FILE_I_begin(const FILE *__f) { return (char*) __f->_base; }

inline char* _FILE_I_next(const FILE *__f) { return (char*) __f->_ptr; }

inline char* _FILE_I_end(const FILE *__f) { return (char*)__f->_end; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return __f->_cnt; }

inline char& _FILE_I_preincr(FILE *__f) { --__f->_cnt; return*(char*) (++__f->_ptr); }


inline char& _FILE_I_postincr(FILE *__f) { --__f->_cnt; return*(char*) (__f->_ptr++); }

inline char& _FILE_I_predecr(FILE *__f) { ++__f->_cnt; return*(char*) (--__f->_ptr); }

inline char& _FILE_I_postdecr(FILE *__f) { ++__f->_cnt; return*(char*) (__f->_ptr--); }

inline void _FILE_I_bump(FILE *__f, int __n) { __f->_cnt -= __n; __f->_ptr += __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char* __end)
{
	__f->_base = (unsigned char*)__begin;
	__f->_ptr  = (unsigned char*)__next;
	__f->_end  = (unsigned char*)__end;
	__f->_cnt  = __end - __next;
	__f->_size = __end - __begin;
}

# define _STLP_FILE_I_O_IDENTICAL

#elif defined (__MVS__)

typedef unsigned char* _File_ptr_type;

inline int _FILE_fd(const FILE *__f) { return fileno(__CONST_CAST(FILE
								  *,__f)); }
inline char* _FILE_I_begin(const FILE *__f) { return (char*)
						__f->__fp->__bufPtr; }
inline char* _FILE_I_next(const FILE *__f) { return (char*)
					       __f->__fp->__bufPtr; }
inline char* _FILE_I_end(const FILE *__f)
{ return (char*) __f->__fp->__bufPtr + __f->__fp->__countIn; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return
						    __f->__fp->__countIn; }

inline char& _FILE_I_preincr(FILE *__f)
{ --__f->__fp->__countIn; return *(char*) (++__f->__fp->__bufPtr); }
inline char& _FILE_I_postincr(FILE *__f)
{ --__f->__fp->__countIn; return *(char*) (__f->__fp->__bufPtr++); }
inline char& _FILE_I_predecr(FILE *__f)
{ ++__f->__fp->__countIn; return *(char*) (--__f->__fp->__bufPtr); }
inline char& _FILE_I_postdecr(FILE *__f)
{ ++__f->__fp->__countIn; return *(char*) (__f->__fp->__bufPtr--); }
inline void _FILE_I_bump(FILE *__f, int __n)
{ __f->__fp->__bufPtr += __n; __f->__fp->__countIn -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char*
			__end) {
  if(__f->__fp) {
    __f->__fp->__bufPtr = (_File_ptr_type) __next;
    __f->__fp->__countIn = __end - __next;
  }
}

inline char* _FILE_O_begin(const FILE *__f) { return (char*)__f->__fp->__bufPtr;}
inline char* _FILE_O_next(const FILE *__f) { return (char*) __f->__fp->__bufPtr;}
inline char* _FILE_O_end(const FILE *__f) { return (char*) __f->__fp->__bufPtr + __f->__fp->__countOut; }
inline ptrdiff_t _FILE_O_avail(const FILE *__f) { return __f->__fp->__countOut; }

inline char& _FILE_O_preincr(FILE *__f)
{ --__f->__fp->__countOut; return *(char*) (++__f->__fp->__bufPtr); }
inline char& _FILE_O_postincr(FILE *__f)
{ --__f->__fp->__countOut; return *(char*) (__f->__fp->__bufPtr++); }
inline char& _FILE_O_predecr(FILE *__f)
{ ++__f->__fp->__countOut; return *(char*) (--__f->__fp->__bufPtr); }
inline char& _FILE_O_postdecr(FILE *__f)
{ ++__f->__fp->__countOut; return *(char*) (__f->__fp->__bufPtr--); }
inline void _FILE_O_bump(FILE *__f, int __n)
{ __f->__fp->__bufPtr += __n; __f->__fp->__countOut -= __n; }

inline void _FILE_O_set(FILE *__f, char* __begin, char* __next, char*
			__end) {
  if(__f->__fp) {
    __f->__fp->__bufPtr = (_File_ptr_type) __next;
    __f->__fp->__countOut = __end - __next;
  }
}

#elif defined(__QNXNTO__)

inline int _FILE_fd(const FILE *__f) { return __f->_handle;
}
inline char* _FILE_I_begin(const FILE *__f) { return
                                                (char*) __f->_base; }
inline char* _FILE_I_next(const FILE *__f) { return
                                               (char*) __f->_ptr; }
inline char* _FILE_I_end(const FILE *__f)
{ return (char*) __f->_ptr + __f->_cnt; }

inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return
                                                    __f->_cnt; }

inline char& _FILE_I_preincr(FILE *__f)
{ --__f->_cnt; return *(char*) (++__f->_ptr); }
inline char& _FILE_I_postincr(FILE *__f)
{ --__f->_cnt; return *(char*) (__f->_ptr++); }
inline char& _FILE_I_predecr(FILE *__f)
{ ++__f->_cnt; return *(char*) (--__f->_ptr); }
inline char& _FILE_I_postdecr(FILE *__f)
{ ++__f->_cnt; return *(char*) (__f->_ptr--); }
inline void _FILE_I_bump(FILE *__f, int __n)
{ __f->_ptr += __n; __f->_cnt -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char*
                        __next, char*
                        __end) {
  __f->_base = (unsigned char*) __begin;
  __f->_ptr = (unsigned char*) __next;
  __f->_cnt = __end - __next;
}

# define _STLP_FILE_I_O_IDENTICAL
 
#elif defined(__WATCOMC__)                   // Nikolaev
 
inline int       _FILE_fd      (const FILE *__f) { return __f->_handle;}
inline char*     _FILE_I_begin (const FILE *__f) { return __REINTERPRET_CAST(char*, __f->_link); }
inline char*     _FILE_I_next  (const FILE *__f) { return __REINTERPRET_CAST(char*, __f->_ptr); }
inline char*     _FILE_I_end   (const FILE *__f) { return __REINTERPRET_CAST(char*, __f->_ptr + __f->_cnt); }
inline ptrdiff_t _FILE_I_avail (const FILE *__f) { return __f->_cnt; }
 
inline char& _FILE_I_preincr(FILE *__f)
{
  --__f->_cnt;
  return *__REINTERPRET_CAST(char*, ++__f->_ptr);
}
 
inline char& _FILE_I_postincr(FILE *__f)
{
  --__f->_cnt;
  return *__REINTERPRET_CAST(char*, __f->_ptr++);
}

inline char& _FILE_I_predecr(FILE *__f)
{
  ++__f->_cnt;
  return *__REINTERPRET_CAST(char*, --__f->_ptr);
}
 
inline char& _FILE_I_postdecr(FILE *__f)
{
  ++__f->_cnt;
  return *__REINTERPRET_CAST(char*, __f->_ptr--);
}
 
inline void _FILE_I_bump(FILE *__f, int __n)
{
  __f->_ptr += __n;
  __f->_cnt -= __n;
}
 
inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char* __end)
{
  __f->_link = __REINTERPRET_CAST(__stream_link*, __begin);
  __f->_ptr  = __REINTERPRET_CAST(unsigned char*, __next);
  __f->_cnt  = __end - __next;
}
 
# define _STLP_FILE_I_O_IDENTICAL

#elif defined (__Lynx__)

inline int   _FILE_fd(const FILE *__f) { return __f->_fd; }
inline char* _FILE_I_begin(const FILE *__f) { return (char*) __f->_base; }
inline char* _FILE_I_next(const FILE *__f) { return (char*) __f->_ptr; }  
inline char* _FILE_I_end(const FILE *__f)
  { return (char*) __f->_ptr + __f->_cnt; }
 
inline ptrdiff_t _FILE_I_avail(const FILE *__f) { return __f->_cnt; }
 
inline char& _FILE_I_preincr(FILE *__f)
  { --__f->_cnt; return *(char*) (++__f->_ptr); }
inline char& _FILE_I_postincr(FILE *__f)
  { --__f->_cnt; return *(char*) (__f->_ptr++); }
inline char& _FILE_I_predecr(FILE *__f)
   { ++__f->_cnt; return *(char*) (--__f->_ptr); }
inline char& _FILE_I_postdecr(FILE *__f)
  { ++__f->_cnt; return *(char*) (__f->_ptr--); }
inline void  _FILE_I_bump(FILE *__f, int __n)
  { __f->_ptr += __n; __f->_cnt -= __n; }

inline void _FILE_I_set(FILE *__f, char* __begin, char* __next, char* __end) {
  __f->_base = __begin;
  __f->_ptr  = __next;
  __f->_cnt  = __end - __next;
}
# define _STLP_FILE_I_O_IDENTICAL

#else  /* A C library that we don't have an implementation for. */

# error The C++ I/O library is not configured for this compiler

#endif


# ifdef _STLP_FILE_I_O_IDENTICAL
inline char* _FILE_O_begin(const FILE *__f) { return _FILE_I_begin(__f); }
inline char* _FILE_O_next(const FILE *__f)  { return _FILE_I_next(__f); }
inline char* _FILE_O_end(const FILE *__f)   { return _FILE_I_end(__f); }

inline ptrdiff_t _FILE_O_avail(const FILE *__f) { return _FILE_I_avail(__f); }

inline char& _FILE_O_preincr(FILE *__f)  { return _FILE_I_preincr(__f); }
inline char& _FILE_O_postincr(FILE *__f) { return _FILE_I_postincr(__f); }
inline char& _FILE_O_predecr(FILE *__f)  { return _FILE_I_predecr(__f); }
inline char& _FILE_O_postdecr(FILE *__f) { return _FILE_I_postdecr(__f); }

inline void  _FILE_O_bump(FILE *__f, int __n) { _FILE_I_bump(__f, __n); }
inline void _FILE_O_set(FILE *__f, char* __begin, char* __next, char* __end)
  { _FILE_I_set(__f, __begin, __next, __end); }
# endif

#else
inline int _FILE_fd(const FILE *__f) { return (int)::_fileno(__CONST_CAST(FILE *, __f)); }
#endif /* _STLP_WINCE */

_STLP_END_NAMESPACE

#endif /* _STLP_STDIO_FILE_H */

