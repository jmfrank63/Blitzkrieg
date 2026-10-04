#include "project.h"

#include "future_blob.h"

namespace NResourceModel
{

bool Load( const std::string &szXml, Project &project, std::string &szError )
{
	project = Project();
	if ( !NResourceXml::Parse( szXml, project.document, szError ) )
		return false;

	// Hand the root element to the factory. For unknown tags (every tag during
	// the scaffold task) wrap the raw node as a FutureBlob so a Save path can
	// reproduce the bytes verbatim.
	auto &factory = CTreeItemFactory::Instance();
	// The factory is keyed by ETreeItemType; the loader does not know which
	// numeric key a root tag maps to yet (T02 will add a tag->type map). Until
	// then, every root is a FutureBlob.
	(void)factory;
	project.root = std::make_unique<FutureBlob>( project.document.root );
	return true;
}

std::string Save( const Project &project )
{
	// Scaffold writer: round-trip through the stored Document. Once T02 adds
	// typed root items, Save will instead rebuild Document.root from
	// project.root and fall back to the stored Node only for FutureBlob
	// children (so unknown subtrees still round-trip byte-identically).
	return NResourceXml::Serialise( project.document );
}

}
