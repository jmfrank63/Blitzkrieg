#include "System.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace
{
NPlatform::UiHandler gErrorHandler = nullptr;
NPlatform::UiHandler gOpenHandler = nullptr;

std::string ErrorText()
{
	const char *error = SDL_GetError();
	return error != nullptr ? error : "system operation failed";
}

#if defined(_WIN32) || defined(_WIN64)
std::string QuoteWindowsArgument( const std::string &argument )
{
	std::string result = "\"";
	std::size_t slashes = 0;
	for ( const char character : argument )
	{
		if ( character == '\\' ) { ++slashes; continue; }
		if ( character == '"' ) result.append( slashes * 2 + 1, '\\' );
		else result.append( slashes, '\\' );
		slashes = 0;
		result += character;
	}
	result.append( slashes * 2, '\\' );
	result += '"';
	return result;
}
#endif
}

namespace NPlatform
{
std::string ExecutablePath()
{
	const char *basePath = SDL_GetBasePath();
	return basePath != nullptr ? basePath : std::string();
}

std::string GetEnvironment( const char *name )
{
	if ( name == nullptr ) return std::string();
#if defined(_WIN32) || defined(_WIN64)
	const DWORD length = GetEnvironmentVariableA( name, nullptr, 0 );
	if ( length == 0 ) return std::string();
	std::string value( length, '\0' );
	const DWORD written = GetEnvironmentVariableA( name, &value[0], length );
	if ( written == 0 ) return std::string();
	value.resize( written );
	return value;
#else
	const char *value = std::getenv( name );
	return value != nullptr ? value : std::string();
#endif
}

bool SetEnvironment( const char *name, const char *value )
{
	if ( name == nullptr || name[0] == 0 || value == nullptr ) return false;
#if defined(_WIN32) || defined(_WIN64)
	return SetEnvironmentVariableA( name, value ) != 0;
#else
	return setenv( name, value, 1 ) == 0;
#endif
}

bool ShowError( const char *title, const char *text )
{
	if ( gErrorHandler != nullptr ) return gErrorHandler( title, text );
	return SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, title != nullptr ? title : "Error", text != nullptr ? text : "", nullptr );
}

bool OpenUrl( const char *url )
{
	if ( gOpenHandler != nullptr ) return gOpenHandler( url, nullptr );
	return url != nullptr && SDL_OpenURL( url );
}

bool OpenFile( const char *path )
{
	if ( path == nullptr ) return false;
	std::string url( path );
	if ( url.rfind( "file://", 0 ) != 0 )
	{
		std::replace( url.begin(), url.end(), '\\', '/' );
		url = "file://" + ( !url.empty() && url[0] == '/' ? std::string() : "/" ) + url;
	}
	return OpenUrl( url.c_str() );
}

void SetUiHandlers( const UiHandler errorHandler, const UiHandler openHandler )
{
	gErrorHandler = errorHandler;
	gOpenHandler = openHandler;
}

bool RunProcess( const std::vector<std::string> &arguments, const std::string &workingDirectory, int *exitCode )
{
	if ( exitCode != nullptr ) *exitCode = -1;
	if ( arguments.empty() || arguments[0].empty() ) return false;
#if defined(_WIN32) || defined(_WIN64)
	std::string commandLine;
	for ( const std::string &argument : arguments )
	{
		if ( !commandLine.empty() ) commandLine += ' ';
		commandLine += QuoteWindowsArgument( argument );
	}
	STARTUPINFOA startupInfo{};
	startupInfo.cb = sizeof( startupInfo );
	PROCESS_INFORMATION processInfo{};
	std::vector<char> mutableCommand( commandLine.begin(), commandLine.end() );
	mutableCommand.push_back( 0 );
	const BOOL created = CreateProcessA( nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr,
		workingDirectory.empty() ? nullptr : workingDirectory.c_str(), &startupInfo, &processInfo );
	if ( !created ) return false;
	WaitForSingleObject( processInfo.hProcess, INFINITE );
	DWORD status = 1;
	GetExitCodeProcess( processInfo.hProcess, &status );
	CloseHandle( processInfo.hThread );
	CloseHandle( processInfo.hProcess );
	if ( exitCode != nullptr ) *exitCode = static_cast<int>( status );
	return true;
#else
	std::vector<char *> argv;
	argv.reserve( arguments.size() + 1 );
	for ( const std::string &argument : arguments ) argv.push_back( const_cast<char *>( argument.c_str() ) );
	argv.push_back( nullptr );
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init( &actions );
#if defined(__APPLE__) || defined(__linux__)
	if ( !workingDirectory.empty() && posix_spawn_file_actions_addchdir_np( &actions, workingDirectory.c_str() ) != 0 )
	{
		posix_spawn_file_actions_destroy( &actions );
		return false;
	}
#else
	if ( !workingDirectory.empty() ) { posix_spawn_file_actions_destroy( &actions ); return false; }
#endif
	pid_t process = 0;
	const int spawned = posix_spawn( &process, arguments[0].c_str(), &actions, nullptr, argv.data(), environ );
	posix_spawn_file_actions_destroy( &actions );
	if ( spawned != 0 ) return false;
	int status = 0;
	if ( waitpid( process, &status, 0 ) < 0 ) return false;
	if ( exitCode != nullptr ) *exitCode = WIFEXITED( status ) ? WEXITSTATUS( status ) : -1;
	return WIFEXITED( status );
#endif
}
}
