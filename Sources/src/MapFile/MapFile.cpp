// The game's reader (GameTT/iMissionInternal.cpp:1362-1392) and the MFC
// editor's writer (MapEditor/TemplateEditorFrame1.cpp:3238-3258), lifted into
// something with no UI and no renderer under it. Unlike the MFC editor
// (TemplateEditorFrame1.cpp:1644) this does not call RemoveNonExistingObjects:
// a map the editor could not fully understand still has to come back out
// unchanged.
#include "StdAfx.h"
#include "MapFile.h"
#include "../RandomMapGen/MapInfo_Types.h"

namespace NMapFile
{
// The two formats, once. An .xml map is a data tree with the map as its typed
// super; a .bzm is chunk 1 of a structure saver (iMissionInternal.cpp:1376-1388).
static void ReadStream( IDataStream *pStream, bool bXml, CMapInfo *pMap )
{
	if ( bXml )
	{
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		saver.AddTypedSuper( pMap );
	}
	else
	{
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSaver;
		saver.Add( 1, pMap );
	}
}

static bool HasExtension( const char *pszPath, const char *pszExtension )
{
	const size_t nPath = strlen( pszPath ), nExt = strlen( pszExtension );
	if ( nPath < nExt )
		return false;
	return NStr::CompareAsciiNoCase( pszPath + nPath - nExt, pszExtension ) == 0;
}

bool Read( const char *pszPath, CMapInfo *pMap, std::string *pError )
{
	if ( pszPath == 0 || pMap == 0 )
		return false;
	const bool bXml = HasExtension( pszPath, ".xml" );
	if ( !bXml && !HasExtension( pszPath, ".bzm" ) )
	{
		if ( pError ) *pError = std::string( pszPath ) + ": not a .bzm or .xml map";
		return false;
	}
	try
	{
		CPtr<IDataStream> pStream = OpenFileStream( pszPath, STREAM_ACCESS_READ );
		if ( pStream == 0 )
		{
			if ( pError ) *pError = std::string( pszPath ) + ": cannot open";
			return false;
		}
		ReadStream( pStream, bXml, pMap );
	}
	catch ( ... )
	{
		if ( pError ) *pError = std::string( pszPath ) + ": the stream threw while reading";
		return false;
	}
	if ( !pMap->IsValid() )
	{
		if ( pError ) *pError = std::string( pszPath ) + ": CMapInfo::IsValid() is false";
		return false;
	}
	return true;
}

// The game's rule, through the game's mechanism: ask the registered storage
// for both files' stats and take the newer (iMissionInternal.cpp:1370-1381).
// .xml wins only when it is strictly newer, which is what lets a freshly
// written .bzm take precedence over a stale .xml beside it. Zeroing the stats
// first matters - GetStreamStats leaves them untouched for a file that is not
// there, and an uninitialised mtime compares any way it likes.
bool ReadNewest( const char *pszBase, CMapInfo *pMap, std::string *pError )
{
	if ( pszBase == 0 || pMap == 0 )
		return false;
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 )
	{
		if ( pError ) *pError = "no storage is registered; call NDataOnly::Start first";
		return false;
	}
	const std::string szXml = std::string( pszBase ) + ".xml";
	const std::string szBzm = std::string( pszBase ) + ".bzm";
	SStorageElementStats statsXml, statsBzm;
	Zero( statsXml );
	Zero( statsBzm );
	pStorage->GetStreamStats( szXml.c_str(), &statsXml );
	pStorage->GetStreamStats( szBzm.c_str(), &statsBzm );
	if ( statsXml.mtime == 0 && statsBzm.mtime == 0 )
	{
		if ( pError ) *pError = std::string( pszBase ) + ": neither .xml nor .bzm is there";
		return false;
	}
	// The stream comes from the storage here, not from a path: a storage name
	// is not a filesystem path once a .pak is mounted over it.
	const std::string &szName = statsXml.mtime > statsBzm.mtime ? szXml : szBzm;
	const bool bXml = statsXml.mtime > statsBzm.mtime;
	try
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
		if ( pStream == 0 )
		{
			if ( pError ) *pError = szName + ": the storage has stats for it but will not open it";
			return false;
		}
		ReadStream( pStream, bXml, pMap );
	}
	catch ( ... )
	{
		if ( pError ) *pError = szName + ": the stream threw while reading";
		return false;
	}
	if ( !pMap->IsValid() )
	{
		if ( pError ) *pError = szName + ": CMapInfo::IsValid() is false";
		return false;
	}
	return true;
}

// The MFC editor's writer (TemplateEditorFrame1.cpp:3238-3258) without the
// PackFrameIndices call in front of it: what is in rMap is what goes to disk.
// SQuickLoadMapInfo::FillFromMapInfo (MapInfo_Methods.cpp:15-29) reads only
// fields of the map, so the quick chunk needs no object database either.
bool Write( const char *pszPath, const CMapInfo &rMap, std::string *pError )
{
	if ( pszPath == 0 )
		return false;
	const bool bXml = HasExtension( pszPath, ".xml" );
	if ( !bXml && !HasExtension( pszPath, ".bzm" ) )
	{
		if ( pError ) *pError = std::string( pszPath ) + ": not a .bzm or .xml map";
		return false;
	}
	try
	{
		SQuickLoadMapInfo quickLoadMapInfo;
		quickLoadMapInfo.FillFromMapInfo( rMap );
		CPtr<IDataStream> pStream = CreateFileStream( pszPath, STREAM_ACCESS_WRITE );
		if ( pStream == 0 )
		{
			if ( pError ) *pError = std::string( pszPath ) + ": cannot create";
			return false;
		}
		// The savers take a non-const reference; neither writes to the map.
		CMapInfo &rWritable = const_cast<CMapInfo&>( rMap );
		if ( bXml )
		{
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
			CTreeAccessor saver = pSaver;
			saver.AddTypedSuper( &rWritable );
			saver.Add( RMGC_QUICK_LOAD_MAP_INFO_NAME, &quickLoadMapInfo );
		}
		else
		{
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
			CSaverAccessor saver = pSaver;
			saver.Add( 1, &rWritable );
			saver.Add( RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER, &quickLoadMapInfo );
		}
	}
	catch ( ... )
	{
		if ( pError ) *pError = std::string( pszPath ) + ": the stream threw while writing";
		return false;
	}
	return true;
}
}
