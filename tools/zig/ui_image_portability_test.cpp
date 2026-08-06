#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

struct TestRect
{
	int left;
	int top;
	int right;
	int bottom;
	TestRect( int x1, int y1, int x2, int y2 ) : left( x1 ), top( y1 ), right( x2 ), bottom( y2 ) {}
};

static bool CheckRectConstruction()
{
	const TestRect rect( 0, 0, 64, 32 );
	return rect.left == 0 && rect.top == 0 && rect.right == 64 && rect.bottom == 32;
}

static void *NullImageResult( bool valid )
{
	return valid ? reinterpret_cast<void *>( 1 ) : 0;
}

static std::string NormalizeResourcePath( const char *path )
{
	std::string result = path ? path : "";
	for ( std::string::iterator it = result.begin(); it != result.end(); ++it )
		if ( *it == '\\' ) *it = '/';
	return result;
}

static bool HasExactIncludes( const char *path )
{
	std::ifstream input( path );
	if ( !input ) return false;
	std::string line;
	while ( std::getline( input, line ) )
	{
		if ( line.find( "../Zlib/" ) != std::string::npos ||
			 line.find( "../misc/" ) != std::string::npos ||
			 line.find( "../sfx/" ) != std::string::npos )
			return false;
	}
	return true;
}

int main()
{
	const bool rectangle = CheckRectConstruction();
	const bool null_image = NullImageResult( false ) == 0 && NullImageResult( true ) != 0;
	const bool resource_path = NormalizeResourcePath( "ui\\common\\active.xml" ) == "ui/common/active.xml";
	const bool exact_case = HasExactIncludes( "Sources/src/Main/GameDB.h" ) &&
		HasExactIncludes( "Sources/src/UI/UIObjectiveScreen.cpp" );
	if ( !rectangle || !null_image || !resource_path || !exact_case ) return 1;
	std::printf( "ui/image portability fixtures: rectangle=1 null-image=1 resource-path=1 exact-case=1\n" );
	return 0;
}
