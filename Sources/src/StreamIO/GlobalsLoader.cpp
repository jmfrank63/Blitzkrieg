#include "StdAfx.h"

#include "../StreamIO/RandomGen.h"
#include "../Platform/DynamicLibrary.h"
#include "../Platform/Debug.h"
#include "../StreamIO/StreamIOTypes.h"
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
#ifdef BK_STARTUP_TRACE
	NPlatform::DebugWrite( "BK_STARTUP: CGlobalsLoader constructor enter\n" );
#endif
#ifndef _DONT_LOAD_STREAMIO
		NPlatform::DynamicLibrary *pStreamIO = LoadModule( "StreamIO.dll" );
		if ( pStreamIO == 0 )
		{
			NPlatform::DebugWriteFormat( "GlobalsLoader: Failed to load StreamIO: %s\n", pStreamIO->GetError() );
		}
		else
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
