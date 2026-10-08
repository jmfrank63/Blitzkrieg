#include "project.h"

#include "future_blob.h"
#include "items/stats_item.h"

#include <cstdio>
#include <cstdlib>
#include <map>

// MFC's CParentFrame::LoadComposerFile opens the project with the
// *_Composer_Project element as the data tree's base node and runs the root
// item's operator& on it (CETreeCtrl::LoadTree); the frame's own data
// (own_data, RPG) sits beside the item's fields in the same element. The port
// does the same: the root item reads and writes its fields, and the frame's
// elements, which no item owns, are kept in the root's layout and written
// back unchanged until the frames are ported.

namespace NResourceModel
{

bool Load( const std::string &szXml, Project &project, std::string &szError )
{
	project = Project();
	if ( !NResourceXml::Parse( szXml, project.document, szError ) )
		return false;

	// Touch the factory before LookupRootTag: the per-kind RegisterRootTag
	// calls run from PopulateFactory on the factory's first use.
	auto &factory = CTreeItemFactory::Instance();
	const int nType = LookupRootTag( project.document.root.name );
	if ( nType != 0 )
	{
		if ( auto typed = factory.Create( nType ) )
		{
			typed->parse( project.document.root );
			project.root = std::move( typed );
			return true;
		}
	}

	project.root = std::make_unique<FutureBlob>( project.document.root );
	return true;
}

std::string Save( const Project &project )
{
	// An unknown root (a FutureBlob) is written back as it was read.
	if ( !project.root || FutureBlob::IsFutureBlob( *project.root ) )
		return NResourceXml::Serialise( project.document );

	NResourceXml::Document rebuilt;
	rebuilt.declaration = project.document.declaration;
	rebuilt.hasDeclaration = project.document.hasDeclaration;
	rebuilt.layout = project.document.layout;
	rebuilt.root.kind = NResourceXml::Node::Element;
	rebuilt.root.name = project.document.root.name;
	project.root->SerialiseRoot( rebuilt.root );
	return NResourceXml::Serialise( rebuilt );
}

namespace
{

// The engine's stats writer prints a float widened to a double with every digit;
// MFC's CDataTreeXML prints six significant digits (%lg, DataTreeXML.cpp:326).
void FormatFloats( NResourceXml::Node &node )
{
	for ( auto &attr : node.attrs )
	{
		const std::string &szValue = attr.second;
		if ( szValue.find_first_of( ".eE" ) == std::string::npos )
			continue;
		char *pszEnd = nullptr;
		const double fValue = std::strtod( szValue.c_str(), &pszEnd );
		if ( pszEnd == szValue.c_str() || *pszEnd != '\0' )
			continue;
		char buf[64];
		std::snprintf( buf, sizeof( buf ), "%g", fValue );
		attr.second = buf;
	}
	for ( auto &child : node.children )
		if ( child.kind == NResourceXml::Node::Element )
			FormatFloats( child );
}

// MSXML writes a string value as <a></a> (an element holding an empty text node)
// and a container with no items as <a/>; the engine's writer prints both as <a/>.
// The block the project already holds says which form a path has; a path it does
// not hold keeps the engine's form.
void TakeEmptyForm( NResourceXml::Node &node, const NResourceXml::Node &old )
{
	std::map<std::string, size_t> seen;
	for ( auto &child : node.children )
	{
		if ( child.kind != NResourceXml::Node::Element )
			continue;
		const size_t nWanted = seen[child.name]++;
		size_t nSame = 0;
		const NResourceXml::Node *pOld = nullptr;
		for ( const auto &candidate : old.children )
			if ( candidate.kind == NResourceXml::Node::Element && candidate.name == child.name && nSame++ == nWanted )
			{
				pOld = &candidate;
				break;
			}
		if ( pOld == nullptr )
			continue;
		const bool bOldString = pOld->children.size() == 1 && pOld->children[0].kind == NResourceXml::Node::Text && pOld->children[0].text.empty();
		if ( child.children.empty() && child.attrs.empty() && bOldString )
			child.children.push_back( pOld->children[0] );
		else
			TakeEmptyForm( child, *pOld );
	}
}

}

const char *CachedBlockName( const std::string &szExtension )
{
	static const struct { const char *pszExtension; const char *pszBlock; } kBlocks[] =
	{
		{ "wpn", "RPG" }, { "mcp", "RPG" }, { "trc", "RPG" }, { "scp", "RPG" }, { "unt", "RPG" }, { "msh", "RPG" },
		{ "obt", "desc" }, { "bdg", "RPG" }, { "pcp", "KeyData" }, { "eff", "effect" }, { "3rd", "VSODescription" },
		{ "3rv", "VSODescription" }, { "mip", "RPG" }, { "chc", "RPG" }, { "cgc", "RPG" }, { "mdc", "RPG" },
	};
	for ( const auto &entry : kBlocks )
		if ( szExtension == entry.pszExtension )
			return entry.pszBlock;
	return nullptr;
}

bool EnsureOwnData( NResourceXml::Node &projectElement, const std::string &szExtension )
{
	if ( szExtension == "gui" || NResourceXml::FindChild( projectElement, "own_data" ) != nullptr )
		return false;
	// The shapes the shipped editor writes for a new project (the mfc-new fixtures), with the
	// values an absent element has always read as: a bridge's Begin and End are the frame's
	// constructor value (BridgeFrm.cpp:91), a building's and an object's positions are zero
	// as the exporters read them (MFC's 724.077 is a new frame's spot on its scene).
	const char *pszOwnData = "<own_data><export_file_name></export_file_name></own_data>";
	if ( szExtension == "bdg" )
		pszOwnData = "<own_data Front=\"0\" Back=\"0\"><export_dir></export_dir><export_file_name></export_file_name>"
		             "<Begin x=\"424.077\" y=\"724.077\" z=\"0\"/><End x=\"424.077\" y=\"724.077\" z=\"0\"/></own_data>";
	else if ( szExtension == "bld" )
		pszOwnData = "<own_data><sprite_pos x=\"0\" y=\"0\" z=\"0\"/><krest_pos x=\"0\" y=\"0\" z=\"0\"/>"
		             "<export_dir></export_dir><export_file_name></export_file_name></own_data>";
	else if ( szExtension == "obt" )
		pszOwnData = "<own_data><sprite_pos x=\"0\" y=\"0\" z=\"0\"/><krest_pos x=\"0\" y=\"0\" z=\"0\"/>"
		             "<export_file_name></export_file_name><TransLines/></own_data>";
	NResourceXml::Document doc;
	std::string szError;
	if ( !NResourceXml::Parse( std::string( "<r>" ) + pszOwnData + "</r>", doc, szError ) || doc.root.children.empty() )
		return false;
	projectElement.children.insert( projectElement.children.begin(), std::move( doc.root.children.front() ) );
	return true;
}

void PutCachedBlock( NResourceXml::Node &projectElement, NResourceXml::Node block )
{
	auto &children = projectElement.children;
	FormatFloats( block );
	for ( auto &child : children )
		if ( child.kind == NResourceXml::Node::Element && child.name == block.name )
		{
			TakeEmptyForm( block, child );
			child = std::move( block );
			return;
		}
	// After own_data when there is one, else before the first of the tree's own
	// elements, which MFC writes after the frame data.
	const size_t kNone = size_t( -1 );
	size_t nAt = kNone;
	for ( size_t i = 0; i < children.size(); ++i )
		if ( children[i].kind == NResourceXml::Node::Element && children[i].name == "own_data" )
			nAt = i + 1;
	if ( nAt == kNone )
		for ( size_t i = 0; i < children.size(); ++i )
			if ( children[i].kind == NResourceXml::Node::Element &&
			     ( children[i].name == "History" || children[i].name == "default_name" || children[i].name == "display_name" ||
			       children[i].name == "values" || children[i].name == "childs" ) )
			{
				nAt = i;
				break;
			}
	if ( nAt == kNone )
		nAt = children.size();
	children.insert( children.begin() + nAt, std::move( block ) );
}

bool FindStatsBlock( const NResourceXml::Node &statsRoot, const std::string &szName, NResourceXml::Node &block )
{
	for ( const auto &child : statsRoot.children )
		if ( child.kind == NResourceXml::Node::Element && child.name == szName )
		{
			block = child;
			return true;
		}
	if ( statsRoot.name == szName )
	{
		block = statsRoot;
		return true;
	}
	return false;
}

}
