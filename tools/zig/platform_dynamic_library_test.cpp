#include "Platform/DynamicLibrary.h"
#include "PlatformABI/PlatformClient.h"

#include <utility>

using TestValue = int (*)();

int main( int argc, char **argv )
{
	if ( argc != 2 ) return 1;
	NPlatform::DynamicLibrary original( argv[1] );
	if ( !original.IsLoaded() )
		return 2;
	if ( original.GetFunction( "missing_platform_symbol" ) != nullptr ) return 3;
	if ( original.GetError()[0] == 0 ) return 4;
	NPlatform::DynamicLibrary moved( std::move( original ) );
	if ( original.IsLoaded() || !moved.IsLoaded() ) return 5;
	TestValue value = reinterpret_cast<TestValue>( moved.GetFunction( "bk_platform_test_value" ) );
	if ( value == nullptr || value() != 42 ) return 6;
	moved.Unload();
	moved.Unload();
	if ( moved.IsLoaded() ) return 7;
	if ( !BkPlatform::Client::IsAttached() ) return 8;
	BkPlatformCreateInfo info = {};
	info.struct_size = sizeof( info );
	info.requested_abi_version = BK_PLATFORM_ABI_VERSION;
	if ( BkPlatform::Client::Create( info ) != BK_PLATFORM_OK ) return 9;
	const BkPlatformUtf8Span path = {sizeof( BkPlatformUtf8Span ), argv[1], static_cast<uint32_t>( std::char_traits<char>::length( argv[1] ) )};
	BkPlatformHandle raw = 0;
	if ( BkPlatform::Client::LibraryOpen( path, &raw ) != BK_PLATFORM_OK || raw == 0 ) return 10;
	const char symbolName[] = "bk_platform_test_value";
	const BkPlatformUtf8Span symbol = {sizeof( BkPlatformUtf8Span ), symbolName, static_cast<uint32_t>( sizeof( symbolName ) - 1 )};
	void *rawFunction = nullptr;
	if ( BkPlatform::Client::LibrarySymbol( raw, symbol, &rawFunction ) != BK_PLATFORM_OK || rawFunction == nullptr ) return 11;
	if ( BkPlatform::Client::LibraryClose( raw ) != BK_PLATFORM_OK ) return 12;
	if ( BkPlatform::Client::LibrarySymbol( raw, symbol, &rawFunction ) != BK_PLATFORM_ERROR_INVALID_ARGUMENT ) return 13;
	if ( BkPlatform::Client::LibraryClose( raw ) != BK_PLATFORM_ERROR_INVALID_ARGUMENT ) return 14;
	if ( BkPlatform::Client::LibraryOpen( path, &raw ) != BK_PLATFORM_OK ) return 15;
	BkPlatform::Client::Destroy();
	if ( BkPlatform::Client::LibrarySymbol( raw, symbol, &rawFunction ) != BK_PLATFORM_ERROR_INVALID_ARGUMENT ) return 16;
	if ( BkPlatform::Client::LibraryClose( raw ) != BK_PLATFORM_ERROR_INVALID_ARGUMENT ) return 17;
	return 0;
}
