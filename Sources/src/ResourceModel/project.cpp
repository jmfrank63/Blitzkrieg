#include "project.h"

#include "future_blob.h"
#include "items/stats_item.h"

namespace NResourceModel
{

namespace
{

// Build a FutureBlob child for every element under the typed root. Each blob
// stashes the raw NResourceXml::Node so Save() can emit the authored bytes
// verbatim - exactly what the "unknown-node preservation" guarantee requires.
void AdoptChildrenAsBlobs( CTreeItem &root, const NResourceXml::Node &parsed )
{
	for ( const auto &child : parsed.children )
	{
		// Preserve non-element nodes (comments, CDATA, PIs, significant text)
		// alongside elements so a round-trip keeps every byte. The FutureBlob
		// writer path emits a node of any kind.
		root.AddChild( std::make_unique<FutureBlob>( child ) );
	}
}

// Build a Document.root from the typed subtree. For T02 the typed items carry
// no SProp values yet; the authored data under them is still in FutureBlob
// children. The emitted node is: typed-root-tag + the attrs/text/children of
// the originally stored root (so attributes on the root tag survive) + the
// FutureBlob children in order.
NResourceXml::Node RebuildRootNode( const CTreeItem &root, const NResourceXml::Node &stored )
{
	NResourceXml::Node out;
	out.kind = NResourceXml::Node::Element;
	// Typed items carry an xmlTag; non-typed callers fall back to the stored
	// name so this helper is safe to call on a FutureBlob-rooted tree too.
	const auto *stats = dynamic_cast<const CStatsItem *>( &root );
	out.name = ( stats && !stats->GetXmlTag().empty() ) ? stats->GetXmlTag() : stored.name;
	out.attrs = stored.attrs;
	// First emit each typed child's serialised form (SProp walk - empty for T02).
	// Then re-emit every FutureBlob child verbatim. Both lists live in the same
	// treeItemList and preserve order of first append, which matches the parsed
	// child order so bytes line up.
	for ( const auto &childPtr : root.GetChildren() )
	{
		const CTreeItem *child = childPtr.get();
		if ( FutureBlob::IsFutureBlob( *child ) )
		{
			out.children.push_back( static_cast<const FutureBlob *>( child )->GetNode() );
		}
		else
		{
			NResourceXml::Node emitted;
			emitted.kind = NResourceXml::Node::Element;
			// Typed child without its own stashed node: serialise through the
			// prop walk. In T02 this still produces an empty element because
			// the SProp vectors are deferred to later tasks.
			child->serialise( emitted );
			out.children.push_back( std::move( emitted ) );
		}
	}
	return out;
}

}

bool Load( const std::string &szXml, Project &project, std::string &szError )
{
	project = Project();
	if ( !NResourceXml::Parse( szXml, project.document, szError ) )
		return false;

	// If the root tag matches a registered sub-editor, instantiate the typed
	// root and adopt every parsed child as a FutureBlob (which gives
	// unknown-node preservation for free: Save re-emits the raw Node bytes).
	// Unknown root tags still land in a plain FutureBlob so the scaffold
	// contract (every project round-trips) continues to hold.
	//
	// Touch the factory before LookupRootTag: the per-kind REGISTER_CLASS
	// calls that populate the tag map run from PopulateFactory which fires
	// on the factory's Meyers singleton init. If a caller hits LookupRootTag
	// before anything triggers that init, the tag map is empty and no root
	// looks typed.
	auto &factory = CTreeItemFactory::Instance();
	const int nType = LookupRootTag( project.document.root.name );
	if ( nType != 0 )
	{
		auto typed = factory.Create( nType );
		if ( typed )
		{
			AdoptChildrenAsBlobs( *typed, project.document.root );
			project.root = std::move( typed );
			return true;
		}
	}

	project.root = std::make_unique<FutureBlob>( project.document.root );
	return true;
}

std::string Save( const Project &project )
{
	// For an unknown-root project (FutureBlob at the root) re-emit the stored
	// Document directly - the scaffold contract. For a typed root, rebuild
	// Document.root from the typed subtree so authored edits reach the bytes;
	// FutureBlob children still produce byte-identical subtree bytes.
	if ( !project.root || FutureBlob::IsFutureBlob( *project.root ) )
		return NResourceXml::Serialise( project.document );

	NResourceXml::Document rebuilt = project.document;
	rebuilt.root = RebuildRootNode( *project.root, project.document.root );
	return NResourceXml::Serialise( rebuilt );
}

}
