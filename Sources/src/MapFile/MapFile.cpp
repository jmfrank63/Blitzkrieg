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
}
