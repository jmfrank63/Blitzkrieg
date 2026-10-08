#include "localization.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>


namespace NResourceModel
{

namespace
{

std::string Lower( std::string s )
{
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
	return s;
}

// Linux does not fold case; find the entry of dir whose name equals `name`
// ignoring case, as the DataFile helper in tools/zig/editor_bridge_test.cpp does.
bool ReadFileCI( const std::filesystem::path &dir, const std::string &name, std::string *pOut )
{
	std::error_code ec;
	for ( std::filesystem::directory_iterator it( dir, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		if ( Lower( it->path().filename().string() ) != name )
			continue;
		std::ifstream in( it->path(), std::ios::binary );
		if ( !in )
			return false;
		pOut->assign( ( std::istreambuf_iterator<char>( in ) ), std::istreambuf_iterator<char>() );
		return true;
	}
	return false;
}

}

bool loadLocalization( const std::filesystem::path &localeDir, SLocalizationItem *pOut )
{
	*pOut = SLocalizationItem();
	pOut->hasName = ReadFileCI( localeDir, "name.txt", &pOut->name );
	pOut->hasDesc = ReadFileCI( localeDir, "desc.txt", &pOut->desc );
	pOut->hasStats = ReadFileCI( localeDir, "stats.txt", &pOut->stats );
	return pOut->hasName && pOut->hasDesc;
}

}
