#ifndef __UI_TEXT_SHADOW_H__
#define __UI_TEXT_SHADOW_H__

// A layout can ask for its text to be drawn twice: once in the shadow colour,
// offset by ShadowShift, and once in the text colour on top. That reads as a
// drop shadow only while the text is light enough to stand clear of it. Where
// the text is itself dark - the paper-styled screens draw black on cream, and
// the screens a mod restyles inherit the shadow the original dark-metal layout
// asked for - the two draws differ by a pixel or two and nothing else, so the
// glyph simply appears doubled. Requiring the text to be at least half the
// luminance range above the shadow keeps every light-on-dark shadow the game
// was designed with and drops the ones that only smear.

inline int UITextLuminance( DWORD dwColor )
{
	const int nR = ( dwColor >> 16 ) & 0xff;
	const int nG = ( dwColor >> 8 ) & 0xff;
	const int nB = dwColor & 0xff;
	return ( 299 * nR + 587 * nG + 114 * nB ) / 1000;
}

const int UI_TEXT_SHADOW_MIN_CONTRAST = 128;

inline bool UIShouldDrawTextShadow( DWORD dwShadowColor, DWORD dwTextColor )
{
	if ( ( dwShadowColor & 0xff000000 ) == 0 )
		return false;
	return UITextLuminance( dwTextColor ) - UITextLuminance( dwShadowColor ) >= UI_TEXT_SHADOW_MIN_CONTRAST;
}

#endif // __UI_TEXT_SHADOW_H__
