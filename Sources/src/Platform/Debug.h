#ifndef BLITZKRIEG_PLATFORM_DEBUG_H
#define BLITZKRIEG_PLATFORM_DEBUG_H



namespace NPlatform
{
	void DebugWrite( const char *text );
	void DebugWriteFormatV( const char *format, va_list args );
	void DebugWriteFormat( const char *format, ... );
	bool IsDebuggerAttached();
	void BreakIntoDebugger();
}

#endif
