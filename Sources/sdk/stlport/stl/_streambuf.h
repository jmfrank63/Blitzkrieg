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
#ifndef _STLP_INTERNAL_STREAMBUF
#define _STLP_INTERNAL_STREAMBUF

#ifndef _STLP_IOS_BASE_H
#include <stl/_ios_base.h>      // Needed for ios_base bitfield members.
#endif

#ifndef _STLP_STDIO_FILE_H
#include <stl/_stdio_file.h>     // Declaration of struct FILE, and of
#endif

_STLP_BEGIN_NAMESPACE





template <class _CharT, class _Traits>
class basic_streambuf
{
  friend class basic_istream<_CharT, _Traits>;
  friend class basic_ostream<_CharT, _Traits>;

public:                         // Typedefs.
  typedef _CharT                     char_type;
  typedef typename _Traits::int_type int_type;
  typedef typename _Traits::pos_type pos_type;
  typedef typename _Traits::off_type off_type;
  typedef _Traits                    traits_type;

private:                        // Data members.

  char_type* _M_gbegin;         // Beginning of get area
  char_type* _M_gnext;          // Current position within the get area
  char_type* _M_gend;           // End of get area

  char_type* _M_pbegin;         // Beginning of put area
  char_type* _M_pnext;          // Current position within the put area
  char_type* _M_pend;           // End of put area

  locale _M_locale;             // The streambuf's locale object

public:                         // Extension: locking, for thread safety.
  _STLP_mutex _M_lock;

public:                         // Destructor.
  virtual ~basic_streambuf();

protected:                      // The default constructor.
  basic_streambuf();

protected:                      // Protected interface to the get area.
  char_type* eback() const { return _M_gbegin; } // Beginning
  char_type* gptr()  const { return _M_gnext; }  // Current position
  char_type* egptr() const { return _M_gend; }   // End
  
  void gbump(int __n) { _M_gnext += __n; }
  void setg(char_type* __gbegin, char_type* __gnext, char_type* __gend) {
    _M_gbegin = __gbegin;
    _M_gnext  = __gnext;
    _M_gend   = __gend;
  }

public:

  char_type* _M_eback() const { return eback(); }
  char_type* _M_gptr()  const { return gptr(); }
  char_type* _M_egptr() const { return egptr(); }
  void _M_gbump(int __n)      { gbump(__n); }
  void _M_setg(char_type* __gbegin, char_type* __gnext, char_type* __gend)
    { setg(__gbegin, __gnext, __gend); }

protected:                      // Protected interface to the put area

  char_type* pbase() const { return _M_pbegin; } // Beginning
  char_type* pptr()  const { return _M_pnext; }  // Current position
  char_type* epptr() const { return _M_pend; }   // End

  void pbump(int __n) { _M_pnext += __n; }
  void setp(char_type* __pbegin, char_type* __pend) {
    _M_pbegin = __pbegin;
    _M_pnext  = __pbegin;
    _M_pend   = __pend;
  }

protected:                      // Virtual buffer management functions.

  virtual basic_streambuf<_CharT, _Traits>* setbuf(char_type*, streamsize);

  virtual pos_type seekoff(off_type, ios_base::seekdir,
                           ios_base::openmode = ios_base::in | ios_base::out);

  virtual pos_type
  seekpos(pos_type, ios_base::openmode = ios_base::in | ios_base::out);

  virtual int sync();


public:                         // Buffer management.
  basic_streambuf<_CharT, _Traits>* pubsetbuf(char_type* __s, streamsize __n) 
    { return this->setbuf(__s, __n); }

  pos_type pubseekoff(off_type __offset, ios_base::seekdir __way,
                      ios_base::openmode __mod = ios_base::in | ios_base::out)
    { return this->seekoff(__offset, __way, __mod); }

  pos_type pubseekpos(pos_type __sp,
                      ios_base::openmode __mod = ios_base::in | ios_base::out)
    { return this->seekpos(__sp, __mod); }

  int pubsync() { return this->sync(); }

protected:                      // Virtual get area functions, as defined in
  virtual streamsize showmanyc();

  virtual streamsize xsgetn(char_type* __s, streamsize __n);

  virtual int_type underflow();

  virtual int_type uflow();

  virtual int_type pbackfail(int_type = traits_type::eof());

protected:                      // Virtual put area functions, as defined in

  virtual streamsize xsputn(const char_type* __s, streamsize __n);

  virtual streamsize _M_xsputnc(char_type __c, streamsize __n);

  virtual int_type overflow(int_type = traits_type::eof());

public:                         // Public members for writing characters.
  int_type sputc(char_type __c) {
    return ((_M_pnext < _M_pend) ? _Traits::to_int_type(*_M_pnext++ = __c)
      : this->overflow(_Traits::to_int_type(__c)));
  }

  streamsize sputn(const char_type* __s, streamsize __n)
    { return this->xsputn(__s, __n); }

