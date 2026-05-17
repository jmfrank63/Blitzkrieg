#include "StdAfx.h"

#include "StructureSaver2.h"
#include "ProgressHook.h"

#ifndef _FINALRELEASE
#include "..\AILogic\AIClassesID.h"
#include "..\AILogic\AILogic.h"
#endif // _FINALRELEASE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// int nDataSize;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStructureSaver2::CStructureSaver2( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, 
																	  IProgressHook *pLoadHook, IObjectFactory *_pFactory, IGDB *_pGDB )
: pDstStream( pStream ), pFactory( _pFactory ), pGDB( _pGDB )
{ 
#ifndef _FINALRELEASE
	bCheckResourcesOnLoad = GetGlobalVar( "crconload", 0 ) != 0;
	bCalculateCRC = false;
	bCollectReferedObjects = false;
#endif // _FINALRELEASE
	Start( eAccessMode, pLoadHook ); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStructureSaver2::~CStructureSaver2() 
{ 
#ifndef _FINALRELEASE
	objinfos.clear();
	objset.clear();
	referedObjects.clear();
#endif // _FINALRELEASE
	Finish(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::CChunkLevel::ClearCache() 
{ 
	idLastChunk = (SSChunkID)0xff;
	nLastPos = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::CChunkLevel::Clear() 
{
	idChunk = (SSChunkID)0xff; 
	nChunkNumber = 0; 
	nStart = 0; 
	nLength = 0; 
	ClearCache(); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// chunks operations with whole saves
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool ReadShortChunkSave( IDataStream *pFile, SSChunkID &dwID, CMemoryStream &chunk )
{
	DWORD dwLeng = 0;
	pFile->Read( &dwID, sizeof( dwID ) );
	pFile->Read( &dwLeng, 1 );
	if ( dwLeng & 1 )
		pFile->Read( ((char*)&dwLeng)+1, 3 );
	dwLeng >>= 1;
	/*
	if ( dwLeng > 10000000 )
		return false;
		*/
	chunk.SetSizeDiscard( dwLeng );
	pFile->Read( chunk.GetBufferForWrite(), dwLeng );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool WriteShortChunkSave( IDataStream *pFile, SSChunkID dwID, CMemoryStream &chunk )
{
	DWORD dwLeng;
	pFile->Write( &dwID, sizeof( dwID ) );
	dwLeng = chunk.GetSize();
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool GetShortChunkSave( IDataStream *pFile, SSChunkID dwID, CMemoryStream &chunk )
{
	SSChunkID dwRid;
	pFile->Seek( 0, STREAM_SEEK_SET );
	while( ReadShortChunkSave( pFile, dwRid, chunk ) )
	{
		if ( dwRid == dwID )
			return true;
	}
	chunk.Clear();
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// chunks operations with ChunkLevels
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void ReadPtrData( const unsigned char *pData, void *pDst, int &nPos, int nSize )
{
	memcpy( pDst, pData + nPos, nSize );
	nPos += nSize;
}
// should copy data from start
static void WritePtrData( unsigned char *pDst, const void *pSrc, int *nPos, int nSize )
{
	memcpy( pDst + *nPos, pSrc, nSize );
	*nPos += nSize;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStructureSaver2::ReadShortChunk( CChunkLevel &src, int &nPos, CChunkLevel &res )
{
	const unsigned char *pSrc = data.GetBuffer() + src.nStart;
	DWORD dwLeng = 0;
	if ( nPos + 2 > src.nLength )
		return false;
	ReadPtrData( pSrc, &res.idChunk, nPos, sizeof( res.idChunk ) );
	ReadPtrData( pSrc, &dwLeng, nPos, 1 );
	if ( dwLeng & 1 )
		ReadPtrData( pSrc, ((char*)&dwLeng)+1, nPos, 3 );
	dwLeng >>= 1;
	if ( nPos + dwLeng > src.nLength )
		return false;
	res.nStart = nPos + src.nStart;
	res.nLength = dwLeng;
	nPos += dwLeng;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStructureSaver2::WriteShortChunk( CChunkLevel &dst, SSChunkID dwID, 
																			const unsigned char *pData, int nLength )
{
	DWORD dwLeng;
	data.SetSize( dst.nStart + dst.nLength + 1 + 4 + nLength );
	unsigned char *pDst = data.GetBufferForWrite() + dst.nStart;
	WritePtrData( pDst, &dwID, &dst.nLength, sizeof( dwID ) );
	dwLeng = nLength;
	dwLeng <<= 1;
	if ( dwLeng >= 256 )
	{
		dwLeng |= 1;
		WritePtrData( pDst, &dwLeng, &dst.nLength, sizeof( dwLeng ) );
	}
	else
		WritePtrData( pDst, &dwLeng, &dst.nLength, 1 );
	// prevent copying to itself
	if ( pDst + dst.nLength != pData )
		WritePtrData( pDst, pData, &dst.nLength, nLength );
	else
		dst.nLength += nLength;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStructureSaver2::GetShortChunk( CChunkLevel &src, SSChunkID dwID, CChunkLevel &res, int nNumber )
{
	NI_ASSERT_SLOW( dwID != 0xff );
	int nPos = src.nLastPos; // search from last found position
	int nCounter = nNumber;
	if ( src.idLastChunk == dwID )
	{
		if ( nNumber == src.nLastNumber + 1 )
			nCounter = 1;
		else
		{
			// not sequential access, fall back to linear search
			src.ClearCache();
			return GetShortChunk( src, dwID, res, nNumber );
		}
	}
	else 
	{
		if ( nNumber != 0 )
		{
			if ( src.nLastPos != 0 )
			{
				src.ClearCache();
				return GetShortChunk( src, dwID, res, nNumber );
			}
		}
		else
			nCounter = 1;
	}
	while ( ReadShortChunk( src, nPos, res ) )
	{
		if ( res.idChunk == dwID )
		{
			if ( nCounter == 1 )
			{
				src.idLastChunk = dwID;
				src.nLastPos = nPos;
				src.nLastNumber = nNumber;
				return true;
			}
			nCounter--;
		}
	}
	if ( src.nLastPos == 0 )
		return false;
	// search from start
	src.ClearCache();
	return GetShortChunk( src, dwID, res, nNumber );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CStructureSaver2::CountShortChunks( CChunkLevel &src, SSChunkID dwID )
{
	int nPos = 0, nRes = 0;
	CChunkLevel temp;
	while ( ReadShortChunk( src, nPos, temp ) )
	{
		if ( temp.idChunk == dwID )
			nRes++;
	}
	return nRes;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CStructureSaver2 main methods
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::DataChunk( const SSChunkID idChunk, void *pData, int nSize )
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
		//
#ifndef _FINALRELEASE
		if ( bCalculateCRC ) 
			crcBuffer.CopyToBufRaw( pData, nSize );
#endif // _FINALRELEASE
	}
	else
	{
		// nDataSize += nSize;
		WriteShortChunk( last, idChunk, (const unsigned char*) pData, nSize );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::DataChunk( IDataStream *pStream )
{
	// remember current position in the stream
	int nStreamPos = pStream->GetPos();
	// do all read/write actions
	int nSize = pStream->GetSize();
	DataChunk( 1, &nSize, sizeof(nSize) );
	std::vector<BYTE> buffer( nSize );
	if ( IsReading() )
	{
		DataChunk( 2, &(buffer[0]), nSize );
		pStream->Write( &(buffer[0]), nSize );
		//
#ifndef _FINALRELEASE
		if ( bCalculateCRC ) 
			crcBuffer.CopyToBufRaw( &(buffer[0]), nSize );
#endif // _FINALRELEASE
	}
	else
	{
		pStream->Read( &(buffer[0]), nSize );
		DataChunk( 2, &(buffer[0]), nSize );
	}
	// restore current position in the stream
	pStream->Seek( nStreamPos, STREAM_SEEK_SET );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::WriteRawData( const void *pData, int nSize )
{
	CChunkLevel &res = chunks.back();
	data.SetSize( res.nStart + nSize );
	unsigned char *pDst = data.GetBufferForWrite() + res.nStart;
	WritePtrData( pDst, pData, &res.nLength, nSize );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::RawData( void *pData, int nSize )
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::StoreObject( IRefCount *pObject )
{
	if ( (pObject != 0) && (storedObjects.find(pObject) == storedObjects.end()) )
	{
#ifdef _DO_ASSERT_SLOW
		CPObjectsHashSet::iterator pos = storedObjects.find( pObject );
		NI_ASSERT_SLOW_T( (pos == storedObjects.end()) || ((pos != storedObjects.end()) && (*pos == pObject)), NStr::Format("storing object 0x.8x of type \"%s\", but such object of type \"%s\" already exist", pObject, typeid(*pObject).name(), typeid(*(*pos)).name()) );
#endif // _DO_ASSERT_SLOW
		toStore.push_back( pObject );
		storedObjects.insert( pObject );
	}

	// nDataSize += 4;
	RawData( &pObject, 4 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IRefCount* CStructureSaver2::LoadObject()
{
	void *pServerPtr = 0;
	RawData( &pServerPtr, 4 );
	if ( pServerPtr != 0 )
	{
		CObjectsHash::iterator pFound = objects.find( pServerPtr );
		if ( pFound != objects.end() )
		{
#ifndef _FINALRELEASE
			if ( bCollectReferedObjects ) 
				referedObjects.push_back( pFound->second );
#endif // _FINALRELEASE
			return pFound->second;
		}
		NI_ASSERT_SLOW_T( 0, "Here we are in problem - stored object does not exist. Actually I think we got to throw the exception" );
		// here we are in problem - stored object does not exist
		// actually i think we got to throw the exception
	}
	return reinterpret_cast<IRefCount*>( pServerPtr );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	bool CStructureSaver2::StartChunk( const SSChunkID idChunk )
{
	CChunkLevel &last = chunks.back();
	chunks.emplace_back();
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::FinishChunk()
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::AlignDataFileSize()
{
	CChunkLevel &last = chunks.back();
	data.SetSize( last.nStart + last.nLength );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CStructureSaver2::CountChunks( const SSChunkID idChunk )
{
	return CountShortChunks( chunks.back(), idChunk );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::Start( IStructureSaver::EAccessMode eAccessMode, IProgressHook *pLoadHook )
{
	IDataStream *pRes = pDstStream;
	//
	chunks.clear();
	obj.Clear();
	data.Clear();
	chunks.emplace_back();
	bIsReading = eAccessMode == IStructureSaver::READ;
	if ( IsReading() )
	{
		// read chunk with objects description
		GetShortChunkSave( pRes, 0, obj );
		GetShortChunkSave( pRes, 2, data );
		chunks.back().nLength = data.GetSize();
		// create all objects from obj
		while ( obj.GetPosition() < obj.GetSize() )
		{
			int nTypeID = 0;
			void *pServer = 0;
			bool bValid;
			obj.Read( &nTypeID, 4 );
			obj.Read( &pServer, 4 );
			obj.Read( &bValid, 1 );
			IRefCount *pObject = pFactory->CreateObject( nTypeID );
			NI_ASSERT_SLOW( pObject != 0 );
			toStore.push_back( pObject );
			objects[pServer] = pObject;
			if ( !bValid )
				CObj<IRefCount> pObj = pObject;
			//
#ifndef _FINALRELEASE
			if ( bCheckResourcesOnLoad && (nTypeID > AILOGIC_BASE_VALUE) && (nTypeID < AILOGIC_BASE_VALUE + 0x00010000) ) 
				objset.insert( pObject );
#endif // _FINALRELEASE
		}
		// read information about every created object
		const int nCount = CountChunks( (SSChunkID) 1 );
		if ( nCount > 0 ) 
			NStr::DebugTrace( "**************** ===> Loading %d objects <=== ****************\n", nCount );
		if ( pLoadHook ) 
			pLoadHook->SetNumSteps( nCount + 1, 0.75f );
		for ( int i = 0; i < nCount; ++i )
		{
			void *pServer = 0;
			IRefCount *pObject;
			SetChunkCounter( i + 1 );
			StartChunk( (SSChunkID) 1 );
			DataChunk( 0, &pServer, 4 );
			pObject = objects[pServer];
			NI_ASSERT_SLOW_T( pObject != 0, "NULL object during storing" );

#ifndef _FINALRELEASE
			if ( bCheckResourcesOnLoad && objset.find(pObject) != objset.end() ) 
			{
				bCollectReferedObjects = true;
				bCalculateCRC = true;
				referedObjects.clear();
				crcBuffer.Clear();
			}
#endif // _FINALRELEASE
			if ( pObject )
			{
				if ( StartChunk( 1 ) )
				{
					pObject->operator&( *this );
					FinishChunk();
				}
			}

#ifndef _FINALRELEASE
			if ( bCheckResourcesOnLoad && objset.find(pObject) != objset.end() ) 
			{
				objinfos.push_back( SObjectInfo() );
				SObjectInfo &info = objinfos.back();
				info.pObj = pObject;
				// info.nUID = GetSingleton<IAILogic>()->GetUniqueID( pObject );
				info.referedObjects = referedObjects;
				info.uCheckSum = crcBuffer.GetCRC();
				info.nSize = crcBuffer.GetSize();
				referedObjects.clear();
				crcBuffer.Clear();
			}
			bCollectReferedObjects = false;
			bCalculateCRC = false;
#endif // _FINALRELEASE

			FinishChunk();
			// step hook
			if ( pLoadHook )
				pLoadHook->Step();
		}
		SetChunkCounter( 0 );

		// read main objects data
		chunks.back().Clear();
		GetShortChunkSave( pRes, 1, data );
		chunks.back().nLength = data.GetSize();
		// step hook
		if ( pLoadHook )
			pLoadHook->Step();
		// sort collected AI objects info, generate UIDs for non-UID objects and dump statistics to file
#ifndef _FINALRELEASE
		if ( bCheckResourcesOnLoad && !objinfos.empty() ) 
		{
			IAILogic *pAILogic = GetSingleton<IAILogic>();
			for ( std::vector<SObjectInfo>::iterator it = objinfos.begin(); it != objinfos.end(); ++it )
				it->nUID = pAILogic->GetUniqueID( it->pObj );
			bool bChanged = true;
			int nLastUID = 1L << 30;
			while ( bChanged ) 
			{
				bChanged = false;
				std::sort( objinfos.begin(), objinfos.end() );
				std::vector<SObjectInfo>::iterator pos = std::find_if( objinfos.begin(), objinfos.end(), CFindObjGreaterUID(0) );
				if ( pos != objinfos.end() )
				{
					// make UIDs for non-UID 
					for ( std::vector<SObjectInfo>::iterator it = pos; it != objinfos.end(); ++it )
					{
						for ( std::list<IRefCount*>::iterator itRefObj = it->referedObjects.begin(); itRefObj != it->referedObjects.end(); ++itRefObj )
						{
							std::vector<SObjectInfo>::iterator refobj = std::find_if( objinfos.begin(), pos, CFindObjByObj(*itRefObj) );
							if ( refobj->nUID == 0 ) 
							{
								refobj->nUID = ++nLastUID;
								bChanged = true;
							}
							if ( std::find(it->referedUIDs.begin(), it->referedUIDs.end(), refobj->nUID) == it->referedUIDs.end() ) 
								it->referedUIDs.push_back( refobj->nUID );
						}
					}
				}
			}
			std::sort( objinfos.begin(), objinfos.end() );
			// dump statistics
			std::string szModuleName = "c:\\a7\\savedump.txt";
			/*
			{
				char buffer[2048];
				GetModuleFileName( 0, buffer, 2048 );
				szModuleName = buffer;
				szModuleName.resize( szModuleName.rfind('\\') );
				szModuleName += "savedump.txt";
			}
			*/
			if ( FILE *file = fopen(szModuleName.c_str(), "wt") ) 
			{
				for ( std::vector<SObjectInfo>::iterator it = objinfos.begin(); it != objinfos.end(); ++it )
				{
					// UID (class name): 
					fprintf( file, "0x%.8x: %s has checksum 0x%.8x (size = %d).", it->nUID, typeid(*(it->pObj)).name(), it->uCheckSum, it->nSize );
					if ( !it->referedUIDs.empty() ) 
					{
						fprintf( file, " refered objects: " );
						for ( std::list<int>::const_iterator itRefObj = it->referedUIDs.begin(); itRefObj != it->referedUIDs.end(); ++itRefObj )
							fprintf( file, "0x%.8x ", *itRefObj );
					}
					fprintf( file, "\n" );
				}
				fclose( file );
			}
		}
#endif // _FINALRELEASE		
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// std::unordered_map<int, std::string> type2name;
// std::map<int, int> type2size;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStructureSaver2::Finish()
{
	IDataStream *pRes = pDstStream;
	NI_ASSERT_SLOW( chunks.size() == 1 );
	if ( !IsReading() )
	{
		// save standard data
		AlignDataFileSize();
		WriteShortChunkSave( pRes, 1, data );
		// store referenced objects
		data.Clear();
		chunks.back().Clear();

//		nDataSize = 0;
//		type2name.clear();
//		type2size.clear();

		//CRAP{ for testing
/*
		if ( !IsReading() )
			toStore.push_back( GetSingleton<IAILogic>() );
*/
		//CRAP}
		while ( !toStore.empty() )
		{
			CPtr<IRefCount> pObject = toStore.front();
			toStore.pop_front();
			// save object type and its server pointer
			const int nTypeID = pFactory->GetObjectTypeID( pObject );
			const bool bValid = pObject->IsValid();
			NI_ASSERT_SLOW_T( nTypeID != -1, NStr::Format("unregistered object of type \"%s\"", typeid(*pObject).name()) );

//			if ( nTypeID >= 0x10001000 && nTypeID < 0x10001000 + 255 )
			{
				// const int nOldDataSize = nDataSize;
				
				obj.Write( &nTypeID, 4 );
				obj.Write( &pObject, 4 );
				obj.Write( &bValid, 1 );
				// nDataSize += 9;
				// save object data
				StartChunk( SSChunkID(1) );
				DataChunk( 0, &pObject, 4 );
				//
				if ( StartChunk( 1 ) )
				{
					pObject->operator&( *this );
					FinishChunk();
				}
				FinishChunk();

				/*
				if ( type2size.find( nTypeID ) == type2size.end() )
				{
					type2size[nTypeID] = nDataSize - nOldDataSize;
					type2name[nTypeID] = typeid( *pObject ).name();
				}
				else
					type2size[nTypeID] += nDataSize - nOldDataSize;
					*/
			}
		}
		// save data into resulting file
		WriteShortChunkSave( pRes, 0, obj );
		AlignDataFileSize();
		WriteShortChunkSave( pRes, 2, data );
	}

	/*
	if ( !IsReading() )
	{
		NStr::DebugTrace( "===================================> data size %d <=================================\n", nDataSize );

		for ( std::map<int, int>::iterator iter = type2size.begin(); iter != type2size.end(); ++iter )
		{
			GetSingleton<IConsoleBuffer>()->WriteASCII( 10,
				NStr::Format( "object %s, size %d", type2name[iter->first].c_str(), iter->second ),
				0, true
			);
		}

		GetSingleton<IConsoleBuffer>()->DumpLog( 10 );
	}
	*/

	obj.Clear();
	data.Clear();
	objects.clear();
	storedObjects.clear();
	toStore.clear();
	chunks.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
