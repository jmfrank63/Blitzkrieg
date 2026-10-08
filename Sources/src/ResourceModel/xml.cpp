#include "xml.h"

#include <cctype>
#include <cstdlib>

namespace NResourceXml
{

namespace
{

struct Parser
{
	const std::string &s;
	size_t i = 0;
	std::string err;
	bool sawLayout = false;	// whitespace between elements, so the file is not MSXML's compact layout
	explicit Parser( const std::string &str ) : s( str ) {}

	bool Fail( const char *szWhat )
	{
		if ( err.empty() )
			err = std::string( szWhat ) + " at byte " + std::to_string( i );
		return false;
	}
	bool At( const char *sz ) const { return s.compare( i, std::char_traits<char>::length( sz ), sz ) == 0; }
	void SkipWs() { while ( i < s.size() && std::isspace( (unsigned char)s[i] ) ) ++i; }
	static bool IsWs( const std::string &t )
	{
		for ( char c : t )
			if ( !std::isspace( (unsigned char)c ) )
				return false;
		return true;
	}
	static bool NameChar( char c ) { return std::isalnum( (unsigned char)c ) || c == '_' || c == ':' || c == '-' || c == '.'; }

	static void AppendUtf8( std::string &out, unsigned long cp )
	{
		if ( cp < 0x80 ) out += (char)cp;
		else if ( cp < 0x800 ) { out += (char)( 0xC0 | ( cp >> 6 ) ); out += (char)( 0x80 | ( cp & 0x3F ) ); }
		else if ( cp < 0x10000 ) { out += (char)( 0xE0 | ( cp >> 12 ) ); out += (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) ); out += (char)( 0x80 | ( cp & 0x3F ) ); }
		else { out += (char)( 0xF0 | ( cp >> 18 ) ); out += (char)( 0x80 | ( ( cp >> 12 ) & 0x3F ) ); out += (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) ); out += (char)( 0x80 | ( cp & 0x3F ) ); }
	}

	bool Decode( const std::string &raw, std::string &out )
	{
		out.clear();
		for ( size_t k = 0; k < raw.size(); ++k )
		{
			if ( raw[k] != '&' ) { out += raw[k]; continue; }
			size_t semi = raw.find( ';', k );
			if ( semi == std::string::npos ) return Fail( "unterminated entity" );
			std::string ent = raw.substr( k + 1, semi - k - 1 );
			if ( ent == "amp" ) out += '&';
			else if ( ent == "lt" ) out += '<';
			else if ( ent == "gt" ) out += '>';
			else if ( ent == "quot" ) out += '"';
			else if ( ent == "apos" ) out += '\'';
			else if ( ent.size() > 1 && ent[0] == '#' )
			{
				bool hex = ent[1] == 'x';
				const char *pDigits = ent.c_str() + ( hex ? 2 : 1 );
				char *pEnd = nullptr;
				unsigned long cp = std::strtoul( pDigits, &pEnd, hex ? 16 : 10 );
				// A malformed or NUL reference is an error, not a silent zero byte.
				if ( *pDigits == 0 || *pEnd != 0 || cp == 0 || cp > 0x10FFFF ) return Fail( "bad character reference" );
				AppendUtf8( out, cp );
			}
			else return Fail( "unknown entity" );
			k = semi;
		}
		return true;
	}

	bool ParseName( std::string &name )
	{
		size_t b = i;
		while ( i < s.size() && NameChar( s[i] ) ) ++i;
		if ( i == b ) return Fail( "expected name" );
		name = s.substr( b, i - b );
		return true;
	}

	bool ParseElement( Node &el )
	{
		el.kind = Node::Element;
		++i; // '<'
		if ( !ParseName( el.name ) ) return false;
		for ( ;; )
		{
			SkipWs();
			if ( i >= s.size() ) return Fail( "unexpected end in tag" );
			if ( s[i] == '/' )
			{
				if ( !At( "/>" ) ) return Fail( "bad empty-element tag" );
				i += 2;
				return true;
			}
			if ( s[i] == '>' ) { ++i; break; }
			std::string key, raw, val;
			if ( !ParseName( key ) ) return false;
			SkipWs();
			if ( i >= s.size() || s[i] != '=' ) return Fail( "expected '='" );
			++i;
			SkipWs();
			if ( i >= s.size() || ( s[i] != '"' && s[i] != '\'' ) ) return Fail( "expected quote" );
			char q = s[i++];
			size_t e = s.find( q, i );
			if ( e == std::string::npos ) return Fail( "unterminated attribute" );
			raw = s.substr( i, e - i );
			i = e + 1;
			if ( !Decode( raw, val ) ) return false;
			el.attrs.emplace_back( key, val );
		}
		return ParseContent( el );
	}

