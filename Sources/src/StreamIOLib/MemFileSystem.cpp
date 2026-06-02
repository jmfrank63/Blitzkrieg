#include "StdAfx.h"

#include "MemFileSystem.h"
int CMemFileStream::Read( void *pBuffer, int nLength )
{
	nLength = Min( nLength, int(data.size() - nCurrPos) );
	if ( nLength > 0 )
		memcpy( pBuffer, &(data[nCurrPos]), nLength );
	nCurrPos += nLength;
	return nLength;
}
int CMemFileStream::Write( const void *pBuffer, int nLength )
{
	ResizeToFit( nCurrPos + nLength );
	if ( nLength > 0 )
		memcpy( &(data[nCurrPos]), pBuffer, nLength );
	nCurrPos += nLength;
	return nLength;
}
int CMemFileStream::LockBegin()
{
	nBeginPos = nCurrPos;
	return nBeginPos;
}
int CMemFileStream::UnlockBegin()
{
	int nOldBeginPos = nBeginPos;
	nBeginPos = 0;
	return nOldBeginPos;
}
int CMemFileStream::GetPos() const
{
	return nCurrPos - nBeginPos;
}
int CMemFileStream::Seek( int offset, STREAM_SEEK from )
{
	switch ( from )
	{
		case STREAM_SEEK_SET:
			nCurrPos = nBeginPos + offset;
			break;
		case STREAM_SEEK_CUR:
			nCurrPos += offset;
			break;
		case STREAM_SEEK_END:
			nCurrPos = data.size() + offset;
			break;
		default:
			NI_ASSERT_T( 0, "unknown seek position" );
	}
	NI_ASSERT_T( (nCurrPos >= nBeginPos) && (nCurrPos <= data.size()), "Stream position out of range" );
	return nCurrPos;
}
int CMemFileStream::GetSize() const
{
	return data.size() - nBeginPos;
}
bool CMemFileStream::SetSize( int nSize )
{
	data.resize( nBeginPos + nSize );
	if ( nCurrPos > nBeginPos + nSize )
		nCurrPos = nBeginPos + nSize;
	return true;
}
int CMemFileStream::CopyTo( IDataStream *pDstStream, int nLength )
{
	nLength = Min( nLength, int(data.size() - nCurrPos) );
	const int nLastPos = nCurrPos;
	nCurrPos += nLength;
	return nLength > 0 ? pDstStream->Write( &(data[nLastPos]), nLength ) : 0;
}
void CMemFileStream::Flush()
{
}
void CMemFileStream::GetStats( SStorageElementStats *pStats )
{
	memcpy( pStats, &stats, sizeof(stats) );
	pStats->nSize = GetSize();
}
int CMemFileStream::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pStorage );
	saver.Add( 2, &data );
	saver.Add( 3, &nBeginPos );
	saver.Add( 4, &nCurrPos );
	saver.Add( 5, &szName );
	saver.Add( 6, &stats );
	if ( saver.IsReading() ) 
		stats.pszName = szName.c_str();
	return 0;
}
CMemFileSystem::CMemFileSystem( DWORD dwAccessMode )
: dwStorageAccessMode( dwAccessMode )
{

}
IDataStream* CMemFileSystem::CreateStream( const char *pszName, DWORD dwAccessMode )
{
	NI_ASSERT_TF( (dwAccessMode & dwStorageAccessMode) == dwAccessMode, "Can't create stream - invalid access mode", return 0 );
	IDataStream *pStream = new CMemFileStream();
	streams[pszName] = pStream;
	return pStream;
}
IDataStream* CMemFileSystem::OpenStream( const char *pszName, DWORD dwAccessMode )
{
	NI_ASSERT_TF( (dwAccessMode & dwStorageAccessMode) == dwAccessMode, "Can't open stream - invalid access mode", return 0 );
	CStreamsMap::iterator pos = streams.find( pszName );
	if ( pos != streams.end() )
	{
		pos->second->Seek( 0, STREAM_SEEK_SET );
		return pos->second;
	}
	else
		return 0;
}
bool CMemFileSystem::GetStreamStats( const char *pszName, SStorageElementStats *pStats )
{
	CStreamsMap::iterator pos = streams.find( pszName );
	if ( pos == streams.end() )
		return false;
	if ( pos->second.IsValid() ) 
	{
		pos->second->GetStats( pStats );
		pStats->pszName = 0;
		return true;
	}
	return false;
}
bool CMemFileSystem::DestroyElement( const char *pszName )
{
	streams.erase( pszName );
	return true;
}
bool CMemFileSystem::RenameElement( const char *pszOldName, const char *pszNewName )
{
	CPtr<IDataStream> pStream = streams[pszOldName];
	streams.erase( pszOldName );
	if ( pStream != 0 )
	{
		streams[pszNewName] = pStream;
		return true;
	}
	return false;
}
IStorageEnumerator* CMemFileSystem::CreateEnumerator()
{
	return 0;
}
bool CMemFileSystem::AddStorage( IDataStorage *pStorage, const char *pszName )
{
	NI_ASSERT_T( 0, "Can't add new storage to the mem file system" );
	return false;
}
bool CMemFileSystem::RemoveStorage( const char *pszName )
{
	NI_ASSERT_T( 0, "Can't remove storage from mem file system" );
	return false;
}
const bool CMemFileSystem::IsStreamExist( const char *pszName )
{
	return streams.find( pszName ) != streams.end();
}
