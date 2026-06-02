#include "StdAfx.h"

#include <sys/stat.h>
#include "FileSystem.h"
inline std::string GetFullPath( const std::string &szPath )
{
	const DWORD BUFFER_SIZE = 1024;
	char buffer[BUFFER_SIZE];
	char *pszBufferFileName = 0;
	GetFullPathName( szPath.c_str(), 1024, buffer, &pszBufferFileName );
	std::string szResult = buffer;
	if ( !szResult.empty() && szResult[szResult.size() - 1] != '\\' ) 
		szResult += '\\';
	return szResult;
}
inline SWin32Time MakeWin32Time( const FILETIME &filetime )
{
	return filetime.dwLowDateTime != 0 ? FILETIMEToWin32DateTime( filetime ) : 0;
}
CFileSystemEnumerator::~CFileSystemEnumerator()
{
	Close();
}
void CFileSystemEnumerator::Close()
{
	if ( IsFindValid() ) 
		::FindClose( hFind );
	hFind = INVALID_HANDLE_VALUE;
}
void CFileSystemEnumerator::FillStats()
{
	if ( !IsFindValid() )
		Zero( stats );
	else
	{
		szFoundFileName = findinfo.cFileName;
		stats.nSize = findinfo.nFileSizeLow;
		stats.type = IsDirectory() ? SET_STORAGE : SET_STREAM;
		stats.pszName = szFoundFileName.c_str();
		stats.ctime = MakeWin32Time( findinfo.ftCreationTime );
		stats.mtime = MakeWin32Time( findinfo.ftLastWriteTime );
		stats.atime = MakeWin32Time( findinfo.ftLastAccessTime );
	}
}
bool CFileSystemEnumerator::FindFirstFile()
{
	int pos = szMask.rfind( '\\' );
	if ( pos == std::string::npos )
		szPath.clear();
	else
	{
		szPath = szMask.substr( 0, pos );
		szMask = szMask.substr( pos + 1 );
	}
	szPath = szBase + szPath;
	szPath = GetFullPath( szPath );
	szMask = szPath + szMask;
	hFind = ::FindFirstFile( szMask.c_str(), &findinfo );
	return IsFindValid();
}
bool CFileSystemEnumerator::FindNextFile()
{
	if ( !IsFindValid() )
		return false;
	if ( ::FindNextFile( hFind, &findinfo ) == 0 )
	{
		Close();
		return false;
	}
	if ( IsDots() )
		return FindNextFile();
	return true;
}
void CFileSystemEnumerator::Reset( const char *pszMask )
{
	Close();
	szMask = pszMask;
}
bool CFileSystemEnumerator::Next()
{
	if ( !IsFindValid() )
	{
		if ( !FindFirstFile() )
			return false;
		if ( IsDots() )
		{
			if ( !FindNextFile() )
				Close();
		}
	}
	else
	{
		if ( !FindNextFile() )
			Close();
	}
	FillStats();
	return IsFindValid();
}
CFileStream::CFileStream( const char *pszFileName, DWORD _dwAccessMode ) 
: nStreamBegin( 0 )
{
	dwAccessMode = _dwAccessMode;
	DWORD dwDesiredAccess = 0;
	DWORD dwShareMode = 0;
	DWORD dwCreationDesposition = 0;
	DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;

	DWORD dwRW = STREAM_ACCESS_READ | STREAM_ACCESS_WRITE;
	DWORD dwRWA = dwRW | STREAM_ACCESS_APPEND;
	DWORD dwWA = STREAM_ACCESS_WRITE | STREAM_ACCESS_APPEND;
	if ( (dwAccessMode & dwRWA) == dwRWA ) // RWA
	{
		dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
		dwCreationDesposition = OPEN_ALWAYS;
	}
	else if ( (dwAccessMode & dwRW) == dwRW ) // RW
	{
		dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
		dwCreationDesposition = OPEN_ALWAYS;
	}
	else if ( (dwAccessMode & dwWA) == dwWA )
	{
		dwDesiredAccess = GENERIC_WRITE;
		dwCreationDesposition = OPEN_ALWAYS;
	}
	else if ( dwAccessMode & STREAM_ACCESS_READ )
	{
		dwDesiredAccess = GENERIC_READ;
		dwShareMode = FILE_SHARE_READ;
		dwCreationDesposition = OPEN_EXISTING;
	}
	else if ( dwAccessMode & STREAM_ACCESS_WRITE )
	{
		dwDesiredAccess = GENERIC_WRITE;
		dwShareMode = FILE_SHARE_READ;
		dwCreationDesposition = CREATE_ALWAYS;
	}

	hFile = ::CreateFile( pszFileName, dwDesiredAccess, dwShareMode, 0, dwCreationDesposition, dwFlagsAndAttributes, 0 );
	DWORD dwResult = ::GetLastError();
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		szName = pszFileName;
		stats.pszName = szName.c_str();
		stats.nSize = ::GetFileSize( hFile, 0 );
		stats.type = SET_STREAM;
		BY_HANDLE_FILE_INFORMATION fileinfo;
		if ( ::GetFileInformationByHandle(hFile, &fileinfo) != FALSE )
		{
			stats.ctime = MakeWin32Time( fileinfo.ftCreationTime );
			stats.mtime = MakeWin32Time( fileinfo.ftLastWriteTime );
			stats.atime = MakeWin32Time( fileinfo.ftLastAccessTime );
		}
	}
}
CFileStream::~CFileStream()
{
	if ( hFile != INVALID_HANDLE_VALUE )
		CloseHandle( hFile );
}
inline int GetCurrPos( HANDLE hFile ) { return SetFilePointer( hFile, 0, 0, FILE_CURRENT ); }
int CFileStream::LockBegin()
{
	nStreamBegin = GetCurrPos( hFile );
	return nStreamBegin;
}
int CFileStream::UnlockBegin()
{
	int nOldBegin = nStreamBegin;
	nStreamBegin = 0;
	return nOldBegin;
}
int CFileStream::GetPos() const
{
	return GetCurrPos( hFile ) - nStreamBegin;
}
inline int StreamSeekToFileSeek( STREAM_SEEK from )
{
	switch ( from )
	{
		case STREAM_SEEK_SET: return FILE_BEGIN;
		case STREAM_SEEK_CUR: return FILE_CURRENT;
		case STREAM_SEEK_END: return FILE_END;
	}
	return FILE_CURRENT;
}
int CFileStream::Seek( int offset, STREAM_SEEK from )
{
	const int nSeek = StreamSeekToFileSeek( from );
	if ( nSeek == FILE_BEGIN )
		SetFilePointer( hFile, nStreamBegin + offset, 0, nSeek );
	else
		SetFilePointer( hFile, offset, 0, nSeek );
	return GetPos();
}
int CFileStream::Read( void *pBuffer, int nLength )
{
	if ( !CanRead() )
		return 0;
	DWORD dwRead = 0;
	return ReadFile( hFile, pBuffer, nLength, &dwRead, 0 ) != FALSE ? dwRead : 0;
}
int CFileStream::Write( const void *pBuffer, int nLength )
{
	if ( !CanWrite() )
		return 0;
	DWORD dwWritten = 0;
	return WriteFile( hFile, pBuffer, nLength, &dwWritten, 0 ) != FALSE ? dwWritten : 0;
}
int CFileStream::GetSize() const
{
	const int nLength = IsOpen() ? GetFileSize( hFile, 0 ) : 0;
	return nLength - nStreamBegin;
}
bool CFileStream::SetSize( int nSize )
{
	return false;
}
int CFileStream::CopyTo( IDataStream *pDstStream, int nLength )
{
	const int nBufferSize = Min( nLength, 1024*1024 );
	std::vector<char> buffer( nBufferSize );
	int nTotalWrote = 0;
	while ( nLength >= nBufferSize )
	{
		const int nReaded = Read( &(buffer[0]), nBufferSize );
		nTotalWrote += pDstStream->Write( &(buffer[0]), nReaded );
		nLength -= nReaded;
		if ( nReaded != nBufferSize ) 
			return nTotalWrote;
	}
	if ( nLength )
	{
		nLength = Read( &(buffer[0]), nLength );
		nTotalWrote += pDstStream->Write( &(buffer[0]), nLength );
	}
	return nTotalWrote;
}
void CFileStream::Flush()
{
	FlushFileBuffers( hFile );
}
void CFileStream::GetStats( SStorageElementStats *pStats )
{
	if ( pStats == 0 )
		return;
	pStats->pszName = szName.c_str();
	pStats->nSize = GetSize();
	pStats->type = SET_STREAM;
	pStats->ctime = stats.ctime;
	pStats->atime = stats.atime;
	pStats->mtime = stats.mtime;
}
static bool IsAbsolutePath( const std::string &szPath )
{
	return szPath.size() >= 2 &&
		( (szPath[1] == ':') ||
		  (szPath[0] == '\\' && szPath[1] == '\\') );
}

