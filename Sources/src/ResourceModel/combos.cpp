#include "combos.h"

#include <fstream>
#include <iterator>

namespace NResourceModel
{

const std::vector<std::string> &aiClasses()
{
	static const std::vector<std::string> s = { "wheel", "halftrack", "track", "human" };
	return s;
}

const std::vector<std::string> &playerSides()
{
	static const std::vector<std::string> s = { "USSR", "German", "GB", "African_GB" };
	return s;
}

std::vector<std::string> readPlayerSides( const std::filesystem::path &partysXml )
{
	std::vector<std::string> out;
	std::ifstream in( partysXml, std::ios::binary );
	if ( !in )
		return out;
	std::string text( ( std::istreambuf_iterator<char>( in ) ), std::istreambuf_iterator<char>() );
	const std::string open = "<PartyName>", close = "</PartyName>";
	for ( std::size_t p = text.find( open ); p != std::string::npos; p = text.find( open, p ) )
	{
		p += open.size();
		std::size_t e = text.find( close, p );
		if ( e == std::string::npos )
			break;
		out.push_back( text.substr( p, e - p ) );
		p = e;
	}
	return out;
}

}
