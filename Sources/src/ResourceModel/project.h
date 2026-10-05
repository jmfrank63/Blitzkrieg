#pragma once
// Project wraps the Document+root-tree pairing: a parsed NResourceXml::Document
// plus the root CTreeItem the factory made for its *_Composer_Project element.
// Load runs the root item's operator& over that element (MFC's
// CETreeCtrl::LoadTree), which builds the typed tree from the childs lists;
// Save writes it back the same way. An item keeps the layout it was read
// with (tree_item.h), so Save( Load( x ) ) has the content of x, and elements
// no item owns (the frame's own_data and RPG, unknown entries) survive. A
// root tag the factory does not know loads as one FutureBlob and is written
// back verbatim.

#include <memory>
#include <string>

#include "factory.h"
#include "root.h"
#include "xml.h"

namespace NResourceModel
{

struct Project
{
	// The parsed XML as read. Save takes the declaration from it, and the
	// whole document when the root is not typed.
	NResourceXml::Document document;
	// The root item the factory built for document.root, or a FutureBlob
	// wrapping it when the root tag is not a registered sub-editor.
	std::unique_ptr<Root> root;
};

// Parse szXml into project. Returns false on malformed input and fills szError
// with the parser's offset-tagged message.
bool Load( const std::string &szXml, Project &project, std::string &szError );

// Serialise project back to the authoring form (CRLF, tabs). An unedited
// project saves with the content it was loaded with; a root tag the factory
// does not know is written back byte for byte.
std::string Save( const Project &project );

}
