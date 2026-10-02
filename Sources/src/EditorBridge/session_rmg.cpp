// The RMG storage-folder scan (M3, D-08/D-21): the lists the Fields tool's
// field-set combo, Create Random Map (05-08) and the composers (05-09/10)
// share. The MFC editor read Editor\Default*.xml list files instead - they
// are not shipped, so its composers opened empty (D-08) - this walks the
// mounted storages' own folders.
//
// Names are storage-relative, lowercased like the MFC combo's own entries
// (TabTerrainFieldsDialog.cpp:138), the .xml stripped, sorted, deduped. A
// folder no storage carries is an empty list, not an error (D-08).
#include "StdAfx.h"
#include "bridge.h"
#include "session.h"
#include "../Main/GameStats.h"
#include "../RandomMapGen/RMG_Types.h"
#include "../RandomMapGen/Resource_Types.h"
#include "../RandomMapGen/MapInfo_Types.h"
#include "../Platform/Paths.h"
#include "../StreamIO/ProgressHook.h"
#include "../StreamIO/RandomGen.h"
#include "../StreamIO/StreamIOTypes.h"
#include <algorithm>
#include <filesystem>
#include <random>
#include <set>

namespace
{

// The folder each kind walks, storage-relative with the trailing separator
// the storage masks want.
const char *RmgFolder( int nKind )
{
	switch ( nKind )
	{
		case 0: return "scenarios\\fieldsets\\";
		case 1: return "scenarios\\templates\\";
		case 2: return "graphs\\";
		case 3: return "scenarios\\containers\\";
		case 4: return "scenarios\\settings\\";
		case 5: return "scenarios\\chapters\\";
		default: return 0;
	}
}

}

bool ListRmgFolder( SEditorSession *pSession, int nKind, std::vector<std::string> *pNames )
{
	pNames->clear();
	if ( pSession == 0 )
		return false;
	const char *pszFolder = RmgFolder( nKind );
	if ( pszFolder == 0 )
	{
		pSession->szMessage = "no such RMG folder kind";
		return false;
	}
	IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
	if ( pDataStorage == 0 )
	{
		pSession->szMessage = "the data storage is not there";
		return false;
	}
	CPtr<IStorageEnumerator> pEnum = pDataStorage->CreateEnumerator();
	if ( pEnum == 0 )
	{
		pSession->szMessage = "the data storage does not enumerate";
		return false;
	}
	std::set<std::string> names;
	// The enumerator walks whole storages: a folder-scoped mask is not a
	// filter, it answers every element (full mount-relative paths) - the
	// folder is this function's own prefix filter, exactly the VSO
	// descriptors' scan (session_vso.cpp VsoDescriptors).
	const std::string szFolder = pszFolder;
	for ( pEnum->Reset( "*.*" ); pEnum->Next(); )
	{
		const SStorageElementStats *pStats = pEnum->GetStats();
		if ( pStats == 0 || pStats->pszName == 0 )
			continue;
		std::string szName = pStats->pszName;
		NStr::ToLower( szName );
		std::replace( szName.begin(), szName.end(), '/', '\\' );
		// Under the folder, ending .xml, with something between: the whole
		// storage-relative path less the extension - "scenarios\fieldsets\
		// summer\field00", the shape the engine's own templates carry in
		// their Fields lists and LoadDataResource opens back.
		if ( szName.size() <= szFolder.size() + 4 || szName.compare( 0, szFolder.size(), szFolder ) != 0 )
			continue;
		if ( szName.compare( szName.size() - 4, 4, ".xml" ) != 0 )
			continue;
		names.insert( szName.substr( 0, szName.size() - 4 ) );
	}
	pNames->assign( names.begin(), names.end() );
	return true;
}


// ---------------------------------------------------------------------------
// Create Random Map (M3, D-01..D-05)
// ---------------------------------------------------------------------------

