#include "StdAfx.h"

#include "BkTextBridge.h"
#include "FontGpu.h"
#include "TextureGpu.h"

#include <SDL3/SDL.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace NBkTextBridge
{
	static BkTextContext *g_pContext = 0;
	// A plain buffer: a global std::string registers its destructor through atexit, and on MSVC that pulls the
	// static C runtime in next to the debug DLL one (duplicate _cexit, _wctype) in the Zig-linked executables.
	static char g_szLanguage[64] = "";
	static unsigned g_nSerial = 0;
	static TextureGpu *g_pAtlas = 0;
	static uint32_t g_nAtlasW = 0, g_nAtlasH = 0, g_nLoggedGeneration = 0xffffffffu, g_nFullGeneration = 0xffffffffu;
	static uint64_t g_nFrame = 0;

	static bool LogEnabled()
	{
		static const bool bEnabled = []{ const char *psz = std::getenv( "BK_DEBUG_LOG" ); return psz != 0 && *psz != 0 && *psz != '0'; }();
		return bEnabled;
	}

	bool Requested()
	{
		// bk_text is the default (M004 S05); BK_TEXT_ENGINE=bitmap is the explicit escape hatch for tests.
		static const bool bRequested = []{ const char *psz = std::getenv( "BK_TEXT_ENGINE" ); return psz == 0 || std::strcmp( psz, "bitmap" ) != 0; }();
		return bRequested;
	}

	BkTextContext *Context() { return g_pContext; }
	std::string Language() { return std::string( g_szLanguage ); }

	std::string LanguageCode( const char *pszGameLanguage )
	{
		const char *pszOverride = std::getenv( "BK_TEXT_LANG" );
		std::string sz = ( pszOverride != 0 && *pszOverride != 0 ) ? pszOverride : ( pszGameLanguage != 0 ? pszGameLanguage : "" );
		for ( size_t i = 0; i < sz.size(); ++i ) sz[i] = char( std::tolower( (unsigned char)sz[i] ) );
		if ( sz.compare( 0, 5, "lang-" ) == 0 ) sz.erase( 0, 5 );
		// The primary subtag only: de-at -> de, zh-hans -> zh.
		// A loop, not find_first_of: on MSVC that is a separately compiled vectorized routine, whose static
		// runtime pieces clash with the debug DLL runtime in the Zig-linked test executables (duplicate _cexit).
		size_t nDash = std::string::npos;
		for ( size_t i = 0; i < sz.size() && nDash == std::string::npos; ++i ) if ( sz[i] == '-' || sz[i] == '_' ) nDash = i;
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
		if ( !Requested() )
		{
			static bool bLogged = false;
			if ( !bLogged && LogEnabled() )
				std::fprintf( stderr, "bk_text: engine bitmap (BK_TEXT_ENGINE=bitmap)\n" );
			bLogged = true;
			return false;
		}
		if ( g_pContext != 0 ) return true;
		const char *pszBase = SDL_GetBasePath();
		// BK_TEXT_FONTS_DIR names the folder directly: a test runs from the build cache, not from an install.
		const char *pszFontsDir = std::getenv( "BK_TEXT_FONTS_DIR" );
		const std::string szDir = ( pszFontsDir != 0 && *pszFontsDir != 0 ) ? std::string( pszFontsDir ) : std::string( pszBase != 0 ? pszBase : "" ) + "Fonts";
		const std::string szCode = LanguageCode( GetGlobalVar( "Text.Language", "Default" ) );
		const BkTextResult eResult = bk_text_context_create( szDir.c_str(), szCode.c_str(), &g_pContext );
		if ( eResult != BK_TEXT_OK || g_pContext == 0 )
		{
			g_pContext = 0;
			if ( LogEnabled() )
				std::fprintf( stderr, "bk_text: engine bitmap (fallback: context create failed with %d, fonts %s, lang %s)\n", int( eResult ), szDir.c_str(), szCode.c_str() );
			return false;
		}
		std::snprintf( g_szLanguage, sizeof( g_szLanguage ), "%s", szCode.c_str() );
		++g_nSerial;
		if ( LogEnabled() )
			std::fprintf( stderr, "bk_text: engine ttf (%s), lang %s, fonts %s, %d faces\n", std::getenv( "BK_TEXT_ENGINE" ) != 0 ? "BK_TEXT_ENGINE=ttf" : "default", szCode.c_str(), szDir.c_str(), CountFaces( szDir ) );
		return true;
	}

	static void ReleaseAtlas()
	{
		if ( g_pAtlas != 0 ) g_pAtlas->Release();
		g_pAtlas = 0;
		g_nAtlasW = g_nAtlasH = 0;
	}

	void Destroy()
	{
		ReleaseAtlas();
		if ( g_pContext != 0 ) bk_text_context_destroy( g_pContext );
		g_pContext = 0;
		g_nLoggedGeneration = g_nFullGeneration = 0xffffffffu;
	}

	unsigned Serial() { return g_nSerial; }

	bool LanguageChanged()
	{
		return g_pContext != 0 && LanguageCode( GetGlobalVar( "Text.Language", "Default" ) ) != g_szLanguage;
	}

	bool Recreate()
	{
		Destroy();
		return Create();
	}

	uint64_t Frame() { return g_nFrame; }

	void BeginFrame()
	{
		if ( g_pContext != 0 ) (void)bk_text_atlas_begin_frame( g_pContext, ++g_nFrame );
	}

	unsigned AtlasGeneration()
	{
		uint32_t w = 0, h = 0, nGeneration = 0;
		if ( g_pContext != 0 ) (void)bk_text_atlas_info( g_pContext, &w, &h, &nGeneration );
		return nGeneration;
	}

	void ReportAtlasFull()
	{
		const unsigned nGeneration = AtlasGeneration();
		if ( nGeneration == g_nFullGeneration ) return;
		g_nFullGeneration = nGeneration;
		if ( LogEnabled() )
			std::fprintf( stderr, "bk_text: BK_TEXT_ATLAS_FULL, the glyphs of one frame do not fit the %ux%u page (generation %u); the line is not drawn\n", g_nAtlasW, g_nAtlasH, nGeneration );
	}

	IGFXTexture *Atlas( GraphicsEngineGpu *pEngine )
	{
		if ( g_pContext == 0 || pEngine == 0 ) return 0;
		uint32_t w = 0, h = 0, nGeneration = 0;
		if ( bk_text_atlas_info( g_pContext, &w, &h, &nGeneration ) != BK_TEXT_OK || w == 0 || h == 0 ) return 0;
		if ( g_pAtlas != 0 && ( g_nAtlasW != w || g_nAtlasH != h ) ) ReleaseAtlas();
		if ( g_pAtlas == 0 )
		{
			TextureGpu *pTexture = new TextureGpu( pEngine, int( w ), int( h ), 1, GFXPF_BK_R8, GFXD_STATIC );
			pTexture->AddRef();
			if ( !pTexture->IsValid() )
			{
				pTexture->Release();
				return 0;
			}
			g_pAtlas = pTexture;
			g_nAtlasW = w;
			g_nAtlasH = h;
			// A fresh page holds nothing: send everything the atlas has produced so far.
			const uint8_t *pPixels = 0;
			uint32_t nPitch = 0;
			if ( bk_text_atlas_pixels( g_pContext, &pPixels, &nPitch ) == BK_TEXT_OK && pPixels != 0 )
				(void)g_pAtlas->UploadRegion( 0, 0, int( w ), int( h ), pPixels, int( nPitch ) );
			BkTextRect rects[64];
			uint32_t nCount = 0;
			(void)bk_text_atlas_take_dirty( g_pContext, rects, 64, &nCount );
		}
		return g_pAtlas;
	}

	bool FlushAtlas( GraphicsEngineGpu *pEngine )
	{
		if ( Atlas( pEngine ) == 0 ) return false;
		BkTextRect fixed[64];
		BkTextRect *rects = fixed;
		std::vector<BkTextRect> more;
		uint32_t nCount = 0;
		BkTextResult eResult = bk_text_atlas_take_dirty( g_pContext, fixed, 64, &nCount );
		// The atlas keeps its rects when the buffer is too small and says how many it holds.
		if ( eResult == BK_TEXT_BUFFER_TOO_SMALL )
		{
			more.resize( nCount );
			rects = more.data();
			eResult = bk_text_atlas_take_dirty( g_pContext, rects, nCount, &nCount );
		}
		if ( eResult != BK_TEXT_OK ) return false;
		if ( nCount == 0 ) return true;
		uint32_t x0 = g_nAtlasW, y0 = g_nAtlasH, x1 = 0, y1 = 0;
		for ( uint32_t i = 0; i < nCount; ++i )
		{
			if ( rects[i].w == 0 || rects[i].h == 0 ) continue;
			x0 = rects[i].x < x0 ? rects[i].x : x0;
			y0 = rects[i].y < y0 ? rects[i].y : y0;
			x1 = rects[i].x + rects[i].w > x1 ? rects[i].x + rects[i].w : x1;
			y1 = rects[i].y + rects[i].h > y1 ? rects[i].y + rects[i].h : y1;
		}
		if ( x1 <= x0 || y1 <= y0 ) return true;
		if ( x1 > g_nAtlasW ) x1 = g_nAtlasW;
		if ( y1 > g_nAtlasH ) y1 = g_nAtlasH;
		const uint8_t *pPixels = 0;
		uint32_t nPitch = 0;
		if ( bk_text_atlas_pixels( g_pContext, &pPixels, &nPitch ) != BK_TEXT_OK || pPixels == 0 ) return false;
		const unsigned nGeneration = AtlasGeneration();
		if ( LogEnabled() && nGeneration != g_nLoggedGeneration )
		{
			if ( g_nLoggedGeneration != 0xffffffffu )
				std::fprintf( stderr, "bk_text: atlas generation %u -> %u, whole page %ux%u re-sent\n", g_nLoggedGeneration, nGeneration, g_nAtlasW, g_nAtlasH );
			g_nLoggedGeneration = nGeneration;
		}
		return g_pAtlas->UploadRegion( int( x0 ), int( y0 ), int( x1 - x0 ), int( y1 - y0 ), pPixels + size_t( y0 ) * nPitch + x0, int( nPitch ) );
	}
}

