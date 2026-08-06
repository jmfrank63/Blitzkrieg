#include "GameMain.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace
{
std::string ToUtf8( const wchar_t *value )
{
	if ( value == nullptr || *value == L'\0' ) return std::string();
	const int length = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr );
	if ( length <= 1 ) return std::string();
	std::string result( static_cast<std::size_t>( length ), '\0' );
	WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, &result[0], length, nullptr, nullptr );
	result.resize( static_cast<std::size_t>( length - 1 ) );
	return result;
}
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	int argc = 0;
	LPWSTR *wide_argv = CommandLineToArgvW( GetCommandLineW(), &argc );
	if ( wide_argv == nullptr || argc <= 0 ) return 0xDEAD;
	std::vector<std::string> utf8_arguments;
	std::vector<const char *> argv;
	utf8_arguments.reserve( static_cast<std::size_t>( argc ) );
	argv.reserve( static_cast<std::size_t>( argc ) );
	for ( int index = 0; index < argc; ++index )
	{
		utf8_arguments.push_back( ToUtf8( wide_argv[index] ) );
		if ( utf8_arguments.back().empty() && wide_argv[index] != nullptr && *wide_argv[index] != L'\0' )
		{
			LocalFree( wide_argv );
			return 0xDEAD;
		}
	}
	for ( const std::string &argument : utf8_arguments ) argv.push_back( argument.c_str() );
	const int result = GameMain( NPlatform::Arguments{ argc, argv.data() } );
	LocalFree( wide_argv );
	return result;
}
#endif
