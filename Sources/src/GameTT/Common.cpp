#include "StdAfx.h"
#include "CommonId.h"
#include "../Misc/FileUtils.h"


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
	NFile::CFile file;
	if ( !file.Open( pszFileName, NFile::CFile::modeRead ) )
		return zero;
	NFile::CFile::SStatus fileStatus;
	if ( !file.GetStatus( &fileStatus ) )
		return zero;
	
	if ( fileStatus.ctime > fileStatus.mtime )
		return fileStatus.ctime;
	else
		return fileStatus.mtime;
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
