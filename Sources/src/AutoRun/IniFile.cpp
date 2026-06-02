#include "StdAfx.h"

#include "StrProc.h"
#include "IniFile.h"

#include <io.h>
const int nIniFileBufferSize = 65535;
using namespace NStr;
bool CIniFile::Open( const std::vector<BYTE> &rData, DWORD _dwAccessMode )
{
	dwAccessMode = _dwAccessMode;
	std::string szString;
	szString.resize( rData.size() );
	memcpy( &( szString[0] ), &( rData[0] ), rData.size() );
	szString.resize( rData.size() );
	
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
			ASSERT( nPos != std::string::npos );
			szEntry = line->substr( 0, nPos );
			TrimBoth( szEntry );
			szData = line->substr( nPos + 1 );
			TrimBoth( szData );
			if ( !szRow.empty() && !szEntry.empty() && !szData.empty() )
				table[szRow][szEntry] = szData;
		}
	}
	return true;
}
int CIniFile::GetRowNames( char *pszBuffer, int nBufferSize )
{
	ASSERT( CanRead() );
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
	ASSERT( CanRead() );
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
	ASSERT( CanRead() );
	const SEntry *pEntry = GetEntry( pszRow, pszEntry );
	if ( pEntry == 0 )
	{
		return defval;
	}
	else
	{
		int nNumber = defval;
		sscanf( pEntry->val.c_str(), "%i", &nNumber );
		return nNumber;
	}
}
double CIniFile::GetDouble( const char *pszRow, const char *pszEntry, double defval )
{
	ASSERT( CanRead() );
	const SEntry *pEntry = GetEntry( pszRow, pszEntry );
	return pEntry == 0 ? defval : atof( pEntry->val.c_str() );
}
const char* CIniFile::GetString( const char *pszRow, const char *pszEntry, const char *defval, char *pszBuffer, int nBufferSize )
{
	ASSERT( CanRead() );
	const SEntry *pEntry = GetEntry( pszRow, pszEntry );
	if ( pEntry == 0 )
		strcpy( pszBuffer, defval );
	else
		strcpy( pszBuffer, table[pszRow][pszEntry].val.c_str() );
	return pszBuffer;
}

void CIniFile::SetInt( const char *pszRow, const char *pszEntry, int val )
{
	ASSERT( CanWrite() );
	char buff[64];
	sprintf( buff, "%d", val );
	SetString( pszRow, pszEntry, buff );
}
void CIniFile::SetDouble( const char *pszRow, const char *pszEntry, double val )
{
	ASSERT( CanWrite() );
	char buff[128];
	sprintf( buff, "%g", val );
	SetString( pszRow, pszEntry, buff );
}
void CIniFile::SetString( const char *pszRow, const char *pszEntry, const char *val )
{
	ASSERT( CanWrite() );
	bChanged = true;
	table[pszRow][pszEntry].val = val;
}
