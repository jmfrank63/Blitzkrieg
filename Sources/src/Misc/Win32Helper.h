#ifndef __WIN32HELPER_H__
#define __WIN32HELPER_H__
#include "../Platform/Sync.h"
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NWin32Helper
{
class CEvent
{
	NPlatform::Event event;
	CEvent( const CEvent& ) = delete;
	CEvent& operator=( const CEvent& ) = delete;
public:
	CEvent( bool bInitState = false, bool bManualReset = true ) : event( bInitState, bManualReset ) {}
	bool Set() { return event.Set(); }
	bool Pulse() { return event.Pulse(); }
	bool Reset() { return event.Reset(); }
	void Wait() { event.Wait(); }
	bool IsSet() { return event.IsSet(); }
};
class CCriticalSection
{
	NPlatform::Mutex sect;
	CCriticalSection( const CCriticalSection & ) = delete;
	CCriticalSection& operator=( const CCriticalSection &) = delete;
	void Enter() { sect.Lock(); }
	void Leave() { sect.Unlock(); }
public:
	CCriticalSection() = default;
	~CCriticalSection() = default;
	friend class CCriticalSectionLock;
};
class CCriticalSectionLock
{
	CCriticalSection &lock;
	bool bInsideCriticalSection;
public:
	CCriticalSectionLock( CCriticalSection &_lock ): lock(_lock) { bInsideCriticalSection = true; lock.Enter(); }
	~CCriticalSectionLock() { if ( bInsideCriticalSection ) lock.Leave(); }

	void Enter() { lock.Enter(); bInsideCriticalSection = true; }
	void Leave() { if ( bInsideCriticalSection ) lock.Leave(); bInsideCriticalSection = false; }
};
template <class T>
class com_ptr
{
	T *pData;
	void Assign( T *_pData ) { if ( _pData ) { _pData->AddRef(); } pData = _pData; }
	void Free() { if ( pData ) pData->Release(); }
public:
	com_ptr( T *_pData = 0 ) { Assign( _pData ); }
	~com_ptr() { Free(); }
	com_ptr( const com_ptr &a ) { Assign( a.pData ); }
	com_ptr& operator=( const com_ptr &a ) { if ( pData == a.pData ) return *this; Free(); Assign( a.pData ); return *this; }
	com_ptr& operator=( T *pObj ) { if ( pData == pObj ) return *this; Free(); Assign( pObj ); return *this; }
	operator T*() const { return pData; }
	T* operator->() const { return pData; }
	void Create( T *_pData ) { Free(); pData = _pData; }
	T** GetAddr() { Free(); pData = 0; return &pData; }
};
#if !defined(BLITZKRIEG_PLATFORM_SYNC_ONLY)
class CDLLHandle
{
	HMODULE handle;												// DLL handle
	std::string szName;										// file name
	CDLLHandle( const CDLLHandle &dll ) {  }
	CDLLHandle& operator=( const CDLLHandle &dll ) { return *this; }
	CDLLHandle() : handle( 0 ) {  }
public:
	CDLLHandle( const char *pszFileName ) : szName( pszFileName ) { const std::wstring szWideName = NStr::ToUnicode( szName ); handle = ::LoadLibraryW( reinterpret_cast<LPCWSTR>( szWideName.c_str() ) ); }
	CDLLHandle( const std::string &szFileName ) : szName( szFileName ) { const std::wstring szWideName = NStr::ToUnicode( szName ); handle = ::LoadLibraryW( reinterpret_cast<LPCWSTR>( szWideName.c_str() ) ); }
	~CDLLHandle() { if ( handle ) ::FreeLibrary( handle ); }
	bool IsLoaded() const { return handle != 0; }
	template <class TProc> 
		TProc GetProcAddress( const char *pszProcName, TProc )
	{
		return IsLoaded() ? (TProc)::GetProcAddress( handle, pszProcName ) : (TProc)0;
	}
	template <class TProc> 
		TProc GetProcAddress( int nProcID, TProc )
	{
		return IsLoaded() ? (TProc)::GetProcAddress( handle, (const char *)nProcID ) : (TProc)0;
	}
	HMODULE GetHMdule() const { return handle; }
	const std::string& GetModuleName() const { return szName; }
	operator HMODULE() const { return handle; }
	operator const char*() const { return szName.c_str(); }
};
#endif
}
#endif

