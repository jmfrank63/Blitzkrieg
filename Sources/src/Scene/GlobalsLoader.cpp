#include "StdAfx.h"

#include "RandomGen.h"
#include "..\Platform\DynamicLibrary.h"
#include "StreamIOTypes.h"
typedef ISaveLoadSystem* (STDCALL *GETSLS_HOOK)();
typedef ISingleton* (STDCALL *GETSINGLETONGLOBAL_HOOK)();
typedef void* (STDCALL *GETTEMPRAWBUFFER_HOOK)( int nAmount, int nBufferIndex );
#ifdef _STREAMIO_DLL
ISaveLoadSystem* STDCALL GetSLS_Hook();
ISingleton* STDCALL GetSingletonGlobal_Hook();
void* STDCALL GetTempRawBuffer_Hook( int nSize, int nIndex );
#endif // _STREAMIO_DLL
IRandomGen *g_pGlobalRandomGen = 0;
ISaveLoadSystem *g_pGlobalSaveLoadSystem = 0;
ISingleton *g_pGlobalSingleton = 0;
GETTEMPRAWBUFFER_HOOK g_pfnGlobalGetTempRawBuffer = 0;
static const char *StreamIOName()
{
#if defined(_WIN32) || defined(_WIN64)
    return "StreamIO.dll";
#elif defined(__APPLE__)
    return "libStreamIO.dylib";
#else
    return "libStreamIO.so";
#endif
}
static std::list<NPlatform::DynamicLibrary*> dllhandles;
NPlatform::DynamicLibrary* LoadModule( const char *pszName )
{
	NPlatform::DynamicLibrary *pDLL = new NPlatform::DynamicLibrary( pszName );
	if ( !pDLL->IsLoaded() ) 
	{
		delete pDLL;
		return 0;
	}
	return pDLL;
}
class CGlobalsLoader
{
public:
	CGlobalsLoader()
	{
#ifndef _DONT_LOAD_STREAMIO
		NPlatform::DynamicLibrary *pStreamIO = LoadModule( StreamIOName() );
		if ( pStreamIO != 0 )
		{
			if ( GETSLS_HOOK pfnGetSLS_Hook = reinterpret_cast<GETSLS_HOOK>( pStreamIO->GetFunction( "GetSLS_Hook" ) ) ) 
				g_pGlobalSaveLoadSystem = (*pfnGetSLS_Hook)();
		if ( GETSINGLETONGLOBAL_HOOK pfnGetSingletonGlobal_Hook = reinterpret_cast<GETSINGLETONGLOBAL_HOOK>( pStreamIO->GetFunction( "GetSingletonGlobal_Hook" ) ) ) 
				g_pGlobalSingleton = (*pfnGetSingletonGlobal_Hook)();
		g_pfnGlobalGetTempRawBuffer = reinterpret_cast<GETTEMPRAWBUFFER_HOOK>( pStreamIO->GetFunction( "GetTempRawBuffer_Hook" ) );

			dllhandles.push_back( pStreamIO );
		}
#endif // _DONT_LOAD_STREAMIO
#ifdef _STREAMIO_DLL
		g_pGlobalSaveLoadSystem = GetSLS_Hook();
		g_pGlobalSingleton = GetSingletonGlobal_Hook();
		g_pfnGlobalGetTempRawBuffer = GetTempRawBuffer_Hook;
#endif // _STREAMIO_DLL
#ifndef _DONT_LOAD_SINGLETONS		
		ISingleton *pSingleton = GetSingletonGlobal();
		if ( pSingleton )
		{
			g_pGlobalRandomGen = GetSingleton<IRandomGen>( pSingleton );
		}
#endif // _DONT_LOAD_SINGLETONS		
	}
	~CGlobalsLoader()
	{
		for ( std::list<NPlatform::DynamicLibrary*>::iterator it = dllhandles.begin(); it != dllhandles.end(); ++it )
			delete (*it);
		dllhandles.clear();
	}
};
static CGlobalsLoader theGlobalsLoader;
