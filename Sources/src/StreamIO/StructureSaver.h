#ifndef __STRUCTURESAVER_H__
#define __STRUCTURESAVER_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** structure saver interface
// ** для записи данных в поток в виде chunk'ов
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	// start new complex chunk
	virtual bool STDCALL StartChunk( const SSChunkID idChunk ) = 0;
	// finish complex chunk
	virtual void STDCALL FinishChunk() = 0;
	// simply raw data chunk
	virtual void STDCALL DataChunk( const SSChunkID idChunk, void *pData, int nSize ) = 0;
	// data stream as data chunk
	virtual void STDCALL DataChunk( IDataStream *pStream ) = 0;
	// count number of subchunks in the given chunk
	virtual int STDCALL CountChunks( const SSChunkID idChunk ) = 0;
	// set number of subchunks in the given chunk
	virtual void STDCALL SetChunkCounter( int nCount ) = 0;
	// is structure saver opened in the READ mode?
	virtual bool STDCALL IsReading() const = 0;
	// загрузка объекта с воссозданием его
	virtual IRefCount* STDCALL LoadObject() = 0;
	// запись объекта и данных, необходимых для его воссоздания при загрузке
	virtual void STDCALL StoreObject( IRefCount *pObj ) = 0;
	// получить указатель на игровую базу данных
	virtual interface IGDB* STDCALL GetGDB() = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** data tree interface
// **	для записи/чтения данных из потока в виде дерева.
// **	отличается от IStructureSaver'а тем, что предназначен только для данных (не поддерживает восстановление объектов)
// **	и, т.ж., записывает информацию в текстовые файлы в удобном для последующего просмотра в third-party editor'е виде.
// ** По большому счёту, данный интерфейс избыточен. 
// ** Но это идёт из-за того, что необходимо записать данные в текстовый файл так, 
// ** чтобы потом их было удобно читать в каком либо third-party viewer'е
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef const char* DTChunkID;
interface IDataTree : public IRefCount
{
	enum EAccessMode
	{
		WRITE = 1,
		READ = 2
	};
	// is opened in the READ mode?
	virtual bool STDCALL IsReading() const = 0;
	// start new complex chunk
	virtual int STDCALL StartChunk( DTChunkID idChunk ) = 0;
	// finish complex chunk
	virtual void STDCALL FinishChunk() = 0;
	// simply data chunk: text, integer, fp
	virtual int STDCALL GetChunkSize() = 0;
	virtual bool STDCALL RawData( void *pData, int nSize ) = 0;
	virtual bool STDCALL StringData( char *pData ) = 0;
	virtual bool STDCALL StringData( WORD *pData ) = 0;
	virtual bool STDCALL DataChunk( DTChunkID idChunk, int *pData ) = 0;
	virtual bool STDCALL DataChunk( DTChunkID idChunk, double *pData ) = 0;
	// array data serialization (special case)
	virtual int STDCALL CountChunks( DTChunkID idChunk ) = 0;
	virtual bool STDCALL SetChunkCounter( int nCount ) = 0;
	virtual int STDCALL StartContainerChunk( DTChunkID idChunk ) = 0;
	virtual void STDCALL FinishContainerChunk() = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** save/load system
// ** создание saver'а и регистрация объектов, производных от IRefCount 
// ** для последующего автоматического создания их при загрузке
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface ISaveLoadSystem
{
	// common factory
	virtual void STDCALL AddFactory( IObjectFactory *pFactory ) = 0;
	virtual IObjectFactory* STDCALL GetCommonFactory() = 0;
	virtual void STDCALL SetGDB( IGDB *pGDB ) = 0;
	// structure and text tree savers
	virtual IStructureSaver* STDCALL CreateStructureSaver( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, 
		                                                     interface IProgressHook *pLoadHook = 0 ) = 0;
	virtual IDataTree* STDCALL CreateDataTreeSaver( IDataStream *pStream, IDataTree::EAccessMode eAccessMode, 
		                                              DTChunkID idBaseNode = "base" ) = 0;
	// storage opening/creating
	virtual IDataStorage* STDCALL OpenStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE ) = 0;
	virtual IDataStorage* STDCALL CreateStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE ) = 0;
	// database and data table opening
	virtual IDataBase* STDCALL OpenDataBase( const char *pszName, DWORD dwAccessMode, DWORD type = DB_TYPE_INI ) = 0;
	virtual IDataTable* STDCALL OpenDataTable( IDataStream *pStream, const char *pszBaseNode = "base" ) = 0;
	virtual IDataTable* STDCALL OpenIniDataTable( IDataStream *pStream ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// функции для работа с save/load системой и глобальной фабрикой
extern ISaveLoadSystem *g_pGlobalSaveLoadSystem;
inline ISaveLoadSystem* GetSLS() { return g_pGlobalSaveLoadSystem; }
// save/load system
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
// global factory
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** open/create storage/file helper functions
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline IDataStorage* OpenStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE )
{
	return GetSLS()->OpenStorage( pszName, dwAccessMode, type );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline IDataStorage* CreateStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE )
{
	return GetSLS()->CreateStorage( pszName, dwAccessMode, type );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline IDataStream* OpenFileStream( const std::string &szFullName, DWORD dwAccessMode )
{
	const int nPos = szFullName.rfind( '\\' );
	const std::string szPathName = nPos != std::string::npos ? szFullName.substr( 0, nPos + 1 ) : ".\\";
	const std::string szFileName = nPos != std::string::npos ? szFullName.substr( nPos + 1 ) : szFullName;
	CPtr<IDataStorage> pStorage = OpenStorage( szPathName.c_str(), dwAccessMode, STORAGE_TYPE_FILE );
	return pStorage->OpenStream( szFileName.c_str(), dwAccessMode );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline IDataStream* CreateFileStream( const std::string &szFullName, DWORD dwAccessMode )
{
	const int nPos = szFullName.rfind( '\\' );
	const std::string szPathName = nPos != std::string::npos ? szFullName.substr( 0, nPos + 1 ) : ".\\";
	const std::string szFileName = nPos != std::string::npos ? szFullName.substr( nPos + 1 ) : szFullName;
	CPtr<IDataStorage> pStorage = CreateStorage( szPathName.c_str(), dwAccessMode, STORAGE_TYPE_FILE );
	return pStorage->CreateStream( szFileName.c_str(), dwAccessMode );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** open database/datatable
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __STRUCTURESAVER_H__
