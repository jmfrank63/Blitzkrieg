#include "StdAfx.h"

#include "IniFile.h"

#include <io.h>
const int nIniFileBufferSize = 65535;
using namespace NStr;
bool CIniFile::Load( IDataStream *pStream )
{
	const int nSize = pStream->GetSize();
	if ( nSize == 0 )
		return false;

	std::string szString;
	szString.resize( nSize );
	const int nRead = pStream->Read( &(szString[0]), nSize );
	szString.resize( nRead );
	if ( szString.empty() )
		return false;

	dwAccessMode = TABLE_ACCESS_READ;
	LoadTables( szString );
	return true;
}
bool CIniFile::Open( const char *pszIniFileName, DWORD _dwAccessMode )
{
	szIniFileName = pszIniFileName;
	dwAccessMode = _dwAccessMode;
	FILE *file = fopen( pszIniFileName, "rt" );
	if ( file == 0 )
		return false;
	int nLength = _filelength( _fileno( file ) );
	std::string szString;
	szString.resize( nLength );
	nLength = fread( &(szString[0]), 1, nLength, file );
	fclose( file );
	szString.resize( nLength );
	LoadTables( szString );
	return true;
}
void CIniFile::LoadTables( const std::string &szString )
{
	std::string szRow, szEntry, szData;
	int nOrder = 0;
	for ( CStringIterator<> line( szString, CCharSeparator('\n') ); !line.IsEnd(); ++line )
	{
		TrimBoth( *line );
		if ( line->empty() )
			continue;
		if ( ( (*line)[0] == '[' ) && ((*line)[line->size() - 1] == ']') )
		{
			TrimLeft( *line, '[' );
			TrimRight( *line, ']' );
			szRow = *line;
			nOrder = 0;
		}
		else if ( (*line)[0] == ';' )				// this is a comment
			continue;
		else
		{
			int nPos = line->find( '=' );
			NI_ASSERT( nPos != std::string::npos );
			szEntry = line->substr( 0, nPos );
			TrimBoth( szEntry );
			szData = line->substr( nPos + 1 );
			TrimBoth( szData );
			if ( !szRow.empty() && !szEntry.empty() && !szData.empty() )
				table[szRow][szEntry] = szData;
		}
	}
}
CIniFile::~CIniFile()
{
	if ( !bChanged )
		return;
	NI_ASSERT_TF( !szIniFileName.empty(), "Trying to save changed Ini-file with empty file name", return );
	FILE *file = fopen( szIniFileName.c_str(), "wt" );
	if ( file == 0 )
		return;
	for ( STable::CValList::const_iterator row = table.elist.begin(); row != table.elist.end(); ++row )
	{
		fprintf( file, "\n[%s]\n", row->key.c_str() );
		for ( SRow::CValList::const_iterator elem = row->elist.begin(); elem != row->elist.end(); ++elem )
			fprintf( file, "%s=%s\n", elem->key.c_str(), elem->val.c_str() );
	}
	fclose( file );
}
int CIniFile::GetRowNames( char *pszBuffer, int nBufferSize )
{
	NI_ASSERT( CanRead() );
	int nCurrPos = 0;
	for ( STable::CValList::const_iterator pos = table.elist.begin(); pos != table.elist.end(); ++pos )
	{
		memcpy( pszBuffer + nCurrPos, pos->key.c_str(), pos->key.size() );
		nCurrPos += pos->key.size();
		*( pszBuffer + nCurrPos ) = '\0';
		++nCurrPos;
	}
	*( pszBuffer + nCurrPos ) = '\0';
	return nCurrPos + 1;
}
int CIniFile::GetEntryNames( const char *pszRow, char *pszBuffer, int nBufferSize )
{
	NI_ASSERT( CanRead() );
	int nCurrPos = 0;
	const SRow &row = table[pszRow];
	for ( SRow::CValList::const_iterator pos = row.elist.begin(); pos != row.elist.end(); ++pos )
	{
		memcpy( pszBuffer + nCurrPos, pos->key.c_str(), pos->key.size() );
		nCurrPos += pos->key.size();
		*( pszBuffer + nCurrPos ) = '\0';
		++nCurrPos;
	}
	*( pszBuffer + nCurrPos ) = '\0';
	return nCurrPos + 1;
}
int CIniFile::GetInt( const char *pszRow, const char *pszEntry, int defval )
{
	NI_ASSERT( CanRead() );
	const SEntry *pEntry = GetEntry( pszRow, pszEntry );
	return pEntry == 0 ? defval : atoi( pEntry->val.c_str() );
}
double CIniFile::GetDouble( const char *pszRow, const char *pszEntry, double defval )
{
	NI_ASSERT( CanRead() );
	const SEntry *pEntry = GetEntry( pszRow, pszEntry );
	return pEntry == 0 ? defval : atof( pEntry->val.c_str() );
}
const char* CIniFile::GetString( const char *pszRow, const char *pszEntry, const char *defval, char *pszBuffer, int nBufferSize )
{
	NI_ASSERT( CanRead() );
	const SEntry *pEntry = GetEntry( pszRow, pszEntry );
	if ( pEntry == 0 )
		strcpy( pszBuffer, defval );
	else
		strcpy( pszBuffer, table[pszRow][pszEntry].val.c_str() );
	return pszBuffer;
}
int CIniFile::GetRawData( const char *pszRow, const char *pszEntry, void *pBuffer, int nBufferSize )
{
	NI_ASSERT_T( 0, "not realized yet" );
	return 0;
}
void CIniFile::SetInt( const char *pszRow, const char *pszEntry, int val )
{
	NI_ASSERT( CanWrite() );
	char buff[64];
	sprintf( buff, "%d", val );
	SetString( pszRow, pszEntry, buff );
}
void CIniFile::SetDouble( const char *pszRow, const char *pszEntry, double val )
{
	NI_ASSERT( CanWrite() );
	char buff[128];
	sprintf( buff, "%g", val );
	SetString( pszRow, pszEntry, buff );
}
void CIniFile::SetString( const char *pszRow, const char *pszEntry, const char *val )
{
	NI_ASSERT( CanWrite() );
	bChanged = true;
	table[pszRow][pszEntry].val = val;
}
void CIniFile::SetRawData( const char *pszRow, const char *pszEntry, const void *pBuffer, int nBufferSize )
{
	NI_ASSERT_T( 0, "not realized yet" );
}
