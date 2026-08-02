#include "Debug.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

#if defined(_MSC_VER)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent( void );
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA( const char *text );
#endif

namespace
{
	std::mutex &DebugMutex()
	{
		static std::mutex mutex;
		return mutex;
	}
}

namespace NPlatform
{
	void DebugWrite( const char *text )
	{
		if ( text == nullptr ) return;
		std::lock_guard<std::mutex> lock( DebugMutex() );
		std::fputs( text, stderr );
		std::fflush( stderr );
#if defined(_MSC_VER)
		::OutputDebugStringA( text );
#endif
	}

	void DebugWriteFormatV( const char *format, va_list args )
	{
		if ( format == nullptr ) return;
		char buffer[2048];
		va_list copy;
		va_copy( copy, args );
		const int length = std::vsnprintf( buffer, sizeof( buffer ), format, copy );
		va_end( copy );
		if ( length < 0 ) return;
		buffer[sizeof( buffer ) - 1] = 0;
		DebugWrite( buffer );
	}

	void DebugWriteFormat( const char *format, ... )
	{
		va_list args;
		va_start( args, format );
		DebugWriteFormatV( format, args );
		va_end( args );
	}

	bool IsDebuggerAttached()
	{
#if defined(_MSC_VER)
		return ::IsDebuggerPresent() != 0;
#else
		return false;
#endif
	}

	void BreakIntoDebugger()
	{
#if defined(_MSC_VER)
		__debugbreak();
#else
		std::abort();
#endif
	}
}
