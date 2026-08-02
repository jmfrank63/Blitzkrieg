#include "Platform/DynamicLibrary.h"

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
	return 0;
}
