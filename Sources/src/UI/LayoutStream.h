#ifndef __UI_LAYOUT_STREAM_H__
#define __UI_LAYOUT_STREAM_H__

// Opens a layout by its data name (no extension), preferring the repository's
// restyle for the mod that is loaded. Both the screens and the pieces a screen
// builds itself from - a list's column templates, for one - go through here, so
// a restyle can reach a row as well as the dialog around it.
// See the note on the definition in UIScreen.cpp.
CPtr<IDataStream> OpenLayoutStream( const std::string &szResourceName );

#endif // __UI_LAYOUT_STREAM_H__
