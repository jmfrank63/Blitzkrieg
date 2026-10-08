// Fidelity tests for Sources/src/ResourceModel against the MFC tree items
// (S03 reopened, D-04, D-07). The reference is generated, never hand-written:
// tools/zig/mfc_item_inventory.py reads Sources/src/editor and writes
// tools/zig/fixtures/resource_editor/mfc-item-inventory.json.
//
// Three families of checks, each with a stable id:
//   inventory:<Class>         the port class the factory makes for the MFC
//                             ClassTypeID has MFC's property table (name, DT_*
//                             type, nId, default, combo strings, in order), its
//                             default children, and serialise() writes MFC's
//                             <item> shape (ClassTypeID/expand/scalar attributes,
//                             element order, one values/item per prop).
//   roundtrip-bytes:<p>       load -> save of an unedited project is byte-identical.
//   roundtrip-content:<p>     load -> save -> reload gives the same XML content.
//   roundtrip-typed:<p>       the file is in MFC project shape (root with childs)
//                             and every <item ClassTypeID=...> under childs loads
//                             as a typed item of that type with that many props
//                             (not as an opaque blob). MFC's reader also accepts
//                             the older writer's type="..." (DTHelper.h), so does this.
//   roundtrip-edit:<p>        an edit to the first typed item reaches the saved
//                             file and survives a reload.
//   insert:<Class>            an item of that type inserted through the model
//                             (under its MFC parent where MFC has one) saves as
//                             valid XML in MFC's reader shape and reloads typed.
//
// Projects are every MFC project in Data/Editor/TestProjects and every fixture
// tools/zig/fixtures/resource_editor/<ext>/project.<ext>, copied to
// zig-out/local-test/resource_model/fidelity/ first; nothing is written in place.
//
// Known gaps are listed in tools/zig/fixtures/resource_editor/resource-model-xfail.txt.
// A listed check that fails is XFAIL; one that passes is XPASS and fails the run,
// so the list only ever shrinks to what is really still missing (T02-T05 empty it).
// An id in the list that no check produces fails the run as well.
//
// Output: one line per check to stdout and
// zig-out/local-test/resource_model/fidelity.log, then a summary line.

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../../Sources/src/ResourceModel/combos.h"
#include "../../Sources/src/ResourceModel/editor_env.h"
#include "../../Sources/src/ResourceModel/factory.h"
#include "../../Sources/src/ResourceModel/future_blob.h"
#include "../../Sources/src/ResourceModel/mfc_value.h"
#include "../../Sources/src/ResourceModel/items/stats_item.h"
#include "../../Sources/src/ResourceModel/project.h"
#include "../../Sources/src/ResourceModel/xml.h"

namespace fs = std::filesystem;
using namespace NResourceModel;

