#include "variant.h"

#include <cstdio>
#include <cstdlib>

namespace NResourceModel
{

std::string CVariant::ToString() const
{
	char buf[64];
	switch ( GetKind() )
	{
	case VK_NULL: return "";
	case VK_INT:
		std::snprintf( buf, sizeof( buf ), "%d", AsInt() );
		return buf;
	case VK_FLOAT:
		// %.9g is lossless for IEEE-754 single precision - the MFC editor used
		// %g with the default six digits and could round the last mantissa bit,
		// but a round-trip then fails. The comparator carries the authored
		// value through its own tolerance; here the storage is exact.
		std::snprintf( buf, sizeof( buf ), "%.9g", AsFloat() );
		return buf;
	case VK_BOOL: return AsBool() ? "1" : "0";
	case VK_STR: return AsStr();
	case VK_VEC3:
	{
		const Vec3 &v = AsVec3();
		std::snprintf( buf, sizeof( buf ), "%.9g %.9g %.9g", v.x, v.y, v.z );
		return buf;
	}
	case VK_COLOR:
		std::snprintf( buf, sizeof( buf ), "%08X", (unsigned)AsColor() );
		return buf;
	case VK_COMBO:
		std::snprintf( buf, sizeof( buf ), "%d", AsCombo().index );
		return buf;
	case VK_REF: return AsRef().value;
	}
	return "";
}

CVariant CVariant::FromString( EKind kind, const std::string &text )
{
	switch ( kind )
	{
	case VK_NULL: return CVariant();
	case VK_INT: return CVariant( (int)std::strtol( text.c_str(), nullptr, 10 ) );
	case VK_FLOAT: return CVariant( std::strtof( text.c_str(), nullptr ) );
	case VK_BOOL:
		return CVariant( text == "1" || text == "true" || text == "True" );
	case VK_STR: return CVariant( text );
	case VK_VEC3:
	{
		Vec3 v;
		std::sscanf( text.c_str(), "%f %f %f", &v.x, &v.y, &v.z );
		return CVariant( v );
	}
	case VK_COLOR:
		return CVariant( (Color)std::strtoul( text.c_str(), nullptr, 16 ) );
	case VK_COMBO:
	{
		ComboIndex c;
		c.index = (int)std::strtol( text.c_str(), nullptr, 10 );
		return CVariant( c );
	}
	case VK_REF: return CVariant( Ref{ text } );
	}
	return CVariant();
}

}
