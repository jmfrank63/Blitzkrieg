#include "StdAfx.h"
/*
#include "MemorySystem.h"
void* BK_CDECL operator new( size_t n )
{
	void *pRes;
#ifdef _DEBUG
	pRes = malloc( n );
#else
	if ( n <= 32768 )//512 )//32768 )
		pRes = NBugSlayer::FastDumbAlloc( n );
	else
		pRes = malloc( n );
#endif
#ifdef _DEBUG
#endif
	return pRes;
}
void BK_CDECL operator delete( void *p )
{
#ifdef _DEBUG
#endif
#ifdef _DEBUG
	free( p );
#else
	if ( !NBugSlayer::FastDumbFree(p) )
		free( p );
#endif
}
void *BK_CDECL operator new[](size_t count) //_THROW1(std::bad_alloc)
{
	return operator new(count);
}
void BK_CDECL operator delete[]( void * p )
{
	operator delete(p);
}
*/
