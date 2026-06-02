#include "StdAfx.h"

#include "BuildVersion.h"
const bool HasExtension( const std::string &szFileName, const char *pszExtension )
{
	const int nPos = szFileName.rfind( '.' );
	return ( nPos != std::string::npos ) && ( szFileName.compare(nPos, std::string::npos, pszExtension) == 0 );
}
int PrintUsage()
{
	printf( "Build Increment Utility\n" );
	printf( "Written by Yuri V. Blazhevich\n" );
	printf( "(C) Nival Interactive, 2002\n" );
	printf( "Usage:\n" );
	printf( " BuildVersion.exe <WorkSpaceFileName>.dsw <MainProjectName> [<IniFileName>.ini]\n" );
	return 0xDEAD;
}
int main( int argc, char *argv[] )
{
	if ( argc < 3 ) 
		return PrintUsage();
	std::string szWorkSpaceFileName, szConfigFileName = ".\\BuildVersion.ini";
	std::vector<std::string> szProjectNames;
	for ( int i = 1; i < argc; ++i )
	{
		std::string szString = argv[i];
		NStr::ToLower( szString );
		if ( HasExtension(szString, ".dsw") ) 
			szWorkSpaceFileName = argv[i];
		else if ( HasExtension(szString, ".ini") ) 
		{
			const DWORD BUFFER_SIZE = 1024;
			char buffer[BUFFER_SIZE];
			char *pszBufferFileName = 0;
			GetFullPathName( argv[i], 1024, buffer, &pszBufferFileName );
			szConfigFileName = buffer;
		}
		else
			szProjectNames.push_back( argv[i] );
	}
	if ( szWorkSpaceFileName.empty() || szProjectNames.empty() || szConfigFileName.empty() ) 
		return PrintUsage();
	CBuildVersion build( szConfigFileName.c_str() );
	build.UpdateVersion( szWorkSpaceFileName.c_str(), szProjectNames.back().c_str() );
	for ( std::vector<std::string>::const_iterator it = szProjectNames.begin(); it != szProjectNames.end(); ++it )
		build.MakeBuild( it->c_str() );
	return 0;
}
