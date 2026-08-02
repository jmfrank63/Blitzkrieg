#include "Platform/Debug.h"

#include <cstdio>
#include <cstring>

int main()
{
	NPlatform::DebugWrite( nullptr );
	NPlatform::DebugWriteFormat( nullptr );
	NPlatform::DebugWrite( "debug newline preserved\n" );
	NPlatform::DebugWriteFormat( "assert expression: %s at %s(%d)\n", "value != 0", "platform_debug_test.cpp", 42 );
	char longText[4096];
	std::memset( longText, 'x', sizeof( longText ) - 1 );
	longText[sizeof( longText ) - 1] = 0;
	NPlatform::DebugWriteFormat( "bounded:%s\n", longText );
	std::printf( "debugger attached: %s\n", NPlatform::IsDebuggerAttached() ? "yes" : "no" );
	return 0;
}
