#ifndef __STREAMIOLIB_STREAMIO_H__
#define __STREAMIOLIB_STREAMIO_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EStorageElementType
{
	SET_STORAGE	= 1,
	SET_STREAM	= 2,
	SET_FORCE_DWORD	= 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EStreamAccessMode
{
	STREAM_ACCESS_READ   = 0x00000001,
	STREAM_ACCESS_WRITE  = 0x00000002,
	STREAM_ACCESS_APPEND = 0x00000004,
	STREAM_ACCESS_TEXT   = 0x00000008,
	STREAM_ACCESS_FORCE_DWORD = 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EStorageType
{
	STORAGE_TYPE_MOD		= 0,
	STORAGE_TYPE_COMMON = 1,
	STORAGE_TYPE_FILE		= 2,
	STORAGE_TYPE_ZIP		= 3,
	STORAGE_TYPE_MEM		= 4,
	STORAGE_TYPE_FORCE_DWORD = 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// special structure, which's bit fields represent Win32 date/time format in the correct form
struct SWin32Time
{
	union
	{
		struct
		{
			DWORD seconds : 5;								// seconds (0..29 with 2 sec. interval)
			DWORD minutes : 6;								// minutes (0..59)
			DWORD hours   : 5;								// hours (0..23)
			DWORD day     : 5;								// day (1..31)
			DWORD month   : 4;								// month(1..12)
			DWORD year    : 7;								// year (0..119 relative to 1980)
		};
		struct
		{
			WORD wTime;
			WORD wDate;
		};
		DWORD dwFulltime;
	};
	//
	SWin32Time() : dwFulltime( 0 ) {  }
	SWin32Time( DWORD _dwFulltime ) : dwFulltime( _dwFulltime ) {  }
	WORD GetDate() const { return wDate; }
	WORD GetTime() const { return wTime; }
	operator DWORD() const { return dwFulltime; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SStorageElementStats
{
	const char *pszName;									// element name
	EStorageElementType type;							// type: storage/stream
	int nSize;														// storage size
	SWin32Time ctime;											// creation time
	SWin32Time mtime;											// modification time
	SWin32Time atime;											// last access time
	SStorageElementStats() : pszName( 0 ), type( SET_STORAGE ), nSize( 0 ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IStorageEnumerator : public IRefCount
{
	virtual void STDCALL Reset( const char *pszName ) = 0;
	virtual bool STDCALL Next() = 0;
	virtual const SStorageElementStats* STDCALL GetStats() const = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IDataStream : public IRefCount
{
	// ������/������ ������
	virtual int STDCALL Read( void *pBuffer, int nLength ) = 0;
	virtual int STDCALL Write( const void *pBuffer, int nLength ) = 0;
	// �������� ������� ������� � ������ �� ������ ������
	virtual int STDCALL LockBegin() = 0;
	// ������� ������ ������ � ������� �������
	virtual int STDCALL UnlockBegin() = 0;
	// ������� ������� � ������
	virtual int STDCALL GetPos() const = 0;
	// ��������� ������� ������� � ������
	virtual int STDCALL Seek( int offset, STREAM_SEEK from ) = 0;
	// �������� ������ ������
	virtual int STDCALL GetSize() const = 0;
	// �������� ������ ������
	virtual bool STDCALL SetSize( int nSize ) = 0;
	// ����������� 'nLength' ���� �� ������� ������� ������ � ������ ������� 'pDstStream' ������
	virtual int STDCALL CopyTo( IDataStream *pDstStream, int nLength ) = 0;
	// �������� ��� �������������� ������
	virtual void STDCALL Flush() = 0;
	// �������� ���������� � ������
	virtual void STDCALL GetStats( SStorageElementStats *pStats ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IDataStorage : public IRefCount
{
	// type ID
	enum { tidTypeID = 0 };
	// ���������, ���� �� ����� �����
	virtual const bool STDCALL IsStreamExist( const char *pszName ) = 0;
	// ������� � ������� ����� � ��������� ������ � ������� �������
	virtual IDataStream* STDCALL CreateStream( const char *pszName, DWORD dwAccessMode ) = 0;
	// ������� ������������ ����� � ��������� ������ � ������� �������
	virtual IDataStream* STDCALL OpenStream( const char *pszName, DWORD dwAccessMode ) = 0;
	// �������� �������� stream'�
	virtual bool STDCALL GetStreamStats( const char *pszName, SStorageElementStats *pStats ) = 0;
	// ����� ������� ���������
	virtual bool STDCALL DestroyElement( const char *pszName ) = 0;
	// ������������� �������
	virtual bool STDCALL RenameElement( const char *pszOldName, const char *pszNewName ) = 0;
	// ������������ ���������
	virtual IStorageEnumerator* STDCALL CreateEnumerator() = 0;
	// �������� ��� ����� storage
	virtual const char* STDCALL GetName() const = 0;
	// �������� ����� MOD �� �����
	virtual bool STDCALL AddStorage( IDataStorage *pStorage, const char *pszName ) = 0;
	// ������ MOD �� �����
	virtual bool STDCALL RemoveStorage( const char *pszName ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline std::string GetStreamName( IDataStream *pStream )
{
	SStorageElementStats stats;
	pStream->GetStats( &stats );
	return stats.pszName == 0 ? "" : stats.pszName;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __STREAMIO_H__
