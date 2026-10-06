#pragma once
// The <value> element MFC's CVariant::operator&( IDataTree & ) writes
// (Sources/src/editor/COI/Variant.cpp): attributes type, flag, float_value,
// int_value, then a string_value element. The MFC writer dumps every slot of
// the variant, so a value usually carries stale bytes in the slots its type
// does not use (float_value="3.38437e-037" next to a string). The port keeps
// the authored type in CVariant and the element as read in SProp::mfcValue,
// so an unedited value is written back with those stale slots intact.
//
// The current MFC source also writes int64low/int64high, which neither
// shipped project carries (an older writer). A value read from a file keeps
// the attribute set it was read with (int64low/int64high are added only when
// the value becomes a VT_INT64, where they hold the data); a new value gets
// every slot the current writer emits.

#include <string>

#include "variant.h"
#include "xml.h"

namespace NResourceModel
{

// MFC's CVariant::EVarialeType, the number in the type attribute.
enum EMfcVariantType
{
	MFC_VT_NULL  = 0,
	MFC_VT_INT   = 1,
	MFC_VT_FLOAT = 2,
	MFC_VT_STR   = 4,
	MFC_VT_BOOL  = 8,
	MFC_VT_INT64 = 16,
	MFC_VT_INT32 = 32
};

// A double as CDataTreeXML::DataChunk( double ) writes it: NStr::Format( "%lg" )
// on the MSVC runtime, which pads the exponent to three digits (1e-005).
std::string MfcFloat( double f );
// An int as DataChunk( int ) writes it: "%d".
std::string MfcInt( int n );

// Reads a <value> element into the authored type its type attribute names.
CVariant DecodeMfcValue( const NResourceXml::Node &value );

// Writes v as a <value> element. With a stored element the write starts from
// it, as MFC's CVariant::SetNewValue keeps the other slots; without one the
// slots are the zeros CVariant's constructors set. bReadAsInt is true for an
// item whose MFC code reads its bools through CVariant::operator bool before the
// save (the Road editor's common properties): that adds VT_INT to the flags, so
// MFC saves such a bool with flag 9 and, unlike flag 8, reads it back as set.
void EncodeMfcValue( const CVariant &v, const NResourceXml::Node *pStored, NResourceXml::Node &out, bool bReadAsInt = false );

// Small helpers shared by the item (de)serialisers.
const NResourceXml::Node *FindElement( const NResourceXml::Node &parent, const std::string &name );
const std::string *FindAttr( const NResourceXml::Node &node, const std::string &name );
void SetAttr( NResourceXml::Node &node, const std::string &name, const std::string &value );
// Text of a string chunk (an element whose children are text), as MFC's
// AddStringData reads it.
std::string ElementText( const NResourceXml::Node &node );
NResourceXml::Node StringElement( const std::string &name, const std::string &text );
// A CVec3 as DTHelper.h writes it: an element with x, y and z attributes.
NResourceXml::Node Vec3Element( const std::string &name, const Vec3 &v );
void ReadVec3( const NResourceXml::Node &node, Vec3 &v );
// A scalar attribute; the value is left as it is when the attribute is absent,
// as CTreeAccessor::Add leaves a member the file does not carry.
void ReadFloat( const NResourceXml::Node &node, const char *name, float &f );
void ReadInt( const NResourceXml::Node &node, const char *name, int &n );

}
