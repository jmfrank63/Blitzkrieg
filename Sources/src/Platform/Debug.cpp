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

	// A release build keeps its diagnostic commentary to itself: the engine's
	// DebugTrace/DebugWriteFormat calls (module loading, time meters, bind
	// sections, load trace, video and audio setup, cloud sync) would otherwise
	// print on every run. Set BK_DEBUG_LOG=1 to get them back. Debug builds and
	// an attached platform host are unaffected.
	bool DiagnosticStderrEnabled()
	{
#if defined(NDEBUG) || defined(_FINALRELEASE)
		static const bool enabled = []
		{
			const char *pszEnabled = std::getenv( "BK_DEBUG_LOG" );
			return pszEnabled != nullptr && pszEnabled[0] != 0 && pszEnabled[0] != '0';
		}();
		return enabled;
#else
		return true;
#endif
	}
}

namespace NPlatform
{
	namespace
	{
		void WriteDiagnostic( const char *text, uint32_t level )
		{
			if ( text == nullptr ) return;
			const std::size_t length = std::strlen( text );
			thread_local bool dispatching = false;
			if ( !dispatching && length <= UINT32_MAX && BkPlatform::Client::IsAttached() )
			{
				dispatching = true;
				const BkPlatformUtf8Span message = {sizeof( BkPlatformUtf8Span ), text, static_cast<uint32_t>( length )};
				const BkPlatformResult result = BkPlatform::Client::DiagnosticWrite( level, message );
				dispatching = false;
				if ( result == BK_PLATFORM_OK ) return;
			}
			if ( level < BK_PLATFORM_DIAGNOSTIC_LEVEL_TRACE && !DiagnosticStderrEnabled() ) return;
			std::lock_guard<std::mutex> lock( DebugMutex() );
			std::fwrite( text, 1, length, stderr );
			std::fflush( stderr );
		}

		void WriteDiagnosticFormatV( const char *format, va_list args, uint32_t level )
		{
			if ( format == nullptr ) return;
			char buffer[2048];
			va_list copy;
			va_copy( copy, args );
			const int length = std::vsnprintf( buffer, sizeof( buffer ), format, copy );
			va_end( copy );
			if ( length < 0 ) return;
			buffer[sizeof( buffer ) - 1] = 0;
			WriteDiagnostic( buffer, level );
		}
	}

	bool IsDiagnosticStderrEnabled()
	{
		return DiagnosticStderrEnabled();
	}

	void DebugWrite( const char *text )
	{
		WriteDiagnostic( text, BK_PLATFORM_DIAGNOSTIC_LEVEL_DEFAULT );
	}

	void DebugWriteFormatV( const char *format, va_list args )
	{
		WriteDiagnosticFormatV( format, args, BK_PLATFORM_DIAGNOSTIC_LEVEL_DEFAULT );
	}

	void DebugWriteFormat( const char *format, ... )
	{
		va_list args;
		va_start( args, format );
		DebugWriteFormatV( format, args );
		va_end( args );
	}

	void TraceWrite( const char *text )
	{
		WriteDiagnostic( text, BK_PLATFORM_DIAGNOSTIC_LEVEL_TRACE );
	}

	void TraceWriteFormatV( const char *format, va_list args )
	{
		WriteDiagnosticFormatV( format, args, BK_PLATFORM_DIAGNOSTIC_LEVEL_TRACE );
	}

	void TraceWriteFormat( const char *format, ... )
	{
		va_list args;
		va_start( args, format );
		TraceWriteFormatV( format, args );
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
