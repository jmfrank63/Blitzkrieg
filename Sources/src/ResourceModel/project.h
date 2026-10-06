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

// What MFC's CParentFrame::SaveFrame writes beside the tree: <own_data>
// (SaveFrameOwnData: the export path and the frame's own data) and the stats
// block SaveRPGStats caches, both before History. MFC reads them unguarded
// when it opens a project, so a project without them crashes the shipped
// editor. Which block a kind caches is fixed by its frame: RPG for most, desc
// for objects, KeyData for particles, effect, VSODescription for roads and
// rivers; buildings, fences, sprites, tile sets and screens cache none.
// nullptr for a kind without one.
const char *CachedBlockName( const std::string &szExtension );

// Adds the project element's <own_data> as MFC writes it for the kind when it
// has none: a project that has one keeps it, because the export path and the
// frame's positions are the frame's own state and no tree holds them. The
// fields MFC leaves uninitialised in a new frame are written as zero. Does
// nothing for a kind that has no frame data (gui). Returns whether it added.
bool EnsureOwnData( NResourceXml::Node &projectElement, const std::string &szExtension );

// Puts block (an element named as CachedBlockName( ext )) into the project
// element: it replaces the one already there, or goes after <own_data> and
// before History as MFC writes it. Every save refreshes the block from the
// tree this way; the loaded text is not copied. The block takes MFC's form: floats
// with six significant digits (%lg), and an empty element as <a></a> where the
// block already in the project has a string at that path.
void PutCachedBlock( NResourceXml::Node &projectElement, NResourceXml::Node block );

// The element szName in an exported stats document, at its root or one level
// below, the form CTreeAccessor::Add( szName, ... ) writes. The deeper one
// wins when the root has the same name (the effect file's <effect>). False
// when the document has none.
bool FindStatsBlock( const NResourceXml::Node &statsRoot, const std::string &szName, NResourceXml::Node &block );

}
