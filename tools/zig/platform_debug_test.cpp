#include "Platform/Debug.h"
#include "PlatformABI/PlatformClient.h"

#include <cstdio>
#include <cstring>

namespace {
uint32_t callbackCount = 0;
BkPlatformResult nestedResult = BK_PLATFORM_OK;
bool throwCallback = false;

void BK_PLATFORM_CALL captureLog(void *, uint32_t, BkPlatformUtf8Span message)
{
	if ( message.data == nullptr || message.length == 0 ) return;
	++callbackCount;
	if ( throwCallback ) throw 1;
	const char nestedText[] = "nested diagnostic";
	const BkPlatformUtf8Span nested = {sizeof( BkPlatformUtf8Span ), nestedText, static_cast<uint32_t>( sizeof( nestedText ) - 1 )};
	nestedResult = BkPlatform::Client::DiagnosticWrite( 1, nested );
	BkPlatform::Client::Destroy();
}
}

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
	if ( !BkPlatform::Client::Attach() ) return 1;
	BkPlatformCreateInfo info = {};
	info.struct_size = sizeof( info );
	info.requested_abi_version = BK_PLATFORM_ABI_VERSION;
	info.log = &captureLog;
	if ( BkPlatform::Client::Create( info ) != BK_PLATFORM_OK ) return 2;
	NPlatform::DebugWrite( "callback diagnostic\n" );
	if ( callbackCount != 1 || nestedResult != BK_PLATFORM_ERROR_BUSY || BkPlatform::Client::Generation() == 0 ) return 3;
	throwCallback = true;
	NPlatform::DebugWrite( "throwing callback diagnostic\n" );
	throwCallback = false;
	if ( callbackCount != 2 ) return 4;
	BkPlatform::Client::Destroy();
	std::printf( "debugger attached: %s\n", NPlatform::IsDebuggerAttached() ? "yes" : "no" );
	return 0;
}
