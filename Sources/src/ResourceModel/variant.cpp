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
	case VK_INT64:
		std::snprintf( buf, sizeof( buf ), "%lld", (long long)AsInt64() );
		return buf;
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
	case VK_INT64: return CVariant( (std::int64_t)std::strtoll( text.c_str(), nullptr, 10 ) );
	}
	return CVariant();
}

void CVariant::SetType( EKind kind )
{
	if ( kind == GetKind() )
		return;
	const EKind from = GetKind();
	auto asInt = [&]() -> std::int64_t {
		switch ( from )
		{
		case VK_INT: return AsInt();
		case VK_FLOAT: return (std::int64_t)AsFloat();
		case VK_BOOL: return AsBool() ? 1 : 0;
		case VK_STR: return std::atoi( AsStr().c_str() );
		case VK_INT64: return AsInt64();
		case VK_COLOR: return (std::int32_t)AsColor();
		case VK_COMBO: return AsCombo().index;
		case VK_REF: return std::atoi( AsRef().value.c_str() );
		default: return 0;
		}
	};
	auto asFloat = [&]() -> float {
		switch ( from )
		{
		case VK_FLOAT: return AsFloat();
		case VK_STR: return (float)std::atof( AsStr().c_str() );
		case VK_REF: return (float)std::atof( AsRef().value.c_str() );
		default: return (float)asInt();
		}
	};
	auto asStr = [&]() -> std::string {
		char buf[64];
		switch ( from )
		{
		case VK_STR: return AsStr();
		case VK_REF: return AsRef().value;
		case VK_NULL: return std::string();
		// MFC's SetType writes an int64 as _ui64toa( ..., 16 ), which
		// OptimizeInt64 reads back with MyHexStrTo64: a mask keeps its high bits.
		case VK_INT64:
			std::snprintf( buf, sizeof( buf ), "%llx", (unsigned long long)AsInt64() );
			return buf;
		case VK_FLOAT:
			std::snprintf( buf, sizeof( buf ), "%g", AsFloat() );
			return buf;
		default:
			std::snprintf( buf, sizeof( buf ), "%i", (int)asInt() );
			return buf;
		}
	};
	switch ( kind )
	{
	case VK_NULL: m_value = std::monostate(); break;
	case VK_INT: m_value = (int)asInt(); break;
	case VK_FLOAT: m_value = asFloat(); break;
	case VK_BOOL: m_value = asInt() != 0; break;
	case VK_STR: m_value = asStr(); break;
	case VK_INT64: m_value = asInt(); break;
	case VK_COLOR: m_value = (Color)asInt(); break;
	case VK_COMBO: m_value = ComboIndex{ (int)asInt() }; break;
	case VK_REF: m_value = Ref{ asStr() }; break;
	case VK_VEC3: m_value = Vec3(); break;
	}
}

}
