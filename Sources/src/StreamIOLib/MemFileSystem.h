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
	// ������/������ ������
	virtual int STDCALL Read( void *pBuffer, int nLength );
	virtual int STDCALL Write( const void *pBuffer, int nLength );
	// �������� ������� ������� � ������ �� ������ ������
	virtual int STDCALL LockBegin();
	// ������� ������ ������ � ������� �������
	virtual int STDCALL UnlockBegin();
	// ������� ������� � ������
	virtual int STDCALL GetPos() const;
	// ��������� ������� ������� � ������
	virtual int STDCALL Seek( int offset, STREAM_SEEK from );
	// �������� ������ ������
	virtual int STDCALL GetSize() const;
	// �������� ������ ������
	virtual bool STDCALL SetSize( int nSize );
	// ����������� 'nLength' ���� �� ������� ������� ������ � ������ ������� 'pDstStream' ������
	virtual int STDCALL CopyTo( IDataStream *pDstStream, int nLength );
	// �������� ��� �������������� ������
	virtual void STDCALL Flush();
	// �������� ���������� � ������
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
	// ���������, ���� �� ����� �����
	virtual const bool STDCALL IsStreamExist( const char *pszName );
	// ������� � ������� ����� � ��������� ������ � ������� �������
	virtual IDataStream* STDCALL CreateStream( const char *pszName, DWORD dwAccessMode );
	// ������� ������������ ����� � ��������� ������ � ������� �������
	virtual IDataStream* STDCALL OpenStream( const char *pszName, DWORD dwAccessMode );
	// �������� �������� stream'�
	virtual bool STDCALL GetStreamStats( const char *pszName, SStorageElementStats *pStats );
	// ����� ������� ���������
	virtual bool STDCALL DestroyElement( const char *pszName );
	// ������������� �������
	virtual bool STDCALL RenameElement( const char *pszOldName, const char *pszNewName );
	// ������������ ���������
	virtual IStorageEnumerator* STDCALL CreateEnumerator();
	// �������� ��� ����� storage
	virtual const char* STDCALL GetName() const { return szBase.c_str(); }
	// �������� ����� MOD
	virtual bool STDCALL AddStorage( IDataStorage *pStorage, const char *pszName );
	// ������ MOD
	virtual bool STDCALL RemoveStorage( const char *pszName );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MEMFILESYSTEM_H__
