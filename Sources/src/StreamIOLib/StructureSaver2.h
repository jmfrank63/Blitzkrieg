#ifndef __BASICCHUNK1_H_
#define __BASICCHUNK1_H_
#pragma ONCE
#include "Streams.h"
#include "StructureSaver.h"
#include "../Misc/CheckSums.h"

#ifndef _FINALRELEASE
struct SObjectInfo
{
	IRefCount *pObj;											// object itself
	int nUID;															// object's unique ID
	uLong uCheckSum;											// checksum (CRC32)
	int nSize;														// object's size
	std::list<IRefCount*> referedObjects;	// refered objects
	std::list<int> referedUIDs;
	SObjectInfo() : nUID( 0 ) {  }
	explicit SObjectInfo( int _nUID ) : nUID( _nUID ) {  }
};
inline bool operator<( const SObjectInfo &i1, const SObjectInfo &i2 ) { return i1.nUID < i2.nUID; }
class CFindObjGreaterUID
{
	const int nUID;
public:
	CFindObjGreaterUID( const int _nUID ) : nUID( _nUID ) {  }
	bool operator()( const SObjectInfo &info ) const { return info.nUID > nUID; }
};
class CFindObjByObj
{
	const IRefCount *pObj;
public:
	CFindObjByObj( const IRefCount *_pObj ) : pObj( _pObj ) {  }
	bool operator()( const SObjectInfo &info ) const { return info.pObj == pObj; }
};
#endif // _FINALRELEASE
class CStructureSaver2 : public IStructureSaver
{
	OBJECT_MINIMAL_METHODS( CStructureSaver2 );
	struct CChunkLevel
	{
		SSChunkID idChunk, idLastChunk;
		int nStart, nLength;
		int nChunkNumber; // ����� ����� �� ������� ��� ���������� - ������������ ��� ������/���������� vector/list
		int nLastPos, nLastNumber;

		void ClearCache();
		void Clear();
		CChunkLevel() { Clear(); }
	};
	CPtr<IDataStream> pDstStream;
	IObjectFactory *pFactory;
	IGDB *pGDB;
	CMemoryStream obj;
	CMemoryStream data;
	std::list<CChunkLevel> chunks;
	typedef std::list<CChunkLevel>::iterator CChunkLevelIterator;
	typedef std::list<CChunkLevel>::reverse_iterator CChunkLevelReverseIterator;
	bool bIsReading;
	typedef std::unordered_map<void*, CPtr<IRefCount>, SDefaultPtrHash> CObjectsHash;
	CObjectsHash objects;
	typedef std::unordered_set<IRefCount*, SDefaultPtrHash> CPObjectsHashSet;
	CPObjectsHashSet storedObjects;
	std::list< CPtr<IRefCount> > toStore;
#ifndef _FINALRELEASE
	bool bCheckResourcesOnLoad;
	NCheckSums::SCheckSumBufferStorage crcBuffer;
	bool bCalculateCRC;
	bool bCollectReferedObjects;
	typedef std::vector<SObjectInfo> CObjectInfoList;
	CObjectInfoList objinfos;
	std::unordered_set<IRefCount*, SDefaultPtrHash> objset;
	std::list<IRefCount*> referedObjects;
#endif // _FINALRELEASE
	bool ReadShortChunk( CChunkLevel &src, int &nPos, CChunkLevel &res );
	bool WriteShortChunk( CChunkLevel &dst, SSChunkID dwID, const unsigned char *pData, int nLength );
	bool GetShortChunk( CChunkLevel &src, SSChunkID dwID, CChunkLevel &res, int nNumber );
	int CountShortChunks( CChunkLevel &src, SSChunkID dwID );
	void AlignDataFileSize();
	void RawData( void *pData, int nSize );
	void WriteRawData( const void *pData, int nSize );
	void Start( IStructureSaver::EAccessMode eAccessMode, interface IProgressHook *pHook );
	void Finish();
public:
	CStructureSaver2( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, 
		                interface IProgressHook *pLoadHook, IObjectFactory *_pFactory, IGDB *_pGDB );
	~CStructureSaver2();
	virtual bool STDCALL StartChunk( const SSChunkID idChunk );
	virtual void STDCALL FinishChunk();
	virtual void STDCALL DataChunk( const SSChunkID idChunk, void *pData, int nSize );
	virtual void STDCALL DataChunk( IDataStream *pStream );
	virtual int STDCALL CountChunks( const SSChunkID idChunk );
	virtual void STDCALL SetChunkCounter( int nCount ) { chunks.back().nChunkNumber = nCount; }
	virtual bool STDCALL IsReading() const { return bIsReading; }
	virtual IRefCount* STDCALL LoadObject();
	virtual void STDCALL StoreObject( IRefCount *pObj );
	virtual interface IGDB* STDCALL GetGDB() { return pGDB; }
};
#endif
