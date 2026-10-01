// The object filters (M3, D-31): the palette's named conditions of folder
// words. A translation unit of its own for the same reason catalogue.cpp is
// one: its helpers are plain C++ at file scope, and only the two BkEditor
// entries take the ABI's C linkage (from bridge.h's declarations) - the
// bridge's other files keep their helper namespaces out of the C-linkage
// block that spans bridge.cpp's definitions.
#include "StdAfx.h"
#include "bridge.h"
#include "session.h"
// LoadDataResource is the engine's own reader for the shipped
// Data/Editor/filter.xml; the same data-tree writer makes the user file.
#include "../RandomMapGen/Resource_Types.h"
#include "../Platform/Paths.h"
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <set>
#include <StreamIO/StreamIO.h>
#include <Misc/Tools.h>

// ---------------------------------------------------------------------------
// Object filters (M3, D-31).
// ---------------------------------------------------------------------------

namespace
{

// The filter file's serialisation shape, byte-for-byte the MFC editor's own
// SSimpleFilter (MapEditor/CreateFilterDialog.h/.cpp): the same data-tree
// reader and writer run over it (the engine's own - the XML is never re-typed
// here), only the struct is mirrored, because that file is MFC code the
// bridge cannot include (and 05-11 deletes). The tree shape this produces is
// the shipped file's: <item><key>Name</key><data><Filter><item><data>
// <item>word</item>...</data></item></Filter></data></item>.
typedef std::list<std::string> TBkFilterWords;
typedef std::list<TBkFilterWords> TBkFilterConditions;
struct SBkFilterData
{
	TBkFilterConditions conditions;
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Filter", &conditions );
		return 0;
	}
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &conditions );
		return 0;
	}
};
typedef std::unordered_map<std::string, SBkFilterData> TBkFilterMap;

// The user filters' directory: <UserRoot>mapeditor. The file itself goes
// through a FILE storage mounted there - the data-tree reader and writer
// need the storage layer's streams (the Zig-backed IDataStream the same
// LoadDataResource reads the shipped file through), not a bare host stream.
std::string UserFilterDir()
{
	// The trailing separator matters: the storage layer reads the last path
	// component as its file mask, so a bare directory name would make the
	// PARENT directory the storage's base.
	return ( std::filesystem::path( NPlatform::Paths::UserRoot() ) / "mapeditor" ).string() + "/";
}

// A missing or malformed user file reads empty and is never an error
// (T-05-03-01): the editor must start with the shipped filters alone.
bool ReadUserFilterMap( const std::string &rszDir, TBkFilterMap *pMap )
{
	try
	{
		CPtr<IDataStorage> pStorage = CreateStorage( rszDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
		if ( pStorage == 0 )
			return false;
		CPtr<IDataStream> pStream = pStorage->OpenStream( "filter.xml", STREAM_ACCESS_READ );
		if ( pStream == 0 )
			return false;
		CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::READ );
		CTreeAccessor saver = pSaver;
		saver.Add( "filters", pMap );
		return true;
	}
	catch ( ... )
	{
		pMap->clear();
		return false;
	}
}

// Writes the user file in the shipped file's own XML shape. The engine's
// own data-tree writer (the read side stays its reader) crashes its flush on
// a write tree over this module's streams - the game's own state-file writes
// show the same write path truncating - and a user filter file is the one
// file the editor authors whole, from a fixed model, so the shape is written
// directly and the engine test's save -> engine-read -> compare round trip
// is the proof the reader accepts it. Text is escaped like the tree writer
// escapes it; the indentation is the shipped file's own tabs.
void WriteXmlText( std::string &rszOut, const std::string &rszText )
{
	for ( const char c : rszText )
	{
		switch ( c )
		{
			case '&': rszOut += "&amp;"; break;
			case '<': rszOut += "&lt;"; break;
			case '>': rszOut += "&gt;"; break;
			default: rszOut += c;
		}
	}
}