	bool ParseContent( Node &el )
	{
		bool hasElement = false;
		for ( ;; )
		{
			if ( i >= s.size() ) return Fail( "unclosed element" );
			if ( At( "</" ) )
			{
				// MSXML writes an element holding an empty text node (an empty
				// string chunk) as <a></a>; keep that node so it is written back so.
				if ( el.children.empty() )
				{
					Node empty;
					empty.kind = Node::Text;
					el.children.push_back( std::move( empty ) );
				}
				i += 2;
				std::string close;
				if ( !ParseName( close ) ) return false;
				if ( close != el.name ) return Fail( "mismatched close tag" );
				SkipWs();
				if ( i >= s.size() || s[i] != '>' ) return Fail( "expected '>'" );
				++i;
				break;
			}
			Node child;
			if ( At( "<!--" ) )
			{
				size_t e = s.find( "-->", i + 4 );
				if ( e == std::string::npos ) return Fail( "unterminated comment" );
				child.kind = Node::Comment;
				child.text = s.substr( i + 4, e - i - 4 );
				i = e + 3;
			}
			else if ( At( "<![CDATA[" ) )
			{
				size_t e = s.find( "]]>", i + 9 );
				if ( e == std::string::npos ) return Fail( "unterminated CDATA" );
				child.kind = Node::CData;
				child.text = s.substr( i + 9, e - i - 9 );
				i = e + 3;
			}
			else if ( At( "<?" ) )
			{
				size_t e = s.find( "?>", i + 2 );
				if ( e == std::string::npos ) return Fail( "unterminated PI" );
				std::string body = s.substr( i + 2, e - i - 2 );
				size_t sp = body.find_first_of( " \t\r\n" );
				child.kind = Node::Pi;
				child.name = body.substr( 0, sp );
				size_t data = sp == std::string::npos ? std::string::npos : body.find_first_not_of( " \t\r\n", sp );
				child.text = data == std::string::npos ? "" : body.substr( data );
				i = e + 2;
			}
			else if ( s[i] == '<' )
			{
				hasElement = true;
				if ( !ParseElement( child ) ) return false;
			}
			else
			{
				size_t e = s.find( '<', i );
				if ( e == std::string::npos ) return Fail( "unclosed element" );
				std::string raw = s.substr( i, e - i ), val;
				i = e;
				if ( !Decode( raw, val ) ) return false;
				// Whitespace-only text is the whole value of a leaf element
				// (<string_value> </string_value>); it is dropped below only when a
				// sibling element shows it is layout.
				child.kind = Node::Text;
				child.text = val;
			}
			el.children.push_back( std::move( child ) );
		}
		// Text beside child elements is layout-padded by the serialiser, so it is stored trimmed.
		bool mixed = hasElement;
		if ( mixed )
		{
			std::vector<Node> kept;
			for ( Node &c : el.children )
			{
				if ( c.kind == Node::Text )
				{
					if ( IsWs( c.text ) )
					{
						sawLayout = true;
						continue; // layout whitespace is not data
					}
					size_t b = c.text.find_first_not_of( " \t\r\n" ), e = c.text.find_last_not_of( " \t\r\n" );
					c.text = c.text.substr( b, e - b + 1 );
				}
				kept.push_back( std::move( c ) );
			}
			el.children = std::move( kept );
		}
		return true;
	}
};

void Escape( std::string &out, const std::string &t, bool attr )
{
	for ( char c : t )
	{
		switch ( c )
		{
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		case '"': if ( attr ) out += "&quot;"; else out += c; break;
		// A parser normalises literal tabs and line breaks in an attribute value
		// to spaces; character references keep them.
		case '\t': if ( attr ) out += "&#9;"; else out += c; break;
		case '\n': if ( attr ) out += "&#10;"; else out += c; break;
		case '\r': if ( attr ) out += "&#13;"; else out += c; break;
		default: out += c;
		}
	}
}

