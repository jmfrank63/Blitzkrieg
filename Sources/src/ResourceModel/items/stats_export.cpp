#include "StdAfx.h"

#include "stats_export.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <filesystem>

#include "../mfc_value.h"
#include "../xml.h"

namespace NResourceModel
{

namespace NStatsExport
{

std::unique_ptr<Project> PreparedCopy( const Project &project, int nRootType, const char *pszKind, SExportOutcome &outcome )
{
	auto pCopy = std::make_unique<Project>();
	std::string szError;
	if ( !Load( Save( project ), *pCopy, szError ) )
	{
		outcome.szError = std::string( "the " ) + pszKind + " project cannot be re-read for export: " + szError;
		return nullptr;
	}
	if ( !pCopy->root || pCopy->root->GetItemType() != nRootType )
	{
		outcome.szError = std::string( "the project is not a " ) + pszKind + " project: its root is <" + project.document.root.name + ">";
		return nullptr;
	}
	pCopy->root->CreateDefaultChilds();
	return pCopy;
}

static const char kImageRect[] = "image_rect";
static const char *const kRectAttrs[4] = { "x1", "y1", "x2", "y2" };

void KeepImageRect( NResourceXml::Node &root, const float ( &rect )[4] )
{
	NResourceXml::Node element;
	element.kind = NResourceXml::Node::Element;
	element.name = kImageRect;
	for ( int i = 0; i < 4; ++i )
		SetAttr( element, kRectAttrs[i], MfcFloat( rect[i] ) );
	for ( NResourceXml::Node &child : root.children )
		if ( child.kind == NResourceXml::Node::Element && child.name == kImageRect )
		{
			child = std::move( element );
			return;
		}
	root.children.push_back( std::move( element ) );
}

bool KeptImageRect( const NResourceXml::Node &root, float ( &rect )[4] )
{
	const NResourceXml::Node *pElement = NResourceXml::FindChild( root, kImageRect );
	if ( pElement == nullptr )
		return false;
	for ( int i = 0; i < 4; ++i )
	{
		const std::string *pValue = FindAttr( *pElement, kRectAttrs[i] );
		if ( pValue == nullptr )
			return false;
		rect[i] = float( std::strtod( pValue->c_str(), nullptr ) );
	}
	return true;
}

const CTreeItem *ChildItem( const CTreeItem &item, int nType, int nIndex )
{
	for ( const auto &pChild : item.GetChildren() )
		if ( pChild && pChild->GetItemType() == nType && nIndex-- == 0 )
			return pChild.get();
	return nullptr;
}

const CTreeItem *RequireChild( const CTreeItem &item, int nType, int nIndex, const char *pszWhat, SExportOutcome &outcome )
{
	const CTreeItem *pChild = ChildItem( item, nType, nIndex );
	if ( pChild == nullptr && outcome.szError.empty() )
		outcome.szError = std::string( "the project has no \"" ) + pszWhat + "\" item under \"" + item.GetDisplayName() + "\"";
	return pChild;
}

namespace
{

const CVariant *ValueAt( const CTreeItem &item, int nIndex )
{
	const CPropVector &values = item.GetValues();
	return nIndex >= 0 && nIndex < int( values.size() ) ? &values[nIndex].value : nullptr;
}

}

int ValueInt( const CTreeItem &item, int nIndex )
{
	const CVariant *pValue = ValueAt( item, nIndex );
	if ( pValue == nullptr )
		return 0;
	switch ( pValue->GetKind() )
	{
		case CVariant::VK_INT:   return pValue->AsInt();
		case CVariant::VK_FLOAT: return int( pValue->AsFloat() );
		case CVariant::VK_BOOL:  return pValue->AsBool() ? 1 : 0;
		case CVariant::VK_STR:   return std::atoi( pValue->AsStr().c_str() );
		case CVariant::VK_REF:   return std::atoi( pValue->AsRef().value.c_str() );
		case CVariant::VK_COMBO: return pValue->AsCombo().index;
		case CVariant::VK_COLOR: return int( pValue->AsColor() );
		case CVariant::VK_INT64: return int( pValue->AsInt64() );
		default:                 return 0;
	}
}

float ValueFloat( const CTreeItem &item, int nIndex )
{
	const CVariant *pValue = ValueAt( item, nIndex );
	if ( pValue == nullptr )
		return 0;
	switch ( pValue->GetKind() )
	{
		case CVariant::VK_FLOAT: return pValue->AsFloat();
		case CVariant::VK_STR:   return float( std::atof( pValue->AsStr().c_str() ) );
		case CVariant::VK_REF:   return float( std::atof( pValue->AsRef().value.c_str() ) );
		// MFC's OptimizeFloat leaves a VT_INT64's float slot as it was: zero.
		case CVariant::VK_INT64: return 0;
		default:                 return float( ValueInt( item, nIndex ) );
	}
}

bool ValueBool( const CTreeItem &item, int nIndex )
{
	// CVariant::operator bool is OptimizeInt, then m_intVal != 0.
	return ValueInt( item, nIndex ) != 0;
}

std::string ValueStr( const CTreeItem &item, int nIndex )
{
	const CVariant *pValue = ValueAt( item, nIndex );
	if ( pValue == nullptr )
		return std::string();
	char buf[64];
	switch ( pValue->GetKind() )
	{
		case CVariant::VK_STR: return pValue->AsStr();
		case CVariant::VK_REF: return pValue->AsRef().value;
		case CVariant::VK_FLOAT:
			std::snprintf( buf, sizeof( buf ), "%g", pValue->AsFloat() );
			return buf;
		case CVariant::VK_INT:
		case CVariant::VK_BOOL:
		case CVariant::VK_COMBO:
			std::snprintf( buf, sizeof( buf ), "%i", ValueInt( item, nIndex ) );
			return buf;
		default:
			return std::string();
	}
}

namespace
{

std::string Lower( std::string s )
{
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return char( std::tolower( c ) ); } );
	return s;
}

}

