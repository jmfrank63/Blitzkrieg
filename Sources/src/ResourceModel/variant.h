#pragma once
// MFC-free replacement for Sources/src/editor/COI/Variant.h. The original stored
// an EVarialeType plus flag-gated optimisation caches so UI widgets could keep
// typing the same value as int, float and string in succession without losing
// precision. The port needs only the authored type: a project XML either stores
// an int, a float, a bool, a string, a vector (vec3), a packed RGBA colour, a
// combo-box index, or a reference string. The variant tag names below are the
// authored types; DT_* domen ids in prop.h decide the widget that renders them.
//
// A note on string encoding: szDisplayName in the MFC build was windows-1251
// Russian; the comparator never decodes it. The port stores every string as an
// opaque std::string of bytes and the XML serialiser escapes only &, <, > and ".

#include <cstdint>
#include <string>
#include <variant>

namespace NResourceModel
{

struct Vec3
{
	float x = 0, y = 0, z = 0;
	bool operator==( const Vec3 &o ) const { return x == o.x && y == o.y && z == o.z; }
};

// Packed 0xAARRGGBB, matching Sources/src/GFX/Color.h's SColor layout once it is
// serialised. Kept as uint32 so the XML writer can emit the hex form the MFC
// editor already writes without going through float->byte quantisation.
using Color = std::uint32_t;

// A reference-string is the identifier the referenced resource is stored under
// (eg. a weapon name for DT_WEAPON_REF). The comparator T07 adds will validate
// that the identifier exists in the appropriate list; here it is just a tagged
// string so the writer knows to serialise it under <ref> rather than <str>.
struct Ref
{
	std::string value;
	bool operator==( const Ref &o ) const { return value == o.value; }
};

// A combo-box index: the integer the user picked plus the list of strings the
// combo was populated with. The MFC build keeps the list on SProp; the port
// stores it next to the chosen index so a round-trip through Variant alone is
// lossless even when SProp is not available (eg. a FutureBlob leaf).
struct ComboIndex
{
	int index = 0;
	bool operator==( const ComboIndex &o ) const { return index == o.index; }
};

class CVariant
{
public:
	enum EKind
	{
		VK_NULL	= 0,
		VK_INT,
		VK_FLOAT,
		VK_BOOL,
		VK_STR,
		VK_VEC3,
		VK_COLOR,
		VK_COMBO,
		VK_REF
	};

	CVariant() = default;
	explicit CVariant( int v ) : m_value( v ) {}
	explicit CVariant( float v ) : m_value( v ) {}
	explicit CVariant( bool v ) : m_value( v ) {}
	explicit CVariant( const std::string &v ) : m_value( v ) {}
	explicit CVariant( std::string &&v ) : m_value( std::move( v ) ) {}
	explicit CVariant( const char *v ) : m_value( std::string( v ) ) {}
	explicit CVariant( const Vec3 &v ) : m_value( v ) {}
	explicit CVariant( Color v ) : m_value( v ) {}
	explicit CVariant( const ComboIndex &v ) : m_value( v ) {}
	explicit CVariant( const Ref &v ) : m_value( v ) {}

	EKind GetKind() const { return (EKind)m_value.index(); }
	bool IsNull() const { return m_value.index() == VK_NULL; }

	int AsInt() const { return std::get<int>( m_value ); }
	float AsFloat() const { return std::get<float>( m_value ); }
	bool AsBool() const { return std::get<bool>( m_value ); }
	const std::string &AsStr() const { return std::get<std::string>( m_value ); }
	const Vec3 &AsVec3() const { return std::get<Vec3>( m_value ); }
	Color AsColor() const { return std::get<Color>( m_value ); }
	const ComboIndex &AsCombo() const { return std::get<ComboIndex>( m_value ); }
	const Ref &AsRef() const { return std::get<Ref>( m_value ); }

	bool operator==( const CVariant &o ) const { return m_value == o.m_value; }
	bool operator!=( const CVariant &o ) const { return !( *this == o ); }

	// Round-trip through text. ToString writes the textual form the XML carries,
	// FromString reads it back given the expected EKind the SProp knows. These
	// are convenience wrappers around std::to_string / std::strtol / strtof so
	// the serialiser does not depend on a locale.
	std::string ToString() const;
	static CVariant FromString( EKind kind, const std::string &text );

private:
	// The order of the alternatives IS the EKind: std::variant::index() returns
	// the alternative index, so VK_NULL = std::monostate must come first.
	std::variant<std::monostate, int, float, bool, std::string, Vec3, Color, ComboIndex, Ref> m_value;
};

}
