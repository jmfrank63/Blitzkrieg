#include "Debug.h"
#include "../PlatformABI/PlatformClient.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>

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
		const std::size_t length = std::strlen( text );
		thread_local bool dispatching = false;
		if ( !dispatching && length <= UINT32_MAX && BkPlatform::Client::IsAttached() )
		{
			dispatching = true;
			const BkPlatformUtf8Span message = {sizeof( BkPlatformUtf8Span ), text, static_cast<uint32_t>( length )};
			const BkPlatformResult result = BkPlatform::Client::DiagnosticWrite( 1, message );
			dispatching = false;
			if ( result == BK_PLATFORM_OK ) return;
		}
		std::lock_guard<std::mutex> lock( DebugMutex() );
		std::fwrite( text, 1, length, stderr );
		std::fflush( stderr );
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
		return BkPlatform::Client::IsAttached() && BkPlatform::Client::IsDebuggerAttached();
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
