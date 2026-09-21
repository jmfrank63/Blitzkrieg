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
}
#endif // __DATA_ONLY_STARTUP_H__