void WriteNode( std::string &out, const Node &n, int depth )
{
	std::string pad( depth, '\t' );
	switch ( n.kind )
	{
	case Node::Text: out += pad; Escape( out, n.text, false ); out += "\r\n"; return;
	case Node::Comment: out += pad + "<!--" + n.text + "-->\r\n"; return;
	case Node::CData: out += pad + "<![CDATA[" + n.text + "]]>\r\n"; return;
	case Node::Pi: out += pad + "<?" + n.name + ( n.text.empty() ? "" : " " + n.text ) + "?>\r\n"; return;
	case Node::Element: break;
	}
	out += pad + "<" + n.name;
	for ( const auto &a : n.attrs )
	{
		out += " " + a.first + "=\"";
		Escape( out, a.second, true );
		out += "\"";
	}
	if ( n.children.empty() ) { out += "/>\r\n"; return; }
	// A lone text or CDATA child stays inline so the value has no layout whitespace around it.
	if ( n.children.size() == 1 && ( n.children[0].kind == Node::Text || n.children[0].kind == Node::CData ) )
	{
		out += ">";
		if ( n.children[0].kind == Node::Text ) Escape( out, n.children[0].text, false );
		else out += "<![CDATA[" + n.children[0].text + "]]>";
		out += "</" + n.name + ">\r\n";
		return;
	}
	out += ">\r\n";
	for ( const Node &c : n.children )
		WriteNode( out, c, depth + 1 );
	out += pad + "</" + n.name + ">\r\n";
}

// IXMLDOMDocument::save without indentation: no whitespace between nodes, an
// element with no child nodes as <a/>, one with only an empty text node as <a></a>.
void WriteCompact( std::string &out, const Node &n )
{
	switch ( n.kind )
	{
	case Node::Text: Escape( out, n.text, false ); return;
	case Node::Comment: out += "<!--" + n.text + "-->"; return;
	case Node::CData: out += "<![CDATA[" + n.text + "]]>"; return;
	case Node::Pi: out += "<?" + n.name + ( n.text.empty() ? "" : " " + n.text ) + "?>"; return;
	case Node::Element: break;
	}
	out += "<" + n.name;
	for ( const auto &a : n.attrs )
	{
		out += " " + a.first + "=\"";
		Escape( out, a.second, true );
		out += "\"";
	}
	if ( n.children.empty() ) { out += "/>"; return; }
	out += ">";
	for ( const Node &c : n.children )
		WriteCompact( out, c );
	out += "</" + n.name + ">";
}

}

bool Parse( const std::string &szXml, Document &doc, std::string &szError )
{
	Parser p( szXml );
	doc = Document();
	if ( szXml.compare( 0, 3, "\xEF\xBB\xBF" ) == 0 ) p.i = 3;
	p.SkipWs();
	bool haveRoot = false;
	while ( p.i < szXml.size() )
	{
		if ( p.At( "<?xml" ) && p.i + 5 < szXml.size() && ( std::isspace( (unsigned char)szXml[p.i + 5] ) || szXml[p.i + 5] == '?' ) )
		{
			size_t e = szXml.find( "?>", p.i );
			if ( e == std::string::npos ) { p.Fail( "unterminated declaration" ); break; }
			doc.hasDeclaration = true;
			doc.declaration = szXml.substr( p.i + 5, e - p.i - 5 );
			p.i = e + 2;
		}
		else if ( p.At( "<!--" ) || p.At( "<?" ) )
		{
			// Prolog comments and PIs are not part of the project data; skip them.
			size_t e = szXml.find( p.At( "<!--" ) ? "-->" : "?>", p.i );
			if ( e == std::string::npos ) { p.Fail( "unterminated prolog node" ); break; }
			p.i = e + ( p.At( "<!--" ) ? 3 : 2 );
		}
		else if ( szXml[p.i] == '<' && !haveRoot )
		{
			if ( !p.ParseElement( doc.root ) ) break;
			haveRoot = true;
		}
		else { p.Fail( "unexpected content" ); break; }
		p.SkipWs();
	}
	if ( p.err.empty() && !haveRoot ) p.Fail( "no root element" );
	doc.layout = p.sawLayout ? Document::Indented : Document::Mfc;
	szError = p.err;
	return p.err.empty();
}

std::string Serialise( const Document &doc )
{
	std::string out;
	if ( doc.hasDeclaration )
		out += "<?xml" + doc.declaration + "?>\r\n";
	if ( doc.layout == Document::Indented )
		WriteNode( out, doc.root, 0 );
	else
	{
		// MSXML ends the saved document with a line break after the root.
		WriteCompact( out, doc.root );
		out += "\r\n";
	}
	return out;
}

const Node *FindChild( const Node &parent, const std::string &szName )
{
	for ( const Node &c : parent.children )
		if ( c.kind == Node::Element && c.name == szName )
			return &c;
	return nullptr;
}

}
