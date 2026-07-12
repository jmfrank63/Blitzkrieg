#include "StdAfx.h"

#include "..\Misc\Win32Helper.h"
#include "..\Misc\FileUtils.h"
#include "..\RandomMapGen\Registry_Types.h"

using namespace NWin32Helper;
extern "C" WINBASEAPI BOOL WINAPI IsDebuggerPresent(void);
namespace NMain
{
	typedef ISaveLoadSystem* (STDCALL *GETSLS_HOOK)();
	typedef ISingleton* (STDCALL *GETSINGLETONGLOBAL_HOOK)();
	typedef void* (STDCALL *GETTEMPRAWBUFFER_HOOK)( int nAmount, int nBufferIndex );

	static void EnsureGlobalHooks()
	{
		if ( GetSLS() != 0 && GetSingletonGlobal() != 0 && g_pfnGlobalGetTempRawBuffer != 0 )
			return;

		HMODULE hStreamIO = ::GetModuleHandleA( "StreamIO.dll" );
		if ( hStreamIO == 0 )
			hStreamIO = ::LoadLibraryA( "StreamIO.dll" );
		if ( hStreamIO == 0 )
			return;

		if ( GetSLS() == 0 )
		{
			if ( GETSLS_HOOK pfnGetSLS = (GETSLS_HOOK)::GetProcAddress( hStreamIO, "GetSLS_Hook" ) )
				g_pGlobalSaveLoadSystem = (*pfnGetSLS)();
		}
		if ( GetSingletonGlobal() == 0 )
		{
			if ( GETSINGLETONGLOBAL_HOOK pfnGetSingleton = (GETSINGLETONGLOBAL_HOOK)::GetProcAddress( hStreamIO, "GetSingletonGlobal_Hook" ) )
				g_pGlobalSingleton = (*pfnGetSingleton)();
		}
		if ( g_pfnGlobalGetTempRawBuffer == 0 )
			g_pfnGlobalGetTempRawBuffer = (GETTEMPRAWBUFFER_HOOK)::GetProcAddress( hStreamIO, "GetTempRawBuffer_Hook" );
	}

