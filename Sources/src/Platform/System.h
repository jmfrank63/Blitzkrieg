#ifndef BLITZKRIEG_PLATFORM_SYSTEM_H
#define BLITZKRIEG_PLATFORM_SYSTEM_H

#include <string>
#include <vector>

namespace NPlatform
{
using UiHandler = bool (*)( const char *first, const char *second );

std::string ExecutablePath();
std::string GetEnvironment( const char *name );
bool SetEnvironment( const char *name, const char *value );
bool ShowError( const char *title, const char *text );
bool OpenUrl( const char *url );
bool OpenFile( const char *path );
void SetUiHandlers( UiHandler errorHandler, UiHandler openHandler );
bool RunProcess( const std::vector<std::string> &arguments, const std::string &workingDirectory, int *exitCode );
// The system clipboard, through SDL when its video subsystem is up (the
// game), and a process-local string otherwise (tests, headless tools) so
// the paste path stays exercisable without a display. Main thread only.
bool SetClipboardText( const char *text );
std::string GetClipboardText();
// The system mouse pointer. Cursor art handed to the window system is drawn by
// the compositor, so the pointer keeps up with the mouse at the device's own
// rate instead of moving once per presented frame, which is all a cursor
// blitted into the scene can do. Pixels are 32-bit ARGB, top row first, and
// are not retained: the window system takes its own copy. Main thread only.
// The pixels are the size the pointer should occupy on screen; the hot spot is
// in those same units. A window system that magnifies cursors (macOS scales
// every one of them by the accessibility pointer-size setting) blows that size
// up, so the caller hands over the size that comes back right and passes the
// art at its own resolution as detail - the high-DPI variant the window system
// draws from, which keeps the magnified pointer crisp.
bool SetSystemCursorImage( const void *pixels, int width, int height, int pitch, int hotX, int hotY,
	const void *detailPixels = nullptr, int detailWidth = 0, int detailHeight = 0, int detailPitch = 0 );
void ClearSystemCursorImage();
bool HasSystemCursorImage();
bool ShowSystemCursor( bool show );
}

#endif
