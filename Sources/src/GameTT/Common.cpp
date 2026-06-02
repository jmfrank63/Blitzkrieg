#include "StdAfx.h"
#include "CommonId.h"


bool operator > ( FILETIME a, FILETIME b )
{
	if ( a.dwHighDateTime > b.dwHighDateTime || (a.dwHighDateTime == b.dwHighDateTime && a.dwLowDateTime > b.dwLowDateTime) )
		return true;
	else
		return false;
}

bool operator < ( FILETIME a, FILETIME b ) { return (b > a); }

bool operator == ( FILETIME a, FILETIME b ) { if ( a.dwHighDateTime == b.dwHighDateTime && a.dwLowDateTime == b.dwLowDateTime ) return true; return false; }

FILETIME GetFileChangeTime( const char *pszFileName )
{
	FILETIME zero;
	zero.dwHighDateTime = 0;
	zero.dwLowDateTime = 0;
	HANDLE hFile = CreateFile( pszFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	if ( hFile == INVALID_HANDLE_VALUE )
	{
		DWORD dwErr = GetLastError();
		return zero;
	}
	BY_HANDLE_FILE_INFORMATION fileInfo;
	bool bRes = GetFileInformationByHandle( hFile, &fileInfo );
	CloseHandle( hFile );
	if ( !bRes )
		return zero;
	
	if ( fileInfo.ftCreationTime > fileInfo.ftLastWriteTime )
		return fileInfo.ftCreationTime;
	else
		return fileInfo.ftLastWriteTime;
}

std::string GetFileChangeTimeString( const char *pszFileName )
{
	FILETIME fTime = GetFileChangeTime( pszFileName );
	if ( fTime.dwHighDateTime == 0 && fTime.dwLowDateTime == 0 )
		return "";		//не нашли файл или это директория

	FileTimeToLocalFileTime( &fTime, &fTime );
	SYSTEMTIME st;
	FileTimeToSystemTime( &fTime, &st );
	return NStr::Format( "%.2d.%.2d.%.4d %.2d:%.2d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute );
}
