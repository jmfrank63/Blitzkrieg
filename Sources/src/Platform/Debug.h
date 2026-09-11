#ifndef BLITZKRIEG_PLATFORM_DEBUG_H
#define BLITZKRIEG_PLATFORM_DEBUG_H

#include <cstdarg>

namespace NPlatform
{
	void DebugWrite( const char *text );
	void DebugWriteFormatV( const char *format, va_list args );
	void DebugWriteFormat( const char *format, ... );
	// For BK_*_TRACE channels: routed exactly like DebugWrite - to an attached
	// host's log callback when there is one - but never suppressed, because the
	// user turned the channel on deliberately.
	void TraceWrite( const char *text );
	void TraceWriteFormatV( const char *format, va_list args );
	void TraceWriteFormat( const char *format, ... );
	// Whether DebugWrite's stderr fallback is active in this build/environment.
	bool IsDiagnosticStderrEnabled();
	bool IsDebuggerAttached();
	void BreakIntoDebugger();
}

#endif
