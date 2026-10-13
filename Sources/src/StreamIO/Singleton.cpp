#include "StdAfx.h"
#include <cstdio>
#include <cstdlib>
#include <typeinfo>

#ifndef __REDUCED_SINGLETON__
#include "OptionSystemInternal.h"
#include "GlobalVars.h"
#include "ConsoleBuffer.h"
#include "RandomGenInternal.h"
#endif // __REDUCED_SINGLETON__
static std::vector<BYTE> tempbuffers[10];
struct STempBufferAutomatic
{
	STempBufferAutomatic()
	{
		for ( int i=0; i<10; ++i )
			tempbuffers[i].resize( 32 );
	}
};
static STempBufferAutomatic tempinitautomagic;
void* STDCALL GetTempRawBuffer_Hook( int nSize, int nIndex )
{
	NI_ASSERT_SLOW_TF( nIndex < 10, "Can use only 10 temp buffers", return 0 );
	tempbuffers[nIndex].reserve( nSize );
	return &( tempbuffers[nIndex][0] );
}
#ifdef __REDUCED_SINGLETON__
typedef void* (STDCALL *GETTEMPRAWBUFFER_HOOK)( int nAmount, int nBufferIndex );
GETTEMPRAWBUFFER_HOOK g_pfnGlobalGetTempRawBuffer = GetTempRawBuffer_Hook;
#endif // __REDUCED_SINGLETON__
class CSingleton : public ISingleton
{
	typedef std::unordered_map< int, CPtr<IRefCount> > CObjectIDs;
	CObjectIDs objects;
	// The registration order, so the teardown can release in the reverse of
	// the order things were created (leaf feature objects first, the option
	// system and its static-backed companions last). An unordered_map cannot
	// answer that, so Register/UnRegister maintain this vector.
	std::vector<int> registrationOrder;
public:
	CSingleton();
	virtual bool STDCALL Register( int nID, IRefCount *pObj );
	virtual bool STDCALL UnRegister( int nID );
	virtual bool STDCALL UnRegister( IRefCount *pObj );
	virtual IRefCount* STDCALL Get( int nID );
	virtual int STDCALL GetAllObjects( IRefCount ***pBuffer, int *pnBufferSize );
	// Releases every registered object in the reverse of its registration
	// order (S04/T04's owner ordering). Each CPtr's Release runs as the map
	// entry is erased, so objects held only here are destroyed now, and
	// objects that another owner still holds CPtrs to survive until those
	// owners release. UnRegister/SafeRelease keeps the order vector in sync.
	virtual void STDCALL Done() { SafeReleaseReverse(); }
private:
	void SafeReleaseReverse()
	{
		// Release each object and let the container's own refcounts carry it
		// away. UnRegister removes the entry from `objects` (its CPtr releases
		// there) without touching `registrationOrder`; the reverse walk then
		// erases the tid itself so the order vector ends empty.
		// BK_REF_AUDIT=1 (S04/T08a): an entry still referenced above one
		// survives this release - name it (type + count) so the second owners
		// become breakable. The count is peeked at offset 8, the
		// single-inheritance CTRefCount layout ([vptr][int nRefData],
		// Misc/Basic.h); a multiple-inheritance object reads a wrong number
		// and identifies itself by its typeid either way. Debug captures only.
		const bool bAudit = std::getenv( "BK_REF_AUDIT" ) != nullptr;
		int nLogged = 0;
		while ( !registrationOrder.empty() )
		{
			const int nID = registrationOrder.back();
			IRefCount *pObj = Get( nID );
			if ( pObj != 0 )
			{
				if ( bAudit && nLogged < 200 )
				{
					const int nRefs = *reinterpret_cast<const int *>( reinterpret_cast<const char *>( pObj ) + sizeof( void * ) );
					if ( nRefs > 1 && nRefs < 1000 )
					{
						++nLogged;
						std::fprintf( stderr, "ref-audit: tid=%d refs=%d type=%s\n", nID, nRefs, typeid( *pObj ).name() );
					}
				}
				UnRegister( nID );
			}
			registrationOrder.pop_back();
		}
		objects.clear();
		NI_ASSERT_TF( registrationOrder.empty(), "the registration order vector ended non-empty", return );
	}
};
CSingleton::CSingleton()
{
#ifndef __REDUCED_SINGLETON__	
	Register( IOptionSystem::tidTypeID, new COptionSystem() );
	Register( IGlobalVars::tidTypeID, new CGlobalVars() );
	Register( IConsoleBuffer::tidTypeID, new CConsoleBuffer() );
	{
		CRandomGenerator *pRandomGen = new CRandomGenerator();
		Register( IRandomGen::tidTypeID, pRandomGen );
	}
#endif // __REDUCED_SINGLETON__
}
CSingleton theSingleton;
ISingleton* STDCALL GetSingletonGlobal_Hook()
{
	return &theSingleton;
}

#ifdef __REDUCED_SINGLETON__
ISingleton *g_pGlobalSingleton = &theSingleton;
#endif // __REDUCED_SINGLETON__
bool CSingleton::Register( int nID, IRefCount *pObj )
{
	CObjectIDs::const_iterator pos = objects.find( nID );
	NI_ASSERT_TF( pos == objects.end(), NStr::Format("object 0x%x already registered", nID), return false );
	objects[nID] = pObj;
	registrationOrder.push_back( nID );
	return true;
}
bool CSingleton::UnRegister( int nID )
{
	CObjectIDs::iterator pos = objects.find( nID );
	if ( pos != objects.end() )
		objects.erase( pos );
	return true;
}
bool CSingleton::UnRegister( IRefCount *pObj )
{
	for ( CObjectIDs::iterator it = objects.begin(); it != objects.end(); ++it )
	{
		if ( it->second.GetPtr() == pObj )
		{
			objects.erase( it );
			return true;
		}
	}
	return false;
}
IRefCount* CSingleton::Get( int nID )
{
	CObjectIDs::iterator pos = objects.find( nID );
	return pos == objects.end() ? 0 : pos->second;
}
int CSingleton::GetAllObjects( IRefCount ***ppBuffer, int *pnBufferSize )
{
	NI_ASSERT_TF( (ppBuffer != 0) && (pnBufferSize != 0), "NULL pointer passed to request", return -1 );
	*pnBufferSize = objects.size();
	*ppBuffer = GetTempBuffer<IRefCount*>( *pnBufferSize );
	IRefCount **pBuffer = *ppBuffer;
	for ( CObjectIDs::iterator it = objects.begin(); it != objects.end(); ++it )
		*pBuffer++ = it->second;
	return *pnBufferSize;
}
