#include "StdAfx.h"

#include "BkTextBridge.h"

#include <SDL3/SDL.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace NBkTextBridge
{
	static BkTextContext *g_pContext = 0;
	static std::string g_szLanguage;

	static bool LogEnabled()
	{
		static const bool bEnabled = []{ const char *psz = std::getenv( "BK_DEBUG_LOG" ); return psz != 0 && *psz != 0 && *psz != '0'; }();
		return bEnabled;
	}

	bool Requested()
	{
		static const bool bRequested = []{ const char *psz = std::getenv( "BK_TEXT_ENGINE" ); return psz != 0 && std::strcmp( psz, "ttf" ) == 0; }();
		return bRequested;
	}

	BkTextContext *Context() { return g_pContext; }
	const std::string &Language() { return g_szLanguage; }

	std::string LanguageCode( const char *pszGameLanguage )
	{
		const char *pszOverride = std::getenv( "BK_TEXT_LANG" );
		std::string sz = ( pszOverride != 0 && *pszOverride != 0 ) ? pszOverride : ( pszGameLanguage != 0 ? pszGameLanguage : "" );
		for ( size_t i = 0; i < sz.size(); ++i ) sz[i] = char( std::tolower( (unsigned char)sz[i] ) );
		if ( sz.compare( 0, 5, "lang-" ) == 0 ) sz.erase( 0, 5 );
		// The primary subtag only: de-at -> de, zh-hans -> zh.
		const size_t nDash = sz.find_first_of( "-_" );
		if ( nDash != std::string::npos ) sz.erase( nDash );
		if ( sz.empty() || sz == "default" ) return "en";
		return sz;
	}

	// The faces the folder holds, for the log line.
	static int CountFaces( const std::string &szDir )
	{
		int nCount = 0, nFiles = 0;
		char **ppszFiles = SDL_GlobDirectory( szDir.c_str(), "*", 0, &nFiles );
		if ( ppszFiles == 0 ) return 0;
		for ( int i = 0; i < nFiles; ++i )
		{
			const size_t n = std::strlen( ppszFiles[i] );
			if ( n > 4 && ( SDL_strcasecmp( ppszFiles[i] + n - 4, ".ttf" ) == 0 || SDL_strcasecmp( ppszFiles[i] + n - 4, ".otf" ) == 0 ) ) ++nCount;
		}
		SDL_free( ppszFiles );
		return nCount;
	}

	bool Create()
	{
		if ( !Requested() ) return false;
		if ( g_pContext != 0 ) return true;
		const char *pszBase = SDL_GetBasePath();
		const std::string szDir = std::string( pszBase != 0 ? pszBase : "" ) + "Fonts";
		const std::string szCode = LanguageCode( GetGlobalVar( "Text.Language", "Default" ) );
		const BkTextResult eResult = bk_text_context_create( szDir.c_str(), szCode.c_str(), &g_pContext );
		if ( eResult != BK_TEXT_OK || g_pContext == 0 )
		{
			g_pContext = 0;
			if ( LogEnabled() )
				std::fprintf( stderr, "bk_text: engine bitmap (fallback: context create failed with %d, fonts %s, lang %s)\n", int( eResult ), szDir.c_str(), szCode.c_str() );
			return false;
		}
		g_szLanguage = szCode;
		if ( LogEnabled() )
			std::fprintf( stderr, "bk_text: engine ttf, lang %s, fonts %s, %d faces\n", szCode.c_str(), szDir.c_str(), CountFaces( szDir ) );
		return true;
	}

	void Destroy()
	{
		if ( g_pContext != 0 ) bk_text_context_destroy( g_pContext );
		g_pContext = 0;
	}
}
