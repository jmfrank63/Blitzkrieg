#include "StdAfx.h"

#include <conio.h>
namespace NMain
{
	bool CheckBetaKey();
	DWORD MakeCheckSum( const std::string &szUserName, const std::string &szExpiryDate );
};
std::string GetLine()
{
	std::string szName;
	szName.reserve( 100 );
	while ( 1 ) 
	{
		const BYTE chr = getchar();
		if ( chr == 10 || chr == 13 ) 
			return szName;
		szName += chr;
	}
	return szName;
}
const char *pszDate = "30.5.2003";
int main()
{
	printf( "Blitzkrieg beta key generation utility\n" );
	printf( "(C) Nival Interactive, 2002\n" );
	printf( "Please, enter user name (e.g. John Smith): " );
	const std::string szUserName = GetLine();
	const DWORD dwCheckSum = NMain::MakeCheckSum( szUserName.c_str(), pszDate );
	FILE *file = fopen( "beta.key", "wt" );
	if ( file == 0 ) 
	{
		printf( "ERROR: can't create file 'beta.key'\n" );
		return 0xDEAD;
	}
	fprintf( file, "%s\n", szUserName.c_str() );
	fprintf( file, "%s\n", pszDate );
	fprintf( file, "0x%.8x", dwCheckSum );

	fclose( file );
	return 0;
}
