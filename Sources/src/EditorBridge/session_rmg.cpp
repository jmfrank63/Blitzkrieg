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
#include "../MapFile/MapFile.h"
#include <algorithm>
#include <cmath>
#include <cstring>
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
		case 2: return "scenarios\\graphs\\";
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


// ---------------------------------------------------------------------------
// The user RMG storage root and the composer records (M3 05-09, D-06..D-12)
// ---------------------------------------------------------------------------

namespace
{

// "<UserRoot>[mods/<Folder>/]rmg", absolute and normalised, in the host's own
// separators.
std::filesystem::path RmgRootPath( const std::string &rszModFolder )
{
	std::filesystem::path root( NPlatform::Paths::UserRoot() );
	if ( !rszModFolder.empty() )
		root = root / "mods" / rszModFolder;
	root = root / "rmg";
	std::error_code error;
	return std::filesystem::absolute( root, error ).lexically_normal();
}

std::string HostForm( std::string szPath )
{
#if !defined(_WIN32)
	for ( char &c : szPath )
		if ( c == '\\' )
			c = '/';
#endif
	return szPath;
}

std::string EngineForm( std::string szPath )
{
	for ( char &c : szPath )
		if ( c == '/' )
			c = '\\';
	return szPath;
}

// A bounded field: false when the text does not fit with its terminator
// (nothing is ever truncated).
template<size_t N>
bool PutField( char ( &rField )[N], const std::string &rszText )
{
	if ( rszText.size() >= N )
		return false;
	memset( rField, 0, N );
	memcpy( rField, rszText.c_str(), rszText.size() );
	return true;
}

// A field a caller filled: terminated inside its buffer (a caller bug else).
template<size_t N>
bool GetField( const char ( &rField )[N], std::string *pszOut )
{
	const void *pEnd = memchr( rField, 0, N );
	if ( pEnd == 0 )
		return false;
	*pszOut = rField;
	return true;
}

// Which folder each composer record lives under.
const char *RecordFolder( bool bGraph )
{
	return bGraph ? "scenarios\\graphs\\" : "scenarios\\containers\\";
}

// A composer record's storage name (T-05-09-01): lower-cased, backslashes, no
// .xml, a plain relative name under the kind's folder with something after
// it, nothing a host filesystem would take as a wildcard or a control.
// False with the reason in szMessage.
bool CheckRecordName( SEditorSession *pSession, bool bGraph, const std::string &rszName, std::string *pszOut )
{
	std::string szName = rszName;
	NStr::ToLower( szName );
	std::replace( szName.begin(), szName.end(), '/', '\\' );
	if ( szName.size() > 4 && szName.compare( szName.size() - 4, 4, ".xml" ) == 0 )
		szName.resize( szName.size() - 4 );
	const std::string szFolder = RecordFolder( bGraph );
	bool bPlain = szName.size() < 192 && szName.size() > szFolder.size() && szName.compare( 0, szFolder.size(), szFolder ) == 0 &&
	              NPlatform::Paths::IsRelativeDataName( szName );
	for ( size_t i = 0; bPlain && i < szName.size(); ++i )
		bPlain = ( unsigned char )szName[i] >= 0x20 && strchr( "<>\"|*?", szName[i] ) == 0;
	if ( !bPlain )
	{
		pSession->szMessage = "\"" + rszName + "\" is not a name under " + szFolder + " (a plain relative storage name, no extension)";
		return false;
	}
	*pszOut = szName;
	return true;
}

// Fills one of the record's arrays as far as it fits. *pnCount is always the
// total; the caller compares it with the capacity. False only when an item
// would not fit its own field.
template<class T, class Source>
bool FillArray( T *pOut, int nCapacity, int *pnCount, const std::vector<Source> &rItems, bool ( *pfnPut )( T *, const Source & ) )
{
	*pnCount = int( rItems.size() );
	const int nWrite = Min( int( rItems.size() ), nCapacity );
	for ( int i = 0; i < nWrite; ++i )
		if ( !pfnPut( &pOut[i], rItems[size_t( i )] ) )
			return false;
	return true;
}

bool PutInt( int *pOut, const int &rValue ) { *pOut = rValue; return true; }
bool PutName( BkEditorRmgName *pOut, const std::string &rszValue ) { return PutField( pOut->name, rszValue ); }

// The script lists of a record, filled the same way for all three records.
// *pbShort is set when an array was short; false when a name would not fit.
bool FillScripts( BkEditorRmgScripts *pScripts, const CUsedScriptIDs &rIDs, const CUsedScriptAreas &rAreas, bool *pbShort )
{
	std::vector<int> ids( rIDs.begin(), rIDs.end() );
	std::vector<std::string> areas( rAreas.begin(), rAreas.end() );
	if ( !FillArray<int, int>( pScripts->ids, pScripts->id_capacity, &pScripts->id_count, ids, PutInt ) ||
	     !FillArray<BkEditorRmgName, std::string>( pScripts->areas, pScripts->area_capacity, &pScripts->area_count, areas, PutName ) )
		return false;
	if ( pScripts->id_count > pScripts->id_capacity || pScripts->area_count > pScripts->area_capacity )
		*pbShort = true;
	return true;
}

// The script lists a write was given, checked and read.
bool TakeScripts( SEditorSession *pSession, const BkEditorRmgScripts &rScripts, CUsedScriptIDs *pIDs, CUsedScriptAreas *pAreas, bool *pbBad )
{
	if ( rScripts.id_count < 0 || rScripts.area_count < 0 || ( rScripts.id_count > 0 && rScripts.ids == 0 ) || ( rScripts.area_count > 0 && rScripts.areas == 0 ) )
	{
		pSession->szMessage = "a script list has a negative count or no array";
		*pbBad = true;
		return false;
	}
	if ( rScripts.id_count > BK_EDITOR_RMG_MAX_SCRIPT_IDS || rScripts.area_count > BK_EDITOR_RMG_MAX_SCRIPT_AREAS )
	{
		pSession->szMessage = NStr::Format( "a record holds at most %d script IDs and %d script areas", BK_EDITOR_RMG_MAX_SCRIPT_IDS, BK_EDITOR_RMG_MAX_SCRIPT_AREAS );
		return false;
	}
	for ( int i = 0; i < rScripts.id_count; ++i )
		pIDs->insert( rScripts.ids[i] );
	for ( int i = 0; i < rScripts.area_count; ++i )
	{
		std::string szArea;
		if ( !GetField( rScripts.areas[i].name, &szArea ) )
		{
			pSession->szMessage = "a script area name is not terminated";
			*pbBad = true;
			return false;
		}
		pAreas->insert( szArea );
	}
	return true;
}

// The whole of a record's XML out through the engine's own tree saver to an
// absolute file (engine spelling). SaveDataResource writes under the data
// storage's name - the shipped Data - so the composers' writes do their own
// path, with the same serialiser (operator& under the same label).
template<class Type>
bool WriteRmgXml( const std::string &rszEngineFile, const char *pszLabel, Type &rResource )
{
	try
	{
		CPtr<IDataStream> pStream = CreateFileStream( rszEngineFile.c_str(), STREAM_ACCESS_WRITE );
		if ( pStream == 0 )
			return false;
		CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
		CTreeAccessor saver = pSaver;
		saver.Add( pszLabel, &rResource );
	}
	catch ( ... )
	{
		return false;
	}
	return true;
}

bool SameFloat( float fLeft, float fRight )
{
	return fabsf( fLeft - fRight ) <= 1e-5f * Max( 1.0f, Max( fabsf( fLeft ), fabsf( fRight ) ) );
}

bool SameContainer( const SRMContainer &rA, const SRMContainer &rB )
{
	if ( rA.patches.size() != rB.patches.size() || rA.size != rB.size || rA.nSeason != rB.nSeason || rA.szSeasonFolder != rB.szSeasonFolder ||
	     rA.usedScriptIDs != rB.usedScriptIDs || rA.usedScriptAreas != rB.usedScriptAreas )
		return false;
	for ( size_t i = 0; i < rA.patches.size(); ++i )
		if ( rA.patches[i].size != rB.patches[i].size || rA.patches[i].szFileName != rB.patches[i].szFileName || rA.patches[i].szPlace != rB.patches[i].szPlace )
			return false;
	for ( int d = 0; d < 4; ++d )
		if ( rA.indices[d] != rB.indices[d] )
			return false;
	return true;
}

bool SameGraph( const SRMGraph &rA, const SRMGraph &rB )
{
	if ( rA.nodes.size() != rB.nodes.size() || rA.links.size() != rB.links.size() || rA.size != rB.size || rA.nSeason != rB.nSeason ||
	     rA.szSeasonFolder != rB.szSeasonFolder || rA.usedScriptIDs != rB.usedScriptIDs || rA.usedScriptAreas != rB.usedScriptAreas )
		return false;
	for ( size_t i = 0; i < rA.nodes.size(); ++i )
		if ( rA.nodes[i].rect.minx != rB.nodes[i].rect.minx || rA.nodes[i].rect.miny != rB.nodes[i].rect.miny ||
		     rA.nodes[i].rect.maxx != rB.nodes[i].rect.maxx || rA.nodes[i].rect.maxy != rB.nodes[i].rect.maxy ||
		     rA.nodes[i].szContainerFileName != rB.nodes[i].szContainerFileName )
			return false;
	for ( size_t i = 0; i < rA.links.size(); ++i )
	{
		const SRMGraphLink &a = rA.links[i];
		const SRMGraphLink &b = rB.links[i];
		if ( a.link != b.link || a.nType != b.nType || a.szDescFileName != b.szDescFileName || a.nParts != b.nParts ||
		     !SameFloat( a.fRadius, b.fRadius ) || !SameFloat( a.fMinLength, b.fMinLength ) || !SameFloat( a.fDistance, b.fDistance ) || !SameFloat( a.fDisturbance, b.fDisturbance ) )
			return false;
	}
	return true;
}

// Where a write goes and whether it may: the file under the user root, and a
// refusal (the Save-As message) when the name is a shipped file - present in
// the storage stack but not in the user's own layer.
bool PrepareRecordWrite( SEditorSession *pSession, const std::string &rszName, std::string *pszEngineFile, std::string *pszHostFile )
{
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 || pSession->pRmgStorage == 0 )
	{
		pSession->szMessage = "the user RMG root is not mounted";
		return false;
	}
	const std::string szFile = rszName + ".xml";
	if ( !pSession->pRmgStorage->IsStreamExist( szFile.c_str() ) && pStorage->IsStreamExist( szFile.c_str() ) )
	{
		pSession->szMessage = "\"" + rszName + "\" is shipped data and read-only: Save As a new name";
		return false;
	}
	const std::string szRoot = pSession->szRmgMountedRoot;
	*pszEngineFile = szRoot + szFile;
	*pszHostFile = HostForm( *pszEngineFile );
	std::error_code error;
	std::filesystem::create_directories( std::filesystem::path( *pszHostFile ).parent_path(), error );
	if ( error )
	{
		pSession->szMessage = "the folder under the user RMG root could not be created: " + error.message();
		return false;
	}
	return true;
}

}

