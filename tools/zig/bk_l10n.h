// The string tables of the bk-* tools for the C++ ones (M002 S07, D062): bk_l10n_c.zig behind a C
// ABI, with tools/zig/lang/<code>.lang as the tables. The language is --lang, then BK_LANG, then the
// operating system, then English. A tool calls BkL10nInit first thing in main and uses BK_T for
// every message. What stays English in every language: the "usage: <tool> " and "<tool>: "
// prefixes, exit codes and summary lines.
#ifndef __BK_L10N_H__
#define __BK_L10N_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Results of bk_l10n_init.
#define BK_L10N_OK 0
#define BK_L10N_MISSING_VALUE 1
#define BK_L10N_UNKNOWN_LANGUAGE 2

// Chooses the language and takes --lang out of the arguments (read them with bk_l10n_args). A bad
// --lang is the result; the language is then English.
int bk_l10n_init( int argc, char **argv );
// The arguments without --lang, null-terminated; *pArgc is set to their count.
char **bk_l10n_args( int *pArgc );
// The value of the rejected --lang, "" when it had none.
const char *bk_l10n_bad_value( void );
// "en, ru, zh-Hans, ...": the codes --lang takes.
const char *bk_l10n_valid_codes( void );
// The code of the language in use.
const char *bk_l10n_language( void );
// The text of a key: the language's, else English, else the key. Never null.
const char *bk_l10n_get( const char *pszKey );
// bk_l10n_get with {0}..{9} filled from the nCount strings of ppszArgs into pBuffer (terminated).
// Returns the length the text needs without the terminator, as snprintf does.
size_t bk_l10n_format( char *pBuffer, size_t nCapacity, const char *pszKey, const char *const *ppszArgs, size_t nCount );
// A Windows console printing UTF-8 while the tool runs; a no-op elsewhere and for redirected output.
void bk_l10n_console_utf8( void );
void bk_l10n_console_restore( void );

#ifdef __cplusplus
}

#include <initializer_list>
#include <string>
#include <vector>

// The text of a literal key.
#define BK_T( key ) bk_l10n_get( key )

// The text of a key with {0}, {1}, ... filled from the arguments.
inline std::string BkFormat( const char *pszKey, std::initializer_list<std::string> args )
{
	std::vector<const char *> pointers;
	for ( std::initializer_list<std::string>::const_iterator it = args.begin(); it != args.end(); ++it )
		pointers.push_back( it->c_str() );
	const char *const *ppszArgs = pointers.empty() ? 0 : &pointers[0];
	const size_t nNeeded = bk_l10n_format( 0, 0, pszKey, ppszArgs, pointers.size() );
	std::vector<char> buffer( nNeeded + 1 );
	bk_l10n_format( &buffer[0], buffer.size(), pszKey, ppszArgs, pointers.size() );
	return std::string( &buffer[0], nNeeded );
}
#endif

#endif // __BK_L10N_H__
