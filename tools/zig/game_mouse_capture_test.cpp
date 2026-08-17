#include "../../Sources/src/Game/MouseCapture.h"

#include <cstdio>

#define CHECK(condition) \
	do { \
		if ( !(condition) ) { \
			std::fprintf( stderr, "mouse capture check failed: %s\n", #condition ); \
			return 1; \
		} \
	} while ( false )

static NMouseCapture::SInputs Focused()
{
	NMouseCapture::SInputs inputs;
	inputs.bWindowFocused = true;
	inputs.bPointerOverWindow = true;
	return inputs;
}

static bool Clamped( float fX, float fY, int nWidth, int nHeight, float fWantX, float fWantY )
{
	float fInsideX = -1.0f, fInsideY = -1.0f;
	if ( !NMouseCapture::ClampIntoWindow( fX, fY, nWidth, nHeight, &fInsideX, &fInsideY ) )
		return false;
	return fInsideX == fWantX && fInsideY == fWantY;
}

int main()
{
	// The pointer is over a focused window: take it, so pushing to the edge
	// scrolls the camera instead of sliding onto the desktop.
	CHECK( NMouseCapture::WantGrab( Focused() ) );

	// Focus is the only thing that hands the pointer back. Nothing the player
	// does with the mouse itself releases it: every border of the window is
	// working surface, so any pointer gesture is one they cross by accident.
	NMouseCapture::SInputs grabbed = Focused();
	grabbed.bGrabbed = true;
	CHECK( NMouseCapture::WantGrab( grabbed ) );

	NMouseCapture::SInputs unfocused = grabbed;
	unfocused.bWindowFocused = false;
	CHECK( !NMouseCapture::WantGrab( unfocused ) );

	// ...and comes straight back when focus does.
	CHECK( NMouseCapture::WantGrab( grabbed ) );

	// Ctrl+escape is the one keyboard hatch besides cmd-tab.
	NMouseCapture::SInputs released = Focused();
	released.bReleaseRequested = true;
	CHECK( !NMouseCapture::WantGrab( released ) );

	// A grab already in force stays in force - the pointer is inside by
	// construction, so a pointer-focus flag that only refreshes on motion must
	// not drop it.
	NMouseCapture::SInputs held = Focused();
	held.bPointerOverWindow = false;
	held.bGrabbed = true;
	CHECK( NMouseCapture::WantGrab( held ) );

	// Regaining focus with the pointer parked over another window does not
	// snatch it away from whatever is happening over there.
	NMouseCapture::SInputs outside = Focused();
	outside.bPointerOverWindow = false;
	CHECK( !NMouseCapture::WantGrab( outside ) );

	// The confinement itself, against an 800x600 window. Anything outside is
	// pulled back to the nearest point in the window.
	CHECK( Clamped( -5.0f, 300.0f, 800, 600, 0.0f, 300.0f ) );
	CHECK( Clamped( 900.0f, 300.0f, 800, 600, 799.0f, 300.0f ) );
	CHECK( Clamped( 400.0f, -5.0f, 800, 600, 400.0f, 0.0f ) );
	CHECK( Clamped( 400.0f, 700.0f, 800, 600, 400.0f, 599.0f ) );
	// Both axes at once: the bottom corners are where the pointer was escaping.
	CHECK( Clamped( -5.0f, 700.0f, 800, 600, 0.0f, 599.0f ) );
	CHECK( Clamped( 900.0f, 700.0f, 800, 600, 799.0f, 599.0f ) );

	// Inside needs no move - including the outermost pixel, which is exactly
	// where the camera scrolls and so must be a legal place to sit.
	float fUnusedX = 0.0f, fUnusedY = 0.0f;
	CHECK( !NMouseCapture::ClampIntoWindow( 400.0f, 300.0f, 800, 600, &fUnusedX, &fUnusedY ) );
	CHECK( !NMouseCapture::ClampIntoWindow( 0.0f, 0.0f, 800, 600, &fUnusedX, &fUnusedY ) );
	CHECK( !NMouseCapture::ClampIntoWindow( 799.0f, 599.0f, 800, 600, &fUnusedX, &fUnusedY ) );
	// A window with no size yet has nothing to clamp against.
	CHECK( !NMouseCapture::ClampIntoWindow( 50.0f, 50.0f, 0, 0, &fUnusedX, &fUnusedY ) );

	std::puts( "mouse capture policy passed: confine while focused, only focus loss and ctrl+escape yield, pointer pulled back in on every side and corner" );
	return 0;
}