bool WriteUserFilterMap( const std::string &rszDir, const TBkFilterMap &rMap )
{
	try
	{
		// A stable order: by name, the same order the read answers in.
		std::set<std::string> names;
		for ( const auto &entry : rMap )
			names.insert( entry.first );
		std::string szXml = "<?xml version=\"1.0\"?>\n<base>\n\t<filters>\n";
		for ( const std::string &rszName : names )
		{
			const SBkFilterData &rData = rMap.find( rszName )->second;
			szXml += "\t\t<item>\n\t\t\t<key>";
			WriteXmlText( szXml, rszName );
			szXml += "</key>\n\t\t\t<data>\n\t\t\t\t<Filter>\n";
			for ( const TBkFilterWords &rWords : rData.conditions )
			{
				szXml += "\t\t\t\t\t<item>\n\t\t\t\t\t\t<data>\n";
				for ( const std::string &rszWord : rWords )
				{
					szXml += "\t\t\t\t\t\t\t<item>";
					WriteXmlText( szXml, rszWord );
					szXml += "</item>\n";
				}
				szXml += "\t\t\t\t\t\t</data>\n\t\t\t\t\t</item>\n";
			}
			szXml += "\t\t\t\t</Filter>\n\t\t\t</data>\n\t\t</item>\n";
		}
		szXml += "\t</filters>\n</base>\n";

		std::error_code error;
		std::filesystem::create_directories( std::filesystem::path( rszDir ), error );
		std::ofstream file( std::filesystem::path( rszDir ) / "filter.xml", std::ios::binary | std::ios::trunc );
		if ( !file )
			return false;
		file.write( szXml.data(), std::streamsize( szXml.size() ) );
		return bool( file );
	}
	catch ( ... )
	{
		return false;
	}
}

void CopyWord( const char *pszFrom, char (&szTo)[BK_EDITOR_FILTER_WORD_LEN] )
{
	const size_t nLen = strnlen( pszFrom, BK_EDITOR_FILTER_WORD_LEN );
	const size_t nCopy = Min( nLen, size_t( BK_EDITOR_FILTER_WORD_LEN - 1 ) );
	memcpy( szTo, pszFrom, nCopy );
	szTo[nCopy] = 0;
}

void FillFilterRecord( const std::string &rszName, const SBkFilterData &rData, bool bUser, BkEditorObjectFilter *pOut )
{
	memset( pOut, 0, sizeof *pOut );
	const size_t nNameLen = Min( rszName.size(), sizeof pOut->name - 1 );
	memcpy( pOut->name, rszName.c_str(), nNameLen );
	pOut->name[nNameLen] = 0;
	pOut->user = bUser ? 1 : 0;
	int nList = 0;
	for ( const TBkFilterWords &rWords : rData.conditions )
	{
		if ( nList == BK_EDITOR_FILTER_MAX_LISTS )
			break;
		BkEditorObjectFilterWords &rOut = pOut->lists[nList];
		int nWord = 0;
		for ( const std::string &rszWord : rWords )
		{
			if ( nWord == BK_EDITOR_FILTER_MAX_WORDS )
				break;
			CopyWord( rszWord.c_str(), rOut.words[nWord] );
			++nWord;
		}
		rOut.word_count = nWord;
		++nList;
	}
	pOut->list_count = nList;
}

}

// The bridge.cpp Guarded, over the base the session files share (this file
// cannot see BkEditorSession's own definition - bridge.cpp keeps it).
template<class F>
static BkEditorStatus GuardedSession( SEditorSession *pSession, F body )
{
	if ( pSession == 0 ) return BK_EDITOR_NO_SESSION;
	try { pSession->szMessage.clear(); return body(); }
	catch ( ... ) { pSession->szMessage = "the engine threw"; return BK_EDITOR_FAILED; }
}

