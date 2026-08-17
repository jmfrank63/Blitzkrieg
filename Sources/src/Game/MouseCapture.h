#ifndef BLITZKRIEG_MOUSECAPTURE_H
#define BLITZKRIEG_MOUSECAPTURE_H

namespace NMouseCapture
{
// Whether the window should be confining the pointer right now. The game
// scrolls the camera when the cursor sits on the outermost pixel of the screen
// (CInterfaceMission::Step), so an unconfined pointer simply slides onto the
// desktop or the next display instead of scrolling. The pointer therefore stays
// in the game for as long as the game holds focus: cmd-tab (or alt-tab) is how
// you leave, and losing focus is what hands the pointer back. There is
// deliberately no pointer gesture that releases it - every border of the window
// is working surface, so any such gesture is one the player crosses by accident
// while scrolling the map.
struct SInputs
{
	bool bWindowFocused;					// the window holds keyboard focus
	bool bPointerOverWindow;			// the system pointer is over our window
	bool bReleaseRequested;				// an explicit release (ctrl+escape) still stands
	bool bGrabbed;								// what the window is doing right now

	SInputs()
		: bWindowFocused( false ), bPointerOverWindow( false ), bReleaseRequested( false ),
			bGrabbed( false ) {  }
};

bool WantGrab( const SInputs &inputs );

// Where a pointer that has slipped outside a window we are supposed to own has
// to be put back to, in window coordinates. False when it is already inside and
// nothing needs moving. The platform's own confinement is not airtight - SDL
// asks AppKit for a mouseConfinementRect, and the pointer was still reaching the
// desktop at the bottom corners - so the pointer is checked and pulled back in
// every pump, which closes the path on every platform at once.
bool ClampIntoWindow( float fX, float fY, int nWidth, int nHeight, float *pfInsideX, float *pfInsideY );
}

#endif