namespace
{

// The generator's own IProgressHook (RMGC_CREATE_RANDOM_MAP_STEP_COUNT steps,
// one Step() each, the minimap's included), forwarding to the caller's C
// callback. The real interface, implemented here against the engine's header -
// the MFC's CCreateRandomMapProgress is its model. The callback only stores a
// counter or pumps the platform; it must never call back into the bridge (the
// overlay rule: the generator holds the engine's singletons while it runs).
class SBkProgressHook : public IProgressHook
{
	OBJECT_COMPLETE_METHODS( SBkProgressHook );
	void (*pfnReport)( int nStep, int nTotal, void *pUser );
	void *pUser;
	int nTotal;
	int nPos;
public:
	SBkProgressHook() : pfnReport( 0 ), pUser( 0 ), nTotal( RMGC_CREATE_RANDOM_MAP_STEP_COUNT ), nPos( 0 ) {  }
	void Init( void (*_pfnReport)( int, int, void * ), void *_pUser )
	{
		pfnReport = _pfnReport;
		pUser = _pUser;
	}
	virtual void STDCALL SetNumSteps( const int nRange, const float fPercentage = 1.0f ) { if ( nRange > 0 ) nTotal = nRange; }
	virtual void STDCALL Step()
	{
		++nPos;
		if ( pfnReport != 0 )
			pfnReport( Min( nPos, nTotal ), nTotal, pUser );
	}
	virtual void STDCALL Recover() {  }
	virtual void STDCALL SetCurrPos( const int _nPos ) { nPos = _nPos; }
	virtual int STDCALL GetCurrPos() const { return nPos; }
	virtual void Stop() {  }
};

// The user's own folder the generation lands in (D-17: <UserRoot>maps, or
// <UserRoot>mods/<Folder>/maps with a mod active) is the "maps" child of this
// root, which is what CreateRandomMap appends "maps\\<name>" to. Never taken
// from the caller.
std::filesystem::path GeneratedMapsRootPath( const std::string &rszModFolder )
{
	std::filesystem::path root( NPlatform::Paths::UserRoot() );
	if ( !rszModFolder.empty() )
		root = root / "mods" / rszModFolder;
	std::error_code error;
	return std::filesystem::absolute( root, error ).lexically_normal();
}

// The root the way NGeneratedData::Root spells one (random_missions_test.cpp's
// GeneratedRoot): absolute, backslashes, a trailing one.
std::string EngineRootOf( const std::filesystem::path &rRoot )
{
	std::string sz = rRoot.string();
	for ( char &c : sz )
		if ( c == '/' )
			c = '\\';
	if ( sz.empty() || sz[sz.size() - 1] != '\\' )
		sz += '\\';
	return sz;
}

// A folder for a containment test: absolute, normalised, '/' throughout,
// lower-cased, one trailing separator.
std::string ContainmentForm( const std::string &rszFolder )
{
	std::string sz = rszFolder;
	for ( char &c : sz )
		if ( c == '\\' )
			c = '/';
	std::error_code error;
	std::string szOut = std::filesystem::absolute( sz, error ).lexically_normal().generic_string();
	NStr::ToLower( szOut );
	if ( szOut.empty() || szOut[szOut.size() - 1] != '/' )
		szOut += '/';
	return szOut;
}

// True when rRoot is the game's data folder or inside it - by the storage's own
// name and by the installation's Data folder, whichever way each is spelled.
bool IsInsideTheData( const std::filesystem::path &rRoot, IDataStorage *pStorage )
{
	const std::string szRoot = ContainmentForm( rRoot.string() );
	const std::string szStorage = pStorage != 0 && pStorage->GetName() != 0 && *pStorage->GetName() != 0 ? ContainmentForm( pStorage->GetName() ) : std::string();
	const std::string szData = ContainmentForm( NPlatform::Paths::DataRoot() );
	return ( !szStorage.empty() && szRoot.compare( 0, szStorage.size(), szStorage ) == 0 ) ||
	       szRoot.compare( 0, szData.size(), szData ) == 0;
}

// A map name the way the dialog takes one: one path component under maps\, no
// separator of either kind, no drive, nothing relative.
bool IsBareMapName( const std::string &rszName )
{
	return rszName.size() < 96 && NPlatform::Paths::IsRelativeDataName( rszName ) &&
	       rszName.find_first_of( "\\/" ) == std::string::npos;
}

// A storage-relative RMG name: lower-cased like the game's own stats, no
// extension, not rooted, no ".." - and the file is in the storage.
bool CheckStorageName( SEditorSession *pSession, const char *pszField, std::string szName, bool bEmptyIsAny, std::string *pszOut )
{
	NStr::ToLower( szName );
	if ( szName.size() > 4 && szName.compare( szName.size() - 4, 4, ".xml" ) == 0 )
		szName.resize( szName.size() - 4 );
	if ( szName.empty() )
	{
		*pszOut = szName;
		if ( bEmptyIsAny )
			return true;
		pSession->szMessage = std::string( pszField ) + " is empty";
		return false;
	}
	if ( szName.size() >= 192 || !NPlatform::Paths::IsRelativeDataName( szName ) )
	{
		pSession->szMessage = std::string( pszField ) + " \"" + szName + "\" is not a name in the data";
		return false;
	}
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 || !pStorage->IsStreamExist( ( szName + ".xml" ).c_str() ) )
	{
		pSession->szMessage = std::string( pszField ) + " \"" + szName + "\" is not in the data";
		return false;
	}
	*pszOut = szName;
	return true;
}

