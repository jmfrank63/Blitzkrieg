#ifndef __DATA_ONLY_STARTUP_H__
#define __DATA_ONLY_STARTUP_H__
namespace NDataOnly
{
// Wires the three StreamIO globals and registers one data storage, with no
// window and no GPU device. pszModuleRoot is the directory holding the
// StreamIO shared library (zig-out/bin when a test runs from the repo root);
// pszDataRoot is the game's Data directory, which is what storage-relative
// names like "maps\\x.bzm" are relative to. Returns false and writes the
// reason to stderr.
bool Start( const char *pszModuleRoot, const char *pszDataRoot );
// The first half of Start: the three StreamIO globals only, for a tool that
// reads and writes files by path and needs no Data storage (bk-font).
bool StartStreamIO( const char *pszModuleRoot );
// Loads the Image module from pszModuleRoot and registers its image processor,
// as the game does at start-up, so GetImageProcessor() reads and writes images
// with the engine's own codecs. Call after StartStreamIO.
bool StartImage( const char *pszModuleRoot );
// The game's teardown (Main/LoadDLLs.cpp): releases every registered
// singleton and StreamIO's own buffers, so a debug build's leak check at exit
// finds nothing. Call once, after the last CPtr to an engine object is gone.
void Stop();
}
#endif // __DATA_ONLY_STARTUP_H__
