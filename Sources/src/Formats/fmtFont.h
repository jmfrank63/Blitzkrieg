#ifndef __FONT_FORMAT_H__
#define __FONT_FORMAT_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFontFormat
{
	// texture font header
	struct SFontMetrics
	{
		// metrics
		int nHeight;												// native height of this font (in native pixels!)
		int nAscent;												// ascent (units above the base line) of characters (in native pixels!)
		int nDescent;												// descent (units below the base line) of characters (in native pixels!)
		int nInternalLeading;								// amount of leading (space) inside the bounds set by the dwHeight member
		int nExternalLeading;								// extra leading (space) that the application adds between rows
		int nAveCharWidth;									// average width of characters in the font (generally defined as the width of the letter x).
		int nMaxCharWidth;									// width of the widest character in the font
		float fSpaceWidth;									// width of the character ' ' (for the text formatting)
		// codes
		BYTE cCharSet;											// character set of the font
		WORD wDefaultChar;									// value of the character to be substituted for characters not in the font
		//
		int operator&( IStructureSaver &ss );
	};
	// complete necessary one letter description
	struct SCharDesc
	{
		float x1, y1, x2, y2;               // rect in texture's coords [0..1]
		float fA;                           // character's pre-space (A)
		float fB;														// character's width (B)
		float fC;														// character's post-space (C)
		float fWidth;                       // lone character's width (B + (C > 0 ? C : 0))
		//
		int operator&( IStructureSaver &ss );
	};
	//
	typedef std::unordered_map<WORD, SCharDesc> CCharacterMap;
	typedef std::unordered_map<DWORD, float> CKernMap;
	//
	std::string szFaceName;								// face name of the font
	SFontMetrics metrics;									// font metrics
	CCharacterMap chars;									// all chars description
	CKernMap kerns;												// available kerning pairs
	//
	int operator&( IStructureSaver &ss );
	//
	SFontFormat& operator=( const SFontFormat &format )
	{
		szFaceName = format.szFaceName;
		memcpy( &metrics, &format.metrics, sizeof(metrics) );
		chars.clear();
		for ( CCharacterMap::const_iterator it = format.chars.begin(); it != format.chars.end(); ++it )
			memcpy( &(chars[it->first]), &(it->second), sizeof(it->second) );
		for ( CKernMap::const_iterator it = format.kerns.begin(); it != format.kerns.end(); ++it )
			kerns[it->first] = it->second;
		//
		return *this;
	}
  // get character description
	const SCharDesc& GetChar( const WORD c ) const
	{
		CCharacterMap::const_iterator pos = chars.find( c );
		if ( pos == chars.end() )
		{
			pos = chars.find( metrics.wDefaultChar );
			NI_ASSERT_SLOW_TF( pos != chars.end(), NStr::Format( "Can't find character code %d and default code %d in the font (%d)", c, metrics.wDefaultChar, metrics.nHeight ), return chars.begin()->second );
		}
		return pos->second;
	}
	// get kerning pair
	float GetKern( WORD wLastChar, WORD wCurrChar ) const
	{
		CKernMap::const_iterator pos = kerns.find( (DWORD(wLastChar) << 16) | DWORD(wCurrChar) );
		return pos != kerns.end() ? pos->second : 0;
	}
	//
	int GetHeight() const { return metrics.nHeight; }
  int GetLineSpace() const { return metrics.nHeight + metrics.nExternalLeading; }
  int GetAscent() const { return metrics.nAscent; }
  int GetDescent() const { return metrics.nDescent; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FONT_FORMAT_H__