// A seed as the generator's state. The StreamIO replacement the editor and the
// game run (StreamIOZig/legacy_bridge.cpp) keeps the generator's whole state in
// the first word of the seed blob; Restore reads the blob whole, so the seed is
// a blob with that word set - and the .seed file the generator writes holds the
// same word, which is where the seed used is read back from.
CPtr<IRandomGenSeed> SeedFromNumber( unsigned int nSeed )
{
	CPtr<IRandomGenSeed> pSeed = CreateObject<IRandomGenSeed>( STREAMIO_RANDOM_GEN_SEED );
	CPtr<IDataStream> pBlob = CreateObject<IDataStream>( STREAMIO_MEMORY_STREAM );
	if ( pSeed == 0 || pBlob == 0 )
		return 0;
	// randcnt, randrsl[256], randmem[256], randa, randb, randc.
	unsigned int blob[1 + 256 + 256 + 3];
	memset( blob, 0, sizeof blob );
	blob[0] = nSeed;
	pBlob->Write( blob, sizeof blob );
	pBlob->Seek( 0, STREAM_SEEK_SET );
	pSeed->Restore( pBlob );
	return pSeed;
}

// The first word of a .seed file: the seed a generation ran from.
bool ReadSeedWord( const std::string &rszSeedFile, unsigned int *pnSeed )
{
	CPtr<IDataStream> pStream = CreateFileStream( rszSeedFile.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	unsigned int nWord = 0;
	if ( pStream->Read( &nWord, sizeof nWord ) != int( sizeof nWord ) )
		return false;
	*pnSeed = nWord;
	return true;
}

}