// Lives here, not beside FontGpu, so every binary that links the engine draws can link it.
namespace NTextPerf
{
    namespace
    {
        // One thread draws; the counters are plain on purpose, BK_PERF is a measuring aid.
        long long g_nanoseconds[BucketCount];
        unsigned long long g_calls;
        int g_depth;
        long long Now()
        {
            // SDL's clock: steady_clock pulls the STL's separately compiled time routines in on MSVC.
            return static_cast<long long>( SDL_GetTicksNS() );
        }
    }
    bool Enabled()
    {
        static const bool enabled = getenv( "BK_PERF" ) != 0;
        return enabled;
    }
    Scope::Scope( Bucket bucket ) : bucket_( bucket ), active_( Enabled() )
    {
        if ( !active_ ) return;
        ++g_calls;
        outermost_ = g_depth++ == 0;
        if ( outermost_ ) start_ = Now();
    }
    Scope::~Scope()
    {
        if ( !active_ ) return;
        --g_depth;
        if ( outermost_ ) g_nanoseconds[bucket_] += Now() - start_;
    }
    std::string TakeInterval()
    {
        char line[160];
        std::snprintf( line, sizeof( line ), "measure %.2f ms draw %.2f ms wrap %.2f ms calls %llu",
            g_nanoseconds[Measure] / 1e6, g_nanoseconds[Draw] / 1e6, g_nanoseconds[Wrap] / 1e6, g_calls );
        for ( long long &nanoseconds : g_nanoseconds ) nanoseconds = 0;
        g_calls = 0;
        return line;
    }
}
