#ifndef _NEW_
#define _NEW_
#define _INC_NEW


namespace std
{
	class bad_alloc {  };
	struct nothrow_t {  };
	struct nothrow {  };

	typedef void (BK_CDECL *new_handler)();
	inline new_handler set_new_handler( new_handler a ) { return a; }
}
void* BK_CDECL operator new( size_t n );
void BK_CDECL operator delete( void *p );
void *BK_CDECL operator new[](size_t count); //_THROW1(std::bad_alloc)
void BK_CDECL operator delete[]( void * p );
inline void *BK_CDECL operator new(size_t, void *_P)
{return (_P); }
inline void BK_CDECL operator delete(void *, void *)
{return; }
#endif
