#ifndef BLITZKRIEG_PLATFORM_UTF8_PATH_H
#define BLITZKRIEG_PLATFORM_UTF8_PATH_H

#include <cstdio>
#include <filesystem>
#include <string>

// LegacyText.h takes WORD from the platform headers on Windows, as FileUtils.h does.
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
#include "LegacyText.h"

// Every 8-bit path of the engine is UTF-8 (see LegacyText.h). On Linux and
// macOS that is what the file APIs take. On Windows the narrow APIs - fopen,
// std::filesystem::path from a std::string, path::string(), CreateFileA -
// read the bytes in the ANSI code page instead, so a Cyrillic profile, mod or
// save name became mojibake or "?" there, and path::string() even throws for
// a name the code page cannot spell. These helpers go through UTF-16 on
// Windows, the same conversion LegacyText does for the UI. The Game's
// manifest also sets the process code page to UTF-8 (Game.manifest), which
// covers the narrow calls that do not come through here.
namespace NPlatform
{

inline std::filesystem::path PathFromUtf8( const std::string &szPath )
{
#if defined(_WIN32) || defined(_WIN64)
	std::wstring szWide;
	Utf16FromUtf8( &szWide, szPath );
	return std::filesystem::path( szWide );
#else
	return std::filesystem::path( szPath );
#endif
}

inline std::string Utf8FromPath( const std::filesystem::path &path )
{
#if defined(_WIN32) || defined(_WIN64)
	std::string szResult;
	Utf8FromUtf16( &szResult, path.native() );
	return szResult;
#else
	return path.string();
#endif
}

// fopen for a UTF-8 path; pszMode is ASCII ("rb", "wb+", ...).
inline std::FILE *OpenFileUtf8( const char *pszPath, const char *pszMode )
{
#if defined(_WIN32) || defined(_WIN64)
	std::wstring szWidePath, szWideMode;
	Utf16FromUtf8( &szWidePath, std::string( pszPath ) );
	Utf16FromUtf8( &szWideMode, std::string( pszMode ) );
	return _wfopen( szWidePath.c_str(), szWideMode.c_str() );
#else
	return std::fopen( pszPath, pszMode );
#endif
}

}

#endif
