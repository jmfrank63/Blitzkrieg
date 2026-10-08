#pragma once
// The portable project-XML tree the MFC editors read and write through
// CDataTreeXML (MSXML). It models only what a round trip needs: elements,
// attributes, text, comments, CDATA and processing instructions, so an unknown
// node survives untouched. No MFC, no Windows API.

#include <string>
#include <utility>
#include <vector>

namespace NResourceXml
{

struct Node
{
	enum Kind { Element, Text, Comment, CData, Pi };
	Kind kind = Element;
	std::string name;	// element name or PI target
	std::string text;	// decoded text, comment body, CDATA body or PI data
	std::vector<std::pair<std::string, std::string>> attrs;
	std::vector<Node> children;
};

struct Document
{
	// Mfc is what CDataTreeXML's MSXML save writes: the declaration, a line
	// break, then the whole tree on one line with no layout whitespace. Indented
	// is the tab-indented layout of the port-authored fixtures, kept so that
	// re-saving one of them leaves it byte-identical and its diffs readable.
	enum Layout { Mfc, Indented };
	std::string declaration;	// the text between "<?xml" and "?>", empty when the file has none
	bool hasDeclaration = false;
	Layout layout = Mfc;
	Node root;
};

// Returns false and fills szError (with a byte offset) on malformed input. A document with
// whitespace between its elements is read as Indented, any other as Mfc. <a></a> reads as an
// element with one empty text child and <a/> as one with no children, as MSXML tells them apart.
bool Parse( const std::string &szXml, Document &doc, std::string &szError );
// Writes doc.layout, CRLF line ends. Re-parsing the output and serialising again yields the same
// bytes, and an unedited MFC project comes back byte-identical.
std::string Serialise( const Document &doc );
// First child element of pParent with the given name, or nullptr.
const Node *FindChild( const Node &parent, const std::string &szName );

}