std::string ToSlashes( std::string s )
{
	std::replace( s.begin(), s.end(), '\\', '/' );
	return s;
}

std::filesystem::path FoldedChild( const std::filesystem::path &dir, const std::string &szName )
{
	std::error_code ec;
	std::filesystem::path plain = dir / szName;
	if ( std::filesystem::exists( plain, ec ) )
		return plain;
	const std::string szWanted = Lower( szName );
	for ( std::filesystem::directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
		if ( Lower( it->path().filename().string() ) == szWanted )
			return it->path();
	return plain;
}

bool IsRelatedPath( const std::string &szPath )
{
	return szPath.empty() || ( szPath[0] != '\\' && szPath[0] != '/' && szPath.find( ':' ) == std::string::npos );
}

// A name without a backslash is appended to the folder as is, otherwise the
// folder loses its last component, one more per "..\", and the rest is joined
// with a backslash.
std::string MakeFullPath( const std::string &szFullDirName, const std::string &szRelName )
{
	if ( szRelName.empty() )
		return szFullDirName;
	if ( szRelName.find( '\\' ) == std::string::npos )
		return szFullDirName + szRelName;
	std::string szResult = szFullDirName.substr( 0, szFullDirName.rfind( '\\' ) );
	std::string::size_type nRest = 0, nFound;
	while ( ( nFound = szRelName.find( "..\\", nRest ) ) != std::string::npos )
	{
		const std::string::size_type nPos = szResult.rfind( '\\' );
		if ( nPos == std::string::npos )
			return szFullDirName + szRelName;
		szResult = szResult.substr( 0, nPos );
		nRest = nFound + 3;
	}
	return szResult + '\\' + szRelName.substr( nRest );
}

std::filesystem::path InvalidPicture( const SExportContext &context )
{
	if ( context.szDataRoot.empty() )
		return std::filesystem::path();
	std::error_code ec;
	const std::filesystem::path editorDir = FoldedChild( std::filesystem::path( context.szDataRoot ), "editor" );
	const std::filesystem::path picture = FoldedChild( editorDir, "invalid.tga" );
	return std::filesystem::is_regular_file( picture, ec ) ? picture : std::filesystem::path();
}

std::filesystem::file_time_type ChangeTime( const std::filesystem::path &file )
{
	std::error_code ec;
	const std::filesystem::file_time_type time = std::filesystem::last_write_time( file, ec );
	return ec ? (std::filesystem::file_time_type::min)() : time;
}

std::filesystem::path FoldedFile( const std::string &szPath )
{
	const std::filesystem::path whole( ToSlashes( szPath ) );
	std::filesystem::path result = whole.root_path();
	for ( const std::filesystem::path &part : whole.relative_path() )
		result = result.empty() ? part : FoldedChild( result, part.string() );
	return result;
}

namespace
{

std::string Backslashes( std::string s )
{
	for ( char &c : s )
		if ( c == '/' )
			c = '\\';
	return s;
}

// MFC's IsRelatedPath: neither a drive nor a root.
bool IsRelative( const std::string &szPath )
{
	return !szPath.empty() && szPath[0] != '\\' && szPath[0] != '/' && szPath.find( ':' ) == std::string::npos;
}

std::string OwnDataText( const Project &project, const char *pszName )
{
	const NResourceXml::Node *pOwnData = NResourceXml::FindChild( project.document.root, "own_data" );
	const NResourceXml::Node *pField = pOwnData != nullptr ? NResourceXml::FindChild( *pOwnData, pszName ) : nullptr;
	return pField != nullptr ? ElementText( *pField ) : std::string();
}

}

std::string StatsFileName( const Project &project, const SExportContext &context, const std::string &szAddDir, bool bFileNamedAfterFolder )
{
	// CParentFrame::LoadFrameOwnData: export_dir + "1.xml" from older
	// projects, otherwise export_file_name.
	std::string szStored = OwnDataText( project, "export_dir" );
	if ( !szStored.empty() )
		szStored += "1.xml";
	else
		szStored = OwnDataText( project, "export_file_name" );
	szStored = Backslashes( szStored );
	if ( IsRelative( szStored ) )
		return szAddDir + szStored;

	const std::string szFolder = std::filesystem::path( context.szProjectPath ).parent_path().filename().string();
	if ( szFolder.empty() )
		return szAddDir + "1.xml";
	if ( bFileNamedAfterFolder )
		return szAddDir + szFolder + ".xml";
	return szAddDir + szFolder + "\\1.xml";
}

std::string DirectoryOf( const std::string &szName )
{
	const std::string::size_type nCut = szName.find_last_of( "\\/" );
	return nCut == std::string::npos ? std::string() : szName.substr( 0, nCut + 1 );
}

std::string ProjectDirectory( const SExportContext &context )
{
	const std::string::size_type nCut = context.szProjectPath.find_last_of( "\\/" );
	return nCut == std::string::npos ? std::string( "./" ) : context.szProjectPath.substr( 0, nCut + 1 );
}

bool WriteStats( const SExportContext &context, const std::string &szName, const std::function<void( IDataTree * )> &write, SExportOutcome &outcome, const char *pszRootName )
{
	std::string szRelative = szName;
	for ( char &c : szRelative )
		if ( c == '\\' )
			c = '/';
	const std::filesystem::path file = std::filesystem::path( context.szStagingRoot ) / szRelative;
	std::error_code ec;
	std::filesystem::create_directories( file.parent_path(), ec );
	if ( ec )
	{
		outcome.szError = "cannot create the folder " + file.parent_path().string() + ": " + ec.message();
		return false;
	}
	std::string szDir = file.parent_path().string();
	if ( szDir.empty() || szDir.back() != '/' )
		szDir += '/';
	{
		CPtr<IDataStorage> pStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
		CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->CreateStream( file.filename().string().c_str(), STREAM_ACCESS_WRITE ) : 0;
		if ( pStream == 0 )
		{
			outcome.szError = "Error: can not create stream: " + file.string();
			return false;
		}
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE, pszRootName );
		if ( pDT == 0 )
		{
			outcome.szError = "the engine has no tree saver for " + file.string();
			return false;
		}
		write( pDT );
	}
	if ( !std::filesystem::is_regular_file( file, ec ) )
	{
		outcome.szError = file.string() + " was not written";
		return false;
	}
	++outcome.nWritten;
	return true;
}

}

}
