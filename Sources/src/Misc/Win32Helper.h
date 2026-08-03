#ifndef __WIN32HELPER_H__
#define __WIN32HELPER_H__


#if _MSC_VER > 1000
#pragma once
#endif
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
	CCriticalSection& operator=( const CCriticalSection & ) = delete;
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
	NPlatform::DynamicLibrary library;
	std::string szName;
	CDLLHandle( const CDLLHandle & ) = delete;
	CDLLHandle& operator=( const CDLLHandle & ) = delete;
public:
	CDLLHandle() = default;
	CDLLHandle( const char *pszFileName ) : library( pszFileName ), szName( pszFileName ? pszFileName : "" ) { }
	CDLLHandle( const std::string &szFileName ) : library( szFileName.c_str() ), szName( szFileName ) { }
	~CDLLHandle() = default;
	bool IsLoaded() const { return library.IsLoaded(); }
	template <class TProc>
	TProc GetProcAddress( const char *pszProcName, TProc )
	{
		return IsLoaded() ? reinterpret_cast<TProc>( library.GetFunction( pszProcName ) ) : static_cast<TProc>( 0 );
	}
	const std::string& GetModuleName() const { return szName; }
	operator const char*() const { return szName.c_str(); }
};
#endif
}
#endif