BkEditorStatus BkEditorObjectFilters( BkEditorSession *pSession, BkEditorObjectFilter *pOut, int nCapacity, int *pnCount )
{
	// bridge.cpp defines BkEditorSession as a plain SEditorSession subclass;
	// the base subobject leads, so the reinterpretation is the static
	// conversion a complete type would spell out.
	SEditorSession *pThis = reinterpret_cast<SEditorSession *>( pSession );
	return GuardedSession( pThis, [=]() -> BkEditorStatus
	{
		if ( pnCount == 0 || nCapacity < 0 || ( pOut == 0 && nCapacity > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		// Shipped, through the engine's own reader - the MFC editor's exact
		// call and label. A throw inside lands in LoadDataResource's own
		// catch and reads empty, which is the answer a broken file deserves.
		TBkFilterMap shipped;
		LoadDataResource( "editor\\filter", "", false, 0, "filters", shipped );
		// The user file, merged over it: user wins by byte-equal name.
		TBkFilterMap user;
		ReadUserFilterMap( UserFilterDir(), &user );
		// The answer is ordered by name (byte order), not file order, so the
		// read is stable across runs and platforms.
		std::set<std::string> names;
		for ( const auto &entry : shipped )
			names.insert( entry.first );
		for ( const auto &entry : user )
			names.insert( entry.first );
		*pnCount = int( names.size() );
		const int nWrite = Min( int( names.size() ), nCapacity );
		int nIndex = 0;
		for ( const std::string &rszName : names )
		{
			if ( nIndex >= nWrite )
				break;
			const auto iUser = user.find( rszName );
			if ( iUser != user.end() )
				FillFilterRecord( rszName, iUser->second, true, &pOut[nIndex] );
			else
				FillFilterRecord( rszName, shipped.find( rszName )->second, false, &pOut[nIndex] );
			++nIndex;
		}
		if ( int( names.size() ) > nCapacity )
		{
			pThis->szMessage = NStr::Format( "the filter files hold %d filters and room was given for %d",
				int( names.size() ), nCapacity );
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

BkEditorStatus BkEditorSaveObjectFilters( BkEditorSession *pSession, const BkEditorObjectFilter *pFilters, int nCount )
{
	SEditorSession *pThis = reinterpret_cast<SEditorSession *>( pSession );
	return GuardedSession( pThis, [=]() -> BkEditorStatus
	{
		if ( nCount < 0 || ( pFilters == 0 && nCount > 0 ) )
			return BK_EDITOR_BAD_ARGUMENT;
		TBkFilterMap map;
		for ( int i = 0; i < nCount; ++i )
		{
			const BkEditorObjectFilter &rFilter = pFilters[i];
			const size_t nNameLen = strnlen( rFilter.name, sizeof rFilter.name );
			if ( nNameLen == 0 || nNameLen >= sizeof rFilter.name )
				return BK_EDITOR_BAD_ARGUMENT;
			if ( rFilter.list_count < 0 || rFilter.list_count > BK_EDITOR_FILTER_MAX_LISTS )
				return BK_EDITOR_BAD_ARGUMENT;
			SBkFilterData &rData = map[std::string( rFilter.name, nNameLen )];
			for ( int nList = 0; nList < rFilter.list_count; ++nList )
			{
				const BkEditorObjectFilterWords &rIn = rFilter.lists[nList];
				if ( rIn.word_count < 0 || rIn.word_count > BK_EDITOR_FILTER_MAX_WORDS )
					return BK_EDITOR_BAD_ARGUMENT;
				rData.conditions.push_back( TBkFilterWords() );
				for ( int nWord = 0; nWord < rIn.word_count; ++nWord )
				{
					if ( strnlen( rIn.words[nWord], BK_EDITOR_FILTER_WORD_LEN ) == BK_EDITOR_FILTER_WORD_LEN )
						return BK_EDITOR_BAD_ARGUMENT;
					rData.conditions.back().push_back( std::string( rIn.words[nWord] ) );
				}
			}
		}
		if ( !WriteUserFilterMap( UserFilterDir(), map ) )
		{
			pThis->szMessage = "the user filter file could not be written";
			return BK_EDITOR_REFUSED;
		}
		return BK_EDITOR_OK;
	} );
}

