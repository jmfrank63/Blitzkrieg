// The shaper as the language-mods step calls it from Zig: one CBkClusterTable
// and the face that shapes into it, behind a plain C ABI. The step encodes
// every exported text through the same table and writes it out for bk-font
// (--clusters), so the texts and the generated font agree on every PUA slot.
#include <cstring>
#include <string>

#include "../../Sources/src/FontAtlas/Shaper.h"

struct SLmShaper
{
	std::string font;
	std::string script, language;
	CBkClusterTable table;
};

extern "C"
{

SLmShaper *LmShaperCreate( const unsigned char *pFont, size_t nFontSize, const char *szScript, const char *szLanguage )
{
	SLmShaper *pShaper = new SLmShaper;
	pShaper->font.assign( (const char *)pFont, nFontSize );
	pShaper->script = szScript;
	pShaper->language = szLanguage;
	return pShaper;
}

void LmShaperDestroy( SLmShaper *pShaper )
{
	delete pShaper;
}

// Rewrites pText[0, nLen) into pOut (room for nLen units) and returns the
// units written, or -1 with szError filled. The result is never longer than
// the text: a cluster becomes one character.
long LmShaperEncode( SLmShaper *pShaper, const unsigned short *pText, size_t nLen, unsigned short *pOut, char *szError, size_t nErrorSize )
{
	if ( nLen == 0 )
		return 0;
	SBkShaped *pShaped = BkShapeClusters( (const unsigned char *)pShaper->font.data(), pShaper->font.size(), pText, nLen,
		pShaper->script.c_str(), pShaper->language.c_str(), szError, nErrorSize );
	if ( pShaped == 0 )
		return -1;
	std::u16string result;
	std::string szWhy;
	const bool bOk = pShaper->table.Encode( pText, *pShaped, &result, &szWhy );
	BkShapeFree( pShaped );
	if ( !bOk )
	{
		if ( szError && nErrorSize > 0 )
		{
			strncpy( szError, szWhy.c_str(), nErrorSize - 1 );
			szError[nErrorSize - 1] = 0;
		}
		return -1;
	}
	memcpy( pOut, result.data(), result.size() * sizeof( unsigned short ) );
	return (long)result.size();
}

size_t LmShaperClusters( const SLmShaper *pShaper )
{
	return pShaper->table.Size();
}

// The table as bk-font --clusters reads it. Returns its size; copies it when
// it fits nCapacity.
size_t LmShaperSerialize( const SLmShaper *pShaper, char *pOut, size_t nCapacity )
{
	const std::string sz = pShaper->table.Serialize();
	if ( sz.size() <= nCapacity )
		memcpy( pOut, sz.data(), sz.size() );
	return sz.size();
}

}
