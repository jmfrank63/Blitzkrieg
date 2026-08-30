#ifndef __CLOUDJSON_H__
#define __CLOUDJSON_H__
#pragma ONCE
// A small JSON document model for the documents the cloud facade produces
// (the form, the destination list, the credentials), shared by the
// credentials dialog and the settings screen. Objects, arrays, strings,
// numbers, booleans, null; unknown escapes and malformed bytes degrade
// rather than fail. Header-only: the GameTT project list is a .vcxproj the
// build reads, and a header needs no entry there.
#include <string>
#include <vector>

// ---- a small JSON document model ------------------------------------------
// The form and destination documents nest arrays of objects, which the old
// flat key scan cannot represent - two fields both carry "name". This is a
// minimal recursive parser for exactly the documents the facade produces:
// objects, arrays, strings, numbers, booleans, null; unknown escapes and
// malformed bytes degrade rather than fail.
struct SJsonValue
{
	enum EType { T_NULL, T_BOOL, T_NUMBER, T_STRING, T_ARRAY, T_OBJECT };
	EType eType;
	bool bValue;
	std::string szValue;										// the string, or the number's text
	std::vector<std::string> keys;					// object member names, parallel to children
	std::vector<SJsonValue> children;				// object member values, or array items
	SJsonValue() : eType( T_NULL ), bValue( false ) {}
	const SJsonValue *Get( const char *pszKey ) const
	{
		for ( size_t i = 0; i < keys.size(); ++i )
			if ( keys[i] == pszKey )
				return &children[i];
		return 0;
	}
	std::string Str( const char *pszKey ) const
	{
		const SJsonValue *pValue = Get( pszKey );
		return ( pValue != 0 && pValue->eType == T_STRING ) ? pValue->szValue : std::string();
	}
	bool Bool( const char *pszKey ) const
	{
		const SJsonValue *pValue = Get( pszKey );
		return pValue != 0 && pValue->eType == T_BOOL && pValue->bValue;
	}
};
inline void JsonSkipSpace( const std::string &szDoc, size_t *pnAt )
{
	while ( *pnAt < szDoc.size() && ( szDoc[*pnAt] == ' ' || szDoc[*pnAt] == '\t' || szDoc[*pnAt] == '\n' || szDoc[*pnAt] == '\r' ) )
		++*pnAt;
}
inline bool JsonParseString( const std::string &szDoc, size_t *pnAt, std::string *pszOut )
{
	if ( *pnAt >= szDoc.size() || szDoc[*pnAt] != '"' )
		return false;
	++*pnAt;
	std::string szValue;
	while ( *pnAt < szDoc.size() && szDoc[*pnAt] != '"' )
	{
		if ( szDoc[*pnAt] == '\\' && *pnAt + 1 < szDoc.size() )
		{
			++*pnAt;
			switch ( szDoc[*pnAt] )
			{
				case 'n': szValue += '\n'; break;
				case 't': szValue += '\t'; break;
				case 'r': break;
				case 'u':
					// Rare in these documents; keep the scan aligned and
					// substitute the code point when it is Latin-1.
					if ( *pnAt + 4 < szDoc.size() )
					{
						const int nCode = (int)strtol( szDoc.substr( *pnAt + 1, 4 ).c_str(), 0, 16 );
						if ( nCode > 0 && nCode < 256 )
							szValue += char( nCode );
						*pnAt += 4;
					}
					break;
				default: szValue += szDoc[*pnAt]; break;
			}
			++*pnAt;
		}
		else
			szValue += szDoc[( *pnAt )++];
	}
	if ( *pnAt < szDoc.size() )
		++*pnAt;		// the closing quote
	*pszOut = szValue;
	return true;
}
inline bool JsonParseValue( const std::string &szDoc, size_t *pnAt, SJsonValue *pOut, int nDepth )
{
	if ( nDepth > 24 )
		return false;		// these documents nest four levels; runaway input does not
	JsonSkipSpace( szDoc, pnAt );
	if ( *pnAt >= szDoc.size() )
		return false;
	const char c = szDoc[*pnAt];
	if ( c == '"' )
	{
		pOut->eType = SJsonValue::T_STRING;
		return JsonParseString( szDoc, pnAt, &pOut->szValue );
	}
	if ( c == '{' || c == '[' )
	{
		const bool bObject = ( c == '{' );
		pOut->eType = bObject ? SJsonValue::T_OBJECT : SJsonValue::T_ARRAY;
		++*pnAt;
		JsonSkipSpace( szDoc, pnAt );
		if ( *pnAt < szDoc.size() && szDoc[*pnAt] == ( bObject ? '}' : ']' ) )
		{
			++*pnAt;
			return true;
		}
		while ( *pnAt < szDoc.size() )
		{
			if ( bObject )
			{
				std::string szKey;
				JsonSkipSpace( szDoc, pnAt );
				if ( !JsonParseString( szDoc, pnAt, &szKey ) )
					return false;
				JsonSkipSpace( szDoc, pnAt );
				if ( *pnAt >= szDoc.size() || szDoc[*pnAt] != ':' )
					return false;
				++*pnAt;
				pOut->keys.push_back( szKey );
			}
			pOut->children.push_back( SJsonValue() );
			if ( !JsonParseValue( szDoc, pnAt, &pOut->children.back(), nDepth + 1 ) )
				return false;
			JsonSkipSpace( szDoc, pnAt );
			if ( *pnAt >= szDoc.size() )
				return false;
			if ( szDoc[*pnAt] == ',' )
			{
				++*pnAt;
				continue;
			}
			if ( szDoc[*pnAt] == ( bObject ? '}' : ']' ) )
			{
				++*pnAt;
				return true;
			}
			return false;
		}
		return false;
	}
	if ( szDoc.compare( *pnAt, 4, "true" ) == 0 )
	{
		pOut->eType = SJsonValue::T_BOOL;
		pOut->bValue = true;
		*pnAt += 4;
		return true;
	}
	if ( szDoc.compare( *pnAt, 5, "false" ) == 0 )
	{
		pOut->eType = SJsonValue::T_BOOL;
		pOut->bValue = false;
		*pnAt += 5;
		return true;
	}
	if ( szDoc.compare( *pnAt, 4, "null" ) == 0 )
	{
		pOut->eType = SJsonValue::T_NULL;
		*pnAt += 4;
		return true;
	}
	pOut->eType = SJsonValue::T_NUMBER;
	while ( *pnAt < szDoc.size() && ( isdigit( (unsigned char)szDoc[*pnAt] ) || szDoc[*pnAt] == '-' || szDoc[*pnAt] == '+' || szDoc[*pnAt] == '.' || szDoc[*pnAt] == 'e' || szDoc[*pnAt] == 'E' ) )
		pOut->szValue += szDoc[( *pnAt )++];
	return !pOut->szValue.empty();
}
inline bool JsonParse( const std::string &szDoc, SJsonValue *pOut )
{
	size_t nAt = 0;
	return JsonParseValue( szDoc, &nAt, pOut, 0 );
}

// Read a facade document that follows the required-size contract: the
// return value is the length, written only when it fit; otherwise retry
// with the reported size.
template <typename TCall>
inline bool ReadSizedDocument( TCall call, std::string *pszOut )
{
	std::vector<char> buffer( 16384 );
	int nLength = call( &buffer[0], (unsigned int)buffer.size() );
	if ( nLength < 0 )
		return false;
	if ( nLength >= (int)buffer.size() )
	{
		buffer.resize( (size_t)nLength + 1 );
		nLength = call( &buffer[0], (unsigned int)buffer.size() );
		if ( nLength < 0 || nLength >= (int)buffer.size() )
			return false;
	}
	pszOut->assign( &buffer[0], (size_t)nLength );
	return true;
}
#endif // __CLOUDJSON_H__
