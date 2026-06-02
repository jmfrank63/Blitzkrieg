#include "StdAfx.h"

#include "CommonFileSystem.h"
void CCommonFileSystemEnumerator::Reset( const char *pszName )
{
	szMask = pszName;
	szFileName.clear();
	Zero( stats );
	bReset = true;
	itCurrFile = files.end();
}
bool CCommonFileSystemEnumerator::Next()
{
	if ( bReset )
	{
		itCurrFile = files.begin();
		bReset = false;
		if ( itCurrFile == files.end() )
			return false;
	}
	++itCurrFile;
	if ( itCurrFile == files.end() )
		return false;
	szFileName = itCurrFile->first;
	stats.pszName = szFileName.c_str();
	stats.mtime = stats.atime = stats.ctime = itCurrFile->second.dwModTime;
	stats.type = SET_STREAM;
	stats.nSize = -1;

	return true;
}
CCommonFileSystem::CCommonFileSystem( const char *pszName, DWORD dwAccessMode )
: dwStorageAccessMode( dwAccessMode )
{
	NI_ASSERT_SLOW_T( (dwAccessMode & (STREAM_ACCESS_WRITE | STREAM_ACCESS_APPEND)) == 0, "Can't write to common file system - still not realized for all components" );
	dwStorageAccessMode &= ~( STREAM_ACCESS_WRITE | STREAM_ACCESS_APPEND );
	std::string szName = pszName;
	std::string szMask;
	int nPos = szName.rfind( '\\' );
	if ( nPos == std::string::npos )
	{
		szMask = szName.empty() ? "*.zip" : szName;
		szName = ".\\";
	}
	else
	{
		szMask = nPos + 1 == szName.size() ? "*.zip" : szName.substr( nPos + 1 );
		szName = szName.substr( 0, nPos + 1 );
	}
	pZipStorage = OpenStorage( pszName, dwStorageAccessMode, STORAGE_TYPE_ZIP );
	EnumerateFiles( "", pZipStorage );
	pFileStorage = OpenStorage( szName.c_str(), dwStorageAccessMode, STORAGE_TYPE_FILE );
	EnumerateFiles( "", pFileStorage );
	szBase = pFileStorage->GetName();
}
void CCommonFileSystem::EnumerateFiles( const std::string &szName, IDataStorage *pStorage )
{
	CPtr<IStorageEnumerator> pEnum = pStorage->CreateEnumerator();
	for ( pEnum->Reset( (szName + "*.*").c_str() ); pEnum->Next(); )
	{
		const SStorageElementStats *pStats = pEnum->GetStats();
		if ( pStats->type == SET_STORAGE )
			EnumerateFiles( szName + pStats->pszName + '\\', pStorage );
		else
		{
			std::string szFileName = szName + pStats->pszName;
			NStr::ToLower( szFileName );
			SCommonFileInfo &info = files[szFileName];
			if ( (info.pStorage == 0) || (info.dwModTime < pStats->mtime) )
			{
				info.dwModTime = pStats->mtime;
				info.pStorage = pStorage;
			}
		}
	}
}
IDataStream* CCommonFileSystem::CreateStream( const char *pszName, DWORD dwAccessMode )
{
	NI_ASSERT_T( 0, "Have no write access to common file system" );
	return 0;
}
IDataStream* CCommonFileSystem::OpenStream( const char *pszName, DWORD dwAccessMode )
{
	std::string szName = pszName;
	NStr::ToLower( szName );
	CFilesMap::iterator pos = files.find( szName );
	IDataStream *pRes = 0;
	if ( pos != files.end() )							// открываем файл из соответствующего ему storage
		pRes = pos->second.pStorage->OpenStream( szName.c_str(), dwAccessMode );
	else																	// попробуем открыть файл с диска (если он появился после инициализации common file system - для редакторов)
		pRes = pFileStorage->OpenStream( pszName, dwAccessMode );
#ifndef _DONT_USE_SINGLETON	
	if ( (pRes == 0) && (GetGlobalVar("report", 0) == 1) ) 
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, NStr::Format("Can't open stream \"%s\" with access mode 0x%.8x", pszName, dwAccessMode), 0xffff0000, true );
#endif // _DONT_USE_SINGLETON
	return pRes;
}
bool CCommonFileSystem::GetStreamStats( const char *pszName, SStorageElementStats *pStats )
{
	if ( !IsStreamExist(pszName) ) 
		return false;
	std::string szName = pszName;
	NStr::ToLower( szName );
	CFilesMap::iterator pos = files.find( szName );
	if ( pos != files.end() )							// файл из соответствующего ему storage
		return pos->second.pStorage->GetStreamStats( pszName, pStats );
	else																	// попробуем файл с диска (если он появился после инициализации common file system - для редакторов)
		return pFileStorage->GetStreamStats( pszName, pStats );
}
bool CCommonFileSystem::DestroyElement( const char *pszName )
{
	NI_ASSERT_T( 0, "Have no write access to common file system" );
	return false;
}
bool CCommonFileSystem::RenameElement( const char *pszOldName, const char *pszNewName )
{
	NI_ASSERT_T( 0, "Have no write access to common file system" );
	return false;
}
IStorageEnumerator* CCommonFileSystem::CreateEnumerator()
{
	return new CCommonFileSystemEnumerator( files );
}
bool CCommonFileSystem::AddStorage( IDataStorage *pStorage, const char *pszName )
{
	NI_ASSERT_T( 0, "Can't add new storage to the common file system" );
	return false;
}
bool CCommonFileSystem::RemoveStorage( const char *pszName )
{
	NI_ASSERT_T( 0, "Can't remove storage from common file system" );
	return false;
}
const bool CCommonFileSystem::IsStreamExist( const char *pszName )
{
	std::string szName = pszName;
	NStr::ToLower( szName );
	return ( files.find(szName) != files.end() ) || pFileStorage->IsStreamExist( szName.c_str() );
}
