#include "StdAfx.h"

#include "ModFileSystem.h"
void CModFileSystemEnumerator::Reset( const char *pszName )
{
	Zero( stats );
	bReset = true;
	itCurrFile = files.end();
}
bool CModFileSystemEnumerator::Next()
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
	stats = itCurrFile->second;
	stats.pszName = itCurrFile->first.c_str();
	stats.type = SET_STREAM;

	return true;
}
CModFileSystem::CModFileSystem( const char *pszName, DWORD dwAccessMode )
: dwStorageAccessMode( dwAccessMode )
{
	NI_ASSERT_SLOW_T( (dwAccessMode & (STREAM_ACCESS_WRITE | STREAM_ACCESS_APPEND)) == 0, "Can't write to common file system - still not realized for all components" );
	dwStorageAccessMode &= ~( STREAM_ACCESS_WRITE | STREAM_ACCESS_APPEND );
	IDataStorage *pStorage = OpenStorage( pszName, STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
	AddStorage( pStorage, "MAIN_BASE_STORAGE" );
}
IDataStream* CModFileSystem::CreateStream( const char *pszName, DWORD dwAccessMode )
{
	NI_ASSERT_T( 0, "Have no write access to common file system" );
	return 0;
}
IDataStream* CModFileSystem::OpenStream( const char *pszName, DWORD dwAccessMode )
{
	for ( CFileSystemsList::iterator it = filesystems.begin(); it != filesystems.end(); ++it )
	{
		if ( IDataStream *pStream = it->second->OpenStream(pszName, dwAccessMode) )
			return pStream;
	}
	return 0;
}
bool CModFileSystem::GetStreamStats( const char *pszName, SStorageElementStats *pStats )
{
	for ( CFileSystemsList::iterator it = filesystems.begin(); it != filesystems.end(); ++it )
	{
		if ( it->second->GetStreamStats(pszName, pStats) )
			return true;
	}
	return false;
}
bool CModFileSystem::DestroyElement( const char *pszName )
{
	NI_ASSERT_T( 0, "Have no write access to common file system" );
	return false;
}
bool CModFileSystem::RenameElement( const char *pszOldName, const char *pszNewName )
{
	NI_ASSERT_T( 0, "Have no write access to common file system" );
	return false;
}
bool CModFileSystem::AddStorage( IDataStorage *pStorage, const char *pszName )
{
	std::string szName = pszName;
	NStr::ToLower( szName );
	for ( CFileSystemsList::iterator it = filesystems.begin(); it != filesystems.end(); ++it )
	{
		if ( it->first == szName ) 
		{
			it->second = pStorage;
			return true;
		}
	}
	filesystems.push_front( SFileSystemDesc(szName, pStorage) );
	return true;
}
bool CModFileSystem::RemoveStorage( const char *pszName )
{
	std::string szName = pszName;
	NStr::ToLower( szName );
	for ( CFileSystemsList::iterator it = filesystems.begin(); it != filesystems.end(); ++it )
	{
		if ( it->first == szName ) 
		{
			filesystems.erase( it );
			return true;
		}
	}
	return false;
}
const bool CModFileSystem::IsStreamExist( const char *pszName )
{
	for ( CFileSystemsList::const_iterator it = filesystems.begin(); it != filesystems.end(); ++it )
	{
		if ( it->second->IsStreamExist(pszName) ) 
			return true;
	}
	return false;
}
const char* CModFileSystem::GetName() const 
{ 
	return filesystems.empty() ? ".\\" : filesystems.back().second->GetName();
}
IStorageEnumerator* CModFileSystem::CreateEnumerator()
{
	if ( filesystems.size() == 0 ) 
		return filesystems.back().second->CreateEnumerator();
	else
	{
		CModFileSystemEnumerator *pModEnum = new CModFileSystemEnumerator();
		for ( CFileSystemsList::reverse_iterator it = filesystems.rbegin(); it != filesystems.rend(); ++it )
		{
			CPtr<IStorageEnumerator> pEnumerator = it->second->CreateEnumerator();
			pEnumerator->Reset( "*.*" );
			while (	pEnumerator->Next() )
			{
				const SStorageElementStats *pStats = pEnumerator->GetStats();
				pModEnum->AddFile( pStats->pszName, *pStats );
			}
		}
		return pModEnum;
	}
}
