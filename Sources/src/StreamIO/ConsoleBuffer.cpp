#include "StdAfx.h"

#include "ConsoleBuffer.h"
CConsoleBuffer::~CConsoleBuffer()
{
	DumpLog( -1 );
}
bool CConsoleBuffer::Configure( const char *pszConfigure )
{
	std::vector<std::string> szTokens;
	NStr::SplitString( pszConfigure, szTokens, ';' );
	if ( szTokens.size() <= 1 )
		return false;
	if ( szTokens[0] == "dublicate" )
	{
		if ( szTokens.size() < 3 )
			return false;
		const int nChannel = NStr::ToInt( szTokens[1] );
		std::list<int> &channels = dublicates[nChannel];
		for ( int i = 2; i != szTokens.size(); ++i )
			channels.push_back( NStr::ToInt(szTokens[i]) );
	}
	else if ( szTokens[0] == "name" )
	{
		if ( szTokens.size() < 3 )
			return false;
		const int nChannel = NStr::ToInt( szTokens[1] );
		names[nChannel] = szTokens[2];
	}
	else if ( szTokens[0] == "logfile" )
	{
		if ( szTokens.size() < 2 )
			return false;
		szLogFileName = szTokens[1];
	}
	return true;
}
void CConsoleBuffer::WriteLocal( int nStreamID, const wchar_t *pszString, DWORD color, bool bBackupLog )
{
	streams[nStreamID].push_back( std::pair<std::wstring, DWORD>(pszString, color) );
	if ( bBackupLog )
		logs[nStreamID].push_back( std::pair<std::wstring, DWORD>(pszString, color) );
}
void CConsoleBuffer::WriteASCII( int nStreamID, const char *pszString, DWORD color, bool bBackupLog )
{
	std::wstring szUnicode;
	NStr::ToUnicode( &szUnicode, pszString );
	Write( nStreamID, szUnicode.c_str(), color, bBackupLog );
}
void CConsoleBuffer::Write( int nStreamID, const wchar_t *pszString, DWORD color, bool bBackupLog )
{
	WriteLocal( nStreamID, pszString, color, bBackupLog );
	CDublicateMap::const_iterator pos = dublicates.find( nStreamID );
	if ( pos != dublicates.end() )
	{
		for ( std::list<int>::const_iterator it = pos->second.begin(); it != pos->second.end(); ++it )
			WriteLocal( *it, pszString, color, false );	// don't backup log for dublicated channels
	}
}
const wchar_t* CConsoleBuffer::Read( int nStreamID, DWORD *pColor )
{
	CStringsList &stream = streams[nStreamID];
	if ( stream.empty() )
		return 0;
	szTempString = stream.front().first;
	if ( pColor )
		*pColor = stream.front().second;
	stream.pop_front();
	return szTempString.c_str();
}
const char* CConsoleBuffer::ReadASCII( int nStreamID, DWORD *pColor )
{
	const wchar_t *pszString = Read( nStreamID, pColor );
	if ( pszString == 0 )
		return 0;
	std::wstring szUnicode = pszString;
	NStr::ToAscii( &szTempStringASCII, szUnicode );
	return szTempStringASCII.c_str();
}
inline bool DumpLocal( const std::string &szString, FILE *file )
{
	if ( file != 0 )
		fprintf( file, szString.c_str() );
	return file != 0;
}
bool CConsoleBuffer::DumpLog( int nStreamID )
{
#if !defined(_FINALRELEASE) || defined(_DEVVERSION)	
	bool bLogFileCreated = false;
	if ( nStreamID != -1 )								// dump particular stream
	{
		FILE *file = szLogFileName.empty() ? 0 : fopen( szLogFileName.c_str(), "at" );
		CStringsList &stream = logs[nStreamID];
		if ( stream.empty() )
			return false;
		CStreamNamesMap::const_iterator pos = names.find( nStreamID );
		std::string szDumpString;
/* for debug
		if ( pos == names.end() )
			szDumpString = NStr::Format( "\n***** Dumping console stream %d *****\n", nStreamID );
		else
			szDumpString = NStr::Format( "\n***** Dumping console stream \"%s\" (%d) *****\n", pos->second.c_str(), nStreamID );

		DumpLocal( szDumpString, file );
*/
		while ( !stream.empty() )
		{
			szDumpString = NStr::Format( "%s\n", NStr::ToAscii(stream.front().first).c_str() );
			DumpLocal( szDumpString, file ) || bLogFileCreated;
			stream.pop_front();
		}
		if ( file != 0 )
			fclose( file );
		bLogFileCreated = true;
	}
	else																	// dump all streams
	{
		std::list<int> ids;
		for ( CStreamsMap::const_iterator it = logs.begin(); it != logs.end(); ++it )
			ids.push_back( it->first );
		ids.sort();
		for ( std::list<int>::const_iterator it = ids.begin(); it != ids.end(); ++it )
		{
			if ( *it != -1 )
			{
				bool bRetVal = DumpLog( *it );
				bLogFileCreated = bLogFileCreated || bRetVal;
			}
		}
	}
	return bLogFileCreated;
#endif // !defined(_FINALRELEASE) || defined(_DEVVERSION)
	return true;
}
