#ifndef BLITZKRIEG_STREAMIO_PROFILE_NAMES_H
#define BLITZKRIEG_STREAMIO_PROFILE_NAMES_H

#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <filesystem>
#include <algorithm>
#include "../Platform/Paths.h"
#include "../Platform/Utf8Path.h"

// Player profile names and the profiles/<name>/ directories they own (see
// ProfilePaths.h for the active profile). Header-only because the callers
// span several dylibs; free of the engine's globals so a plain test can use it.
//
// A profile name is the name the player typed, kept as typed: the directory
// is its UTF-8 spelling and the UI decodes the directory name back, so a
// Cyrillic or CJK name is shown exactly as it was typed. The 2003 code, and this port until now, kept only
// printable ASCII - the edit box handed the name through a wchar->char
// truncation - so a Cyrillic name came out empty and the fallback "Player"
// silently switched the player into that profile and its saves. A name that
// cannot be a directory on every platform the profiles travel to (cloud sync
// carries them between Windows, macOS and Linux) is refused instead.
namespace NProfile
{
// Profile names are short - the edit box allows 12 characters - and a name is
// part of cloud sync's session file name and fills the cloud sync facade's
// 128-byte job buffer (CloudSyncFacade.cpp), so whatever a directory listing
// brings beyond this is not a profile. Bytes, not characters: a Cyrillic
// letter is two of them, a CJK one three.
const std::string::size_type MAX_NAME_BYTES = 120;

// Decodes one code point at szName[*pnPos], advancing *pnPos; a malformed or
// overlong sequence, or a surrogate, gives 0xFFFFFFFF.
inline unsigned long NextCodePoint( const std::string &szName, std::string::size_type *pnPos )
{
	const unsigned char c = szName[(*pnPos)++];
	if ( c < 0x80 )
		return c;
	int nMore = 0;
	unsigned long nCode = 0, nMin = 0;
	if ( c >= 0xc2 && c < 0xe0 ) { nCode = c & 0x1f; nMore = 1; nMin = 0x80; }
	else if ( c >= 0xe0 && c < 0xf0 ) { nCode = c & 0x0f; nMore = 2; nMin = 0x800; }
	else if ( c >= 0xf0 && c < 0xf5 ) { nCode = c & 0x07; nMore = 3; nMin = 0x10000; }
	else
		return 0xffffffff;
	for ( ; nMore > 0; --nMore )
	{
		if ( *pnPos >= szName.size() || ( (unsigned char)szName[*pnPos] & 0xc0 ) != 0x80 )
			return 0xffffffff;
		nCode = ( nCode << 6 ) | ( (unsigned char)szName[(*pnPos)++] & 0x3f );
	}
	if ( nCode < nMin || nCode > 0x10ffff || ( nCode >= 0xd800 && nCode < 0xe000 ) )
		return 0xffffffff;
	return nCode;
}

// A code point no file name may hold anywhere: the C0 and C1 controls, DEL,
// what Windows forbids, and U+FFFD, which only appears when a conversion
// already lost the real character.
inline bool IsForbiddenCodePoint( unsigned long nCode )
{
	if ( nCode < 0x20 || ( nCode >= 0x7f && nCode < 0xa0 ) || nCode == 0xfffd || nCode == 0xffffffff )
		return true;
	return nCode < 0x80 && strchr( "/\\:*?\"<>|", int( nCode ) ) != 0;
}

// CON, PRN, AUX, NUL, COM1-9 and LPT1-9 (Windows also counts the superscript
// digits 1-3, U+00B9, U+00B2, U+00B3) name devices on Windows, with or without an extension and
// whatever their case: a directory called "con" cannot be made there.
inline bool IsReservedWindowsName( const std::string &szName )
{
	std::string szBase = szName.substr( 0, szName.find( '.' ) );
	while ( !szBase.empty() && szBase[szBase.size() - 1] == ' ' )
		szBase.erase( szBase.size() - 1 );
	std::string szUpper;
	for ( std::string::size_type i = 0; i < szBase.size(); ++i )
		szUpper += ( szBase[i] >= 'a' && szBase[i] <= 'z' ) ? char( szBase[i] - 'a' + 'A' ) : szBase[i];
	if ( szUpper == "CON" || szUpper == "PRN" || szUpper == "AUX" || szUpper == "NUL" )
		return true;
	if ( szUpper.compare( 0, 3, "COM" ) != 0 && szUpper.compare( 0, 3, "LPT" ) != 0 )
		return false;
	const std::string szDigit = szUpper.substr( 3 );
	return ( szDigit.size() == 1 && szDigit[0] >= '1' && szDigit[0] <= '9' ) ||
		szDigit == "\xc2\xb9" || szDigit == "\xc2\xb2" || szDigit == "\xc2\xb3";
}

// True when szName (UTF-8) can be a profile directory on Windows, macOS and
// Linux as it is: valid UTF-8 without forbidden characters, no leading or
// trailing space, no trailing dot (Windows drops both, which also rules out
// "." and ".."), not a reserved device name and not longer than
// MAX_NAME_BYTES.
inline bool IsValidName( const std::string &szName )
{
	if ( szName.empty() || szName.size() > MAX_NAME_BYTES )
		return false;
	if ( szName[0] == ' ' || szName[szName.size() - 1] == ' ' || szName[szName.size() - 1] == '.' )
		return false;
	for ( std::string::size_type i = 0; i < szName.size(); )
	{
		if ( IsForbiddenCodePoint( NextCodePoint( szName, &i ) ) )
			return false;
	}
	return !IsReservedWindowsName( szName );
}

// The profile name for what the player typed (UTF-16 code units, as the edit
// box holds them): its UTF-8 spelling with leading and trailing spaces
// trimmed, or an empty string when that is no valid name - which the dialog
// refuses rather than picking some other profile.
template <class TWideString>
inline std::string NameFromTyped( const TWideString &szTyped )
{
	std::string szName;
	NPlatform::Utf8FromUtf16( &szName, szTyped );
	const std::string::size_type nFirst = szName.find_first_not_of( ' ' );
	if ( nFirst == std::string::npos )
		return std::string();
	szName = szName.substr( nFirst, szName.find_last_not_of( ' ' ) - nFirst + 1 );
	return IsValidName( szName ) ? szName : std::string();
}

// The name as the UI shows it: UTF-16 code units in a std::wstring, the
// engine's convention whatever the size of wchar_t.
inline std::wstring DisplayName( const std::string &szName )
{
	std::wstring szWide;
	NPlatform::Utf16FromUtf8( &szWide, szName );
	return szWide;
}

// For names that come from the command line or profiles/active.cfg rather
// than the dialog: what cannot appear in a directory name is dropped (as
// before, so every existing ASCII profile keeps its directory), and a result
// that is still no valid name falls back to "Player".
inline std::string Sanitize( const std::string &szName )
{
	std::string szResult;
	for ( std::string::size_type i = 0; i < szName.size(); )
	{
		const std::string::size_type nStart = i;
		if ( !IsForbiddenCodePoint( NextCodePoint( szName, &i ) ) )
			szResult.append( szName, nStart, i - nStart );
	}
	// Leading/trailing spaces and trailing dots are invalid on Windows and a
	// nuisance everywhere ("saves/ Mission Start Auto.sav" taught us that).
	while ( !szResult.empty() && szResult[0] == ' ' )
		szResult.erase( 0, 1 );
	while ( !szResult.empty() && (szResult[szResult.size() - 1] == ' ' || szResult[szResult.size() - 1] == '.') )
		szResult.erase( szResult.size() - 1 );
	if ( !IsValidName( szResult ) )
		szResult = "Player";
	return szResult;
}

// Profile directories live on case-insensitive filesystems too (NTFS, APFS),
// where a lower- and an upper-case spelling are one directory, so names compare without case.
// The folding covers the scripts players type names in - Latin with its
// accented letters, Greek, Cyrillic, fullwidth Latin - by plain table, no
// locale involved; letters beyond that compare exactly.
inline unsigned long FoldCase( unsigned long c )
{
	if ( c >= 'A' && c <= 'Z' ) return c + 0x20;
	if ( c < 0xc0 ) return c;
	if ( c <= 0xde ) return c == 0xd7 ? c : c + 0x20;
	if ( ( c >= 0x100 && c <= 0x137 ) || ( c >= 0x14a && c <= 0x177 ) ) return c | 1;
	if ( ( c >= 0x139 && c <= 0x148 ) || ( c >= 0x179 && c <= 0x17e ) ) return ( c & 1 ) ? c + 1 : c;
	if ( c == 0x178 ) return 0xff;
	if ( c >= 0x391 && c <= 0x3a9 && c != 0x3a2 ) return c + 0x20;
	if ( c >= 0x400 && c <= 0x40f ) return c + 0x50;
	if ( c >= 0x410 && c <= 0x42f ) return c + 0x20;
	if ( ( c >= 0x460 && c <= 0x481 ) || ( c >= 0x48a && c <= 0x4bf ) || ( c >= 0x4d0 && c <= 0x4ff ) ) return c | 1;
	if ( c >= 0xff21 && c <= 0xff3a ) return c + 0x20;
	return c;
}

inline bool NameLess( const std::string &a, const std::string &b )
{
	std::string::size_type i = 0, j = 0;
	while ( i < a.size() && j < b.size() )
	{
		const unsigned long ca = FoldCase( NextCodePoint( a, &i ) ), cb = FoldCase( NextCodePoint( b, &j ) );
		if ( ca != cb )
			return ca < cb;
	}
	return i >= a.size() && j < b.size();
}

inline bool NameEquals( const std::string &a, const std::string &b )
{
	return !NameLess( a, b ) && !NameLess( b, a );
}

// The folder that holds the profiles, as a host path with no trailing
// separator: "profiles" under the working directory, or the folder named by
// BK_PROFILES_ROOT. Tests set it so a Game run against an install never writes
// profiles, saves or active.cfg into that install (the install's profiles
// folder holds the player's own). Not a player setting.
inline std::string Dir()
{
	const char *pszRoot = std::getenv( "BK_PROFILES_ROOT" );
	if ( pszRoot == 0 || pszRoot[0] == 0 )
		return "profiles";
	std::string szRoot = pszRoot;
	while ( szRoot.size() > 1 && (szRoot[szRoot.size() - 1] == '/' || szRoot[szRoot.size() - 1] == '\\') )
		szRoot.erase( szRoot.size() - 1 );
	return szRoot;
}

// profiles/<name> as a host path, through UTF-16 on Windows.
inline std::filesystem::path Directory( const std::string &szName )
{
	return NPlatform::PathFromUtf8( Dir() + "/" + szName );
}

// The filesystem is the profile registry: a profile is any directory
// directly under profiles/ whose name is a valid profile name. A directory
// the user made by hand with any other name is neither listed nor ever
// touched. A missing profiles/ directory is an empty list, not an error.
inline std::vector<std::string> List()
{
	std::vector<std::string> names;
	std::error_code ec;
	std::filesystem::directory_iterator it( NPlatform::PathFromUtf8( Dir() ), ec );
	const std::filesystem::directory_iterator end;
	for ( ; !ec && it != end; it.increment( ec ) )
	{
		std::error_code ecEntry;
		if ( !it->is_directory( ecEntry ) || ecEntry )
			continue;
		const std::string szName = NPlatform::Utf8FromPath( it->path().filename() );
		if ( !IsValidName( szName ) )
			continue;
		names.push_back( szName );
	}
	std::sort( names.begin(), names.end(), NameLess );
	return names;
}

// Where the game keeps what it generates for a profile - random missions'
// maps and mission files (see GeneratedData.h): in the user's writable cache,
// not under profiles/, since a save can regenerate all of it from the seed it
// stores. A host path, UTF-8.
inline std::string GeneratedDirectory( const std::string &name )
{
	return NPlatform::Utf8FromPath( NPlatform::PathFromUtf8( NPlatform::Paths::CacheRoot() ) / "generated" / NPlatform::PathFromUtf8( Sanitize( name ) ) );
}

// Renames profiles/<from> to profiles/<to>. Refuses when <to> is no valid
// name or already exists as another profile - one directory cannot be two
// profiles. On failure returns false with the filesystem's message in
// *pError (which may be null).
inline bool Rename( const std::string &from, const std::string &to, std::string *pError )
{
	const std::string szFrom = Sanitize( from );
	const std::string szTo = to;
	if ( !IsValidName( szTo ) )
	{
		if ( pError != 0 )
			*pError = "\"" + szTo + "\" is not a valid profile name";
		return false;
	}
	if ( szFrom == szTo )
		return true;
	std::error_code ec;
	if ( NameEquals( szFrom, szTo ) )
	{
		// Only the case differs, and the filesystem considers that the same
		// directory: a direct rename is a silent no-op on APFS. Hopping
		// through a temporary name forces a real change both times.
		const std::filesystem::path temp = NPlatform::PathFromUtf8( Dir() + "/" + szTo + ".tmp-rename" );
		std::filesystem::rename( Directory( szFrom ), temp, ec );
		if ( !ec )
			std::filesystem::rename( temp, Directory( szTo ), ec );
	}
	else
	{
		const std::vector<std::string> names = List();
		for ( std::vector<std::string>::size_type i = 0; i < names.size(); ++i )
		{
			if ( NameEquals( names[i], szTo ) )
			{
				if ( pError != 0 )
					*pError = "profile \"" + szTo + "\" already exists";
				return false;
			}
		}
		std::filesystem::rename( Directory( szFrom ), Directory( szTo ), ec );
	}
	if ( ec )
	{
		if ( pError != 0 )
			*pError = ec.message();
		return false;
	}
	// The generated data follows, best effort: a save regenerates whatever
	// did not come along.
	std::error_code generatedError;
	const std::filesystem::path generatedTemp = NPlatform::PathFromUtf8( GeneratedDirectory( szTo ) + ".tmp-rename" );
	std::filesystem::rename( NPlatform::PathFromUtf8( GeneratedDirectory( szFrom ) ), generatedTemp, generatedError );
	if ( !generatedError )
		std::filesystem::rename( generatedTemp, NPlatform::PathFromUtf8( GeneratedDirectory( szTo ) ), generatedError );
	return true;
}

// Removes profiles/<name> and everything under it. On failure returns
// false with the filesystem's message in *pError (which may be null).
inline bool Delete( const std::string &name, std::string *pError )
{
	std::error_code ec;
	std::filesystem::remove_all( Directory( Sanitize( name ) ), ec );
	if ( ec )
	{
		if ( pError != 0 )
			*pError = ec.message();
		return false;
	}
	std::error_code generatedError;
	std::filesystem::remove_all( NPlatform::PathFromUtf8( GeneratedDirectory( name ) ), generatedError );
	return true;
}
}

#endif
