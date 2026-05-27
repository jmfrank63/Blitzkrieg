#include "StdAfx.h"

#include "..\Misc\Win32Helper.h"
#include "..\Misc\FileUtils.h"
#include "..\RandomMapGen\Registry_Types.h"

using namespace NWin32Helper;
extern "C" WINBASEAPI BOOL WINAPI IsDebuggerPresent(void);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NMain
{
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
	// DLL module structure - stores DLL handle and module descriptor
	struct SDllModule
	{
		CDLLHandle *pDLLHandle;
		const SModuleDescriptor *pDesc;
		//
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
	// all loaded modules
	typedef std::list<SDllModule> CModulesList;
	CModulesList modules;
	// get module desciptor
	const SModuleDescriptor* STDCALL GetModuleDesc( int nType )
	{
		for ( CModulesList::const_iterator it = modules.begin(); it != modules.end(); ++it )
		{
			if ( it->pDesc->nType == nType )
				return it->pDesc;
		}
		NI_ASSERT_T( false, NStr::Format("can't find module of type 0x%.8x", nType) );
		return 0;
	}
	// load libraries
	int STDCALL LoadAllModules( const char *pszPath )
	{
		std::string szPath = pszPath;
		if ( szPath.empty() )
			szPath = ".\\";
		else if ( szPath[szPath.size() - 1] != '\\' )
			szPath += "\\";
		// enumerate all DLLs in order to find best configuration
		for ( NFile::CFileIterator it( (szPath + "*.dll").c_str() ); !it.IsEnd(); ++it )
		{
			CDLLHandle *pDLL = new CDLLHandle( it.GetFilePath() );
			if ( !pDLL->IsLoaded() )
			{
				delete pDLL;
				continue;
			}
			//
			GETMODULEDESCRIPTOR pfnGetModuleDescriptor = pDLL->GetProcAddress( "GetModuleDescriptor", (GETMODULEDESCRIPTOR)0 );
			if ( pfnGetModuleDescriptor != 0 )
			{
				const SModuleDescriptor *pDesc = (*pfnGetModuleDescriptor)();
				if ( pDesc && pDesc->pFactory )
				{
					// store module info
					modules.push_back( SDllModule() );
					SDllModule &module = modules.back();
					module.pDLLHandle = pDLL;
					module.pDesc = pDesc;
					//
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
		//
		return modules.size();
	}
	// 
	void STDCALL UnloadAllModules()
	{
		GetSingletonGlobal()->Done();
		//
		modules.clear();
	}
	// iterating
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


		// get module path and compare it with installed game folder
		{
			char buffer[2048];
			memset( buffer, 0, 2048 );
			::GetModuleFileName( 0, buffer, 2048 );
			szModulePath = buffer;
			szModulePath.resize( szModulePath.rfind( '\\' ) + 1 );
			NStr::ToLower( szModulePath );
		}
		
		//get registry name
		bool bNeedWriteRegistry = false;
		{
			CRegistrySection registrySection( HKEY_LOCAL_MACHINE, KEY_READ, GAME_REGISTRY_FOLDER );
			registrySection.LoadString( GAME_REGISTRY_KEY, &szGameFolder, "" );
			NStr::ToLower( szGameFolder );
			bNeedWriteRegistry = ( szGameFolder != ( szModulePath + szModuleName ) );
		}

		::SetCurrentDirectory( szModulePath.c_str() );
		//write to registry
		if ( bNeedWriteRegistry )
		{
			CRegistrySection registrySection( HKEY_LOCAL_MACHINE, KEY_WRITE, GAME_REGISTRY_FOLDER );
			return registrySection.SaveString( GAME_REGISTRY_KEY, szModulePath + szModuleName );
		}
#endif // defined(_FINALRELEASE) || defined(_BETARELEASE)

		return true;
	}

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CModuleLoadAutoMagic
{
public:
	CModuleLoadAutoMagic()
	{
		NMain::SetGameDirectory();
		const std::string szModuleDirectory = NMain::GetModuleDirectory();
		//
		NMain::LoadAllModules( szModuleDirectory.c_str() );
		// add modules factory
		for ( NMain::CModulesList::iterator it = NMain::modules.begin(); it != NMain::modules.end(); ++it )
		{
			if ( it->pDesc->pFactory )
				GetSLS()->AddFactory( it->pDesc->pFactory );
			if ( it->pDesc->pChecker ) 
				it->pDesc->pChecker->SetModuleFunctionalityLimits();
		}
	}
	//
	~CModuleLoadAutoMagic()
	{
		NMain::UnloadAllModules();
	}
};
static CModuleLoadAutoMagic moduleLoadAutoMagic;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
