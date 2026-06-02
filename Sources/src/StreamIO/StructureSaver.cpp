#include "StdAfx.h"

#include "StructureSaverInternal.h"
#include "DataTreeXML.h"
CSaveLoadSystem theSaveLoadSystem;
ISaveLoadSystem* STDCALL GetSLS()
{
	return &theSaveLoadSystem;
}
CSaveLoadSystem::CSaveLoadSystem()
{
}
CSaveLoadSystem::~CSaveLoadSystem()
{
	if ( pFactory )
		delete pFactory;
}
void CSaveLoadSystem::AddFactory( IObjectFactory *_pFactory )
{
	if ( pFactory == 0 )	
		pFactory = new CBasicObjectFactory();
	NI_ASSERT_SLOW_TF( pFactory != 0, "basic save-load factory was not created", return );
	pFactory->Aggregate( _pFactory );
}
IStructureSaver* CSaveLoadSystem::CreateStructureSaver( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, 
		IStructureSaver::EStoreMode eStoreMode = IStructureSaver::ALL )
{
	NI_ASSERT_TF( pStream != 0, "Can't create structure saver from NULL stream", return 0 );
	return new CStructureSaver( pStream, eAccessMode, eStoreMode, pFactory, pGDB );
}
IDataTree* CSaveLoadSystem::CreateDataTreeSaver( IDataStream *pStream, IDataTree::EAccessMode eAccessMode, DTChunkID idBaseNode )
{
	NI_ASSERT_TF( pStream != 0, "Can't create data tree saver from NULL stream", return 0 );
	InitCOM();
	CDataTreeXML *pDT = new CDataTreeXML( eAccessMode );
	pDT->Open( pStream, idBaseNode );
	return pDT;
}
bool ReadShortChunkSave( IDataStream *pFile, SSChunkID &dwID, CMemoryStream &chunk )
{
	DWORD dwLeng = 0;
	pFile->Read( &dwID, sizeof( dwID ) );
	pFile->Read( &dwLeng, 1 );
	if ( dwLeng & 1 )
		pFile->Read( ((char*)&dwLeng) + 1, 3 );
	dwLeng >>= 1;
	if ( dwLeng > 10000000 )
		return false;
	chunk.SetSizeDiscard( dwLeng );
	DWORD dwRead = pFile->Read( chunk.GetBufferForWrite(), dwLeng );
	return dwRead == dwLeng;
}
bool WriteShortChunkSave( IDataStream *pFile, SSChunkID dwID, CMemoryStream &chunk )
{
	pFile->Write( &dwID, sizeof( dwID ) );
	DWORD dwLeng = chunk.GetSize();
	dwLeng <<= 1;
	if ( dwLeng >= 256 )
	{
		dwLeng |= 1;
		pFile->Write( &dwLeng, sizeof( dwLeng ) );
	}
	else
		pFile->Write( &dwLeng, 1 );
	pFile->Write( chunk.GetBuffer(), chunk.GetSize() );
	return true;
}
bool GetShortChunkSave( IDataStream *pFile, SSChunkID dwID, CMemoryStream &chunk, int nNumber = 1 )
{
	SSChunkID dwRid;
	pFile->Seek( 0, STREAM_SEEK_SET );
	while( ReadShortChunkSave( pFile, dwRid, chunk ) )
	{
		if ( dwRid == dwID )
		{
			if ( nNumber == 1 )
				return true;
			nNumber--;
		}
	}
	chunk.Clear();
	return false;
}
template <int N>
inline void ReadPtrData( const unsigned char *pData, void *pDst, int &nPos, const SGenericNumber<N> &number )
{
	memcpy( pDst, pData + nPos, N );
	nPos += N;
}
template <>
inline void ReadPtrData( const unsigned char *pData, void *pDst, int &nPos, const SGenericNumber<1> &number )
{
	*((unsigned char*)pDst) = *( pData + nPos );
	++nPos;
}
template <>
inline void ReadPtrData( const unsigned char *pData, void *pDst, int &nPos, const SGenericNumber<3> &number )
{
	*((unsigned short*)pDst) = *((unsigned short*)(pData + nPos));
	*(((unsigned char*)pDst) + 2) = *( pData + nPos + 2 );
	nPos += 3;
}
void WritePtrData( unsigned char *pDst, const void *pSrc, int *nPos, int nSize )
{
	memcpy( pDst + *nPos, pSrc, nSize );
	*nPos += nSize;
}
bool CStructureSaver::ReadShortChunk( CChunkLevel &src, int &nPos, CChunkLevel &res )
{
	const unsigned char *pSrc = data.GetBuffer() + src.nStart;
	DWORD dwLeng = 0;
	if ( nPos + 2 > src.nLength )
		return false;
	ReadPtrData( pSrc, &res.idChunk, nPos, SGenericNumber<sizeof(res.idChunk)>() );
	ReadPtrData( pSrc, &dwLeng, nPos, SGenericNumber<1>() );
	if ( dwLeng & 1 )
		ReadPtrData( pSrc, ((char*)&dwLeng)+1, nPos, SGenericNumber<3>() );
	dwLeng >>= 1;
	if ( nPos + dwLeng > src.nLength )
		return false;
	res.nStart = nPos + src.nStart;
	res.nLength = dwLeng;
	nPos += dwLeng;
	return true;
}
bool CStructureSaver::WriteShortChunk( CChunkLevel &dst, SSChunkID dwID, const unsigned char *pData, int nLength )
{
	data.SetSize( dst.nStart + dst.nLength + 1 + 4 + nLength );
	unsigned char *pDst = data.GetBufferForWrite() + dst.nStart;
	WritePtrData( pDst, &dwID, &dst.nLength, sizeof( dwID ) );
	DWORD dwLeng = nLength << 1;
	if ( dwLeng >= 256 )
	{
		dwLeng |= 1;
		WritePtrData( pDst, &dwLeng, &dst.nLength, sizeof( dwLeng ) );
	}
	else
		WritePtrData( pDst, &dwLeng, &dst.nLength, 1 );
	if ( pDst + dst.nLength != pData )
		WritePtrData( pDst, pData, &dst.nLength, nLength );
	else
		dst.nLength += nLength;
	return true;
}
bool CStructureSaver::GetShortChunk( CChunkLevel &src, SSChunkID dwID, CChunkLevel &res, int nNumber )
{
	int nPos = 0;
	while ( ReadShortChunk( src, nPos, res ) )
	{
		if ( res.idChunk == dwID )
		{
			if ( nNumber == 1 )
				return true;
			nNumber--;
		}
	}
	return false;
}
int CStructureSaver::CountShortChunks( CChunkLevel &src, SSChunkID dwID )
{
	int nPos = 0, nRes = 0;
	CChunkLevel temp;
	while ( ReadShortChunk( src, nPos, temp ) )
	{
		if ( temp.idChunk == dwID )
			++nRes;
	}
	return nRes;
}
void CStructureSaver::DataChunk( const SSChunkID idChunk, void *pData, int nSize )
{
	CChunkLevel &last = chunks.back();
	if ( IsReading() )
	{
		CChunkLevel res;
		if ( GetShortChunk( last, idChunk, res, last.nChunkNumber ) )
		{
			NI_ASSERT_SLOW( res.nLength == nSize );
			memcpy( pData, data.GetBuffer() + res.nStart, nSize );
		}
		else
			memset( pData, 0, nSize );
	}
	else
		WriteShortChunk( last, idChunk, (const unsigned char*) pData, nSize );
}
void CStructureSaver::DataChunk( IDataStream *pStream )
{
	int nStreamPos = pStream->GetPos();
	int nSize = pStream->GetSize();
	DataChunk( 1, &nSize, sizeof(nSize) );
	std::vector<BYTE> buffer( nSize );
	if ( IsReading() )
	{
		DataChunk( 2, &(buffer[0]), nSize );
		pStream->Write( &(buffer[0]), nSize );
	}
	else
	{
		pStream->Read( &(buffer[0]), nSize );
		DataChunk( 2, &(buffer[0]), nSize );
	}
	pStream->Seek( nStreamPos, STREAM_SEEK_SET );
}
void CStructureSaver::WriteRawData( const void *pData, int nSize )
{
	CChunkLevel &res = chunks.back();
	data.SetSize( res.nStart + nSize );
	unsigned char *pDst = data.GetBufferForWrite() + res.nStart;
	WritePtrData( pDst, pData, &res.nLength, nSize );
}
void CStructureSaver::RawData( void *pData, int nSize )
{
	if ( IsReading() )
	{
		CChunkLevel &res = chunks.back();
		NI_ASSERT_SLOW( res.nLength == nSize );
		memcpy( pData, data.GetBuffer() + res.nStart, nSize );
	}
	else
	{
		WriteRawData( pData, nSize );
	}
}
void CStructureSaver::StoreObject( IRefCount *pObject )
{
	if ( (pObject != 0) && (storedObjects.find(pObject) == storedObjects.end()) && !IsDataOnly() )
	{
		toStore.push_back( pObject );
		storedObjects[pObject] = true;			// важно присвоить хоть что-нибудь
	}
	RawData( &pObject, 4 );
}
IRefCount* CStructureSaver::LoadObject()
{
	void *pServerPtr = 0;
	RawData( &pServerPtr, 4 );
	if ( (pServerPtr != 0) && !IsDataOnly() )
	{
		CObjectsHash::iterator pFound = objects.find( pServerPtr );
		if ( pFound != objects.end() )
			return pFound->second;
		NI_ASSERT_SLOW_T( 0, "we are in problem - stored object does not exist" );
	}
	return reinterpret_cast<IRefCount*>( pServerPtr );
}
bool CStructureSaver::StartChunk( const SSChunkID idChunk )
{
	CChunkLevel &last = chunks.back();
	chunks.push_back();
	if ( IsReading() ) 
	{
		bool bRes = GetShortChunk( last, idChunk, chunks.back(), last.nChunkNumber );
		if ( !bRes )
			chunks.pop_back();
		return bRes;
	}
	else 
	{
		CChunkLevel &newChunk = chunks.back();
		newChunk.idChunk = idChunk;
		newChunk.nStart = last.nStart + last.nLength + sizeof( SSChunkID ) + 4;
		return true;
	}
}
void CStructureSaver::FinishChunk()
{
	if ( IsReading() ) 
	{
		chunks.pop_back();
	}
	else 
	{
		CChunkLevelReverseIterator it = chunks.rbegin(), it1;
		it1 = it; ++it1;
		WriteShortChunk( *it1, it->idChunk, data.GetBuffer() + it->nStart, it->nLength );
		chunks.pop_back();
		AlignDataFileSize();
	}
}
void CStructureSaver::AlignDataFileSize()
{
	CChunkLevel &last = chunks.back();
	data.SetSize( last.nStart + last.nLength );
}
int CStructureSaver::CountChunks( const SSChunkID idChunk )
{
	return CountShortChunks( chunks.back(), idChunk );
}
void CStructureSaver::SetChunkCounter( int nCount ) 
{ 
	chunks.back().nChunkNumber = nCount; 
}
void CStructureSaver::Start( IStructureSaver::EAccessMode eAccessMode, IStructureSaver::EStoreMode _eStoreMode )
{
	IDataStream *pRes = pDstStream;
	chunks.clear();
	obj.Clear();
	data.Clear();
	chunks.push_back();
	bReading = eAccessMode == IStructureSaver::READ;
	eStoreMode = _eStoreMode;
	if ( bReading )
	{
		GetShortChunkSave( pRes, 0, obj );
		GetShortChunkSave( pRes, 2, data );
		chunks.back().nLength = data.GetSize();
		while ( obj.GetPosition() < obj.GetSize() )
		{
			int nTypeID = 0;
			void *pServer = 0;
			bool bValid;
			obj.Read( &nTypeID, 4 );
			obj.Read( &pServer, 4 );
			obj.Read( &bValid, 1 );
			IRefCount *pObject = pFactory->CreateObject( nTypeID );
			toStore.push_back( pObject );
			objects[pServer] = pObject;
			if ( !bValid )
				CObj<IRefCount> pObj = pObject;
		}
		int nCount = CountChunks( SSChunkID( 1 ) );
		for ( int i=0; i<nCount; ++i )
		{
			void *pServer = 0;

			SetChunkCounter( i + 1 );
			StartChunk( SSChunkID( 1 ) );
			DataChunk( 0, &pServer, 4 );
			IRefCount *pObject = objects[pServer];
			NI_ASSERT_SLOW_T( pObject != 0, "NULL object during storing" );
			if ( pObject )
			{
				if ( StartChunk( 1 ) )
				{
					pObject->operator&( *this );
					FinishChunk();
				}
			}

			FinishChunk();
		}
		SetChunkCounter( 1 );

		chunks.back().Clear();
		GetShortChunkSave( pRes, 1, data );
		chunks.back().nLength = data.GetSize();
	}
}
void CStructureSaver::Finish()
{
	IDataStream *pRes = pDstStream;
	NI_ASSERT_SLOW( chunks.size() == 1 );
	if ( !IsReading() )
	{
		AlignDataFileSize();
		WriteShortChunkSave( pRes, 1, data );
		data.Clear();
		chunks.back().Clear();
		while ( !toStore.empty() )
		{
			CPtr<IRefCount> pObject = toStore.front();
			toStore.pop_front();
			int nTypeID = pFactory->GetObjectTypeID( pObject );
			bool bValid = pObject->IsValid();
			NI_ASSERT_SLOW_T( nTypeID != -1, NStr::Format("unregistered object of type \"%s\"", typeid(*pObject).name()) );
			obj.Write( &nTypeID, 4 );
			obj.Write( &pObject, 4 );
			obj.Write( &bValid, 1 );
			StartChunk( SSChunkID( 1 ) );
			DataChunk( 0, &pObject, 4 );
			if ( StartChunk( 1 ) )
			{
				pObject->operator&( *this );
				FinishChunk();
			}
			FinishChunk();
		}
		WriteShortChunkSave( pRes, 0, obj );
		AlignDataFileSize();
		WriteShortChunkSave( pRes, 2, data );
	}
	obj.Clear();
	data.Clear();
	objects.clear();
	storedObjects.clear();
	toStore.clear();
	chunks.clear();
}
