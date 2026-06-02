
# if defined ( _STLP_NO_OWN_IOSTREAMS )


# else

# endif

/*
 *  Consistency check : if we use SGI iostreams, we have to use consistent
 *  thread model (single-threaded or multi-threaded) with the compiled library
 *  
 *  Default is multithreaded build. If you want to build and use single-threaded
 *  STLport, please change _STLP_NOTHREADS configuration setting above and rebuild the library
 *
 */

# if defined (_STLP_OWN_IOSTREAMS) \
  && !defined (_STLP_NO_THREADS) && !defined (_REENTRANT)

#  if defined(_MSC_VER) && !defined(__MWERKS__) && !defined (__COMO__) && !defined(_MT)
#   error "Only multi-threaded runtime library may be linked with STLport!"  
#  endif

#  if defined (__BUILDING_STLPORT) /* || defined (_STLP_DEBUG) */
#   define _REENTRANT 1
#  endif

# endif
