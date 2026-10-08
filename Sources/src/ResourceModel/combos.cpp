#include "combos.h"

#include "editor_env.h"
#include "prop.h"

#include <fstream>
#include <iterator>

namespace NResourceModel
{

const std::vector<std::string> &aiClasses()
{
	static const std::vector<std::string> s = { "wheel", "halftrack", "track", "human" };
	return s;
}

void LoadAIClassCombo( SProp *pProp )
{
	for ( const std::string &s : aiClasses() )
		pProp->szStrings.push_back( s );
}

const std::vector<std::string> &playerSides()
{
	static const std::vector<std::string> s = { "USSR", "German", "GB", "African_GB" };
	return s;
}

void FillVectorOfSides( std::vector<std::string> &sides )
{
	std::vector<std::string> read;
	if ( !GetGameDataDir().empty() )
		read = readPlayerSides( GetGameDataDir() + "partys.xml" );
	for ( const std::string &s : read.empty() ? playerSides() : read )
		sides.push_back( s );
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
