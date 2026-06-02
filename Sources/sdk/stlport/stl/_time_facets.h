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


#ifndef _STLP_INTERNAL_TIME_FACETS_H
#define _STLP_INTERNAL_TIME_FACETS_H

#ifndef _STLP_CTIME
# include <ctime>                // Needed (for struct tm) by time facets
#endif

#include <stl/c_locale.h>
#include <stl/_ios_base.h>

_STLP_BEGIN_NAMESPACE






#define _MAXNAMES        64
#define _MAX_NAME_LENGTH 64


class _STLP_CLASS_DECLSPEC _Time_Info {
public:
  string _M_dayname[14];
  string _M_monthname[24];
  string _M_am_pm[2];
  string _M_time_format;
  string _M_date_format;
  string _M_date_time_format;
  string _M_long_date_format;
  string _M_long_date_time_format;
};

void _STLP_CALL _Init_timeinfo(_Time_Info&);
void _STLP_CALL _Init_timeinfo(_Time_Info&, _Locale_time*);

class _STLP_CLASS_DECLSPEC time_base {
public:
  enum dateorder {no_order, dmy, mdy, ymd, ydm};
};


template <class _Ch, __DFL_TMPL_PARAM( _InIt , istreambuf_iterator<_Ch>) >
class time_get : public locale::facet, public time_base 
{
  friend class _Locale;

public:
  typedef _Ch   char_type;
  typedef _InIt iter_type;

  explicit time_get(size_t __refs = 0)   : _BaseFacet(__refs) {
      _Init_timeinfo(_M_timeinfo);
  }
  dateorder date_order() const { return do_date_order(); }
  iter_type get_time(iter_type __s, iter_type  __end, ios_base&  __str,
                     ios_base::iostate&  __err, tm* __t) const
    { return do_get_time(__s,  __end,  __str,  __err, __t); }
  iter_type get_date(iter_type __s, iter_type  __end, ios_base&  __str,
                     ios_base::iostate&  __err, tm* __t) const
    { return do_get_date(__s,  __end,  __str,  __err, __t); }
  iter_type get_weekday(iter_type __s, iter_type  __end, ios_base&  __str,
                        ios_base::iostate&  __err, tm* __t) const
    { return do_get_weekday(__s,  __end,  __str,  __err, __t); }
  iter_type get_monthname(iter_type __s, iter_type  __end, ios_base&  __str,
                          ios_base::iostate&  __err, tm* __t) const
    { return do_get_monthname(__s,  __end,  __str,  __err, __t); }
  iter_type get_year(iter_type __s, iter_type  __end, ios_base&  __str,
                     ios_base::iostate&  __err, tm* __t) const
    { return do_get_year(__s,  __end,  __str,  __err, __t); }

  _STLP_STATIC_MEMBER_DECLSPEC static locale::id id;

protected:
  _Time_Info _M_timeinfo;

  time_get(_Locale_time *, size_t __refs) : _BaseFacet(__refs) {}

  ~time_get() {}

  virtual dateorder do_date_order() const {return no_order;}
    
  virtual iter_type do_get_time(iter_type __s, iter_type  __end,
                                ios_base&, ios_base::iostate&  __err,
                                tm* __t) const;
    
  virtual iter_type do_get_date(iter_type __s, iter_type  __end,
                                ios_base&, ios_base::iostate& __err,
                                tm* __t) const;

  virtual iter_type do_get_weekday(iter_type __s, iter_type  __end,
                                   ios_base&,
                                   ios_base::iostate& __err,
                                   tm* __t) const;
  virtual iter_type do_get_monthname(iter_type __s, iter_type  __end,
                                     ios_base&,
                                     ios_base::iostate& __err,
                                     tm* __t) const;
  
  virtual iter_type do_get_year(iter_type __s, iter_type  __end,
                                ios_base&, ios_base::iostate& __err,
                                tm* __t) const;
};

time_base::dateorder _STLP_CALL
__get_date_order(_Locale_time*);
_Locale_time* _STLP_CALL __acquire_time(const char* __name);
void          _STLP_CALL __release_time(_Locale_time* __time);

template <class _Ch, __DFL_TMPL_PARAM( _InIt , istreambuf_iterator<_Ch>) >
class time_get_byname : public time_get<_Ch, _InIt> 
{
public:
  typedef  time_base::dateorder dateorder;
  typedef _InIt                 iter_type;

  explicit time_get_byname(const char* __name, size_t __refs = 0)
    : time_get<_Ch, _InIt>((_Locale_time*) 0, __refs),
      _M_time(__acquire_time(__name))
    { _Init_timeinfo(this->_M_timeinfo, this->_M_time); }

protected:
  ~time_get_byname() { __release_time(_M_time); }
  dateorder do_date_order() const { return __get_date_order(_M_time); }
private:
  _Locale_time* _M_time;
};




