#pragma once
// Project wraps the Document+root-tree pairing: a parsed NResourceXml::Document
// plus the Root CTreeItem the factory produced from it. Load reads an XML
// string into the pair, Save renders the pair back out. The guarantee the
// resource editor needs is byte-identity: Save( Load( x ) ) == x.
//
// Today the factory is empty, so every project loads into a single FutureBlob
// around the root element and the saved bytes come straight back out of the
// stored NResourceXml::Document. T02 populates the factory with the 21 root
// items; from that point Load walks the subtree and only wraps unknown nodes
// as blobs, but Save continues to prefer the stored Document bytes for any
// subtree the loader did not rebuild (so a round-trip stays byte-identical
// even for unknown children of known elements).

#include <memory>
#include <string>

#include "factory.h"
#include "root.h"
#include "xml.h"

namespace NResourceModel
{

struct Project
{
	// The parsed XML, kept verbatim. For the scaffold the Save path returns the
	// stored declaration + Serialise( root node ) rather than reconstructing
	// bytes from the Root tree - this is what gives Save( Load ) idempotency
	// before the typed item classes land.
	NResourceXml::Document document;
	// The CTreeItem the factory built for document.root. In the scaffold this
	// is always a FutureBlob wrapping document.root; T02 replaces that with the
	// known root item for recognised project extensions.
	std::unique_ptr<Root> root;
};

// Parse szXml into project. Returns false on malformed input and fills szError
// with the parser's offset-tagged message.
bool Load( const std::string &szXml, Project &project, std::string &szError );

// Serialise project back to the authoring form (CRLF, tabs). Guarantee:
// Save( Load( x ) ) == x for every file whose root tag the factory does not
// know - which is every file during the scaffold task.
std::string Save( const Project &project );

}