	static const char GAME_REGISTRY_FOLDER[] = "Software\\Nival Interactive\\Blitzkrieg";
	static const char GAME_REGISTRY_KEY[] = "InstallFolder";
	static std::string GetModuleDirectory()
	{
		char buffer[2048] = { 0 };
		if ( ::GetModuleFileName( 0, buffer, 2048 ) == 0 )
			return ".\\";
		char *pFileName = strrchr( buffer, '\\' );
		if ( pFileName != 0 )
			*(pFileName + 1) = 0;
		return buffer;
	}
	struct SDllModule
	{
		CDLLHandle *pDLLHandle;
		const SModuleDescriptor *pDesc;
		SDllModule() : pDLLHandle( 0 ), pDesc( 0 ) {  }
		~SDllModule() 
		{ 
			if ( pDLLHandle ) 
			{
				if ( pDesc )
					NStr::DebugTrace( "Unloading module \"%s\" of version 0x%x\n", pDesc->pszName, pDesc->nVersion );
				delete pDLLHandle; 
			}
		}
	};
	typedef std::list<SDllModule> CModulesList;
	CModulesList modules;
	static bool HasModuleType( int nType )
	{
		for ( CModulesList::const_iterator it = modules.begin(); it != modules.end(); ++it )
		{
			if ( it->pDesc && it->pDesc->nType == nType )
				return true;
		}
		return false;
	}
	static bool AddAlreadyLoadedModule( HMODULE hModule, const char *pszModuleName )
	{
		if ( hModule == 0 )
			return false;

		GETMODULEDESCRIPTOR pfnGetModuleDescriptor = reinterpret_cast<GETMODULEDESCRIPTOR>( ::GetProcAddress( hModule, "GetModuleDescriptor" ) );
		if ( pfnGetModuleDescriptor == 0 )
			return false;

		const SModuleDescriptor *pDesc = (*pfnGetModuleDescriptor)();
		if ( pDesc == 0 || pDesc->pFactory == 0 )
			return false;
		if ( HasModuleType( pDesc->nType ) )
			return true;

		modules.push_back( SDllModule() );
		SDllModule &module = modules.back();
		module.pDLLHandle = 0;
		module.pDesc = pDesc;
		NStr::DebugTrace( "Using already loaded module \"%s\" of version 0x%x from %s\n", pDesc->pszName, pDesc->nVersion, pszModuleName );
		return true;
	}
	const SModuleDescriptor* STDCALL GetModuleDesc( int nType )
	{
		for ( CModulesList::const_iterator it = modules.begin(); it != modules.end(); ++it )
		{
			if ( it->pDesc->nType == nType )
				return it->pDesc;
		}
		NStr::DebugTrace( "can't find module of type 0x%.8x\n", nType );
		return 0;
	}
	int STDCALL LoadAllModules( const char *pszPath )
	{
		std::string szPath = pszPath;
		if ( szPath.empty() )
			szPath = ".\\";
		else if ( szPath[szPath.size() - 1] != '\\' )
			szPath += "\\";
		for ( NFile::CFileIterator it( (szPath + "*.dll").c_str() ); !it.IsEnd(); ++it )
		{
			std::string szDLLPath = it.GetFilePath();
			std::string szDLLName = szDLLPath;
			const size_t nSlashPos = szDLLName.find_last_of( "\\/" );
			if ( nSlashPos != std::string::npos )
				szDLLName = szDLLName.substr( nSlashPos + 1 );
			NStr::ToLower( szDLLName );
			if ( szDLLName == "streamio.dll" && ::GetModuleHandleA( "streamio.dll" ) != 0 )
			{
				NStr::DebugTrace( "Skipping duplicate StreamIO module \"%s\"\n", it.GetFilePath().c_str() );
				AddAlreadyLoadedModule( ::GetModuleHandleA( "streamio.dll" ), it.GetFilePath().c_str() );
				continue;
			}

			CDLLHandle *pDLL = new CDLLHandle( it.GetFilePath() );
			if ( !pDLL->IsLoaded() )
			{
				NStr::DebugTrace( "Failed to load module \"%s\" (error=%d)\n", it.GetFilePath().c_str(), int(::GetLastError()) );
				delete pDLL;
				continue;
			}
			GETMODULEDESCRIPTOR pfnGetModuleDescriptor = pDLL->GetProcAddress( "GetModuleDescriptor", (GETMODULEDESCRIPTOR)0 );
			if ( pfnGetModuleDescriptor != 0 )
			{
				const SModuleDescriptor *pDesc = (*pfnGetModuleDescriptor)();
				if ( pDesc && pDesc->pFactory )
				{
					modules.push_back( SDllModule() );
					SDllModule &module = modules.back();
					module.pDLLHandle = pDLL;
					module.pDesc = pDesc;
					NStr::DebugTrace( "New module \"%s\" of version 0x%x loaded\n", pDesc->pszName, pDesc->nVersion );
				}
				else
				{
					NStr::DebugTrace( "Module \"%s\" hasn't a module descriptor or object factory", it.GetFilePath().c_str() );
					delete pDLL;
				}
			}
			else
			{
				NStr::DebugTrace( "Module \"%s\" have no GetModuleDescriptor() function\n", it.GetFilePath().c_str() );
				delete pDLL;
			}
		}
		return modules.size();
	}
	void STDCALL UnloadAllModules()
	{
		// Finalize calls this explicitly and the static module loader calls it
		// again during CRT teardown.  Once the provider DLLs are gone the global
		// singleton pointer belongs to an unloaded StreamIO image.
		if ( modules.empty() )
			return;
		if ( GetSingletonGlobal() != 0 )
			GetSingletonGlobal()->Done();
		modules.clear();
	}
	static const SModuleDescriptor* GetModuleByIndex( const int nIndex )
	{
		if ( nIndex >= modules.size() ) 
			return 0;
		CModulesList::iterator pos = modules.begin();
		std::advance( pos, nIndex );
		return pos->pDesc;
	}

	static int nModuleIndex = 0;
	const SModuleDescriptor* GetFirstModuleDesc()
	{
		nModuleIndex = 0;
		return GetModuleByIndex( nModuleIndex );
	}
	const SModuleDescriptor* GetNextModuleDesc()
	{
		++nModuleIndex;
		return GetModuleByIndex( nModuleIndex );
	}
	const std::string GetModuleFileNameByDesc( const SModuleDescriptor *pModule )
	{
		for ( CModulesList::iterator it = modules.begin(); it != modules.end(); ++it )
		{
			if ( it->pDesc == pModule ) 
				return it->pDLLHandle->GetModuleName();
		}
		return "";
	}