char * _STLP_CALL
__write_formatted_time(char * __buf, char __format, char __modifier,
                       const _Time_Info& __table, const tm* __t);

template <class _OuIt>
inline _OuIt _STLP_CALL __put_time(char * __first, char * __last, _OuIt __out,
                                   const ios_base& /* __loc */, char) {
    return copy(__first, __last, __out);
}

# ifndef _STLP_NO_WCHAR_T
template <class _OuIt>
_OuIt _STLP_CALL __put_time(char * __first, char * __last, _OuIt __out,
                            const ios_base& __s, wchar_t);
# endif

template<class _Ch, __DFL_TMPL_PARAM( _OutputIter , ostreambuf_iterator<_Ch> ) >
class time_put : public locale::facet, public time_base
{
  friend class _Locale;
public:
  typedef _Ch      char_type;
  typedef _OutputIter iter_type;

  explicit time_put(size_t __refs = 0) : _BaseFacet(__refs) {
    _Init_timeinfo(_M_timeinfo);
  }

  _OutputIter put(iter_type __s, ios_base& __f, _Ch __fill,
		  const tm* __tmb,
		  const _Ch* __pat, const _Ch* __pat_end) const;
  
  _OutputIter put(iter_type __s, ios_base& __f, _Ch  __fill,
		  const tm* __tmb, char __format, char __modifier = 0) const { 
    return do_put(__s, __f,  __fill, __tmb, __format, __modifier); 
  }
  
  _STLP_STATIC_MEMBER_DECLSPEC static locale::id id;
  
protected:
  _Time_Info _M_timeinfo;

  time_put(_Locale_time* /*__time*/, size_t __refs) : _BaseFacet(__refs) {
  }

  ~time_put() {}
  virtual iter_type do_put(iter_type __s, ios_base& __f,
                           char_type  /* __fill */, const tm* __tmb,
                           char __format, char /* __modifier */) const;
};

template <class _Ch, __DFL_TMPL_PARAM( _InIt , ostreambuf_iterator<_Ch> ) >
class time_put_byname : public time_put<_Ch, _InIt> 
{
  friend class _Locale;
public:
  typedef time_base::dateorder dateorder;
  typedef _InIt iter_type;
  typedef _Ch   char_type;

  explicit time_put_byname(const char * __name, size_t __refs = 0)
    : time_put<_Ch, _InIt>((_Locale_time*) 0, __refs),
    _M_time(__acquire_time(__name))
  { _Init_timeinfo(this->_M_timeinfo, this->_M_time); }
  
protected:
  ~time_put_byname() { __release_time(_M_time); }

private:
  _Locale_time* _M_time;
};

# ifdef _STLP_USE_TEMPLATE_EXPORT
_STLP_EXPORT_TEMPLATE_CLASS time_get<char, istreambuf_iterator<char, char_traits<char> > >;
_STLP_EXPORT_TEMPLATE_CLASS time_put<char, ostreambuf_iterator<char, char_traits<char> > >;
#  ifndef _STLP_NO_WCHAR_T
_STLP_EXPORT_TEMPLATE_CLASS time_get<wchar_t, istreambuf_iterator<wchar_t, char_traits<wchar_t> > >;
_STLP_EXPORT_TEMPLATE_CLASS time_put<wchar_t, ostreambuf_iterator<wchar_t, char_traits<wchar_t> > >;
#  endif /* INSTANTIATE_WIDE_STREAMS */

# endif

# if defined (__BORLANDC__) && defined (_RTLDLL)
inline void _Stl_loc_init_time_facets() {
  
  time_get<char, istreambuf_iterator<char, char_traits<char> > >::id._M_index                      = 16;
  time_get<char, const char*>::id._M_index         = 17;
  time_put<char, ostreambuf_iterator<char, char_traits<char> > >::id._M_index                      = 18;
  time_put<char, char*>::id._M_index               = 19;
  
# ifndef _STLP_NO_WCHAR_T
  
  time_get<wchar_t, istreambuf_iterator<wchar_t, char_traits<wchar_t> > >::id._M_index                   = 35;
  time_get<wchar_t, const wchar_t*>::id._M_index   = 36;
  time_put<wchar_t, ostreambuf_iterator<wchar_t, char_traits<wchar_t> > >::id._M_index                   = 37;
  time_put<wchar_t, wchar_t*>::id._M_index         = 38;
  
# endif
  
}
# endif

_STLP_END_NAMESPACE

#if defined (_STLP_EXPOSE_STREAM_IMPLEMENTATION) && !defined (_STLP_LINK_TIME_INSTANTIATION)
#  include <stl/_time_facets.c>
# endif

#endif /* _STLP_INTERNAL_TIME_FACETS_H */



