#ifndef __TOOL_HOST_H__
#define __TOOL_HOST_H__
// What the C++ bk-* tools (Phase 8, D-03) share on top of the data-only
// startup: reading paths off the command line and finding the engine
// libraries the tool loads at run time. bk_tool.zig's conventions (D-04) say
// what the paths mean; this is the C++ half of them.
#include <filesystem>
#include <string>

namespace NToolHost
{
// A path from the command line: / and \ both separate on every host.
std::filesystem::path InputPath( std::string szArg );
// The same path the way StreamIO's file functions take one: every separator
// a backslash (OpenFileStream splits on '\' only), on every host.
std::string EnginePath( const std::filesystem::path &path );
// The directory the running executable is in; empty if the OS will not say.
std::filesystem::path ExecutableDirectory();
// Where StreamIO and the other engine libraries are: szModules when given,
// else beside the tool (a game install), in ../lib (zig build's zig-out/bin
// and zig-out/lib), or one level up (a Tools/ folder inside the game).
std::filesystem::path ModuleRoot( const std::string &szModules );
// stdout and stderr write the bytes the tool prints on every host. The MSVC
// CRT's text mode turned each \n into \r\n on Windows, so test-bk-map's exact
// stream checks failed there (run 39093744040) while the Zig tools passed.
void BinaryStdio();
}
#endif // __TOOL_HOST_H__
