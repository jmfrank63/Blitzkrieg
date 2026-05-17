#ifndef __STRUCTURESAVER_INTERNAL_H__
#define __STRUCTURESAVER_INTERNAL_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "StructureSaver.h"
#include "Streams.h"
#include "..\Misc\BasicObjectFactory.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSaveLoadSystem : public ISaveLoadSystem
{
	CBasicObjectFactory *pFactory;
	IGDB *pGDB;
public:
	CSaveLoadSystem();
	virtual ~CSaveLoadSystem();
	//
	virtual void STDCALL AddFactory( IObjectFactory *pFactory );
	virtual void STDCALL SetGDB( IGDB *_pGDB ) { pGDB = _pGDB; }
	virtual IObjectFactory* STDCALL GetCommonFactory() { return pFactory; }
	virtual IStructureSaver* STDCALL CreateStructureSaver( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, 
		                                                     IStructureSaver::EStoreMode eStoreMode );
	virtual IDataTree* STDCALL CreateDataTreeSaver( IDataStream *pStream, IDataTree::EAccessMode eAccessMode, DTChunkID idBaseNode );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// a) chunk structure
// b) ptr/ref storage
// system is able to store ref/ptr only for objectbase ancestors
// final save file structure
// -header section list of object types with pointers
// -object data separated in chunks one chunk per object
// c) can replace CMemoryStream with specialized objects to increase perfomance

// chunk with index 0 is used for system and should not be used in user code
typedef char SSChunkID;
class CStructureSaver : public IStructureSaver
{
	OBJECT_MINIMAL_METHODS( CStructureSaver );
	//
	CPtr<IDataStream> pDstStream;
	IObjectFactory *pFactory;
	IGDB *pGDB;
	
	struct CChunkLevel
	{
		SSChunkID idChunk;
		int nStart, nLength;
		int nChunkNumber; // ����� ����� �� ������� ��� ���������� - ������������ ��� ������/���������� vector/list
		
		void Clear() { idChunk = (SSChunkID)0xff; nChunkNumber = 1; nStart = 0; nLength = 0; }
		CChunkLevel() { Clear(); }
	};
	// objects descriptors
	CMemoryStream obj;
	// objects data
	CMemoryStream data;
	std::list<CChunkLevel> chunks;
	typedef std::list<CChunkLevel>::iterator CChunkLevelIterator;
	typedef std::list<CChunkLevel>::reverse_iterator CChunkLevelReverseIterator;
	bool bReading;
	IStructureSaver::EStoreMode eStoreMode;	// we can store data only and can store with objects re-creation...
	// maps objects addresses during save(first) to addresses during load(second) - during loading
	// or serves as a sign that some object has been already stored - during storing
	typedef std::unordered_map<void*, CPtr<IRefCount>, SDefaultPtrHash> CObjectsHash;
	CObjectsHash objects;
	typedef std::unordered_map<void*,bool,SDefaultPtrHash> CPObjectsHash;
	CPObjectsHash storedObjects;
	std::list< CPtr<IRefCount> > toStore;

	bool ReadShortChunk( CChunkLevel &src, int &nPos, CChunkLevel &res );
	bool WriteShortChunk( CChunkLevel &dst, SSChunkID dwID, const unsigned char *pData, int nLength );
	bool GetShortChunk( CChunkLevel &src, SSChunkID dwID, CChunkLevel &res, int nNumber = 1 );
	int CountShortChunks( CChunkLevel &src, SSChunkID dwID );
	//
	void AlignDataFileSize();
	//
	void RawData( void *pData, int nSize );
	void WriteRawData( const void *pData, int nSize );
	//
	void Start( IStructureSaver::EAccessMode eAccessMode, IStructureSaver::EStoreMode eStoreMode );
	void Finish();
	//
	bool IsDataOnly() const { return eStoreMode == IStructureSaver::DATAONLY; }
public:
	CStructureSaver( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, IStructureSaver::EStoreMode eStoreMode, IObjectFactory *_pFactory, IGDB *_pGDB )
		: pDstStream( pStream ), pFactory( _pFactory ), pGDB( _pGDB ) { Start( eAccessMode, eStoreMode ); }
	~CStructureSaver() { Finish(); }
	//
	// start new complex chunk
	virtual bool STDCALL StartChunk( const SSChunkID idChunk );
	// finish complex chunk
	virtual void STDCALL FinishChunk();
	// simply raw data chunk
	virtual void STDCALL DataChunk( const SSChunkID idChunk, void *pData, int nSize );
	// data stream as data chunk
	virtual void STDCALL DataChunk( IDataStream *pStream );
	// count number of subchunks in the given chunk
	virtual int STDCALL CountChunks( const SSChunkID idChunk );
	// set number of subchunks in the given chunk
	virtual void STDCALL SetChunkCounter( int nCount );
	// is structure saver opened in the READ mode?
	virtual bool STDCALL IsReading() const { return bReading; }
	// �������� ������� � ������������ ���
	virtual IRefCount* STDCALL LoadObject();
	// ������ ������� � ������, ����������� ��� ��� ����������� ��� ��������
	virtual void STDCALL StoreObject( IRefCount *pObj );
	// �������� ��������� �� ������� ���� ������
	virtual interface IGDB* STDCALL GetGDB() { return pGDB; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __STRUCTURESAVER_INTERNAL_H__