std::string RmgRootHostPath( const std::string &rszModFolder )
{
	return RmgRootPath( rszModFolder ).string();
}

bool MountRmgRoot( SEditorSession *pSession, const std::string &rszModFolder, IDataStorage *pModStorage, bool bForce )
{
	if ( pSession == 0 )
		return false;
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 )
		return false;
	const std::string szRoot = EngineRootOf( RmgRootPath( rszModFolder ) );
	if ( !bForce && pSession->pRmgStorage != 0 && pSession->szRmgMountedRoot == szRoot )
		return true;
	CPtr<IDataStorage> pRmg = OpenStorage( ( szRoot + "*.pak" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
	// The mod layer comes off first and goes back on top, so the user's layer
	// stays below it however the two changed.
	pStorage->RemoveStorage( "MOD" );
	pStorage->RemoveStorage( "RMG_USER" );
	if ( pRmg != 0 )
		pStorage->AddStorage( pRmg, "RMG_USER" );
	if ( pModStorage != 0 )
		pStorage->AddStorage( pModStorage, "MOD" );
	pSession->pRmgStorage = pRmg;
	pSession->szRmgMountedRoot = pRmg != 0 ? szRoot : std::string();
	return pRmg != 0;
}

bool ReadRmgContainerRecord( SEditorSession *pSession, const std::string &rszName, BkEditorRmgContainerRecord *pRecord, bool *pbRefused )
{
	*pbRefused = false;
	pRecord->patch_count = pRecord->index_counts[0] = pRecord->index_counts[1] = pRecord->index_counts[2] = pRecord->index_counts[3] = 0;
	pRecord->scripts.id_count = pRecord->scripts.area_count = 0;
	std::string szName;
	if ( !CheckRecordName( pSession, false, rszName, &szName ) )
	{
		*pbRefused = true;
		return false;
	}
	SRMContainer container;
	if ( !LoadDataResource( szName, "", false, 0, RMGC_CONTAINER_XML_NAME, container ) )
	{
		pSession->szMessage = "container \"" + szName + "\" is not in the data or does not load as a container";
		*pbRefused = true;
		return false;
	}
	pRecord->size_x = container.size.x;
	pRecord->size_y = container.size.y;
	pRecord->season = container.nSeason;
	if ( !PutField( pRecord->season_folder, container.szSeasonFolder ) )
	{
		pSession->szMessage = "the season folder does not fit its field";
		return false;
	}
	bool bShort = false;
	// Patches.
	pRecord->patch_count = int( container.patches.size() );
	for ( int i = 0; i < Min( pRecord->patch_count, pRecord->patch_capacity ); ++i )
	{
		const SRMPatch &rPatch = container.patches[size_t( i )];
		BkEditorRmgPatch &rOut = pRecord->patches[i];
		rOut.size_x = rPatch.size.x;
		rOut.size_y = rPatch.size.y;
		if ( !PutField( rOut.name, rPatch.szFileName ) || !PutField( rOut.place, rPatch.szPlace ) )
		{
			pSession->szMessage = "a patch name or setting does not fit its field";
			return false;
		}
	}
	bShort = pRecord->patch_count > pRecord->patch_capacity;
	// The four index lists, flat.
	int nIndexTotal = 0;
	for ( int d = 0; d < 4; ++d )
	{
		pRecord->index_counts[d] = int( container.indices[d].size() );
		nIndexTotal += pRecord->index_counts[d];
	}
	if ( nIndexTotal <= pRecord->index_capacity )
	{
		int nAt = 0;
		for ( int d = 0; d < 4; ++d )
			for ( size_t i = 0; i < container.indices[d].size(); ++i )
				pRecord->indices[nAt++] = container.indices[d][i];
	}
	else
		bShort = true;
	if ( !FillScripts( &pRecord->scripts, container.usedScriptIDs, container.usedScriptAreas, &bShort ) )
	{
		pSession->szMessage = "a script area name does not fit its field";
		return false;
	}
	if ( bShort )
		pSession->szMessage = "the record's arrays are short: the counts are the totals";
	return !bShort;
}

bool WriteRmgContainerRecord( SEditorSession *pSession, const std::string &rszName, const BkEditorRmgContainerRecord &rRecord, bool *pbRefused, bool *pbBadArgument )
{
	*pbRefused = *pbBadArgument = false;
	std::string szName;
	if ( !CheckRecordName( pSession, false, rszName, &szName ) )
	{
		*pbBadArgument = true;
		return false;
	}
	const int nPatches = rRecord.patch_count;
	int nIndexTotal = 0;
	for ( int d = 0; d < 4; ++d )
	{
		if ( rRecord.index_counts[d] < 0 )
		{
			pSession->szMessage = "an index count is negative";
			*pbBadArgument = true;
			return false;
		}
		nIndexTotal += Min( rRecord.index_counts[d], 1 << 20 );
	}
	if ( nPatches < 0 || ( nPatches > 0 && rRecord.patches == 0 ) || ( nIndexTotal > 0 && rRecord.indices == 0 ) )
	{
		pSession->szMessage = "a patch or index count has no array, or is negative";
		*pbBadArgument = true;
		return false;
	}
	if ( nPatches > BK_EDITOR_RMG_MAX_PATCHES || nIndexTotal > 4 * BK_EDITOR_RMG_MAX_PATCHES )
	{
		pSession->szMessage = NStr::Format( "a container holds at most %d patches (and an index list at most that many entries)", BK_EDITOR_RMG_MAX_PATCHES );
		*pbRefused = true;
		return false;
	}
	SRMContainer container;
	container.size = CTPoint<int>( rRecord.size_x, rRecord.size_y );
	container.nSeason = rRecord.season;
	if ( !GetField( rRecord.season_folder, &container.szSeasonFolder ) )
	{
		pSession->szMessage = "the season folder is not terminated";
		*pbBadArgument = true;
		return false;
	}
	if ( rRecord.season < 0 || rRecord.season > 3 )
	{
		pSession->szMessage = NStr::Format( "season %d is outside 0..3", rRecord.season );
		*pbRefused = true;
		return false;
	}
	for ( int i = 0; i < nPatches; ++i )
	{
		SRMPatch patch;
		std::string szPatch, szPlace;
		if ( !GetField( rRecord.patches[i].name, &szPatch ) || !GetField( rRecord.patches[i].place, &szPlace ) )
		{
			pSession->szMessage = "a patch name or setting is not terminated";
			*pbBadArgument = true;
			return false;
		}
		if ( !NPlatform::Paths::IsRelativeDataName( szPatch ) )
		{
			pSession->szMessage = NStr::Format( "patch %d: \"%s\" is not a name in the data", i, szPatch.c_str() );
			*pbRefused = true;
			return false;
		}
		patch.size = CTPoint<int>( rRecord.patches[i].size_x, rRecord.patches[i].size_y );
		patch.szFileName = szPatch;
		patch.szPlace = szPlace;
		container.patches.push_back( patch );
	}
	int nAt = 0;
	for ( int d = 0; d < 4; ++d )
		for ( int i = 0; i < rRecord.index_counts[d]; ++i )
		{
			const int nIndex = rRecord.indices[nAt++];
			if ( nIndex < 0 || nIndex >= nPatches )
			{
				pSession->szMessage = NStr::Format( "direction %d lists patch %d, and there are %d patches", d, nIndex, nPatches );
				*pbRefused = true;
				return false;
			}
			container.indices[d].push_back( nIndex );
		}
	if ( !TakeScripts( pSession, rRecord.scripts, &container.usedScriptIDs, &container.usedScriptAreas, pbBadArgument ) )
	{
		*pbRefused = !*pbBadArgument;
		return false;
	}
	std::string szEngineFile, szHostFile;
	if ( !PrepareRecordWrite( pSession, szName, &szEngineFile, &szHostFile ) )
	{
		*pbRefused = true;
		return false;
	}
	if ( !WriteRmgXml( szEngineFile, RMGC_CONTAINER_XML_NAME, container ) )
	{
		pSession->szMessage = "the container file could not be written: " + szHostFile;
		return false;
	}
	// What was written is read back through the storage and compared with
	// what was meant before this says OK (the BkEditorSaveMap habit).
	SRMContainer readBack;
	if ( !LoadDataResource( szName, "", false, 0, RMGC_CONTAINER_XML_NAME, readBack ) || !SameContainer( container, readBack ) )
	{
		pSession->szMessage = "the container written does not read back as the one given: " + szHostFile;
		return false;
	}
	return true;
}

bool ReadRmgGraphRecord( SEditorSession *pSession, const std::string &rszName, BkEditorRmgGraphRecord *pRecord, bool *pbRefused )
{
	*pbRefused = false;
	pRecord->node_count = pRecord->link_count = 0;
	pRecord->scripts.id_count = pRecord->scripts.area_count = 0;
	std::string szName;
	if ( !CheckRecordName( pSession, true, rszName, &szName ) )
	{
		*pbRefused = true;
		return false;
	}
	SRMGraph graph;
	if ( !LoadDataResource( szName, "", false, 0, RMGC_GRAPH_XML_NAME, graph ) )
	{
		pSession->szMessage = "graph \"" + szName + "\" is not in the data or does not load as a graph";
		*pbRefused = true;
		return false;
	}
	pRecord->size_x = graph.size.x;
	pRecord->size_y = graph.size.y;
	pRecord->season = graph.nSeason;
	if ( !PutField( pRecord->season_folder, graph.szSeasonFolder ) )
	{
		pSession->szMessage = "the season folder does not fit its field";
		return false;
	}
	pRecord->node_count = int( graph.nodes.size() );
	for ( int i = 0; i < Min( pRecord->node_count, pRecord->node_capacity ); ++i )
	{
		const SRMGraphNode &rNode = graph.nodes[size_t( i )];
		BkEditorRmgNode &rOut = pRecord->nodes[i];
		rOut.x1 = rNode.rect.minx;
		rOut.y1 = rNode.rect.miny;
		rOut.x2 = rNode.rect.maxx;
		rOut.y2 = rNode.rect.maxy;
		if ( !PutField( rOut.container, rNode.szContainerFileName ) )
		{
			pSession->szMessage = "a node's container name does not fit its field";
			return false;
		}
	}
	pRecord->link_count = int( graph.links.size() );
	for ( int i = 0; i < Min( pRecord->link_count, pRecord->link_capacity ); ++i )
	{
		const SRMGraphLink &rLink = graph.links[size_t( i )];
		BkEditorRmgLink &rOut = pRecord->links[i];
		rOut.a = rLink.link.a;
		rOut.b = rLink.link.b;
		rOut.type = rLink.nType;
		rOut.radius = rLink.fRadius;
		rOut.parts = rLink.nParts;
		rOut.min_length = rLink.fMinLength;
		rOut.distance = rLink.fDistance;
		rOut.disturbance = rLink.fDisturbance;
		if ( !PutField( rOut.desc, rLink.szDescFileName ) )
		{
			pSession->szMessage = "a link's descriptor name does not fit its field";
			return false;
		}
	}
	bool bShort = pRecord->node_count > pRecord->node_capacity || pRecord->link_count > pRecord->link_capacity;
	if ( !FillScripts( &pRecord->scripts, graph.usedScriptIDs, graph.usedScriptAreas, &bShort ) )
	{
		pSession->szMessage = "a script area name does not fit its field";
		return false;
	}
	if ( bShort )
		pSession->szMessage = "the record's arrays are short: the counts are the totals";
	return !bShort;
}

bool WriteRmgGraphRecord( SEditorSession *pSession, const std::string &rszName, const BkEditorRmgGraphRecord &rRecord, bool *pbRefused, bool *pbBadArgument )
{
	*pbRefused = *pbBadArgument = false;
	std::string szName;
	if ( !CheckRecordName( pSession, true, rszName, &szName ) )
	{
		*pbBadArgument = true;
		return false;
	}
	if ( rRecord.node_count < 0 || rRecord.link_count < 0 || ( rRecord.node_count > 0 && rRecord.nodes == 0 ) || ( rRecord.link_count > 0 && rRecord.links == 0 ) )
	{
		pSession->szMessage = "a node or link count has no array, or is negative";
		*pbBadArgument = true;
		return false;
	}
	if ( rRecord.node_count > BK_EDITOR_RMG_MAX_NODES || rRecord.link_count > BK_EDITOR_RMG_MAX_LINKS )
	{
		pSession->szMessage = NStr::Format( "a graph holds at most %d nodes and %d links", BK_EDITOR_RMG_MAX_NODES, BK_EDITOR_RMG_MAX_LINKS );
		*pbRefused = true;
		return false;
	}
	if ( rRecord.season < 0 || rRecord.season > 3 )
	{
		pSession->szMessage = NStr::Format( "season %d is outside 0..3", rRecord.season );
		*pbRefused = true;
		return false;
	}
	SRMGraph graph;
	graph.size = CTPoint<int>( rRecord.size_x, rRecord.size_y );
	graph.nSeason = rRecord.season;
	if ( !GetField( rRecord.season_folder, &graph.szSeasonFolder ) )
	{
		pSession->szMessage = "the season folder is not terminated";
		*pbBadArgument = true;
		return false;
	}
	for ( int i = 0; i < rRecord.node_count; ++i )
	{
		const BkEditorRmgNode &rIn = rRecord.nodes[i];
		SRMGraphNode node;
		if ( !GetField( rIn.container, &node.szContainerFileName ) )
		{
			pSession->szMessage = "a node's container name is not terminated";
			*pbBadArgument = true;
			return false;
		}
		if ( rIn.x2 <= rIn.x1 || rIn.y2 <= rIn.y1 )
		{
			pSession->szMessage = NStr::Format( "node %d has no area (%d,%d)-(%d,%d)", i, rIn.x1, rIn.y1, rIn.x2, rIn.y2 );
			*pbRefused = true;
			return false;
		}
		if ( !node.szContainerFileName.empty() && !NPlatform::Paths::IsRelativeDataName( node.szContainerFileName ) )
		{
			pSession->szMessage = NStr::Format( "node %d: \"%s\" is not a name in the data", i, node.szContainerFileName.c_str() );
			*pbRefused = true;
			return false;
		}
		node.rect = CTRect<int>( rIn.x1, rIn.y1, rIn.x2, rIn.y2 );
		graph.nodes.push_back( node );
	}
	for ( int i = 0; i < rRecord.link_count; ++i )
	{
		const BkEditorRmgLink &rIn = rRecord.links[i];
		SRMGraphLink link;
		if ( !GetField( rIn.desc, &link.szDescFileName ) )
		{
			pSession->szMessage = "a link's descriptor name is not terminated";
			*pbBadArgument = true;
			return false;
		}
		if ( rIn.a < 0 || rIn.a >= rRecord.node_count || rIn.b < 0 || rIn.b >= rRecord.node_count )
		{
			pSession->szMessage = NStr::Format( "link %d joins nodes %d and %d, and there are %d nodes", i, rIn.a, rIn.b, rRecord.node_count );
			*pbRefused = true;
			return false;
		}
		if ( rIn.type < 0 || rIn.type > 1 || rIn.parts < 0 || !std::isfinite( rIn.radius ) || !std::isfinite( rIn.min_length ) || !std::isfinite( rIn.distance ) || !std::isfinite( rIn.disturbance ) )
		{
			pSession->szMessage = NStr::Format( "link %d has a type outside 0..1, a negative part count or a number that is not finite", i );
			*pbRefused = true;
			return false;
		}
		if ( !link.szDescFileName.empty() && !NPlatform::Paths::IsRelativeDataName( link.szDescFileName ) )
		{
			pSession->szMessage = NStr::Format( "link %d: \"%s\" is not a name in the data", i, link.szDescFileName.c_str() );
			*pbRefused = true;
			return false;
		}
		link.link = CTPoint<int>( rIn.a, rIn.b );
		link.nType = rIn.type;
		link.fRadius = rIn.radius;
		link.nParts = rIn.parts;
		link.fMinLength = rIn.min_length;
		link.fDistance = rIn.distance;
		link.fDisturbance = rIn.disturbance;
		graph.links.push_back( link );
	}
	if ( !TakeScripts( pSession, rRecord.scripts, &graph.usedScriptIDs, &graph.usedScriptAreas, pbBadArgument ) )
	{
		*pbRefused = !*pbBadArgument;
		return false;
	}
	std::string szEngineFile, szHostFile;
	if ( !PrepareRecordWrite( pSession, szName, &szEngineFile, &szHostFile ) )
	{
		*pbRefused = true;
		return false;
	}
	if ( !WriteRmgXml( szEngineFile, RMGC_GRAPH_XML_NAME, graph ) )
	{
		pSession->szMessage = "the graph file could not be written: " + szHostFile;
		return false;
	}
	SRMGraph readBack;
	if ( !LoadDataResource( szName, "", false, 0, RMGC_GRAPH_XML_NAME, readBack ) || !SameGraph( graph, readBack ) )
	{
		pSession->szMessage = "the graph written does not read back as the one given: " + szHostFile;
		return false;
	}
	return true;
}

bool ReadRmgPatchInfo( SEditorSession *pSession, const std::string &rszName, BkEditorRmgPatchInfo *pInfo, bool *pbRefused )
{
	*pbRefused = false;
	pInfo->scripts.id_count = pInfo->scripts.area_count = 0;
	std::string szName = rszName;
	NStr::ToLower( szName );
	std::replace( szName.begin(), szName.end(), '/', '\\' );
	if ( szName.size() >= 192 || !NPlatform::Paths::IsRelativeDataName( szName ) )
	{
		pSession->szMessage = "\"" + rszName + "\" is not a name in the data";
		*pbRefused = true;
		return false;
	}
	CMapInfo mapInfo;
	// The .bzm or the .xml, whichever is newer (the MFC's own load of a patch).
	if ( !LoadTypedSuperLatestDataResource( szName, ".bzm", 1, mapInfo ) )
	{
		pSession->szMessage = "patch \"" + szName + "\" is not in the data or does not load as a map";
		*pbRefused = true;
		return false;
	}
	pInfo->size_x = mapInfo.terrain.patches.GetSizeX();
	pInfo->size_y = mapInfo.terrain.patches.GetSizeY();
	pInfo->season = mapInfo.nSeason;
	if ( !PutField( pInfo->season_folder, mapInfo.szSeasonFolder ) )
	{
		pSession->szMessage = "the season folder does not fit its field";
		return false;
	}
	CUsedScriptIDs ids;
	CUsedScriptAreas areas;
	mapInfo.GetUsedScriptIDs( &ids );
	mapInfo.GetUsedScriptAreas( &areas );
	bool bShort = false;
	if ( !FillScripts( &pInfo->scripts, ids, areas, &bShort ) )
	{
		pSession->szMessage = "a script area name does not fit its field";
		return false;
	}
	if ( bShort )
		pSession->szMessage = "the record's arrays are short: the counts are the totals";
	return !bShort;
}

bool ImportRmgPatch( SEditorSession *pSession, const std::string &rszSourcePath, bool bApply, std::string *pszName, bool *pbRefused )
{
	*pbRefused = true;
	pszName->clear();
	std::error_code error;
	const std::filesystem::path source( HostForm( rszSourcePath ) );
	std::string szExtension = source.extension().string();
	NStr::ToLower( szExtension );
	if ( rszSourcePath.empty() || !source.is_absolute() || ( szExtension != ".bzm" && szExtension != ".xml" ) || !std::filesystem::is_regular_file( source, error ) )
	{
		pSession->szMessage = "\"" + rszSourcePath + "\" is not the full path of an existing .bzm or .xml map file";
		return false;
	}
	std::string szStem = source.stem().string();
	NStr::ToLower( szStem );
	bool bPlainStem = !szStem.empty() && szStem.size() < 64 && szStem.find_first_of( "\\/:" ) == std::string::npos && szStem != "." && szStem != "..";
	for ( size_t i = 0; bPlainStem && i < szStem.size(); ++i )
		bPlainStem = ( unsigned char )szStem[i] >= 0x20 && strchr( "<>\"|*?", szStem[i] ) == 0;
	if ( !bPlainStem )
	{
		pSession->szMessage = "the map file's name is not a plain name";
		return false;
	}
	// A readable map: the same reader Open uses, so what a container later
	// loads through the storage is what was checked here.
	CMapInfo mapInfo;
	std::string szReadError;
	if ( !NMapFile::Read( EngineForm( source.string() ).c_str(), &mapInfo, &szReadError ) )
	{
		pSession->szMessage = "the file does not read as a map: " + szReadError;
		return false;
	}
	if ( mapInfo.nSeason < 0 || mapInfo.nSeason > 3 )
	{
		pSession->szMessage = NStr::Format( "the map's season %d is outside 0..3", mapInfo.nSeason );
		return false;
	}
	std::string szSeason;
	RMGGetSeasonNameString( mapInfo.nSeason, mapInfo.szSeasonFolder, &szSeason );
	NStr::ToLower( szSeason );
	const std::string szName = "scenarios\\patches\\" + szSeason + "\\" + szStem;
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 || pSession->pRmgStorage == 0 )
	{
		pSession->szMessage = "the user RMG root is not mounted";
		*pbRefused = false;
		return false;
	}
	*pszName = szName;
	*pbRefused = false;
	if ( !bApply )
		return true;
	// A shipped patch of that name stays: only the user's own layer is replaced.
	for ( int i = 0; i < 2; ++i )
	{
		const std::string szFile = szName + ( i == 0 ? ".bzm" : ".xml" );
		if ( !pSession->pRmgStorage->IsStreamExist( szFile.c_str() ) && pStorage->IsStreamExist( szFile.c_str() ) )
		{
			pSession->szMessage = "\"" + szName + "\" is shipped data and read-only: rename the map file first";
			*pbRefused = true;
			pszName->clear();
			return false;
		}
	}
	const std::string szDestHost = HostForm( pSession->szRmgMountedRoot + szName + szExtension );
	std::filesystem::create_directories( std::filesystem::path( szDestHost ).parent_path(), error );
	if ( error )
	{
		pSession->szMessage = "the patches folder under the user RMG root could not be created: " + error.message();
		pszName->clear();
		return false;
	}
	if ( !std::filesystem::equivalent( source, szDestHost, error ) )
	{
		error.clear();
		std::filesystem::copy_file( source, szDestHost, std::filesystem::copy_options::overwrite_existing, error );
		if ( error )
		{
			pSession->szMessage = "the map could not be copied in: " + error.message();
			pszName->clear();
			return false;
		}
	}
	BkEditorRmgPatchInfo probe;
	memset( &probe, 0, sizeof probe );
	bool bRefused = false;
	// The copy has to load through the storage as a patch: counts only.
	ReadRmgPatchInfo( pSession, szName, &probe, &bRefused );
	if ( bRefused )
	{
		std::filesystem::remove( szDestHost, error );
		pszName->clear();
		*pbRefused = true;
		return false;
	}
	return true;
}
