#ifndef __MEMFILESYSTEM_H__
#define __MEMFILESYSTEM_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMemFileStream : public IDataStream
{
	OBJECT_NORMAL_METHODS( CMemFileStream );
	DECLARE_SERIALIZE;
	//
	CPtr<IDataStorage> pStorage;					// parent storage
	std::vector<BYTE> data;								// memory data of this stream
	int nBeginPos;												// locked begin position
	int nCurrPos;													// current stream position
	// file stats
	std::string szName;
	SStorageElementStats stats;
	//
	void ResizeToFit( int nSize )
	{
		if ( nSize > data.size() )
		{
			data.reserve( int(nSize * 1.3) );
			data.resize( nSize );
		}
	}
public:
	CMemFileStream() : nBeginPos( 0 ), nCurrPos( 0 ) { data.reserve( 1024 ); }
	CMemFileStream( const CMemFileStream &stream ) : data( stream.data ), nBeginPos( 0 ), nCurrPos( 0 ) {  }
	CMemFileStream( BYTE *pData, int nLength ) : data( pData, pData + nLength ), nBeginPos( 0 ), nCurrPos( 0 ) {  }
	explicit CMemFileStream( int nSize, IDataStorage *_pStorage ) : pStorage( _pStorage ), data( nSize ), nBeginPos( 0 ), nCurrPos( 0 ) {  }
	//
	void* GetBuffer() { return &(data[0]); }
	void SetStats( const SStorageElementStats &_stats )
	{
		stats = _stats;
		szName = _stats.pszName;
		stats.pszName = szName.c_str();
	}
	//
	// чтение/запись данных
	virtual int STDCALL Read( void *pBuffer, int nLength );
	virtual int STDCALL Write( const void *pBuffer, int nLength );
	// объявить текущую позицию в потоке за начало потока
	virtual int STDCALL LockBegin();
	// вернуть начало потока в нулевую позицию
	virtual int STDCALL UnlockBegin();
	// текущая позиция в потоке
	virtual int STDCALL GetPos() const;
	// выставить текущую позицию в потоке
	virtual int STDCALL Seek( int offset, STREAM_SEEK from );
	// получить размер потока
	virtual int STDCALL GetSize() const;
	// изменить размер потока
	virtual bool STDCALL SetSize( int nSize );
	// скопировать 'nLength' байт из текущей позиции потока в текущю позицию 'pDstStream' потока
	virtual int STDCALL CopyTo( IDataStream *pDstStream, int nLength );
	// сбросить все закешированные данные
	virtual void STDCALL Flush();
	// получить информацию о потоке
	virtual void STDCALL GetStats( SStorageElementStats *pStats );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMemFileSystem : public IDataStorage
{
	OBJECT_MINIMAL_METHODS( CMemFileSystem );
	std::string szBase;
	DWORD dwStorageAccessMode;
	typedef std::unordered_map< std::string, CPtr<IDataStream> > CStreamsMap;
	CStreamsMap streams;
public:
	CMemFileSystem( DWORD dwAccessMode );
	// проверить, есть ли такой поток
	virtual const bool STDCALL IsStreamExist( const char *pszName );
	// создать и открыть поток с указанным именем и правами доступа
	virtual IDataStream* STDCALL CreateStream( const char *pszName, DWORD dwAccessMode );
	// открыть существующий поток с указанным именем и правами доступа
	virtual IDataStream* STDCALL OpenStream( const char *pszName, DWORD dwAccessMode );
	// получить описание stream'а
	virtual bool STDCALL GetStreamStats( const char *pszName, SStorageElementStats *pStats );
	// убить элемент хранилища
	virtual bool STDCALL DestroyElement( const char *pszName );
	// переименовать элемент
	virtual bool STDCALL RenameElement( const char *pszOldName, const char *pszNewName );
	// перечисление элементов
	virtual IStorageEnumerator* STDCALL CreateEnumerator();
	// получить имя этого storage
	virtual const char* STDCALL GetName() const { return szBase.c_str(); }
	// добавить новый MOD
	virtual bool STDCALL AddStorage( IDataStorage *pStorage, const char *pszName );
	// убрать MOD
	virtual bool STDCALL RemoveStorage( const char *pszName );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MEMFILESYSTEM_H__
