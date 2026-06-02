#include "StdAfx.h"

#include "ZipFileSystem.h"

#include "MemFileSystem.h"
#include "..\Misc\FileUtils.h"
CZipFileSystemEnumerator::CZipFileSystemEnumerator( const CZipFilesList &_zipfiles, IDataStorage *_pStorage )
: pStorage( _pStorage ), zipfiles( _zipfiles ), posZipFile( _zipfiles.begin() ), nFileInZip( 0 )
{
	Zero( stats );
}
void CZipFileSystemEnumerator::Reset( const char *pszMask )
{
	posZipFile = zipfiles.begin();
	nFileInZip = -1;
}
bool CZipFileSystemEnumerator::NextEntry()
{
	if ( posZipFile == zipfiles.end() )
		return false;
	++nFileInZip;
	if ( nFileInZip < posZipFile->GetNumFiles() )
		return true;
	else
	{
		while ( 1 )
		{
			++posZipFile;
			if ( posZipFile == zipfiles.end() )
				return false;
			nFileInZip = 0;
			if ( nFileInZip < posZipFile->GetNumFiles() )
				return true;
		}
	}
}
bool CZipFileSystemEnumerator::Next()
{
	if( NextEntry() == false )
		return false;
	while ( posZipFile->IsDirectory(nFileInZip) )
	{
		if ( NextEntry() == false )
			return false;
	}
	NI_ASSERT_T( posZipFile != zipfiles.end(), "Can't find zip file" );
	posZipFile->GetFileName( nFileInZip, &szFileName );
	stats.pszName = szFileName.c_str();
	stats.type = SET_STREAM;
	stats.nSize = posZipFile->GetFileLen( nFileInZip );
	stats.mtime = posZipFile->GetModDateTime( nFileInZip );
	return true;
}
class CAddZipFileFunctional
{
	CZipFileSystem *pZipFS;								// zip file system
public:
	CAddZipFileFunctional( CZipFileSystem *_pZipFS ) : pZipFS( _pZipFS ) {  }
	bool operator()( NFile::CFileIterator &it )
	{
		if ( !it.IsDirectory() )
		{
			IDataStream *pStream = OpenFileStream( it.GetFilePath().c_str(), STREAM_ACCESS_READ );
			return pZipFS->AddZipFile( pStream, it.GetFilePath() );
		}
		return false;
	}
};
CZipFileSystem::CZipFileSystem( const char *pszName, DWORD dwAccessMode )
: dwStorageAccessMode( dwAccessMode )
{
	NI_ASSERT_SLOW_T( (dwAccessMode & (STREAM_ACCESS_WRITE | STREAM_ACCESS_APPEND)) == 0, "Can't write to zip - still not realized" );
	dwStorageAccessMode &= ~( STREAM_ACCESS_WRITE | STREAM_ACCESS_APPEND );

	std::string szName = pszName;
	std::string szMask;
	const bool bRecursiveSearch = ( szName.find('*') != std::string::npos ) || ( szName.find('?') != std::string::npos );
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
	NFile::EnumerateFiles( szName.c_str(), szMask.c_str(), CAddZipFileFunctional(this), bRecursiveSearch );
}
bool CZipFileSystem::AddZipFile( IDataStream *pStream, const std::string &szZipFileName )
{
	zipfiles.push_back( SZipFileDesc() );
	SZipFileDesc &zipfile = zipfiles.back();
	zipfile.Init( pStream );
	zipfile.szZipFileName = szZipFileName;
	if ( zipfile.IsOk() )
	{
		std::string szFileName;
		for ( int i=0; i<zipfile.GetNumFiles(); ++i )
		{
			if ( !zipfile.IsDirectory( i ) )
			{
				zipfile.GetFileName( i, &szFileName );
				NStr::ToLower( szFileName );
				SZipArchiveFileInfo &info = files[szFileName];
				if ( (info.pZipFile == 0) || (zipfile.GetModDateTime( i ) > info.pZipFile->GetModDateTime( info.nIndex )) )
				{
					info.nIndex = i;
					info.nSize = zipfile.GetFileLen( i );
					info.pZipFile = &( zipfile );
				}
			}
		}
		return true;
	}
	else
		zipfiles.pop_back();
	return false;
}
IDataStream* CZipFileSystem::CreateStream( const char *pszName, DWORD dwAccessMode )
{
	NI_ASSERT_SLOW_T( 0, "Can't create zip stream - still not realized" );
	return 0;
}
IDataStream* CZipFileSystem::OpenStream( const char *pszName, DWORD dwAccessMode )
{
	NI_ASSERT_SLOW_TF( (dwAccessMode & dwStorageAccessMode) == dwAccessMode, "Can't create stream - invalid access mode", return 0 );
	std::string szName = pszName;
	NStr::ToLower( szName );
	CFilesMap::const_iterator pos = files.find( szName );
	if ( pos != files.end() )
	{
		const SZipArchiveFileInfo &info = pos->second;
		CPtr<IDataStream> pZipStream = OpenFileStream( info.pZipFile->szZipFileName, dwStorageAccessMode );
		return info.pZipFile->ReadFile( pZipStream, info.nIndex );
	}
	return 0;
}
bool CZipFileSystem::GetStreamStats( const char *pszName, SStorageElementStats *pStats )
{
	std::string szName = pszName;
	NStr::ToLower( szName );
	CFilesMap::const_iterator pos = files.find( szName );
	if ( pos == files.end() )
		return false;
	const SZipArchiveFileInfo &info = pos->second;
	pStats->nSize = info.nSize;
	info.pZipFile->GetFileName( info.nIndex, &szName );
	pStats->atime = pStats->ctime = pStats->mtime = info.pZipFile->GetModDateTime( info.nIndex );
	pStats->type = SET_STREAM;
	pStats->pszName = 0;
	return true;
}
bool CZipFileSystem::DestroyElement( const char *pszName )
{
	files.erase( pszName );
	return true;
}
bool CZipFileSystem::RenameElement( const char *pszOldName, const char *pszNewName )
{
	files[pszNewName] = files[pszOldName];
	files.erase( pszOldName );
	return true;
}
IStorageEnumerator* CZipFileSystem::CreateEnumerator()
{
	return new CZipFileSystemEnumerator( zipfiles, this );
}
bool CZipFileSystem::AddStorage( IDataStorage *pStorage, const char *pszName )
{
	NI_ASSERT_T( 0, "Can't add new storage to the zip file system" );
	return false;
}
bool CZipFileSystem::RemoveStorage( const char *pszName )
{
	NI_ASSERT_T( 0, "Can't remove storage from zip file system" );
	return false;
}
const bool CZipFileSystem::IsStreamExist( const char *pszName )
{
	std::string szName = pszName;
	NStr::ToLower( szName );
	return files.find(szName) != files.end();
}
