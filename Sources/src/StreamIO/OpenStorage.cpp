#include "StdAfx.h"

#include "FileSystem.h"
#include "ZipFileSystem.h"
#include "MemFileSystem.h"
#include "CommonFileSystem.h"
IDataStorage* STDCALL OpenStorage( const char *pszName, DWORD dwAccessMode, DWORD type )
{
	switch ( type )
	{
		case STORAGE_TYPE_COMMON:
			return new CCommonFileSystem( pszName, dwAccessMode );
		case STORAGE_TYPE_FILE:
			return new CFileSystem( pszName, dwAccessMode, false );
		case STORAGE_TYPE_ZIP:
			return new CZipFileSystem( pszName, dwAccessMode );
		case STORAGE_TYPE_MEM:
			return new CMemFileSystem( dwAccessMode );
	}
	return 0;
}
IDataStorage* STDCALL CreateStorage( const char *pszName, DWORD dwAccessMode, DWORD type )
{
	switch ( type )
	{
		case STORAGE_TYPE_FILE:
			return new CFileSystem( pszName, dwAccessMode, true );
	}
	return 0;
}
