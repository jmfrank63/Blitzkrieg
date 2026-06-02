#ifndef __STRUCTURESAVER_H__
#define __STRUCTURESAVER_H__
typedef char SSChunkID;
interface IStructureSaver : public IRefCount
{
	enum EAccessMode
	{
		READ  = 1,													// open SS to read data from
		WRITE = 2														// open SS to write data to
	};
	enum EStoreMode
	{
		ALL = 1,														// store data and store objects with re-creation, etc... - complete save/load mode
		DATAONLY = 2												// store data only. don't re-create objects - data save/load mode - special for internal data access
	};
	virtual bool STDCALL StartChunk( const SSChunkID idChunk ) = 0;
	virtual void STDCALL FinishChunk() = 0;
	virtual void STDCALL DataChunk( const SSChunkID idChunk, void *pData, int nSize ) = 0;
	virtual void STDCALL DataChunk( IDataStream *pStream ) = 0;
	virtual int STDCALL CountChunks( const SSChunkID idChunk ) = 0;
	virtual void STDCALL SetChunkCounter( int nCount ) = 0;
	virtual bool STDCALL IsReading() const = 0;
	virtual IRefCount* STDCALL LoadObject() = 0;
	virtual void STDCALL StoreObject( IRefCount *pObj ) = 0;
	virtual interface IGDB* STDCALL GetGDB() = 0;
};
typedef const char* DTChunkID;
interface IDataTree : public IRefCount
{
	enum EAccessMode
	{
		WRITE = 1,
		READ = 2
	};
	virtual bool STDCALL IsReading() const = 0;
	virtual int STDCALL StartChunk( DTChunkID idChunk ) = 0;
	virtual void STDCALL FinishChunk() = 0;
	virtual int STDCALL GetChunkSize() = 0;
	virtual bool STDCALL RawData( void *pData, int nSize ) = 0;
	virtual bool STDCALL StringData( char *pData ) = 0;
	virtual bool STDCALL StringData( WORD *pData ) = 0;
	virtual bool STDCALL DataChunk( DTChunkID idChunk, int *pData ) = 0;
	virtual bool STDCALL DataChunk( DTChunkID idChunk, double *pData ) = 0;
	virtual int STDCALL CountChunks( DTChunkID idChunk ) = 0;
	virtual bool STDCALL SetChunkCounter( int nCount ) = 0;
	virtual int STDCALL StartContainerChunk( DTChunkID idChunk ) = 0;
	virtual void STDCALL FinishContainerChunk() = 0;
};
interface ISaveLoadSystem
{
	virtual void STDCALL AddFactory( IObjectFactory *pFactory ) = 0;
	virtual IObjectFactory* STDCALL GetCommonFactory() = 0;
	virtual void STDCALL SetGDB( IGDB *pGDB ) = 0;
	virtual IStructureSaver* STDCALL CreateStructureSaver( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, 
		                                                     interface IProgressHook *pLoadHook = 0 ) = 0;
	virtual IDataTree* STDCALL CreateDataTreeSaver( IDataStream *pStream, IDataTree::EAccessMode eAccessMode, 
		                                              DTChunkID idBaseNode = "base" ) = 0;
	virtual IDataStorage* STDCALL OpenStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE ) = 0;
	virtual IDataStorage* STDCALL CreateStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE ) = 0;
	virtual IDataBase* STDCALL OpenDataBase( const char *pszName, DWORD dwAccessMode, DWORD type = DB_TYPE_INI ) = 0;
	virtual IDataTable* STDCALL OpenDataTable( IDataStream *pStream, const char *pszBaseNode = "base" ) = 0;
	virtual IDataTable* STDCALL OpenIniDataTable( IDataStream *pStream ) = 0;
};
extern ISaveLoadSystem *g_pGlobalSaveLoadSystem;
inline ISaveLoadSystem* GetSLS() { return g_pGlobalSaveLoadSystem; }
inline IStructureSaver* CreateStructureSaver( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, 
		                                          interface IProgressHook *pLoadHook = 0 )
{
	return GetSLS()->CreateStructureSaver( pStream, eAccessMode, pLoadHook );
}
inline IDataTree* CreateDataTreeSaver( IDataStream *pStream, IDataTree::EAccessMode eAccessMode, 
		                                   DTChunkID idBaseNode = "base" )
{
	return GetSLS()->CreateDataTreeSaver( pStream, eAccessMode, idBaseNode );
}
inline void AddFactory( IObjectFactory *pFactory ) { GetSLS()->AddFactory( pFactory ); }
inline IObjectFactory* GetCommonFactory() { return GetSLS()->GetCommonFactory(); }
template <class TYPE>
	inline TYPE* CreateObject( IObjectFactory *pFactory, int nTypeID )
	{
		NI_ASSERT_TF( pFactory != 0, NStr::Format("Can't create object 0x%x: factory is not registered", nTypeID), return 0 );
		return static_cast<TYPE*>( pFactory->CreateObject(nTypeID) );
	}
template <class TYPE> 
	inline TYPE* CreateObject( int nTypeID ) { return CreateObject<TYPE>( GetCommonFactory(), nTypeID ); }
inline IDataStorage* OpenStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE )
{
	return GetSLS()->OpenStorage( pszName, dwAccessMode, type );
}
inline IDataStorage* CreateStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE )
{
	return GetSLS()->CreateStorage( pszName, dwAccessMode, type );
}
inline IDataStream* OpenFileStream( const std::string &szFullName, DWORD dwAccessMode )
{
	const int nPos = szFullName.rfind( '\\' );
	const std::string szPathName = nPos != std::string::npos ? szFullName.substr( 0, nPos + 1 ) : ".\\";
	const std::string szFileName = nPos != std::string::npos ? szFullName.substr( nPos + 1 ) : szFullName;
	CPtr<IDataStorage> pStorage = OpenStorage( szPathName.c_str(), dwAccessMode, STORAGE_TYPE_FILE );
	return pStorage->OpenStream( szFileName.c_str(), dwAccessMode );
}
inline IDataStream* CreateFileStream( const std::string &szFullName, DWORD dwAccessMode )
{
	const int nPos = szFullName.rfind( '\\' );
	const std::string szPathName = nPos != std::string::npos ? szFullName.substr( 0, nPos + 1 ) : ".\\";
	const std::string szFileName = nPos != std::string::npos ? szFullName.substr( nPos + 1 ) : szFullName;
	CPtr<IDataStorage> pStorage = CreateStorage( szPathName.c_str(), dwAccessMode, STORAGE_TYPE_FILE );
	return pStorage->CreateStream( szFileName.c_str(), dwAccessMode );
}
inline IDataBase* OpenDataBase( const char *pszName, DWORD dwAccessMode, DWORD type = DB_TYPE_INI )
{
	return GetSLS()->OpenDataBase( pszName, dwAccessMode, type );
}
inline IDataTable* OpenDataTable( IDataStream *pStream, const char *pszBaseNode = "base" )
{
	return GetSLS()->OpenDataTable( pStream, pszBaseNode );
}
inline IDataTable* OpenIniDataTable( IDataStream *pStream )
{
	return GetSLS()->OpenIniDataTable( pStream );
}
#ifndef _DONT_USE_SINGLETON
namespace NDB
{
	inline IDataTable* OpenDataTable( const char *pszName )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( pszName, STREAM_ACCESS_READ );
		NI_ASSERT_TF( pStream != 0, NStr::Format("Can't open stream \"%s\"", pszName), return 0 );
		return ::OpenDataTable( pStream );
	}
};
#endif // _DONT_USE_SINGLETON
#endif // __STRUCTURESAVER_H__
