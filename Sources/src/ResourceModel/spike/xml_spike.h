#pragma once
// Portable spike of the project-XML tree the MFC editors read and write through CDataTreeXML (MSXML).
// It models only what a round trip needs: elements, attributes, text, comments, CDATA and processing
// instructions, so an unknown node survives untouched. No MFC, no Windows API.
#include <string>
#include <utility>
#include <vector>

namespace XmlSpike
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
	std::string declaration;	// the text between "<?xml" and "?>", empty when the file has none
	bool hasDeclaration = false;
	Node root;
};

// Returns false and fills szError (with a byte offset) on malformed input.
bool Parse( const std::string &szXml, Document &doc, std::string &szError );
// Tab-indented, CRLF line ends, the layout of the shipped project files. Re-parsing the output and
// serialising again yields the same bytes.
std::string Serialise( const Document &doc );
// First child element of pParent with the given name, or nullptr.
const Node *FindChild( const Node &parent, const std::string &szName );

}
