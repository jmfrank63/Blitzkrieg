#include "CloudSyncLoader.h"

#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace NPlatform
{
	void *CloudSyncLoadLibrary()
	{
#ifdef _WIN32
		return LoadLibraryA( "CloudSync.dll" );
#elif defined(__APPLE__)
		void *pModule = dlopen( "libCloudSync.dylib", RTLD_NOW );
		if ( pModule == 0 )
			pModule = dlopen( "./libCloudSync.dylib", RTLD_NOW );
		return pModule;
#else
		void *pModule = dlopen( "libCloudSync.so", RTLD_NOW );
		if ( pModule == 0 )
			pModule = dlopen( "./libCloudSync.so", RTLD_NOW );
		return pModule;
#endif
	}

	void *CloudSyncLoadSymbol( void *pModule, const char *pszName )
	{
		if ( pModule == 0 )
			return 0;
#ifdef _WIN32
		return reinterpret_cast<void *>( GetProcAddress( static_cast<HMODULE>( pModule ), pszName ) );
#else
		return dlsym( pModule, pszName );
#endif
	}

	void CloudSyncHostName( char *pszOut, unsigned int nCap )
	{
		if ( pszOut == 0 || nCap == 0 )
			return;
#ifdef _WIN32
		DWORD dwSize = nCap;
		if ( !GetComputerNameA( pszOut, &dwSize ) )
			std::snprintf( pszOut, nCap, "%s", "host" );
#else
		if ( gethostname( pszOut, nCap ) != 0 )
			std::snprintf( pszOut, nCap, "%s", "host" );
		pszOut[nCap - 1] = 0;
#endif
	}
}
