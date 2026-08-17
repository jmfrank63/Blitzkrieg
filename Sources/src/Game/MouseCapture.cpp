#include "MouseCapture.h"

namespace NMouseCapture
{
bool WantGrab( const SInputs &inputs )
{
	// Another window owns the input: never fight it for the pointer. This is the
	// whole release path - cmd-tab, alt-tab, or clicking another window drops
	// focus, and the pointer comes back the moment focus does.
	if ( !inputs.bWindowFocused )
		return false;
	if ( inputs.bReleaseRequested )
		return false;
	// Already confining: the pointer is inside by construction, so keep going
	// rather than re-deciding from a pointer-focus flag the platform may only
	// refresh on motion.
	if ( inputs.bGrabbed )
		return true;
	// Taking the pointer only happens while it is already over the window, or a
	// window that regains focus with the pointer parked elsewhere would snatch
	// it away from whatever the player is doing over there.
	return inputs.bPointerOverWindow;
}

bool ClampIntoWindow( float fX, float fY, int nWidth, int nHeight, float *pfInsideX, float *pfInsideY )
{
	if ( nWidth <= 0 || nHeight <= 0 || pfInsideX == 0 || pfInsideY == 0 )
		return false;
	// The outermost pixel is a legal place to be: that is exactly where the
	// camera scrolls, so clamping has to land on it rather than inside it.
	const float fMaxX = float( nWidth ) - 1.0f;
	const float fMaxY = float( nHeight ) - 1.0f;
	*pfInsideX = fX < 0.0f ? 0.0f : ( fX > fMaxX ? fMaxX : fX );
	*pfInsideY = fY < 0.0f ? 0.0f : ( fY > fMaxY ? fMaxY : fY );
	return *pfInsideX != fX || *pfInsideY != fY;
}
}