bool CreateRandomMapInSession( SEditorSession *pSession, const SRMGenerateParams &rParams, bool *pbRefused, SRMGenerateResult *pResult )
{
	if ( pbRefused != 0 )
		*pbRefused = false;
	if ( pSession == 0 || pResult == 0 )
		return false;
	// Every refusal names its field and changes nothing: nothing below the
	// checks writes before the generator itself runs.
	struct SRefuse
	{
		bool *pbRefused;
		bool operator()() const { if ( pbRefused != 0 ) *pbRefused = true; return false; }
	} refuse = { pbRefused };
	if ( !pSession->bEngineStarted )
	{
		pSession->szMessage = "the engine is not started";
		return refuse();
	}
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	IRandomGen *pRandomGen = GetSingleton<IRandomGen>();
	if ( pStorage == 0 || pRandomGen == 0 )
	{
		pSession->szMessage = "the data storage or the random generator is not there";
		return refuse();
	}
	SMissionStats missionStats;
	std::string szContext;
	std::string szSetting;
	if ( !CheckStorageName( pSession, "template", rParams.szTemplate, false, &missionStats.szTemplateMap ) ||
	     !CheckStorageName( pSession, "context", rParams.szContext, false, &szContext ) )
		return refuse();
	if ( rParams.szSetting != RMGC_ANY_SETTING_NAME && !CheckStorageName( pSession, "setting", rParams.szSetting, true, &szSetting ) )
		return refuse();
	if ( rParams.nLevel < 0 || rParams.nLevel >= 3 )
	{
		pSession->szMessage = NStr::Format( "level %d is outside 0..2", rParams.nLevel );
		return refuse();
	}
	if ( rParams.nAngle < -1 || rParams.nAngle > 3 )
	{
		pSession->szMessage = NStr::Format( "angle %d is outside -1..3", rParams.nAngle );
		return refuse();
	}
	if ( !IsBareMapName( rParams.szMapName ) )
	{
		pSession->szMessage = "map name \"" + rParams.szMapName + "\" is not a plain name (no folder, drive or dots)";
		return refuse();
	}
	// The template decides the graph range (the MFC's own field is an unchecked
	// edit; the generator asserts on a bad one, which a refusal precedes).
	{
		SRMTemplate randomMapTemplate;
		if ( !LoadDataResource( missionStats.szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate ) )
		{
			pSession->szMessage = "template \"" + missionStats.szTemplateMap + "\" does not load as a template";
			return refuse();
		}
		if ( rParams.nGraph < -1 || rParams.nGraph >= int( randomMapTemplate.graphs.size() ) )
		{
			pSession->szMessage = NStr::Format( "graph %d is outside -1..%d for template \"%s\"", rParams.nGraph, int( randomMapTemplate.graphs.size() ) - 1, missionStats.szTemplateMap.c_str() );
			return refuse();
		}
		if ( randomMapTemplate.graphs.empty() )
		{
			pSession->szMessage = "template \"" + missionStats.szTemplateMap + "\" has no graphs";
			return refuse();
		}
	}
	// The output root: the user's (or the mod's) own folder, never the data.
	const std::filesystem::path rootPath = GeneratedMapsRootPath( rParams.szModFolder );
	if ( rootPath.empty() || IsInsideTheData( rootPath, pStorage ) )
	{
		pSession->szMessage = "the output folder would be inside the game's data";
		return refuse();
	}
	const std::string szRoot = EngineRootOf( rootPath );
	const std::string szMapBase = szRoot + "maps\\" + rParams.szMapName;
	std::string szMapFile = szMapBase + ( rParams.bSaveAsBZM ? ".bzm" : ".xml" );
	std::string szNative = szMapFile;
#if !defined(_WIN32)
	for ( char &c : szNative )
		if ( c == '\\' )
			c = '/';
#endif
	{
		std::error_code error;
		if ( !rParams.bOverwrite && std::filesystem::exists( szNative, error ) )
		{
			pSession->szMessage = "a map named \"" + rParams.szMapName + "\" already exists in " + szRoot + "maps";
			return refuse();
		}
	}

	// The seed: the caller's, or a fresh one - either way a number the result
	// names, so any generation can be asked for again.
	unsigned int nSeed = rParams.nSeed;
	if ( !rParams.bHasSeed )
	{
		std::random_device device;
		nSeed = ( ( unsigned int )( device() ) << 16 ) ^ ( unsigned int )( device() );
	}
	CPtr<IRandomGenSeed> pSeed = SeedFromNumber( nSeed );
	if ( pSeed == 0 )
	{
		pSession->szMessage = "the engine has no seed object";
		return false;
	}
	// Both handles: the engine's Random() draws from the global one, the
	// generator's own seed store reads the singleton (the same object in the
	// replacement StreamIO, kept apart here the way the fields' fills do).
	if ( g_pGlobalRandomGen != 0 )
		g_pGlobalRandomGen->SetSeed( pSeed );
	pRandomGen->SetSeed( pSeed );

	missionStats.szSettingName = szSetting;
	missionStats.szFinalMap = rParams.szMapName;
	// The briefing picture beside the map ("<name>_large"): the MFC left it at
	// the working directory; here everything of one generation sits together.
	missionStats.szMapImage = "maps\\" + rParams.szMapName + "_large";
	CPtr<SBkProgressHook> pHook = new SBkProgressHook();
	pHook->Init( rParams.pfnProgress, rParams.pUser );
	SRMUsedTemplateInfo used;
	if ( !CMapInfo::CreateRandomMap( &missionStats, szContext, rParams.nLevel, rParams.nGraph, rParams.nAngle, rParams.bSaveAsBZM, rParams.bWriteDDS, &used, pHook, szRoot ) )
	{
		pSession->szMessage = "the generator could not create the map \"" + rParams.szMapName + "\" from template \"" + missionStats.szTemplateMap + "\"";
		return false;
	}
	// What was used, for the result: the seed read back from the file the
	// generator wrote (the state it started from), the graph and angle it chose.
	unsigned int nSeedUsed = nSeed;
	if ( !ReadSeedWord( szMapBase + ".seed", &nSeedUsed ) )
		nSeedUsed = nSeed;
	pResult->nSeed = nSeedUsed;
	pResult->nGraph = -1;
	{
		SRMTemplate randomMapTemplate;
		if ( LoadDataResource( missionStats.szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate ) )
			for ( int i = 0; i < int( randomMapTemplate.graphs.size() ); ++i )
				if ( randomMapTemplate.graphs[i] == used.szGraphName )
				{
					pResult->nGraph = i;
					break;
				}
	}
	pResult->nAngle = used.nGraphAngle;
	pResult->szGraphName = used.szGraphName;
	pResult->szMapPath = szNative;
	return true;
}


