// A drop shadow under UI text must darken the glyph's edge, not repeat the
// glyph. These are the colours the shipped layouts actually ask for.
#include <cstdio>

typedef unsigned int DWORD;
#include "../../Sources/src/UI/TextShadow.h"

static int nFailures = 0;

static void Check( bool bCondition, const char *pszWhat )
{
	if ( bCondition )
		return;
	std::printf( "FAIL: %s\n", pszWhat );
	++nFailures;
}

int main()
{
	// Light text over the original dark-metal screens: a black shadow is what
	// gives those letters their edge, and it has to survive.
	Check( UIShouldDrawTextShadow( 0xff000000, 0xff9aceb7 ), "pale green keeps its shadow" );
	Check( UIShouldDrawTextShadow( 0xff000000, 0xfffdf2db ), "cream keeps its shadow" );
	Check( UIShouldDrawTextShadow( 0xff000000, 0xffffbe34 ), "amber keeps its shadow" );
	Check( UIShouldDrawTextShadow( 0xff000000, 0xffffffff ), "white keeps its shadow" );

	// Black on paper: the shadow is a second copy of the glyph, which is the
	// doubled text seen on the restyled cloud screens.
	Check( !UIShouldDrawTextShadow( 0xff000000, 0xff000000 ), "black drops its shadow" );
	// The dialog captions and the disabled labels are dark browns and greys;
	// a black shadow under them smears just as badly.
	Check( !UIShouldDrawTextShadow( 0xff000000, 0xff815335 ), "brown caption drops its shadow" );
	Check( !UIShouldDrawTextShadow( 0xff000000, 0xff7a6a55 ), "grey-brown drops its shadow" );
	Check( !UIShouldDrawTextShadow( 0xff000000, 0xff841a17 ), "dark red drops its shadow" );

	// An emboss - a shadow lighter than the text - is a smear too.
	Check( !UIShouldDrawTextShadow( 0xffffffff, 0xff000000 ), "white shadow under black drops" );
	// Nothing is drawn by a transparent shadow anyway.
	Check( !UIShouldDrawTextShadow( 0x00000000, 0xffffffff ), "transparent shadow drops" );

	if ( nFailures == 0 )
		std::printf( "ui-text-shadow: PASS\n" );
	return nFailures == 0 ? 0 : 1;
}
