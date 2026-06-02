#include "StdAfx.h"

#include "DataBase.h"

#include "IniFile.h"
CIniFileDataBase::CIniFileDataBase( const char *pszName, DWORD dwAccessMode )
: dwStorageAccessMode( dwAccessMode )
{
	if ( pszName == 0 || pszName[0] == '\0' ) 
		szBase.clear();
	else
	{
		szBase = pszName;
		int pos = szBase.rfind( '\\' );
		if ( pos == std::string::npos )
			szBase.clear();
		else
			szBase = szBase.substr( 0, pos );
		const DWORD BUFFER_SIZE = 1024;
		char buffer[BUFFER_SIZE];
		szBase = _fullpath( buffer, szBase.c_str(), BUFFER_SIZE );
		szBase += '\\';
	}
}
IDataTable* CIniFileDataBase::CreateTable( const char *pszName, DWORD dwAccessMode )
{
	NI_ASSERT_TF( (dwStorageAccessMode & dwAccessMode) == dwAccessMode, "incompatible access mode", return 0 );
	CIniFile *pTable = new CIniFile();
	pTable->Open( (szBase + pszName).c_str(), dwAccessMode );
	return pTable;
}
IDataTable* CIniFileDataBase::OpenTable( const char *pszName, DWORD dwAccessMode )
{
	NI_ASSERT_TF( (dwStorageAccessMode & dwAccessMode) == dwAccessMode, "incompatible access mode", return 0 );
	CIniFile *pTable = new CIniFile();
	if ( !szBase.empty() ) 
	{
		if ( !pTable->Open( (szBase + pszName).c_str(), dwAccessMode ) )
		{
			delete pTable;
			pTable = 0;
		}
	}
	else
	{
		if ( CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream(pszName, STREAM_ACCESS_READ) )
		{
			if ( !pTable->Load(pStream) )
			{
				delete pTable;
				pTable = 0;
			}
		}
	}
	return pTable;
}
bool CIniFileDataBase::DestroyElement( const char *pszName )
{
	NI_ASSERT_TF( 0, "not realized yet", return false );
	return false;
}
bool CIniFileDataBase::RenameElement( const char *pszOldName, const char *pszNewName )
{
	NI_ASSERT_TF( 0, "not realized yet", return false );
	return false;
}