namespace
{

const char *const kInventory = "tools/zig/fixtures/resource_editor/mfc-item-inventory.json";
const char *const kXFail = "tools/zig/fixtures/resource_editor/resource-model-xfail.txt";
const char *const kFixtures = "tools/zig/fixtures/resource_editor";
const char *const kTestProjects = "Data/Editor/TestProjects";
const char *const kWorkDir = "zig-out/local-test/resource_model/fidelity";
const char *const kLog = "zig-out/local-test/resource_model/fidelity.log";

std::string ReadAll( const fs::path &path )
{
	std::ifstream in( path, std::ios::binary );
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

void WriteAll( const fs::path &path, const std::string &data )
{
	std::ofstream out( path, std::ios::binary );
	out << data;
}

// ---------------------------------------------------------------------------
// Minimal JSON reader for the inventory (objects, arrays, strings, numbers,
// true/false/null). Numbers are kept as their source text.

struct Json
{
	enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
	std::string text;	// Number source text, String value, Bool "true"/"false"
	std::vector<Json> items;
	std::vector<std::pair<std::string, Json>> members;

	const Json &operator[]( const char *key ) const
	{
		static const Json null;
		for ( const auto &m : members )
			if ( m.first == key )
				return m.second;
		return null;
	}
	bool Has( const char *key ) const
	{
		for ( const auto &m : members )
			if ( m.first == key )
				return true;
		return false;
	}
	long long Int() const { return std::strtoll( text.c_str(), nullptr, 10 ); }
	const std::string &Str() const { return text; }
};

struct JsonParser
{
	const std::string &s;
	size_t i = 0;
	std::string error;

	void Ws() { while ( i < s.size() && std::strchr( " \t\r\n", s[i] ) ) ++i; }

	bool Parse( Json &out )
	{
		Ws();
		if ( i >= s.size() ) return Fail( "unexpected end" );
		const char c = s[i];
		if ( c == '{' )
		{
			out.kind = Json::Object;
			++i; Ws();
			if ( i < s.size() && s[i] == '}' ) { ++i; return true; }
			for ( ;; )
			{
				Json key;
				Ws();
				if ( !ParseString( key ) ) return false;
				Ws();
				if ( i >= s.size() || s[i] != ':' ) return Fail( "expected ':'" );
				++i;
				Json value;
				if ( !Parse( value ) ) return false;
				out.members.emplace_back( key.text, std::move( value ) );
				Ws();
				if ( i < s.size() && s[i] == ',' ) { ++i; continue; }
				if ( i < s.size() && s[i] == '}' ) { ++i; return true; }
				return Fail( "expected ',' or '}'" );
			}
		}
		if ( c == '[' )
		{
			out.kind = Json::Array;
			++i; Ws();
			if ( i < s.size() && s[i] == ']' ) { ++i; return true; }
			for ( ;; )
			{
				Json value;
				if ( !Parse( value ) ) return false;
				out.items.push_back( std::move( value ) );
				Ws();
				if ( i < s.size() && s[i] == ',' ) { ++i; continue; }
				if ( i < s.size() && s[i] == ']' ) { ++i; return true; }
				return Fail( "expected ',' or ']'" );
			}
		}
		if ( c == '"' ) return ParseString( out );
		if ( s.compare( i, 4, "true" ) == 0 ) { out.kind = Json::Bool; out.text = "true"; i += 4; return true; }
		if ( s.compare( i, 5, "false" ) == 0 ) { out.kind = Json::Bool; out.text = "false"; i += 5; return true; }
		if ( s.compare( i, 4, "null" ) == 0 ) { out.kind = Json::Null; i += 4; return true; }
		const size_t start = i;
		while ( i < s.size() && std::strchr( "+-0123456789.eE", s[i] ) ) ++i;
		if ( i == start ) return Fail( "unexpected character" );
		out.kind = Json::Number;
		out.text = s.substr( start, i - start );
		return true;
	}

	bool ParseString( Json &out )
	{
		if ( i >= s.size() || s[i] != '"' ) return Fail( "expected string" );
		++i;
		out.kind = Json::String;
		while ( i < s.size() && s[i] != '"' )
		{
			if ( s[i] != '\\' ) { out.text += s[i++]; continue; }
			++i;
			const char e = s[i++];
			switch ( e )
			{
			case 'n': out.text += '\n'; break;
			case 't': out.text += '\t'; break;
			case 'r': out.text += '\r'; break;
			case 'b': out.text += '\b'; break;
			case 'f': out.text += '\f'; break;
			case 'u':
			{
				// The generator writes latin-1 with ensure_ascii off, so \u only
				// appears for control characters; keep the low byte.
				const unsigned v = std::strtoul( s.substr( i, 4 ).c_str(), nullptr, 16 );
				out.text += char( v & 0xFF );
				i += 4;
				break;
			}
			default: out.text += e; break;
			}
		}
		if ( i >= s.size() ) return Fail( "unterminated string" );
		++i;
		return true;
	}

	bool Fail( const char *what )
	{
		error = std::string( what ) + " at offset " + std::to_string( i );
		return false;
	}
};

// ---------------------------------------------------------------------------
// Result bookkeeping.

struct Results
{
	std::set<std::string> xfail;
	std::set<std::string> seen;
	int pass = 0, xfailed = 0, fail = 0, xpass = 0;
	std::ofstream log;

	void Line( const std::string &line )
	{
		std::printf( "%s\n", line.c_str() );
		log << line << "\n";
	}

	void Report( const std::string &id, bool ok, const std::string &detail )
	{
		seen.insert( id );
		const bool expected = xfail.count( id ) != 0;
		if ( ok && !expected ) { ++pass; Line( "PASS  " + id ); }
		else if ( ok && expected ) { ++xpass; Line( "XPASS " + id + " - passes now; remove it from " + kXFail ); }
		else if ( !ok && expected ) { ++xfailed; Line( "XFAIL " + id + " - " + detail ); }
		else { ++fail; Line( "FAIL  " + id + " - " + detail ); }
	}
};

// ---------------------------------------------------------------------------
// Inventory conformance.

bool NearlyEqualText( const std::string &a, const std::string &b )
{
	return std::strtod( a.c_str(), nullptr ) == std::strtod( b.c_str(), nullptr );
}

// MFC's CVariant converts freely between int, float, bool and string; the port
// may pick a narrower kind, so equality is by value, not by kind.
bool DefaultMatches( const CVariant &v, const Json &def, std::string &actual )
{
	actual = v.ToString();
	const std::string &kind = def["kind"].Str();
	const std::string &text = def["text"].Str();
	switch ( v.GetKind() )
	{
	case CVariant::VK_INT: return ( kind == "int" || kind == "float" ) && NearlyEqualText( std::to_string( v.AsInt() ), text );
	case CVariant::VK_FLOAT: return ( kind == "int" || kind == "float" ) && double( v.AsFloat() ) == double( std::strtof( text.c_str(), nullptr ) );
	case CVariant::VK_BOOL:
		if ( kind == "bool" ) return v.AsBool() == ( text == "true" );
		return kind == "int" && v.AsBool() == ( std::strtoll( text.c_str(), nullptr, 10 ) != 0 );
	case CVariant::VK_COLOR: return kind == "int" && std::int32_t( v.AsColor() ) == std::int32_t( std::strtoll( text.c_str(), nullptr, 10 ) );
	case CVariant::VK_STR: return kind == "str" && v.AsStr() == text;
	case CVariant::VK_REF: return kind == "str" && v.AsRef().value == text;
	case CVariant::VK_COMBO: return kind == "int" && v.AsCombo().index == std::strtoll( text.c_str(), nullptr, 10 );
	case CVariant::VK_INT64: return kind == "int" && v.AsInt64() == std::strtoll( text.c_str(), nullptr, 10 );
	default: return false;
	}
}

const NResourceXml::Node *FirstElement( const NResourceXml::Node &n, const std::string &name )
{
	for ( const auto &c : n.children )
		if ( c.kind == NResourceXml::Node::Element && c.name == name )
			return &c;
	return nullptr;
}

std::string Attr( const NResourceXml::Node &n, const std::string &name, bool *found = nullptr )
{
	for ( const auto &a : n.attrs )
		if ( a.first == name )
		{
			if ( found ) *found = true;
			return a.second;
		}
	if ( found ) *found = false;
	return std::string();
}

std::string ElementText( const NResourceXml::Node *n )
{
	if ( !n ) return std::string();
	std::string out;
	for ( const auto &c : n->children )
		if ( c.kind == NResourceXml::Node::Text || c.kind == NResourceXml::Node::CData )
			out += c.text;
	return out;
}

std::string Join( const std::vector<std::string> &v )
{
	std::string out;
	for ( const auto &s : v ) out += ( out.empty() ? "" : "," ) + s;
	return "[" + out + "]";
}

std::string CheckInventoryClass( const Json &cls )
{
	const std::string name = cls["class"].Str();
	const int type = int( cls["type_id"].Int() );
	std::unique_ptr<CTreeItem> item = CTreeItemFactory::Instance().Create( type );
	if ( !item )
		return "the port factory does not register ClassTypeID " + std::to_string( type ) + " (" + cls["type_name"].Str() + ")";
	if ( item->GetItemType() != type )
		return "item type " + std::to_string( item->GetItemType() ) + ", expected " + std::to_string( type );

	const Json &props = cls["props"];
	const CPropVector &defaults = item->GetDefaultValues();
	for ( size_t i = 0; i < props.items.size() || i < defaults.size(); ++i )
	{
		const std::string where = name + " property " + std::to_string( i );
		if ( i >= defaults.size() )
			return where + ": expected '" + props.items[i]["default_name"].Str() + "' " + props.items[i]["domen_type"].Str() + ", port has none (" + std::to_string( defaults.size() ) + " of " + std::to_string( props.items.size() ) + " props)";
		if ( i >= props.items.size() )
			return where + ": port has extra '" + defaults[i].szDefaultName + "', MFC has " + std::to_string( props.items.size() ) + " props";
		const Json &p = props.items[i];
		const SProp &q = defaults[i];
		if ( q.szDefaultName != p["default_name"].Str() || int( q.nDomenType ) != int( p["domen_value"].Int() ) )
			return where + ": expected '" + p["default_name"].Str() + "' " + p["domen_type"].Str() + ", actual '" + q.szDefaultName + "' domen " + std::to_string( int( q.nDomenType ) );
		if ( q.nId != int( p["id"].Int() ) )
			return where + " '" + q.szDefaultName + "': nId " + std::to_string( q.nId ) + ", expected " + p["id"].Str();
		std::string actual;
		if ( !DefaultMatches( q.value, p["default"], actual ) )
			return where + " '" + q.szDefaultName + "': default '" + actual + "', expected " + p["default"]["kind"].Str() + " '" + p["default"]["text"].Str() + "'";
		// A literal must match; another runtime entry (a project path, a
		// filter) stands for one string; FillVectorOfSides() stands for the
		// party names of the shipped partys.xml, which MFC reads at that point.
		static const std::vector<std::string> sides = readPlayerSides( "Data/partys.xml" );
		static const std::string runtime;
		std::vector<const std::string *> expected;
		for ( const auto &str : p["strings"].items )
		{
			if ( str.kind == Json::String ) expected.push_back( &str.Str() );
			else if ( str["runtime"].Str() == "FillVectorOfSides()" ) for ( const auto &side : sides ) expected.push_back( &side );
			else expected.push_back( &runtime );
		}
		if ( q.szStrings.size() != expected.size() )
			return where + " '" + q.szDefaultName + "': " + std::to_string( q.szStrings.size() ) + " combo/browse strings, expected " + std::to_string( expected.size() );
		for ( size_t s = 0; s < expected.size(); ++s )
			if ( expected[s] != &runtime && q.szStrings[s] != *expected[s] )
				return where + " '" + q.szDefaultName + "': string " + std::to_string( s ) + " '" + q.szStrings[s] + "', expected '" + *expected[s] + "'";
	}
	// An item MFC creates gets CreateDefaultChilds from AddChild before anyone
	// sees it; a few InitDefaultValues bodies (CUnitActionPropsItem) leave
	// values empty until then.
	item->CreateDefaultChilds();
	if ( item->GetValues().size() != defaults.size() )
		return name + ": values has " + std::to_string( item->GetValues().size() ) + " entries after CreateDefaultChilds, defaultValues " + std::to_string( defaults.size() );

	const Json &childs = cls["default_childs"];
	std::vector<CTreeItem::SChildItem> portChilds( item->GetDefaultChilds().begin(), item->GetDefaultChilds().end() );
	for ( size_t i = 0; i < childs.items.size() || i < portChilds.size(); ++i )
	{
		const std::string where = name + " default child " + std::to_string( i );
		if ( i >= portChilds.size() )
			return where + ": expected '" + childs.items[i]["default_name"].Str() + "' (" + childs.items[i]["type_name"].Str() + "), port has none";
		if ( i >= childs.items.size() )
			return where + ": port has extra '" + portChilds[i].szDefaultName + "'";
		if ( portChilds[i].nChildItemType != int( childs.items[i]["type_id"].Int() ) || portChilds[i].szDefaultName != childs.items[i]["default_name"].Str() )
			return where + ": expected '" + childs.items[i]["default_name"].Str() + "' type " + childs.items[i]["type_id"].Str() + ", actual '" + portChilds[i].szDefaultName + "' type " + std::to_string( portChilds[i].nChildItemType );
	}

	// serialise() must write MFC's <item>: the caller names the element (the
	// container does in MFC); the item writes ClassTypeID, expand and scalar
	// fields as attributes and the rest as elements, in MFC's order.
	NResourceXml::Node node;
	node.kind = NResourceXml::Node::Element;
	node.name = "item";
	item->serialise( node );
	const Json &shape = cls["serialise"];
	std::vector<std::string> wantAttrs, gotAttrs, wantElems, gotElems;
	for ( const auto &a : shape["attributes"].items ) wantAttrs.push_back( a.Str() );
	for ( const auto &e : shape["elements"].items ) wantElems.push_back( e.Str() );
	for ( const auto &a : node.attrs ) gotAttrs.push_back( a.first );
	for ( const auto &c : node.children )
		if ( c.kind == NResourceXml::Node::Element ) gotElems.push_back( c.name );
	if ( gotAttrs != wantAttrs )
		return name + " serialise: attributes " + Join( gotAttrs ) + ", MFC writes " + Join( wantAttrs );
	if ( Attr( node, "ClassTypeID" ) != std::to_string( type ) )
		return name + " serialise: ClassTypeID=\"" + Attr( node, "ClassTypeID" ) + "\", expected " + std::to_string( type );
	if ( gotElems != wantElems )
		return name + " serialise: elements " + Join( gotElems ) + ", MFC writes " + Join( wantElems );
	const NResourceXml::Node *values = FirstElement( node, "values" );
	std::vector<const NResourceXml::Node *> written;
	for ( const auto &c : values->children )
		if ( c.kind == NResourceXml::Node::Element ) written.push_back( &c );
	if ( written.size() != props.items.size() )
		return name + " serialise: values has " + std::to_string( written.size() ) + " entries, expected " + std::to_string( props.items.size() );
	for ( size_t i = 0; i < written.size(); ++i )
	{
		const std::string got = ElementText( FirstElement( *written[i], "default_name" ) );
		if ( written[i]->name != "item" || got != props.items[i]["default_name"].Str() || !FirstElement( *written[i], "value" ) )
			return name + " serialise: values entry " + std::to_string( i ) + " is <" + written[i]->name + "> '" + got + "', expected <item><default_name>" + props.items[i]["default_name"].Str() + "</default_name><value .../></item>";
	}
	return std::string();
}

// ---------------------------------------------------------------------------
// Round trip.

std::string FirstByteDifference( const std::string &want, const std::string &got )
{
	size_t at = 0;
	while ( at < want.size() && at < got.size() && want[at] == got[at] ) ++at;
	auto excerpt = []( const std::string &s, size_t at ) {
		const size_t line = s.rfind( '\n', at == 0 ? 0 : at - 1 );
		const size_t from = line == std::string::npos ? 0 : line + 1;
		const size_t begin = at > from + 60 ? at - 60 : from;
		std::string out = s.substr( begin, (std::min<size_t>)( 120, s.size() - (std::min)( begin, s.size() ) ) );
		for ( char &c : out ) if ( c == '\r' || c == '\n' || c == '\t' ) c = ' ';
		return out;
	};
	size_t lineNo = 1;
	for ( size_t k = 0; k < at && k < want.size(); ++k ) if ( want[k] == '\n' ) ++lineNo;
	return "first difference at byte " + std::to_string( at ) + " (line " + std::to_string( lineNo ) + ", sizes " + std::to_string( want.size() ) + " vs " +
		std::to_string( got.size() ) + "); MFC: \"" + excerpt( want, at ) + "\" port: \"" + excerpt( got, at ) + "\"";
}

std::string NodePath( const std::vector<std::string> &stack )
{
	std::string out;
	for ( const auto &s : stack ) out += "/" + s;
	return out;
}

// Content equality: same elements, attributes and text, ignoring the layout
// whitespace the two writers may place differently.
std::string CompareContent( const NResourceXml::Node &a, const NResourceXml::Node &b, std::vector<std::string> &stack )
{
	stack.push_back( a.name );
	if ( a.name != b.name ) return NodePath( stack ) + ": element <" + b.name + ">, expected <" + a.name + ">";
	if ( a.attrs != b.attrs ) return NodePath( stack ) + ": attributes differ";
	auto significant = []( const NResourceXml::Node &n ) {
		std::vector<const NResourceXml::Node *> out;
		for ( const auto &c : n.children )
		{
			if ( c.kind == NResourceXml::Node::Comment || c.kind == NResourceXml::Node::Pi ) continue;
			if ( c.kind == NResourceXml::Node::Text && c.text.find_first_not_of( " \t\r\n" ) == std::string::npos ) continue;
			out.push_back( &c );
		}
		return out;
	};
	const auto ca = significant( a ), cb = significant( b );
	if ( ca.size() != cb.size() ) return NodePath( stack ) + ": " + std::to_string( cb.size() ) + " children, expected " + std::to_string( ca.size() );
	for ( size_t i = 0; i < ca.size(); ++i )
	{
		if ( ca[i]->kind != cb[i]->kind ) return NodePath( stack ) + ": child " + std::to_string( i ) + " kind differs";
		if ( ca[i]->kind != NResourceXml::Node::Element )
		{
			if ( ca[i]->text != cb[i]->text ) return NodePath( stack ) + ": text '" + cb[i]->text + "', expected '" + ca[i]->text + "'";
			continue;
		}
		std::string diff = CompareContent( *ca[i], *cb[i], stack );
		if ( !diff.empty() ) return diff;
	}
	stack.pop_back();
	return std::string();
}

struct TypedEntry
{
	int type = 0;
	size_t props = 0;
	std::string name;
};

// The item class id as MFC's reader takes it: ClassTypeID, else the older type.
bool ItemType( const NResourceXml::Node &n, int &type )
{
	if ( n.kind != NResourceXml::Node::Element ) return false;
	bool found = false;
	std::string v = Attr( n, "ClassTypeID", &found );
	if ( !found ) v = Attr( n, "type", &found );
	if ( found ) type = std::atoi( v.c_str() );
	return found;
}

// The <item ClassTypeID=...> tree MFC's CTreeItem::operator& wrote, pre-order.
void CollectXmlItems( const NResourceXml::Node &owner, std::vector<TypedEntry> &out )
{
	const NResourceXml::Node *childs = FirstElement( owner, "childs" );
	if ( !childs ) return;
	for ( const auto &c : childs->children )
	{
		TypedEntry e;
		if ( !ItemType( c, e.type ) ) continue;
		e.name = ElementText( FirstElement( c, "default_name" ) );
		if ( const NResourceXml::Node *values = FirstElement( c, "values" ) )
			for ( const auto &v : values->children )
				if ( v.kind == NResourceXml::Node::Element ) ++e.props;
		out.push_back( e );
		CollectXmlItems( c, out );
	}
}

void CollectModelItems( const CTreeItem &owner, std::vector<TypedEntry> &out )
{
	for ( const auto &child : owner.GetChildren() )
	{
		if ( FutureBlob::IsFutureBlob( *child ) ) continue;
		TypedEntry e;
		e.type = child->GetItemType();
		e.props = child->GetValues().size();
		e.name = child->GetDefaultName();
		out.push_back( e );
		CollectModelItems( *child, out );
	}
}

std::string CheckTyped( const Project &project )
{
	if ( !FirstElement( project.document.root, "childs" ) )
		return "not an MFC project: <" + project.document.root.name + "> has no childs list (MFC's CTreeItem tree)";
	std::vector<TypedEntry> xml, model;
	CollectXmlItems( project.document.root, xml );
	if ( project.root ) CollectModelItems( *project.root, model );
	if ( !project.root || FutureBlob::IsFutureBlob( *project.root ) )
		return "root <" + project.document.root.name + "> loads as an opaque blob, not a typed root item";
	for ( size_t i = 0; i < xml.size(); ++i )
	{
		if ( i >= model.size() )
			return "item " + std::to_string( i ) + " '" + xml[i].name + "' type " + std::to_string( xml[i].type ) + " is not in the typed tree (" + std::to_string( model.size() ) + " of " + std::to_string( xml.size() ) + " typed)";
		if ( model[i].type != xml[i].type || model[i].props != xml[i].props )
			return "item " + std::to_string( i ) + " '" + xml[i].name + "': typed as " + std::to_string( model[i].type ) + " with " + std::to_string( model[i].props ) + " props, file has type " + std::to_string( xml[i].type ) + " with " + std::to_string( xml[i].props );
	}
	if ( model.size() != xml.size() )
		return "typed tree has " + std::to_string( model.size() ) + " items, file has " + std::to_string( xml.size() );
	return std::string();
}

CTreeItem *FirstTyped( CTreeItem &owner )
{
	for ( auto &child : owner.MutableChildren() )
	{
		if ( FutureBlob::IsFutureBlob( *child ) ) continue;
		return child.get();
	}
	return nullptr;
}

void RoundTrip( Results &r, const std::string &label, const fs::path &source )
{
	const fs::path dir = fs::path( kWorkDir ) / "roundtrip";
	std::string flat = label;
	for ( char &c : flat ) if ( c == '/' || c == '\\' ) c = '_';
	const fs::path copy = dir / flat;
	fs::copy_file( source, copy, fs::copy_options::overwrite_existing );
	const std::string original = ReadAll( copy );

	Project project;
	std::string error;
	if ( !Load( original, project, error ) )
	{
		for ( const char *kind : { "roundtrip-bytes:", "roundtrip-content:", "roundtrip-typed:", "roundtrip-edit:" } )
			r.Report( kind + label, false, copy.string() + ": load failed: " + error );
		return;
	}
	const std::string saved = Save( project );
	WriteAll( copy.string() + ".saved", saved );
	r.Report( "roundtrip-bytes:" + label, saved == original, copy.string() + ": " + ( saved == original ? std::string() : FirstByteDifference( original, saved ) ) );

	Project reloaded;
	std::string content;
	if ( !Load( saved, reloaded, error ) )
		content = "saved file does not parse: " + error;
	else
	{
		std::vector<std::string> stack;
		content = CompareContent( project.document.root, reloaded.document.root, stack );
	}
	r.Report( "roundtrip-content:" + label, content.empty(), copy.string() + ".saved: " + content );

	const std::string typed = CheckTyped( project );
	r.Report( "roundtrip-typed:" + label, typed.empty(), copy.string() + ": " + typed );

	// Edit the first typed item's display name through the model; the saved
	// file and a reload must both carry it.
	std::string edit;
	CTreeItem *target = project.root && !FutureBlob::IsFutureBlob( *project.root ) ? FirstTyped( *project.root ) : nullptr;
	if ( !target )
		edit = "no typed item to edit";
	else
	{
		const std::string probe = "rm-edit-probe";
		target->SetDisplayName( probe );
		const std::string edited = Save( project );
		WriteAll( copy.string() + ".edited", edited );
		Project back;
		if ( !Load( edited, back, error ) )
			edit = "edited file does not parse: " + error;
		else
		{
			const NResourceXml::Node *childs = FirstElement( back.document.root, "childs" );
			const NResourceXml::Node *first = nullptr;
			if ( childs )
				for ( const auto &c : childs->children )
					if ( c.kind == NResourceXml::Node::Element ) { first = &c; break; }
			const std::string got = first ? ElementText( FirstElement( *first, "display_name" ) ) : std::string();
			CTreeItem *backFirst = back.root && !FutureBlob::IsFutureBlob( *back.root ) ? FirstTyped( *back.root ) : nullptr;
			if ( got != probe )
				edit = "saved first item display_name is '" + got + "', expected '" + probe + "'";
			else if ( !backFirst || backFirst->GetDisplayName() != probe )
				edit = "reloaded first typed item does not carry the edited display name";
		}
	}
	r.Report( "roundtrip-edit:" + label, edit.empty(), copy.string() + ": " + edit );
}

// ---------------------------------------------------------------------------
// Insert.

const Json *FindClass( const Json &inventory, const std::string &name )
{
	for ( const auto &c : inventory["classes"].items )
		if ( c["class"].Str() == name )
			return &c;
	return nullptr;
}

std::string CheckInsert( const Json &inventory, const Json &cls )
{
	const Json &editor = cls["editor"];
	const std::string tag = editor["root_tag"].Str();
	const int type = int( cls["type_id"].Int() );
	auto &factory = CTreeItemFactory::Instance();

	// An empty project in MFC's shape: the root element with an empty childs list.
	Project project;
	std::string error;
	if ( !Load( "<?xml version=\"1.0\"?>\r\n<" + tag + "><childs/></" + tag + ">\r\n", project, error ) )
		return "empty " + tag + " project does not load: " + error;
	if ( !project.root || FutureBlob::IsFutureBlob( *project.root ) )
		return "empty " + tag + " project loads as an opaque blob, not a typed root";

	std::unique_ptr<CTreeItem> item = factory.Create( type );
	if ( !item )
		return "the port factory does not register " + cls["type_name"].Str();
	item->SetDefaultName( "rm-insert-probe" );

	// Under its MFC parent when an MFC handler creates it, else under the root.
	int parentType = 0;
	if ( !cls["inserted_by"].items.empty() )
	{
		const Json *parent = FindClass( inventory, cls["inserted_by"].items[0].Str() );
		parentType = parent ? int( ( *parent )["type_id"].Int() ) : 0;
	}
	if ( parentType != 0 && parentType != project.root->GetItemType() )
	{
		std::unique_ptr<CTreeItem> parent = factory.Create( parentType );
		if ( !parent )
			return "the port factory does not register parent type " + std::to_string( parentType );
		parent->AddChild( std::move( item ) );
		project.root->AddChild( std::move( parent ) );
	}
	else
		project.root->AddChild( std::move( item ) );

	const std::string saved = Save( project );
	NResourceXml::Document doc;
	if ( !NResourceXml::Parse( saved, doc, error ) )
	{
		const size_t at = error.find( "byte " ) != std::string::npos ? std::strtoul( error.c_str() + error.find( "byte " ) + 5, nullptr, 10 ) : 0;
		std::string around = saved.substr( at > 40 ? at - 40 : 0, 80 );
		for ( char &c : around ) if ( c == '\r' || c == '\n' || c == '\t' ) c = ' ';
		return "saved project is not valid XML (" + error + "): \"" + around + "\"";
	}
	std::vector<TypedEntry> xml;
	CollectXmlItems( doc.root, xml );
	bool found = false;
	for ( const auto &e : xml )
		if ( e.type == type && e.name == "rm-insert-probe" ) found = true;
	if ( !found )
		return "saved project has no <item ClassTypeID=\"" + std::to_string( type ) + "\"> with default_name rm-insert-probe under " + tag + "/childs";

	Project back;
	if ( !Load( saved, back, error ) )
		return "saved project does not reload: " + error;
	std::vector<TypedEntry> model;
	if ( back.root ) CollectModelItems( *back.root, model );
	for ( const auto &e : model )
		if ( e.type == type && e.name == "rm-insert-probe" ) return std::string();
	return "reloaded project has no typed item of type " + std::to_string( type );
}

std::set<std::string> ReadXFail()
{
	std::set<std::string> out;
	std::istringstream in( ReadAll( kXFail ) );
	std::string line;
	while ( std::getline( in, line ) )
	{
		while ( !line.empty() && ( line.back() == '\r' || line.back() == ' ' ) ) line.pop_back();
		if ( line.empty() || line[0] == '#' ) continue;
		out.insert( line );
	}
	return out;
}

std::set<std::string> ProjectExtensions( const Json &inventory )
{
	std::set<std::string> out;
	for ( const auto &f : inventory["frames"].items ) out.insert( "." + f["extension"].Str() );
	return out;
}

}

// MFC reads <own_data> and the cached stats block unguarded when it opens a project, so every
// fixture the port saved must hold both (a gui screen has neither, a kind with no cached block
// only own_data). One log line per fixture.
static void CheckFrameData( Results &r, const std::string &ext, const fs::path &path )
{
	std::ifstream in( path, std::ios::binary );
	std::stringstream ss;
	ss << in.rdbuf();
	NResourceXml::Document doc;
	std::string szError;
	if ( !NResourceXml::Parse( ss.str(), doc, szError ) )
	{
		r.Report( "frame-data:" + ext, false, szError );
		return;
	}
	const char *pszBlock = NResourceModel::CachedBlockName( ext );
	const bool bOwnData = NResourceXml::FindChild( doc.root, "own_data" ) != nullptr;
	const bool bBlock = pszBlock == nullptr || NResourceXml::FindChild( doc.root, pszBlock ) != nullptr;
	const bool bWanted = ext != "gui";
	const bool bOk = bOwnData == bWanted && bBlock;
	r.Report( "frame-data:" + ext, bOk,
	          std::string( "own_data " ) + ( bOwnData ? "present" : "absent" ) + ", cached " + ( pszBlock ? pszBlock : "none" ) + ( bBlock ? " present" : " missing" ) );
}

// The flag attribute of a <value> is CVariant's m_flagsOptimized, the bit set of the slots that
// are current. A bool read through operator bool gains VT_INT (OptimizeInt), so the editors whose
// save reads their bools (FillRPGStats: road 3dRoadFrm.cpp:121, river layers 3dRiverFrm.cpp:145,
// campaign chapters CampaignFrm.cpp:100, mission objectives MissionFrm.cpp:127, weapon damage
// WeaponFrm.cpp:134) write 9; a bool nothing reads stays 8, as in MFC's own bridge, fence, unit and
// mesh projects (mfc-new), and the terrain's, which only ComposeTiles reads on export. A bool the port
// wrote as 8 in a road is read as false by MFC although int_value is 1 (win-home, 2026-10-06).
static int MfcFlagFor( const std::string &ext, int nType )
{
	const bool bSaveReadsBools = ext == "3rd" || ext == "3rv" || ext == "cgc" || ext == "mip" || ext == "wpn";
	return nType == NResourceModel::MFC_VT_BOOL && bSaveReadsBools ? NResourceModel::MFC_VT_BOOL | NResourceModel::MFC_VT_INT : nType;
}

static void CollectValueFlags( const std::string &ext, const NResourceXml::Node &node, int &nValues, std::string &szBad )
{
	if ( node.kind != NResourceXml::Node::Element )
		return;
	if ( node.name == "value" )
	{
		const std::string *pType = NResourceModel::FindAttr( node, "type" );
		const std::string *pFlag = NResourceModel::FindAttr( node, "flag" );
		if ( pType && pFlag )
		{
			++nValues;
			if ( std::atoi( pFlag->c_str() ) != MfcFlagFor( ext, std::atoi( pType->c_str() ) ) )
				{ if ( szBad.size() < 120 ) szBad += " type=" + *pType + " flag=" + *pFlag; }
		}
	}
	for ( const auto &c : node.children )
		CollectValueFlags( ext, c, nValues, szBad );
}

static void CheckValueFlags( Results &r, const std::string &ext, const fs::path &path, const char *pszSet = "" )
{
	std::ifstream in( path, std::ios::binary );
	std::stringstream ss;
	ss << in.rdbuf();
	NResourceXml::Document doc;
	std::string szError;
	if ( !NResourceXml::Parse( ss.str(), doc, szError ) )
	{
		r.Report( std::string( "value-flags:" ) + pszSet + ext, false, szError );
		return;
	}
	int nValues = 0;
	std::string szBad;
	CollectValueFlags( ext, doc.root, nValues, szBad );
	r.Report( std::string( "value-flags:" ) + pszSet + ext, szBad.empty(), std::to_string( nValues ) + " values" + ( szBad.empty() ? "" : ", not MFC's flag:" + szBad ) );
}

// A bool the port encodes in an item MFC reads as a bool carries flag 9, new or edited over a
// stored value or kept; elsewhere and for the other types the flag is the type.
static void CheckEncodedFlags( Results &r )
{
	std::string szBad;
	auto flagOf = []( const CVariant &v, const NResourceXml::Node *pStored ) {
		NResourceXml::Node out;
		NResourceModel::EncodeMfcValue( v, pStored, out );
		const std::string *p = NResourceModel::FindAttr( out, "flag" );
		return p ? std::atoi( p->c_str() ) : -1;
	};
	NResourceXml::Node stored;
	stored.kind = NResourceXml::Node::Element;
	stored.name = "value";
	NResourceModel::SetAttr( stored, "type", "8" );
	NResourceModel::SetAttr( stored, "flag", "8" );
	NResourceModel::SetAttr( stored, "int_value", "0" );
	auto roadFlagOf = []( const CVariant &v, const NResourceXml::Node *pStored ) {
		NResourceXml::Node out;
		NResourceModel::EncodeMfcValue( v, pStored, out, true );
		const std::string *p = NResourceModel::FindAttr( out, "flag" );
		return p ? std::atoi( p->c_str() ) : -1;
	};
	for ( bool b : { false, true } )
	{
		if ( roadFlagOf( CVariant( b ), nullptr ) != 9 ) szBad += " new road bool";
		if ( roadFlagOf( CVariant( b ), &stored ) != 9 ) szBad += " kept or edited road bool";
		if ( flagOf( CVariant( b ), nullptr ) != 8 ) szBad += " new bool";
	}
	if ( flagOf( CVariant( true ), &stored ) != 8 ) szBad += " kept bool";
	if ( flagOf( CVariant( 5 ), nullptr ) != 1 ) szBad += " int";
	if ( flagOf( CVariant( 0.5f ), nullptr ) != 2 ) szBad += " float";
	if ( flagOf( CVariant( "x" ), nullptr ) != 4 ) szBad += " string";
	r.Report( "value-flags:encoder", szBad.empty(), szBad.empty() ? "road bool 9, other bool 8, int 1, float 2, string 4" : "wrong flag for:" + szBad );
}

// The entries of a folder, or a FAIL line naming it when it is not there: a
// checkout that lacks part of Data (a sparse one) must say which path is
// missing, not abort on the filesystem exception.
static std::vector<fs::directory_entry> ListDirectory( Results &r, const fs::path &dir )
{
	std::vector<fs::directory_entry> entries;
	std::error_code ec;
	for ( fs::directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
		entries.push_back( *it );
	if ( ec )
		r.Report( "folder-present:" + dir.generic_string(), false, dir.generic_string() + " cannot be listed: " + ec.message() );
	return entries;
}

int main()
{
	const std::string text = ReadAll( kInventory );
	Json inventory;
	JsonParser parser{ text };
	if ( text.empty() || !parser.Parse( inventory ) )
	{
		std::fprintf( stderr, "FAIL cannot read %s: %s\n", kInventory, parser.error.c_str() );
		return 1;
	}

	std::error_code ec;
	fs::remove_all( kWorkDir, ec );
	fs::create_directories( fs::path( kWorkDir ) / "roundtrip" );
	Results r;
	r.log.open( kLog, std::ios::binary );
	r.xfail = ReadXFail();
	// The tracked game data, so the combos MFC fills from it (partys.xml) do too.
	SetGameDataDir( "Data/" );

	for ( const auto &cls : inventory["classes"].items )
	{
		const std::string diff = CheckInventoryClass( cls );
		r.Report( "inventory:" + cls["class"].Str(), diff.empty(), diff );
	}

	// Every MFC project in TestProjects, then every fixture project, by path.
	const std::set<std::string> exts = ProjectExtensions( inventory );
	std::vector<std::pair<std::string, fs::path>> projects;
	for ( const auto &dir : ListDirectory( r, kTestProjects ) )
		if ( dir.is_directory() )
			for ( const auto &f : ListDirectory( r, dir.path() ) )
			{
				std::string ext = f.path().extension().string();
				for ( char &c : ext ) c = char( std::tolower( (unsigned char)c ) );
				if ( f.is_regular_file() && exts.count( ext ) )
					projects.emplace_back( "TestProjects/" + dir.path().filename().string() + "/" + f.path().filename().string(), f.path() );
			}
	for ( const auto &f : inventory["frames"].items )
	{
		const std::string ext = f["extension"].Str();
		const fs::path p = fs::path( kFixtures ) / ext / ( "project." + ext );
		if ( fs::exists( p ) )
		{
			projects.emplace_back( "fixtures/" + ext + "/project." + ext, p );
			CheckFrameData( r, ext, p );
			CheckValueFlags( r, ext, p );
		}
		else
			r.Report( "fixture-present:" + ext, false, p.string() + " is missing" );
	}
	// The projects the shipped MFC editor made (mfc-new): each must save back byte for byte.
	for ( const auto &f : ListDirectory( r, fs::path( kFixtures ) / "mfc-new" ) )
		if ( f.is_regular_file() && exts.count( f.path().extension().string() ) )
			projects.emplace_back( "mfc-new/" + f.path().filename().string(), f.path() );
	std::sort( projects.begin(), projects.end() );
	for ( const auto &p : projects )
		RoundTrip( r, p.first, p.second );

	// MFC's own projects: every type and flag pair in them is the rule's.
	for ( const auto &e : ListDirectory( r, fs::path( kFixtures ) / "mfc-new" ) )
		if ( e.path().extension().string().size() > 1 && e.path().extension() != ".md" )
			CheckValueFlags( r, e.path().extension().string().substr( 1 ), e.path(), "mfc-new:" );
	CheckEncodedFlags( r );

	for ( const auto &cls : inventory["classes"].items )
	{
		const std::string tn = cls["type_name"].Str();
		if ( tn.size() > 10 && tn.compare( tn.size() - 10, 10, "_ROOT_ITEM" ) == 0 ) continue;
		const std::string diff = CheckInsert( inventory, cls );
		r.Report( "insert:" + cls["class"].Str(), diff.empty(), diff );
	}

	int stale = 0;
	for ( const auto &id : r.xfail )
		if ( !r.seen.count( id ) )
		{
			++stale;
			r.Line( "FAIL  xfail-list - '" + id + "' names no check; remove it from " + kXFail );
		}

	char summary[256];
	std::snprintf( summary, sizeof( summary ), "resource-model-fidelity: %d PASS, %d XFAIL (known gaps), %d FAIL, %d XPASS, %d stale xfail ids; log %s",
		r.pass, r.xfailed, r.fail, r.xpass, stale, kLog );
	r.Line( summary );
	return ( r.fail == 0 && r.xpass == 0 && stale == 0 ) ? 0 : 1;
}
