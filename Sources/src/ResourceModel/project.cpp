#include "project.h"

#include "future_blob.h"
#include "items/stats_item.h"

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

}
