#include "StdAfx.h"
#include "tool_host.h"
#include "bk_l10n.h"
#include <cstdio>
#include <cstdlib>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif !defined(_WIN32)
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace NToolHost
{
fs::path InputPath( std::string szArg )
{
#if !defined(_WIN32)
	for ( size_t i = 0; i < szArg.size(); ++i )
		if ( szArg[i] == '\\' )
			szArg[i] = '/';
#endif
	return fs::path( szArg );
}

std::string EnginePath( const fs::path &path )
{
	std::string szPath = path.string();
	for ( size_t i = 0; i < szPath.size(); ++i )
		if ( szPath[i] == '/' )
			szPath[i] = '\\';
	return szPath;
}

void BinaryStdio()
{
#if defined(_WIN32)
	_setmode( _fileno( stdout ), _O_BINARY );
	_setmode( _fileno( stderr ), _O_BINARY );
	// Translated help reaches a Windows console as UTF-8; the old code page comes back at exit.
	bk_l10n_console_utf8();
	atexit( bk_l10n_console_restore );
#endif
}

int StartLanguage( int *pArgc, char ***pppArgv, const char *pszTool, std::string ( *pfnUsage )() )
{
	const int nResult = bk_l10n_init( *pArgc, *pppArgv );
	if ( nResult != BK_L10N_OK )
	{
		if ( nResult == BK_L10N_UNKNOWN_LANGUAGE )
			fprintf( stderr, "%s: unknown language '%s'; use one of: %s\n", pszTool, bk_l10n_bad_value(), bk_l10n_valid_codes() );
		else
			fprintf( stderr, "%s: --lang needs a language code; use one of: %s\n", pszTool, bk_l10n_valid_codes() );
		fputs( pfnUsage().c_str(), stderr );
		return 2;
	}
	*pppArgv = bk_l10n_args( pArgc );
	return -1;
}

fs::path ExecutableDirectory()
{
#if defined(_WIN32)
	wchar_t szPath[4096];
	const DWORD nLength = GetModuleFileNameW( 0, szPath, 4096 );
	return nLength > 0 && nLength < 4096 ? fs::path( std::wstring( szPath, nLength ) ).parent_path() : fs::path();
#elif defined(__APPLE__)
	char szPath[4096];
	uint32_t nSize = sizeof( szPath );
	return _NSGetExecutablePath( szPath, &nSize ) == 0 ? fs::path( szPath ).parent_path() : fs::path();
#else
	char szPath[4096];
	const ssize_t nLength = readlink( "/proc/self/exe", szPath, sizeof( szPath ) - 1 );
	return nLength > 0 ? fs::path( std::string( szPath, nLength ) ).parent_path() : fs::path();
#endif
}

fs::path ModuleRoot( const std::string &szModules )
{
	if ( !szModules.empty() )
		return InputPath( szModules );
#if defined(_WIN32)
	const char szStreamIO[] = "StreamIO.dll";
#elif defined(__APPLE__)
	const char szStreamIO[] = "libStreamIO.dylib";
#else
	const char szStreamIO[] = "libStreamIO.so";
#endif
	const fs::path exe = ExecutableDirectory();
	const fs::path candidates[3] = { exe, exe.parent_path() / "lib", exe.parent_path() };
	for ( int i = 0; i < 3; ++i )
	{
		std::error_code ec;
		if ( fs::exists( candidates[i] / szStreamIO, ec ) )
			return candidates[i];
	}
	return exe;
}
}