	bool SetGameDirectory()
	{
#if defined(_FINALRELEASE) || defined(_BETARELEASE)
		std::string szModulePath;
		std::string szGameFolder;
		const std::string szModuleName( "game.exe" );


		{
			char buffer[2048];
			memset( buffer, 0, 2048 );
			::GetModuleFileName( 0, buffer, 2048 );
			szModulePath = buffer;
			szModulePath.resize( szModulePath.rfind( '\\' ) + 1 );
			NStr::ToLower( szModulePath );
		}
		
		bool bNeedWriteRegistry = false;
		{
			CRegistrySection registrySection( HKEY_LOCAL_MACHINE, KEY_READ, GAME_REGISTRY_FOLDER );
			registrySection.LoadString( GAME_REGISTRY_KEY, &szGameFolder, "" );
			NStr::ToLower( szGameFolder );
			bNeedWriteRegistry = ( szGameFolder != ( szModulePath + szModuleName ) );
		}

		::SetCurrentDirectory( szModulePath.c_str() );
		if ( bNeedWriteRegistry )
		{
			CRegistrySection registrySection( HKEY_LOCAL_MACHINE, KEY_WRITE, GAME_REGISTRY_FOLDER );
			return registrySection.SaveString( GAME_REGISTRY_KEY, szModulePath + szModuleName );
		}
#endif // defined(_FINALRELEASE) || defined(_BETARELEASE)

		return true;
	}

};
class CModuleLoadAutoMagic
{
public:
	CModuleLoadAutoMagic()
	{
#ifdef BK_STARTUP_TRACE
		::OutputDebugStringA( "BK_STARTUP: CModuleLoadAutoMagic constructor enter\n" );
#endif
		auto AddPathEntry = []( const std::string &szPath )
		{
			DWORD nLength = ::GetEnvironmentVariableA( "PATH", 0, 0 );
			std::vector<char> buffer( nLength + 1, 0 );
			if ( nLength > 0 )
				::GetEnvironmentVariableA( "PATH", &buffer[0], nLength + 1 );
			std::string szCurrentPath = &buffer[0];
			if ( !szCurrentPath.empty() && szCurrentPath[szCurrentPath.size() - 1] != ';' )
				szCurrentPath += ';';
			szCurrentPath += szPath;
			::SetEnvironmentVariableA( "PATH", szCurrentPath.c_str() );
		};

#ifdef BK_STARTUP_TRACE
		::OutputDebugStringA( "BK_STARTUP: CModuleLoadAutoMagic - SetGameDirectory\n" );
#endif
		NMain::SetGameDirectory();
#ifdef BK_STARTUP_TRACE
		::OutputDebugStringA( "BK_STARTUP: CModuleLoadAutoMagic - GetModuleDirectory\n" );
#endif
		const std::string szModuleDirectory = NMain::GetModuleDirectory();
#ifdef BK_STARTUP_TRACE
		::OutputDebugStringA( "BK_STARTUP: CModuleLoadAutoMagic - GameDebugDirectory\n" );
#endif
		const std::string szGameDebugDirectory = szModuleDirectory + "..\\..\\Game\\Debug\\";
		AddPathEntry( szModuleDirectory );
		AddPathEntry( szGameDebugDirectory );
#ifdef BK_STARTUP_TRACE
		::OutputDebugStringA( "BK_STARTUP: CModuleLoadAutoMagic - LoadAllModules(module)\n" );
#endif

		NMain::LoadAllModules( szModuleDirectory.c_str() );
#ifdef BK_STARTUP_TRACE
		::OutputDebugStringA( "BK_STARTUP: CModuleLoadAutoMagic - LoadAllModules(debug)\n" );
#endif
		std::string szModuleDirectoryLower = szModuleDirectory;
		NStr::ToLower( szModuleDirectoryLower );
		if ( szModuleDirectoryLower.find( "\\game\\debug\\" ) == std::string::npos )
			NMain::LoadAllModules( szGameDebugDirectory.c_str() );
#ifdef BK_STARTUP_TRACE
		::OutputDebugStringA( "BK_STARTUP: CModuleLoadAutoMagic - EnsureGlobalHooks\n" );
#endif
		NMain::EnsureGlobalHooks();

		ISaveLoadSystem *pSLS = GetSLS();
		if ( pSLS == 0 )
			NStr::DebugTrace( "LoadDLLs: GetSLS() is null during module auto-registration\n" );
		for ( NMain::CModulesList::iterator it = NMain::modules.begin(); it != NMain::modules.end(); ++it )
		{
			if ( it->pDesc == 0 )
				continue;
			if ( pSLS != 0 && it->pDesc->pFactory )
				pSLS->AddFactory( it->pDesc->pFactory );
			if ( it->pDesc->pChecker ) 
				it->pDesc->pChecker->SetModuleFunctionalityLimits();
		}
	}
	~CModuleLoadAutoMagic()
	{
		NMain::UnloadAllModules();
	}
};
static CModuleLoadAutoMagic moduleLoadAutoMagic;
