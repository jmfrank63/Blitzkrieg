#ifndef __STREAM_ADAPTOR_H__
#define __STREAM_ADAPTOR_H__
#pragma ONCE
class CStreamRangeAdaptor : public IDataStream
{
	OBJECT_NORMAL_METHODS( CStreamRangeAdaptor );
	CPtr<IDataStream> pStream;						// stream to adapt range
	std::string szName;										// stream name
	SStorageElementStats stats;						// own stats for sub-stream
	bool bHasOwnStats;										// is the 'stats' valid
	int nBeginPos;
	int nEndPos;
	int nSeekPos;													// current seek position
public:
	CStreamRangeAdaptor() : nBeginPos( 0 ), nEndPos( 0 ), nSeekPos( 0 ), bHasOwnStats( false ) {  }
	CStreamRangeAdaptor( IDataStream *_pStream, int _nBeginPos, int _nEndPos, const char *pszName = "", const SStorageElementStats *pStats = 0 )
		: pStream( _pStream ), szName( pszName ), nBeginPos( _nBeginPos ), nEndPos( _nEndPos ), nSeekPos( 0 ) 
	{
		if ( pStats ) 
		{
			stats = *pStats;
			stats.pszName = szName.c_str();
		}
		bHasOwnStats = pStats != 0;
	}
	virtual int STDCALL Read( void *pBuffer, int nLength )
	{
		const int nLastPos = pStream->GetPos();
		pStream->Seek( nBeginPos + nSeekPos, STREAM_SEEK_SET );
		int nLengthToRead = Min( nEndPos - (nBeginPos + nSeekPos), nLength );
		nLengthToRead = pStream->Read( pBuffer, nLengthToRead );
		nSeekPos += nLengthToRead;
		pStream->Seek( nLastPos, STREAM_SEEK_SET );
		return nLengthToRead;
	}
	virtual int STDCALL Write( const void *pBuffer, int nLength )
	{
		const int nLastPos = pStream->GetPos();
		pStream->Seek( nBeginPos + nSeekPos, STREAM_SEEK_SET );
		const int nWrittenLength = pStream->Write( pBuffer, nLength );
		nSeekPos += nWrittenLength;
		pStream->Seek( nLastPos, STREAM_SEEK_SET );
		return nWrittenLength;
	}
	virtual int STDCALL LockBegin() { return -1; }
	virtual int STDCALL UnlockBegin() { return -1; }
	virtual int STDCALL GetPos() const
	{
		return pStream->GetPos() - nBeginPos;
	}
	virtual int STDCALL Seek( int offset, STREAM_SEEK from )
	{
		switch ( from )
		{
			case STREAM_SEEK_SET:
				nSeekPos = offset;
				break;
			case STREAM_SEEK_CUR:
				nSeekPos += offset;
				break;
			case STREAM_SEEK_END:
				nSeekPos = nEndPos - nBeginPos + offset;
				break;
		}
		return nSeekPos;
	}
	virtual int STDCALL GetSize() const
	{
		return nEndPos - nBeginPos;
	}
	virtual bool STDCALL SetSize( int nSize ) { return false; }
	virtual int STDCALL CopyTo( IDataStream *pDstStream, int nLength )
	{
		if ( nLength == 0 )
			return 0;
		std::vector<BYTE> buffer( nLength );
		nLength = Read( &(buffer[0]), nLength );
		return pDstStream->Write( &(buffer[0]), nLength );
	}
	virtual void STDCALL Flush() {  }
	virtual void STDCALL GetStats( SStorageElementStats *pStats )
	{
		if ( bHasOwnStats ) 
			*pStats = stats;
		else
		{
			pStream->GetStats( pStats );
			pStats->nSize = GetSize();
			pStats->pszName = szName.c_str();
		}
	}
};
class CStreamCOMAdaptor : public IStream
{
	int nRefCount;
	CPtr<IDataStream> pStream;
public:
	CStreamCOMAdaptor( IDataStream *_pStream ) : nRefCount( 1 ), pStream( _pStream ) {  }
	virtual HRESULT STDCALL QueryInterface( REFIID iid, void ** ppvObject )
	{
		if ( iid == IID_IUnknown )
			*ppvObject = static_cast<IUnknown*>( this );
		else if ( iid == IID_IStream )
			*ppvObject = static_cast<IStream*>( this );
		else
		{
			*ppvObject = 0;
			return E_NOINTERFACE;
		}
		AddRef();
		return S_OK;
	}
	virtual ULONG STDCALL AddRef() { return ++nRefCount; }
	virtual ULONG STDCALL Release() { int nRef = --nRefCount; if ( nRefCount == 0 ) delete this; return nRef; }
	virtual HRESULT STDCALL Read( void *pv, ULONG cb, ULONG *pcbRead )
	{
		int nLength = pStream->Read( pv, cb );
		if ( pcbRead )
			*pcbRead = nLength;
		return S_OK;
	}
	virtual HRESULT STDCALL Write( void const *pv, ULONG cb, ULONG *pcbWritten )
	{
		int nLength = pStream->Write( pv, cb );
		if ( pcbWritten )
			*pcbWritten = nLength;
		return S_OK;
	}
	virtual HRESULT STDCALL Seek( LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition  )
	{
		int nPos = pStream->Seek( int(dlibMove.QuadPart), STREAM_SEEK(dwOrigin) );
		if ( plibNewPosition )
			plibNewPosition->QuadPart = nPos;
		return S_OK;
	}
	virtual HRESULT STDCALL SetSize( ULARGE_INTEGER libNewSize  )
	{
		pStream->SetSize( int(libNewSize.QuadPart) );
		return S_OK;
	}
	virtual HRESULT STDCALL CopyTo( IStream *pDst, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten )
	{
		std::vector<BYTE> buffer( int(cb.QuadPart) );
		int nRead = pStream->Read( &(buffer[0]), buffer.size() );
		if ( pcbRead )
			pcbRead->QuadPart = nRead;
		ULONG uWrite = 0;
		pDst->Write( &(buffer[0]), Min(nRead, int(buffer.size())), &uWrite );
		if ( pcbWritten )
			pcbWritten->QuadPart = uWrite;
		return S_OK;
	}
	virtual HRESULT STDCALL Commit( DWORD grfCommitFlags )
	{
		pStream->Flush();
		return S_OK;
	}
	virtual HRESULT STDCALL Revert()
	{
		return S_OK;
	}
	virtual HRESULT STDCALL LockRegion( ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType )
	{
		return STG_E_INVALIDFUNCTION;
	}
	virtual HRESULT STDCALL UnlockRegion( ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType )
	{
		return STG_E_INVALIDFUNCTION;
	}
	virtual HRESULT STDCALL Stat( STATSTG *pStats,  DWORD grfStatFlag )
	{
		return STG_E_ACCESSDENIED;
	}
	virtual HRESULT STDCALL Clone( IStream **ppstm )
	{
		return S_OK;
	}
};
#endif // __STREAM_ADAPTOR_H__
