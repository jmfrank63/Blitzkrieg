#include "mfc_value.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace NResourceModel
{

std::string MfcFloat( double f )
{
	char buf[64];
	std::snprintf( buf, sizeof( buf ), "%g", f );
	// The MSVC runtime prints at least three exponent digits; glibc and the
	// macOS libc print two.
	std::string s = buf;
	const size_t e = s.find_first_of( "eE" );
	if ( e != std::string::npos && e + 2 < s.size() )
	{
		const size_t digits = s.size() - ( e + 2 );
		if ( digits < 3 )
			s.insert( e + 2, 3 - digits, '0' );
	}
	return s;
}

std::string MfcInt( int n )
{
	char buf[32];
	std::snprintf( buf, sizeof( buf ), "%d", n );
	return buf;
}

const NResourceXml::Node *FindElement( const NResourceXml::Node &parent, const std::string &name )
{
	for ( const auto &c : parent.children )
		if ( c.kind == NResourceXml::Node::Element && c.name == name )
			return &c;
	return nullptr;
}

const std::string *FindAttr( const NResourceXml::Node &node, const std::string &name )
{
	for ( const auto &a : node.attrs )
		if ( a.first == name )
			return &a.second;
	return nullptr;
}

void SetAttr( NResourceXml::Node &node, const std::string &name, const std::string &value )
{
	for ( auto &a : node.attrs )
		if ( a.first == name )
		{
			a.second = value;
			return;
		}
	node.attrs.emplace_back( name, value );
}

std::string ElementText( const NResourceXml::Node &node )
{
	std::string out;
	for ( const auto &c : node.children )
		if ( c.kind == NResourceXml::Node::Text || c.kind == NResourceXml::Node::CData )
			out += c.text;
	return out;
}

// CDataTreeXML::StringData appends a text node even for an empty string, so
// MSXML writes <name></name>, never <name/>; the empty text child keeps that.
NResourceXml::Node StringElement( const std::string &name, const std::string &text )
{
	NResourceXml::Node n;
	n.kind = NResourceXml::Node::Element;
	n.name = name;
	NResourceXml::Node t;
	t.kind = NResourceXml::Node::Text;
	t.text = text;
	n.children.push_back( std::move( t ) );
	return n;
}

namespace
{

// CDataTreeXML::DataChunk( int ) reads with sscanf "%i" (so 0x.. and 0..
// prefixes count), DataChunk( double ) with "%lg".
int ReadInt( const NResourceXml::Node &node, const char *name )
{
	const std::string *v = FindAttr( node, name );
	return v ? (int)std::strtol( v->c_str(), nullptr, 0 ) : 0;
}

double ReadDouble( const NResourceXml::Node &node, const char *name )
{
	const std::string *v = FindAttr( node, name );
	return v ? std::strtod( v->c_str(), nullptr ) : 0.0;
}

int MfcType( CVariant::EKind kind )
{
	switch ( kind )
	{
	case CVariant::VK_INT:
	case CVariant::VK_COLOR:
	case CVariant::VK_COMBO: return MFC_VT_INT;
	case CVariant::VK_FLOAT: return MFC_VT_FLOAT;
	case CVariant::VK_STR:
	case CVariant::VK_REF:
	case CVariant::VK_VEC3: return MFC_VT_STR;
	case CVariant::VK_BOOL: return MFC_VT_BOOL;
	case CVariant::VK_INT64: return MFC_VT_INT64;
	default: return MFC_VT_NULL;
	}
}

}

CVariant DecodeMfcValue( const NResourceXml::Node &value )
{
	switch ( ReadInt( value, "type" ) )
	{
	case MFC_VT_INT:
	case MFC_VT_INT32: return CVariant( ReadInt( value, "int_value" ) );
	case MFC_VT_FLOAT: return CVariant( (float)ReadDouble( value, "float_value" ) );
	case MFC_VT_BOOL: return CVariant( ReadInt( value, "int_value" ) != 0 );
	case MFC_VT_STR:
	{
		const NResourceXml::Node *s = FindElement( value, "string_value" );
		return CVariant( s ? ElementText( *s ) : std::string() );
	}
	case MFC_VT_INT64:
	{
		const std::uint32_t low = (std::uint32_t)ReadInt( value, "int64low" );
		const std::uint32_t high = (std::uint32_t)ReadInt( value, "int64high" );
		return CVariant( (std::int64_t)( ( (std::uint64_t)high << 32 ) | low ) );
	}
	default: return CVariant();
	}
}

void EncodeMfcValue( const CVariant &v, const NResourceXml::Node *pStored, NResourceXml::Node &out, bool bReadAsInt )
{
	const bool bBoolReadAsInt = bReadAsInt && v.GetKind() == CVariant::VK_BOOL;
	// An unedited value goes back exactly as it was read, stale slots and all.
	if ( pStored && DecodeMfcValue( *pStored ) == v )
	{
		out = *pStored;
		if ( bBoolReadAsInt )
			SetAttr( out, "flag", MfcInt( MFC_VT_BOOL | MFC_VT_INT ) );
		return;
	}

	const int nType = MfcType( v.GetKind() );
	if ( pStored )
		out = *pStored;
	else
	{
		// CVariant's constructors zero the slots the type does not use and
		// set the flags to the type; the writer then emits every slot.
		out = NResourceXml::Node();
		out.kind = NResourceXml::Node::Element;
		out.name = "value";
		SetAttr( out, "type", "0" );
		SetAttr( out, "flag", "0" );
		SetAttr( out, "float_value", "0" );
		SetAttr( out, "int_value", "0" );
		out.children.push_back( StringElement( "string_value", std::string() ) );
		SetAttr( out, "int64low", "0" );
		SetAttr( out, "int64high", "0" );
	}

	// An edit through the inspector is MFC's CVariant::SetNewValue: the type
	// stays, the slot of that type gets the new value and the flags say only
	// that slot is current. The other slots keep what they held.
	SetAttr( out, "type", MfcInt( nType ) );
	// CVariant::operator bool calls OptimizeInt, which adds VT_INT to the flags. The Road
	// editor reads every bool of its common properties that way before it saves, so MFC
	// writes them as 9 (VT_BOOL | VT_INT) and reads a flag-8 one back as false, although
	// int_value is 1. Items nothing reads keep flag == type: MFC's own bridge, fence, unit
	// and mesh projects hold flag 8.
	SetAttr( out, "flag", MfcInt( bBoolReadAsInt ? nType | MFC_VT_INT : nType ) );
	auto setString = [&out]( const std::string &text ) {
		for ( auto &c : out.children )
			if ( c.kind == NResourceXml::Node::Element && c.name == "string_value" )
			{
				c = StringElement( "string_value", text );
				return;
			}
		out.children.push_back( StringElement( "string_value", text ) );
	};
	switch ( v.GetKind() )
	{
	case CVariant::VK_INT: SetAttr( out, "int_value", MfcInt( v.AsInt() ) ); break;
	case CVariant::VK_COLOR: SetAttr( out, "int_value", MfcInt( (int)v.AsColor() ) ); break;
	case CVariant::VK_COMBO: SetAttr( out, "int_value", MfcInt( v.AsCombo().index ) ); break;
	case CVariant::VK_BOOL: SetAttr( out, "int_value", v.AsBool() ? "1" : "0" ); break;
	case CVariant::VK_FLOAT: SetAttr( out, "float_value", MfcFloat( v.AsFloat() ) ); break;
	case CVariant::VK_STR: setString( v.AsStr() ); break;
	case CVariant::VK_REF: setString( v.AsRef().value ); break;
	case CVariant::VK_VEC3: setString( v.ToString() ); break;
	case CVariant::VK_INT64:
	{
		const std::uint64_t u = (std::uint64_t)v.AsInt64();
		SetAttr( out, "int64low", MfcInt( (int)(std::uint32_t)( u & 0xFFFFFFFFu ) ) );
		SetAttr( out, "int64high", MfcInt( (int)(std::uint32_t)( u >> 32 ) ) );
		break;
	}
	default: break;
	}
}

// DTHelper.h writes a CVec3 as an element with x, y and z attributes, each a
// double through "%lg".
NResourceXml::Node Vec3Element( const std::string &name, const Vec3 &v )
{
	NResourceXml::Node n;
	n.kind = NResourceXml::Node::Element;
	n.name = name;
	SetAttr( n, "x", MfcFloat( v.x ) );
	SetAttr( n, "y", MfcFloat( v.y ) );
	SetAttr( n, "z", MfcFloat( v.z ) );
	return n;
}

void ReadFloat( const NResourceXml::Node &node, const char *name, float &f )
{
	if ( const std::string *v = FindAttr( node, name ) )
		f = (float)std::strtod( v->c_str(), nullptr );
}

void ReadInt( const NResourceXml::Node &node, const char *name, int &n )
{
	if ( const std::string *v = FindAttr( node, name ) )
		n = (int)std::strtol( v->c_str(), nullptr, 0 );
}

void ReadVec3( const NResourceXml::Node &node, Vec3 &v )
{
	ReadFloat( node, "x", v.x );
	ReadFloat( node, "y", v.y );
	ReadFloat( node, "z", v.z );
}

}