// ---------------------------------------------------------------------------
// Export lists (M3, D-13): the enumerations MainFrm.cpp OnTool0-3 walk
// ---------------------------------------------------------------------------

bool ListStorageFiles( SEditorSession *pSession, const std::string &rszFolder, const std::string &rszExtension, std::vector<std::string> *pNames, bool *pbRefused )
{
	pNames->clear();
	if ( pbRefused != 0 )
		*pbRefused = false;
	if ( pSession == 0 )
		return false;
	std::string szFolder = rszFolder;
	std::string szExtension = rszExtension;
	NStr::ToLower( szFolder );
	NStr::ToLower( szExtension );
	std::replace( szFolder.begin(), szFolder.end(), '/', '\\' );
	// A folder is a plain relative name with its trailing separator; an
	// extension is a suffix with no separator in it.
	if ( szFolder.size() < 2 || szFolder[szFolder.size() - 1] != '\\' ||
	     !NPlatform::Paths::IsRelativeDataName( szFolder.substr( 0, szFolder.size() - 1 ) ) ||
	     szExtension.empty() || szExtension.size() > 16 || szExtension.find_first_of( "\\/:" ) != std::string::npos )
	{
		pSession->szMessage = "the folder or the extension of the storage listing is not plain";
		if ( pbRefused != 0 )
			*pbRefused = true;
		return false;
	}
	IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
	CPtr<IStorageEnumerator> pEnum = pDataStorage != 0 ? pDataStorage->CreateEnumerator() : 0;
	if ( pEnum == 0 )
	{
		pSession->szMessage = "the data storage does not enumerate";
		return false;
	}
	std::set<std::string> names;
	for ( pEnum->Reset( "*.*" ); pEnum->Next(); )
	{
		const SStorageElementStats *pStats = pEnum->GetStats();
		if ( pStats == 0 || pStats->pszName == 0 )
			continue;
		std::string szName = pStats->pszName;
		NStr::ToLower( szName );
		std::replace( szName.begin(), szName.end(), '/', '\\' );
		// The MFC's own test (Resource_Functions.cpp EnumFilesInDataStorage):
		// under the folder with something after it, ending in the extension.
		if ( szName.size() <= szFolder.size() || szName.compare( 0, szFolder.size(), szFolder ) != 0 )
			continue;
		if ( szName.size() < szExtension.size() || szName.compare( szName.size() - szExtension.size(), szExtension.size(), szExtension ) != 0 )
			continue;
		names.insert( szName );
	}
	pNames->assign( names.begin(), names.end() );
	return true;
}

bool ListTemplateGraphs( SEditorSession *pSession, const std::string &rszTemplate, std::vector<SRMTemplateGraph> *pGraphs, bool *pbRefused )
{
	pGraphs->clear();
	if ( pbRefused != 0 )
		*pbRefused = false;
	if ( pSession == 0 )
		return false;
	std::string szName;
	if ( !CheckStorageName( pSession, "template", rszTemplate, false, &szName ) )
	{
		if ( pbRefused != 0 )
			*pbRefused = true;
		return false;
	}
	SRMTemplate randomMapTemplate;
	if ( !LoadDataResource( szName, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate ) )
	{
		pSession->szMessage = "template \"" + szName + "\" does not load as a template";
		if ( pbRefused != 0 )
			*pbRefused = true;
		return false;
	}
	for ( int i = 0; i < int( randomMapTemplate.graphs.size() ); ++i )
	{
		SRMTemplateGraph graph;
		graph.szName = randomMapTemplate.graphs[i];
		graph.nWeight = int( randomMapTemplate.graphs.GetWeight( i ) );
		pGraphs->push_back( graph );
	}
	return true;
}
