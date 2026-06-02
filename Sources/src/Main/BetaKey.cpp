#include "StdAfx.h"

#include <time.h>

#include "..\Misc\FileUtils.h"
#include "..\zlib\zlib.h"
namespace NMain
{
static const int s_nKey2Length = 20;
static BYTE s_cKey2[s_nKey2Length] = { 151, 186, 179, 161, 73, 225, 127, 233, 147, 69, 6, 46, 90, 162, 2, 30, 101, 251, 13, 48 };
DWORD MakeCheckSum( const std::string &szUserName, const std::string &szExpiryDate )
{
	std::vector<BYTE> checksum;
	checksum.reserve( szUserName.size() + szExpiryDate.size() + s_nKey2Length );
	checksum.insert( checksum.end(), szUserName.begin(), szUserName.end() );
	checksum.insert( checksum.end(), szExpiryDate.begin(), szExpiryDate.end() );
	checksum.insert( checksum.end(), s_cKey2, s_cKey2 + s_nKey2Length );
	const uLong uCheckSum = crc32( 0L, &(checksum[0]), checksum.size() );
	return uCheckSum;
}
bool CheckBetaKey()
{
#ifdef _DO_BETA_CHECK
	char buffer[1024];
	std::vector<std::string> szStrings;
	{
		GetCurrentDirectory( 1024, buffer );
		std::string szFileName = buffer;
		if ( !szFileName.empty() && (szFileName[szFileName.size() - 1] != '\\') ) 
			szFileName += "\\beta.key";
		else
			szFileName += "beta.key";

		NFile::CFile file;
		file.Open( szFileName.c_str(), NFile::CFile::modeRead );
		if ( !file.IsOpen() ) 
			return false;
		const int nReadBytes = file.Read( buffer, 1024 );
		char *pRemove = std::remove( buffer, buffer + nReadBytes, '\r' );
		*pRemove = 0;
		const std::string szReadString = buffer;
		NStr::SplitString( szReadString, szStrings, '\n' );
	}
	if ( szStrings.size() != 3 ) 
		return false;
	const std::string szUserName = szStrings[0];
	const std::string szExpiryDate = szStrings[1];
	const uLong uCode = NStr::ToInt( szStrings[2] );
#ifdef _SET_BETA_KEY_USER
	SetGlobalVar( "CopyUserName", szUserName.c_str() );
#endif // _SET_BETA_KEY_USER
	time_t ltime;
	time( &ltime );
	tm *pTime = localtime( &ltime );
	const int nCurrYear = pTime->tm_year + 1900;
	const int nCurrMonth = pTime->tm_mon + 1;
	const int nCurrDay = pTime->tm_mday;
	szStrings.clear();
	NStr::SplitString( szExpiryDate, szStrings, '.' );
	if ( szStrings.size() != 3 ) 
		return false;
	if ( nCurrYear > NStr::ToInt(szStrings[2]) ) 
		return false;
	else if ( (NStr::ToInt(szStrings[2]) == nCurrYear) ) 
	{
		if ( nCurrMonth > NStr::ToInt(szStrings[1]) ) 
			return false;
		else if ( NStr::ToInt(szStrings[1]) == nCurrMonth ) 
		{
			if ( nCurrDay > NStr::ToInt(szStrings[0]) ) 
				return false;
		}
	}
	if ( uCode != MakeCheckSum(szUserName, szExpiryDate) ) 
		return false;
#endif // _DO_BETA_CHECK
	return true;
}
};