CFileSystem::CFileSystem( const char *pszName, DWORD dwAccessMode, bool bCreate ) 
: szBase( pszName ), dwStorageAccessMode( dwAccessMode )
{
	int pos = szBase.rfind( '\\' );
	if ( pos == std::string::npos )
		szBase.clear();
	else
		szBase = szBase.substr( 0, pos );
	if ( bCreate )
		CreatePathRecursive( szBase );
	if ( !szBase.empty() )
	{
		if ( IsAbsolutePath( szBase ) )
			szBase = GetFullPath( szBase );
		else if ( szBase[ szBase.size() - 1 ] != '\\' )
			szBase += '\\';
	}
	NStr::ToLower( szBase );
}
bool CFileSystem::CreatePathRecursive( const std::string &szName )
{
	std::vector<std::string> szNames;
	NStr::SplitString( szName, szNames, '\\' );
	if ( szNames.empty() )
		return false;
	char pszBuffer[1024];
	GetCurrentDirectory( 1024, pszBuffer );
	SetCurrentDirectory( (szNames.front() + "\\").c_str() );
	for ( std::vector<std::string>::const_iterator it = szNames.begin() + 1; it != szNames.end(); ++it )
	{
		CreateDirectory( it->c_str(), 0 );
		SetCurrentDirectory( ((*it) + "\\").c_str() );
	}
	SetCurrentDirectory( pszBuffer );
	return true;
}
IDataStream* CFileSystem::CreateStream( const char *pszName, DWORD dwAccessMode )
{
	std::string szName = szBase + pszName;
	NI_ASSERT_TF( (dwAccessMode & dwStorageAccessMode) == dwAccessMode, "Can't create stream - invalid access mode", return 0 );
	CFileStream *pStream = new CFileStream( szName.c_str(), dwAccessMode );
	if ( pStream->IsOpen() )
		return pStream;
	delete pStream;
	return 0;
}
IDataStream* CFileSystem::OpenStream( const char *pszName, DWORD dwAccessMode )
{
	std::string szName = szBase + pszName;
	NI_ASSERT_TF( (dwAccessMode & dwStorageAccessMode) == dwAccessMode, "Can't open stream - invalid access mode", return 0 );
	CFileStream *pStream = new CFileStream( szName.c_str(), dwAccessMode );
	if ( pStream->IsOpen() )
		return pStream;
	delete pStream;
	return 0;
}
bool CFileSystem::GetStreamStats( const char *pszName, SStorageElementStats *pStats )
{
	if ( !IsStreamExist(pszName) ) 
		return false;
	if ( CPtr<IDataStream> pStream = OpenStream(pszName, STREAM_ACCESS_READ) )
	{
		pStream->GetStats( pStats );
		pStats->pszName = 0;
		return true;
	}
	return false;
}
bool CFileSystem::DestroyElement( const char *pszName )
{
	return DeleteFile( pszName ) != 0;
}
bool CFileSystem::RenameElement( const char *pszOldName, const char *pszNewName )
{
	return MoveFile( pszOldName, pszNewName ) != 0;
}
IStorageEnumerator* CFileSystem::CreateEnumerator()
{
	return new CFileSystemEnumerator( szBase );
}
bool CFileSystem::AddStorage( IDataStorage *pStorage, const char *pszName )
{
	NI_ASSERT_T( 0, "Can't add new storage to the file file system" );
	return false;
}
bool CFileSystem::RemoveStorage( const char *pszName )
{
	NI_ASSERT_T( 0, "Can't remove storage from file file system" );
	return false;
}
const bool CFileSystem::IsStreamExist( const char *pszName )
{
	std::string szName = szBase + pszName;
	NStr::ToLower( szName );
	return _access( szName.c_str(), 0 ) == 0;
}