  streamsize _M_sputnc(char_type __c, streamsize __n)
    { return this->_M_xsputnc(__c, __n); }

private:                        // Helper functions.
  int_type _M_snextc_aux();


public:                         // Public members for reading characters.
  streamsize in_avail() {
    return (_M_gnext < _M_gend) ? (_M_gend - _M_gnext) : this->showmanyc();
  }
  
  int_type snextc() {
	return ( _M_gend - _M_gnext > 1 ?
             _Traits::to_int_type(*++_M_gnext) :
             this->_M_snextc_aux());
  }

  int_type sbumpc() {
    return _M_gnext < _M_gend ? _Traits::to_int_type(*_M_gnext++) 
      : this->uflow();
  }
  
  int_type sgetc() {
    return _M_gnext < _M_gend ? _Traits::to_int_type(*_M_gnext) 
      : this->underflow();
  }
  
  streamsize sgetn(char_type* __s, streamsize __n)
  { return this->xsgetn(__s, __n); }
  
  int_type sputbackc(char_type __c) {
    return ((_M_gbegin < _M_gnext) && _Traits::eq(__c, *(_M_gnext - 1)))
      ? _Traits::to_int_type(*--_M_gnext)
      : this->pbackfail(_Traits::to_int_type(__c));
  }
  
  int_type sungetc() {
    return (_M_gbegin < _M_gnext)
      ? _Traits::to_int_type(*--_M_gnext)
      : this->pbackfail();
  }

protected:                      // Virtual locale functions.

  virtual void imbue(const locale&);

public:                         // Locale-related functions.
  locale pubimbue(const locale&);
  locale getloc() const { return _M_locale; }

# ifndef _STLP_NO_ANACHRONISMS
  void stossc() { this->sbumpc(); }
# endif
#if defined(__MVS__) || defined(__OS400__)
private: // Data members.

  char_type* _M_gbegin; // Beginning of get area
  char_type* _M_gnext; // Current position within the get area
  char_type* _M_gend; // End of get area

  char_type* _M_pbegin; // Beginning of put area
  char_type* _M_pnext; // Current position within the put area
  char_type* _M_pend; // End of put area
#endif
};




_STLP_TEMPLATE_NULL 
class _STLP_CLASS_DECLSPEC basic_streambuf<char, char_traits<char> >
{
  friend class basic_istream<char, char_traits<char> >;
  friend class basic_ostream<char, char_traits<char> >;
public:                         // Typedefs.
  typedef char                        char_type;
  typedef char_traits<char>::int_type int_type;
  typedef char_traits<char>::pos_type pos_type;
  typedef char_traits<char>::off_type off_type;
  typedef char_traits<char>           traits_type;

private:                        // Data members.

  FILE* _M_get;                 // Reference to the get area
  FILE* _M_put;                 // Reference to the put area

#if defined(__hpux)
  _FILEX  _M_default_get;          // Get area, unless we're syncing with stdio.
  _FILEX  _M_default_put;          // Put area, unless we're syncing with stdio.
#else
  FILE  _M_default_get;          // Get area, unless we're syncing with stdio.
  FILE  _M_default_put;          // Put area, unless we're syncing with stdio.
#endif

  locale _M_locale;

public:                         // Extension: locking, for thread safety.
  _STLP_mutex _M_lock;

public:                         // Destructor.
  virtual ~basic_streambuf _STLP_PSPEC2(char, char_traits<char>) ();

protected:                      // Constructors.

  basic_streambuf _STLP_PSPEC2(char, char_traits<char>) ()
    : _M_get(&_M_default_get),
      _M_put(&_M_default_put), _M_locale()
  {
    _FILE_I_set(_M_get, 0, 0, 0);
    _FILE_O_set(_M_put, 0, 0, 0);
  }
  
  basic_streambuf _STLP_PSPEC2(char, char_traits<char>) (FILE* __get, FILE* __put);

protected:                      // Protected interface to the get area.
  char_type* eback() const { return _FILE_I_begin(_M_get); }
  char_type* gptr()  const { return _FILE_I_next(_M_get); }
  char_type* egptr() const { return _FILE_I_end(_M_get); }
  void gbump(int __n) { _FILE_I_bump(_M_get, __n); }
  void setg(char_type* __gbegin, char_type* __gnext, char_type* __gend)
    { _FILE_I_set(_M_get, __gbegin, __gnext, __gend); }

public:

  char_type* _M_eback() const { return _FILE_I_begin(_M_get); }
  char_type* _M_gptr()  const { return _FILE_I_next(_M_get); }
  char_type* _M_egptr() const { return _FILE_I_end(_M_get); }

  void _M_gbump(int __n) { _FILE_I_bump(_M_get, __n); }
  void _M_setg(char_type* __gbegin, char_type* __gnext, char_type* __gend)
    { _FILE_I_set(_M_get, __gbegin, __gnext, __gend); }

protected:                      // Protected interface to the put area
  char_type* pbase() const { return _FILE_O_begin(_M_put); }
  char_type* pptr()  const { return _FILE_O_next(_M_put); }
  char_type* epptr() const { return _FILE_O_end(_M_put); }

