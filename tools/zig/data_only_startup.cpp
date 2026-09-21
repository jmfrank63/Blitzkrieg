// Everything the save/load system needs and nothing else. The game gets here
// through NMain::LoadAllModules, which loads every module and then calls
// EnsureGlobalHooks (Main/LoadDLLs.cpp:56-67); a map file test needs only the
// three globals that function wires, plus the one storage GameMain.cpp:536-552
// registers, because LoadDataResource reads tilesets and crossets by
// storage-relative name. No window, no GFX, no object database: the object
// database is only needed to pack frame indices, which this tier never does.
#include "StdAfx.h"
#include "data_only_startup.h"
#include "../../Sources/src/StreamIO/RandomGen.h"
#include "../../Sources/src/Platform/DynamicLibrary.h"
#include "../../Sources/src/StreamIO/StreamIOTypes.h"

typedef ISaveLoadSystem* (STDCALL *GETSLS_HOOK)();
typedef ISingleton* (STDCALL *GETSINGLETONGLOBAL_HOOK)();
typedef void* (STDCALL *GETTEMPRAWBUFFER_HOOK)( int nAmount, int nBufferIndex );

// Every module that links the engine statics defines these four itself and
// then fills them from StreamIO's hooks; see any Sources/src/*/GlobalsLoader.cpp.
// A test executable is a module like any other. g_pGlobalRandomGen stays null:
// RandomMapGen's Random() needs it only on the generation paths, which reading
// and writing a map never takes, and leaving it null makes that a crash rather
// than a quiet non-determinism if one ever does.
IRandomGen *g_pGlobalRandomGen = 0;
ISaveLoadSystem *g_pGlobalSaveLoadSystem = 0;
ISingleton *g_pGlobalSingleton = 0;
GETTEMPRAWBUFFER_HOOK g_pfnGlobalGetTempRawBuffer = 0;

namespace NDataOnly
{
static std::string SharedLibraryName( const char *pszRoot )
{
	std::string szPath( pszRoot ? pszRoot : "." );
#if defined(_WIN32) || defined(_WIN64)
	const char cSeparator = '\\';
#else
	const char cSeparator = '/';
#endif
	// One separator, the platform's: the root arrives from the build with
	// whichever the host uses, and a path that mixes them is a needless way to
	// be wrong.
	for ( size_t i = 0; i < szPath.size(); ++i )
		if ( szPath[i] == '/' || szPath[i] == '\\' )
			szPath[i] = cSeparator;
	if ( !szPath.empty() && szPath[szPath.size() - 1] != cSeparator )
		szPath += cSeparator;
#if defined(_WIN32) || defined(_WIN64)
	return szPath + "StreamIO.dll";
#elif defined(__APPLE__)
	return szPath + "libStreamIO.dylib";
#else
	return szPath + "libStreamIO.so";
#endif
}

bool Start( const char *pszModuleRoot, const char *pszDataRoot )
{
	static NPlatform::DynamicLibrary streamio;
	const std::string szLibrary = SharedLibraryName( pszModuleRoot );
	if ( !streamio.IsLoaded() && !streamio.Load( szLibrary.c_str() ) )
	{
		// Say whether the file is even there: "load failed" alone cannot tell a
		// missing library from one whose own imports did not resolve, and those
		// want opposite fixes.
		CPtr<IDataStream> pProbe = OpenFileStream( szLibrary.c_str(), STREAM_ACCESS_READ );
		fprintf( stderr, "data-only startup: cannot load %s: %s (the file is %s)\n",
		         szLibrary.c_str(), streamio.GetError(),
		         pProbe != 0 ? "there, so its own imports did not resolve" : "not there" );
		return false;
	}
	if ( GETSLS_HOOK hook = reinterpret_cast<GETSLS_HOOK>( streamio.GetFunction( "GetSLS_Hook" ) ) )
		g_pGlobalSaveLoadSystem = hook();
	if ( GETSINGLETONGLOBAL_HOOK hook = reinterpret_cast<GETSINGLETONGLOBAL_HOOK>( streamio.GetFunction( "GetSingletonGlobal_Hook" ) ) )
		g_pGlobalSingleton = hook();
	if ( g_pfnGlobalGetTempRawBuffer == 0 )
		g_pfnGlobalGetTempRawBuffer = reinterpret_cast<GETTEMPRAWBUFFER_HOOK>( streamio.GetFunction( "GetTempRawBuffer_Hook" ) );
	if ( GetSLS() == 0 || GetSingletonGlobal() == 0 || g_pfnGlobalGetTempRawBuffer == 0 )
	{
		fprintf( stderr, "data-only startup: %s loaded but a hook is missing\n", szLibrary.c_str() );
		return false;
	}
	// The same pattern the game opens (GameMain.cpp:551): the loose Data
	// directory plus any .pak beside it, so a storage-relative name resolves
	// whether the file is loose or archived.
	const std::string szPattern = std::string( pszDataRoot ? pszDataRoot : "Data" ) + "\\*.pak";
	CPtr<IDataStorage> pStorage = OpenStorage( szPattern.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_MOD );
	if ( pStorage == 0 )
	{
		fprintf( stderr, "data-only startup: cannot open storage at %s\n", szPattern.c_str() );
		return false;
	}
	RegisterSingleton( IDataStorage::tidTypeID, pStorage );
	return true;
}
}
