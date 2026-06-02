

# ifdef _M_IA64
#  define _STLP_NATIVE_HEADER(x) <../crt/##x>
#  define _STLP_NATIVE_C_HEADER(x) <../crt/##x>
#  define _STLP_NATIVE_CPP_C_HEADER(x) <../crt/##x>
#  define _STLP_NATIVE_OLD_STREAMS_HEADER(x) <../crt/##x>
#  define _STLP_NATIVE_CPP_RUNTIME_HEADER(header) <../crt/##header>
#  define _STLP_GLOBAL_NEW_HANDLER
# else
#  define _STLP_NATIVE_HEADER(x) <../include/##x>
#  define _STLP_NATIVE_C_HEADER(x) <../include/##x>
#  define _STLP_NATIVE_CPP_C_HEADER(x) <../include/##x>
#  define _STLP_NATIVE_OLD_STREAMS_HEADER(x) <../include/##x>
#  define _STLP_NATIVE_CPP_RUNTIME_HEADER(header) <../include/##header>
# endif

# define _STLP_CALL __cdecl

# ifndef _STLP_LONG_LONG
#  define _STLP_LONG_LONG  __int64
# endif

# ifndef _CPPUNWIND
#  define _STLP_HAS_NO_EXCEPTIONS 1
# endif

# define _STLP_VENDOR_UNEXPECTED_STD

# if defined ( _MT ) && !defined (_STLP_NO_THREADS)  && !defined (_REENTRANT)
#   define _REENTRANT 1
# endif

# define _STLP_WCHAR_T_IS_USHORT      1
# define _STLP_MINIMUM_IMPORT_STD

# ifdef _STLP_MSVC

# ifndef _STLP_MSVC50_COMPATIBILITY
#  define _STLP_MSVC50_COMPATIBILITY   1
# endif

# define _STLP_DEFAULT_CONSTRUCTOR_BUG 1

#  define _STLP_DLLEXPORT_NEEDS_PREDECLARATION 1


#  define _STLP_HAS_SPECIFIC_PROLOG_EPILOG
#  define _STLP_NO_TYPENAME_IN_TEMPLATE_HEADER
#  define _STLP_SAME_FUNCTION_NAME_RESOLUTION_BUG

#  if (_STLP_MSVC > 1100)
     typedef char __stl_char;
#   define _STLP_DEFAULTCHAR __stl_char
#  endif /* (_STLP_MSVC < 1100 ) */

#  define _STLP_NO_TYPENAME_ON_RETURN_TYPE 1 

# if (_STLP_MSVC <= 1300) 
#   define _STLP_NO_USING_FOR_GLOBAL_FUNCTIONS 1
#  define _STLP_NO_FUNCTION_TMPL_PARTIAL_ORDER 1
#  define _STLP_NO_CLASS_PARTIAL_SPECIALIZATION 1
#  define _STLP_NO_FRIEND_TEMPLATES
#  define _STLP_STATIC_CONST_INIT_BUG   1
#   define _STLP_INLINE_MEMBER_TEMPLATES 1
#   define _STLP_NEEDS_EXTRA_TEMPLATE_CONSTRUCTORS
#   define _STLP_USE_OLD_HP_ITERATOR_QUERIES
# endif

#  define _STLP_NO_MEMBER_TEMPLATE_KEYWORD 1
#  define _STLP_NO_MEMBER_TEMPLATE_CLASSES 1

#  define _STLP_NO_QUALIFIED_FRIENDS    1
#  define _STLP_DONT_USE_BOOL_TYPEDEF 1

# endif /* _STLP_MSVC */


# if (_MSC_VER <= 1300) 
#  define _STLP_VENDOR_GLOBAL_CSTD
# endif /* (_MSC_VER <= 1300) */

# if (_MSC_VER <= 1200)  // including MSVC 6.0
#  define _STLP_GLOBAL_NEW_HANDLER
# endif /* (_MSC_VER <= 1200) */

# if ( _MSC_VER<=1010 )
#  define _STLP_NO_BAD_ALLOC
#  define _STLP_HAS_NO_NEW_C_HEADERS 1
#  define _STLP_NO_NEW_NEW_HEADER 1
# elif (_MSC_VER < 1100)
#  define _STLP_YVALS_H 1
#  define _STLP_HAS_NO_NEW_IOSTREAMS 1
# endif /* 1010 */

# if defined (_STLP_MSVC) && ( _STLP_MSVC < 1200 ) /* VC++ 6.0 */
#  define _STLP_NON_TYPE_TMPL_PARAM_BUG 1 
#  define _STLP_THROW_RETURN_BUG 1
# endif

# if defined (_STLP_MSVC) && ( _STLP_MSVC < 1100 )
#  ifndef _STLP_NO_OWN_IOSTREAMS
#   define _STLP_NO_OWN_IOSTREAMS
#   undef  _STLP_OWN_IOSTREAMS
#  endif
#  ifdef _STLP_DEBUG
#   pragma message ("STLport debug mode does not work for VC++ 4.2, turning _STLP_DEBUG off ...")
#    undef _STLP_DEBUG
#  endif
#  define _STLP_NO_BOOL            1
#  define _STLP_NEED_TYPENAME      1
#  define _STLP_NEED_EXPLICIT      1
#   define _STLP_NEED_MUTABLE       1
#   define _STLP_NO_PARTIAL_SPECIALIZATION_SYNTAX
#   define _STLP_LIMITED_DEFAULT_TEMPLATES 1

#   define _STLP_VENDOR_GLOBAL_STD
#   define _STLP_NONTEMPL_BASE_MATCH_BUG 1
#   define _STLP_BROKEN_USING_DIRECTIVE  1
#   define _STLP_NO_ARROW_OPERATOR
#   define _STLP_NO_SIGNED_BUILTINS 1
#   define _STLP_NO_EXCEPTION_SPEC 1
#   undef  _STLP_DEFAULT_TYPE_PARAM
#   undef  _STLP_HAS_NO_NAMESPACES
#   undef  _STLP_NO_AT_MEMBER_FUNCTION
#   undef  _STLP_NO_MEMBER_TEMPLATES
#   undef  _STLP_NO_MEMBER_TEMPLATE_CLASSES
#   define  _STLP_HAS_NO_NAMESPACES 1
#   define  _STLP_NO_AT_MEMBER_FUNCTION 1
#  define  _STLP_NO_MEMBER_TEMPLATES
#  define  _STLP_NO_MEMBER_TEMPLATE_CLASSES
# endif /* 1100 */


# ifdef UNDER_CE
#   include <config/stl_wince.h>
# endif

# ifdef __ICL
#  define _STLP_LIB_BASENAME "stlport_icl"
# else
# if (_MSC_VER >= 1300) 
#   define _STLP_LIB_BASENAME "stlport_vc7"
# elif (_MSC_VER >= 1200)
#    define _STLP_LIB_BASENAME "stlport_vc6"
#  elif (_MSC_VER >= 1100)
#    define _STLP_LIB_BASENAME "stlport_vc5"
#  endif /* (_MSC_VER >= 1200) */
# endif /* __ICL */


#    if (defined (__ICL) && (__ICL < 450)) || (_MSC_VER < 1200)
#     undef  _STLP_USE_STATIC_LIB
#     define _STLP_USE_STATIC_LIB
#     undef _STLP_NO_CUSTOM_IO
#    endif

#   include <config/vc_select_lib.h>