  void pbump(int __n) { _FILE_O_bump(_M_put, __n); }
  void setp(char_type* __pbegin, char_type* __pend)
    { _FILE_O_set(_M_put, __pbegin, __pbegin, __pend); }

protected:                      // Virtual buffer-management functions.
  virtual basic_streambuf<char, char_traits<char> >* setbuf(char_type*, streamsize);
  virtual pos_type seekoff(off_type, ios_base::seekdir,
                           ios_base::openmode = ios_base::in | ios_base::out);
  virtual pos_type
  seekpos(pos_type, ios_base::openmode = ios_base::in | ios_base::out);
  virtual int sync();

public:                         // Buffer management.
  basic_streambuf<char, char_traits<char> >* pubsetbuf(char_type* __s, streamsize __n) 
    { return this->setbuf(__s, __n); }

  pos_type pubseekoff(off_type __offset, ios_base::seekdir __way,
                      ios_base::openmode __mod = ios_base::in | ios_base::out)
    { return this->seekoff(__offset, __way, __mod); }

  pos_type pubseekpos(pos_type __sp,
                      ios_base::openmode __mod = ios_base::in | ios_base::out)
    { return this->seekpos(__sp, __mod); }

  int pubsync() { return this->sync(); }

protected:                      // Virtual get area functions.
  virtual streamsize showmanyc();
  virtual streamsize xsgetn(char_type* __s, streamsize __n);
  virtual int_type underflow();
  virtual int_type uflow();
  virtual int_type pbackfail(int_type __c = traits_type::eof());

protected:                      // Virtual put area functions.
  virtual streamsize xsputn(const char_type* __s, streamsize __n);
  virtual streamsize _M_xsputnc(char_type __c, streamsize __n);
  virtual int_type overflow(int_type = traits_type::eof());

public:                         // Public members for writing characters.
  int_type sputc(char_type __c) {
    int_type __res;
	if( _FILE_O_avail(_M_put) > 0 )
	{
		_FILE_O_postincr(_M_put) = __c;
		__res = traits_type::to_int_type(__c);
	}
	else
      __res = this->overflow(traits_type::to_int_type(__c));
    return __res;
  }

  streamsize sputn(const char_type* __s, streamsize __n)
    { return this->xsputn(__s, __n); }

  streamsize _M_sputnc(char_type __c, streamsize __n)
    { return this->_M_xsputnc(__c, __n); }

private:                        // Helper functions.
  int_type _M_snextc_aux();

public:                         // Public members for reading characters.
  streamsize in_avail()
    { return _FILE_I_avail(_M_get) > 0 ? _FILE_I_avail(_M_get) 
                                     : this->showmanyc(); }
  
  int_type snextc() {
    return _FILE_I_avail(_M_get) > 1
      ? traits_type::to_int_type(_FILE_I_preincr(_M_get))
      : this->_M_snextc_aux();
  }

  int_type sbumpc() {
    return _FILE_I_avail(_M_get) > 0
      ? traits_type::to_int_type(_FILE_I_postincr(_M_get))
      : this->uflow();
  }

  int_type sgetc() {
    return _FILE_I_avail(_M_get) > 0
      ? traits_type::to_int_type(*_FILE_I_next(_M_get))
      : this->underflow();
  }
    
  streamsize sgetn(char_type* __s, streamsize __n)
    { return this->xsgetn(__s, __n); }

  int_type sputbackc(char_type __c) {
    return _FILE_I_begin(_M_get) < _FILE_I_next(_M_get) &&
           __c == *(_FILE_I_next(_M_get) - 1)
      ? traits_type::to_int_type(_FILE_I_predecr(_M_get))
      : this->pbackfail(traits_type::to_int_type(__c));
  }

  int_type sungetc() {
    return _FILE_I_begin(_M_get) < _FILE_I_next(_M_get)
      ? traits_type::to_int_type(_FILE_I_predecr(_M_get))
      : this->pbackfail();
  }

protected:                      // Virtual locale functions.
  virtual void imbue(const locale&);

public:                         // Locale-related functions.
  locale pubimbue(const locale&);
  locale getloc() const { return _M_locale; }

# ifndef _STLP_NO_ANACHRONISMS
public:
  void stossc() { this->sbumpc(); }
# endif

#if defined(__MVS__) || defined(__OS400__)
private: // Data members.

  char_type* _M_gbegin; // Beginning of get area
  char_type* _M_gnext; // Current position within the get area
  char_type* _M_gend; // End of get area

  char_type* _M_pbegin; // Beginning of put area
  char_type* _M_pnext; // Current position within the put area
  char_type* _M_pend; // End of put area
#endif

};
_STLP_END_NAMESPACE

# if defined (_STLP_EXPOSE_STREAM_IMPLEMENTATION) && !defined (_STLP_LINK_TIME_INSTANTIATION)
#  include <stl/_streambuf.c>
# endif

#endif
