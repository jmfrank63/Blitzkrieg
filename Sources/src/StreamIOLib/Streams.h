#ifndef __STREAMS_H__
#define __STREAMS_H__
#pragma ONCE
class CDataStream
{
protected:
	enum EFlags
	{
		F_Wasted   = 1,
		F_CanRead  = 2,
		F_CanWrite = 4,
		F_Failed   = 8,
	};

	unsigned char *pBuffer, *pCurrent, *pReservedEnd;
	mutable unsigned char *pFileEnd;
	unsigned int nBufferStart;
	int nFlags;

	int CanRead() { return nFlags & F_CanRead; }
	int CanWrite() { return nFlags & F_CanWrite; }
	int IsWasted() { return nFlags & F_Wasted; }
	void SetWasted() { nFlags |= F_Wasted; }
	void SetFailed() { nFlags |= F_Failed; }
	void ClearWasted() { nFlags &= ~F_Wasted; }
protected:
	void FixupSize() const { if ( pCurrent > pFileEnd ) pFileEnd = pCurrent; } // this limits us to max file size 2M
	virtual void AllocForDirectReadAccess( unsigned int nSize ) = 0;
	virtual void AllocForDirectWriteAccess( unsigned int nSize ) = 0;
	virtual void NotifyFinishDirectAccess() {}
	virtual unsigned int DirectRead( void *pDest, unsigned int nSize ) = 0;
	virtual unsigned int DirectWrite( const void *pSrc, unsigned int nSize ) = 0;
	inline unsigned char* ReserveR( unsigned int nSize );
	inline unsigned char* ReserveW( unsigned int nSize );
	void Free( unsigned char *pFinish ) { pCurrent = pFinish; NotifyFinishDirectAccess(); }

	void RRead( void *pDest, unsigned int nSize ) { memcpy( pDest, pCurrent, nSize ); pCurrent += nSize; ASSERT( pCurrent <= pReservedEnd ); }
	void RWrite( const void *pSrc, unsigned int nSize ) { memcpy( pCurrent, pSrc, nSize ); pCurrent += nSize; ASSERT( pCurrent <= pReservedEnd ); }
	unsigned int ReadOverflow( void *pDest, unsigned int nSize );
public:
	CDataStream() { nBufferStart = 0; }
	virtual ~CDataStream() {}
	virtual void Seek( unsigned int nPos ) = 0;
	inline unsigned int Read( void *pDest, unsigned int nSize );
	inline unsigned int Write( const void *pSrc, unsigned int nSize );
	int GetSize() const { FixupSize(); return pFileEnd - pBuffer + nBufferStart; }
	int GetPosition() const { return pCurrent - pBuffer + nBufferStart; }
	bool IsOk() const { return ( nFlags & F_Failed ) == 0; }
	void SetOk() { nFlags &= ~F_Failed; }
	bool ReadString( std::string &res, int nMaxSize = -1 );
	void WriteString( const std::string &res );
	template<class T>
		CDataStream& operator>>( T &res ) { Read( &res, sizeof(res) ); return *this; }
	template<class T>
		CDataStream& operator<<( const T &res ) { Write( &res, sizeof(res) ); return *this; }
	template<>
		CDataStream& operator>>( std::string &res ) { ReadString( res ); return *this; }
	template<>
		CDataStream& operator<<( const std::string &res ) { WriteString( res ); return *this; }
	inline unsigned int ReadTo( CDataStream &dst, unsigned int nSize );
	inline unsigned int WriteFrom( CDataStream &src );

	friend class CBitLocker;
};
class CFixedMemStream: public CDataStream
{
protected:
	virtual void AllocForDirectReadAccess( unsigned int nSize );
	virtual void AllocForDirectWriteAccess( unsigned int nSize );
	virtual unsigned int DirectRead( void *pDest, unsigned int nSize );
	virtual unsigned int DirectWrite( const void *pSrc, unsigned int nSize );
public:
	CFixedMemStream( const void *pCData, int nSize )
	{
		void *pData = const_cast<void*>( pCData );
		pBuffer = (unsigned char*)pData; pCurrent = pBuffer; pReservedEnd = pBuffer + nSize;
		pFileEnd = pReservedEnd;
		nFlags = F_CanRead;
	}
	virtual void Seek( unsigned int nPos ) { pCurrent = pBuffer + nPos; ASSERT( pCurrent <= pReservedEnd ); }
};
class CMemoryStream: public CDataStream
{
protected:
	void FixupBufferSize( int nNewSize );
	virtual void AllocForDirectReadAccess( unsigned int nSize );
	virtual void AllocForDirectWriteAccess( unsigned int nSize );
	virtual unsigned int DirectRead( void *pDest, unsigned int nSize );
	virtual unsigned int DirectWrite( const void *pSrc, unsigned int nSize );
	void CopyMemoryStream( const CMemoryStream &src );
public:
	CMemoryStream();
	~CMemoryStream();
	CMemoryStream( const CMemoryStream &src );
	CMemoryStream& operator=( const CMemoryStream &src );
	void SetRMode() { nFlags = (nFlags&~(F_CanRead|F_CanWrite)) | F_CanRead; }
	void SetWMode() { nFlags = (nFlags&~(F_CanRead|F_CanWrite)) | F_CanWrite; }
	void SetRWMode() { nFlags = nFlags | (F_CanRead|F_CanWrite); }
	virtual void Seek( unsigned int nPos ) { FixupSize(); pCurrent = pBuffer + nPos; if ( pCurrent > pReservedEnd ) FixupBufferSize( pCurrent - pBuffer ); }
	void Clear() { pFileEnd = pBuffer; pCurrent = pBuffer; nFlags = F_CanRead|F_CanWrite; }
	const unsigned char* GetBuffer() const { return pBuffer; }
	unsigned char* GetBufferForWrite() const { return pBuffer; }
	void SetSize( int nSize ) { pFileEnd = pBuffer + nSize; pCurrent = pBuffer; if ( pFileEnd > pReservedEnd ) FixupBufferSize( nSize ); }
	void SetSizeDiscard( int nSize );
};
class CBufferedStream: public CDataStream
{
private:
	void FlushBuffer();
	void SetNewBufferSize( unsigned int nSize );
	void ShiftBuffer();
	void LoadBufferForced( int nPos ); // set pCurrent to nPos and load buffer from nPos
	virtual void AllocForDirectReadAccess( unsigned int nSize );
	virtual void AllocForDirectWriteAccess( unsigned int nSize );
	virtual void NotifyFinishDirectAccess();
	virtual unsigned int DirectRead( void *pDest, unsigned int nSize );
	virtual unsigned int DirectWrite( const void *pSrc, unsigned int nSize );
	CBufferedStream( const CBufferedStream &a ) { ASSERT(0); }
	CBufferedStream& operator=( const CBufferedStream &a ) { ASSERT(0); return *this;}
protected:
	void StartAccess( unsigned int nFileSize, unsigned int nSize );
	void FinishAccess();
	virtual unsigned int DoRead( unsigned int nPos, void *pDest, unsigned int nSize ) = 0;
	virtual unsigned int DoWrite( unsigned int nPos, const void *pSrc, unsigned int nSize ) = 0;
public:
	CBufferedStream() { pBuffer = 0; nFlags = 0; }
	~CBufferedStream() { FinishAccess(); }
	virtual void Seek( unsigned int nPos );
};
class CBitStream
{
public:
	enum Mode
	{
		read,
		write
	};

protected:
	unsigned char *pCurrent;
	unsigned char *pBitPtr;         // for bit writing
	unsigned int nBits;
	unsigned char nBitsCount; // bits and bit counter
	static unsigned int nBitsMask[32];

#ifdef _DEBUG
	Mode mode;
	unsigned char *pReservedEnd;
	void CheckCurrentR() { ASSERT( pCurrent <= pReservedEnd ); ASSERT( mode == read ); }
	void CheckCurrentW() { ASSERT( pCurrent <= pReservedEnd ); ASSERT( mode == write ); }
#else
	void CheckCurrentR() {}
	void CheckCurrentW() {}
#endif

	inline void Init( unsigned char *pData, Mode _mode, int nSize );
public:
	CBitStream( void *pData, Mode _mode, int nSize ) { Init( (unsigned char*)pData, _mode, nSize ); }
	void Read( void *pDest, unsigned int nSize ) { memcpy( pDest, pCurrent, nSize ); pCurrent += nSize; CheckCurrentR(); }
	void Write( const void *pSrc, unsigned int nSize ) { memcpy( pCurrent, pSrc, nSize ); pCurrent += nSize; CheckCurrentW(); }
	void ReadCString( std::string &res ) { int nLeng = strlen( (char*)pCurrent ); res.assign( (char*)pCurrent, nLeng ); pCurrent += nLeng + 1; CheckCurrentR(); }
	void WriteCString( const char *pSrc ) { int nLeng = strlen( pSrc ); memcpy( pCurrent, pSrc, nLeng + 1 ); pCurrent += nLeng + 1; CheckCurrentW(); }
	void FlushBits() { if ( nBitsCount ) { nBitsCount = 0; if ( pBitPtr ) pBitPtr[0] = (char)nBits; } }
	inline void WriteBits( unsigned int _nBits, unsigned int _nBitsCount );
	inline void WriteBit( unsigned int _nBits );
	inline unsigned int ReadBits( unsigned int _nBitsCount );
	inline unsigned int ReadBit();
	const unsigned char* GetCurrentPtr() const { return pCurrent; }
	unsigned char* WriteDelayed( int nSize ) { unsigned char *pRes = pCurrent; pCurrent += nSize; CheckCurrentW(); return pRes; }
	template <class T>
		inline void Write( const T &a ) { Write( &a, sizeof(a) ); }
	template <class T>
		inline void Read( T &a ) { Read( &a, sizeof(a) ); }
	template<>
		inline void Write<std::string>( const std::string &a ) { WriteCString( a.c_str() ); }
	template<>
		inline void Read<std::string>( std::string &a ) { ReadCString( a ); }
	friend class CBitEmbedded;
};
class CBitLocker: public CBitStream
{
	CDataStream *pData;
	unsigned char *pBuffer;
public:
	CBitLocker(): CBitStream( 0, read, 0 ) { pData = 0; }
	~CBitLocker() { FlushBits(); if ( pData ) pData->Free( pCurrent ); }
	void LockRead( CDataStream &data, unsigned int nSize );
	void LockWrite( CDataStream &data, unsigned int nSize );
	void ReserveRead( unsigned int nSize );
	void ReserveWrite( unsigned int nSize );
	void Free() { ASSERT( pData ); FlushBits(); pData->Free( pCurrent ); pData = 0; }
};
class CBitEmbedded: public CBitStream
{
	CBitStream &bits;
public:
	CBitEmbedded( CBitStream &_bits ):
#ifdef _DEBUG
		CBitStream( _bits.pCurrent, _bits.mode, _bits.pReservedEnd - _bits.pCurrent )
#else
		CBitStream( _bits.pCurrent, read, 0 )
#endif
		,bits(_bits) {}
	~CBitEmbedded() { bits.pCurrent = pCurrent; }
};
inline unsigned char* CDataStream::ReserveR( unsigned int nSize )
{
	ASSERT( CanRead() );
	if ( pCurrent + nSize > pReservedEnd )
		AllocForDirectReadAccess( nSize );
	return pCurrent;
}
inline unsigned char* CDataStream::ReserveW( unsigned int nSize )
{
	ASSERT( CanWrite() );
	SetWasted();
	if ( pCurrent + nSize > pReservedEnd )
		AllocForDirectWriteAccess( nSize );
	return pCurrent;
}
inline unsigned int CDataStream::Read( void *pDest, unsigned int nSize )
{
	ASSERT( CanRead() );
	if ( pCurrent + nSize <= pFileEnd )
	{
		if ( pCurrent + nSize <= pReservedEnd )
		{
			{
				RRead( pDest, nSize );
				return nSize;
			}
		}
		else return DirectRead( pDest, nSize );
	}
	return ReadOverflow( pDest, nSize);
}
inline unsigned int CDataStream::Write( const void *pSrc, unsigned int nSize )
{
	ASSERT( CanWrite() );
	if ( pCurrent + nSize <= pReservedEnd )
	{
		SetWasted();
		RWrite( pSrc, nSize );
		return nSize;
	}
	else return DirectWrite( pSrc, nSize );
}
inline unsigned int CDataStream::ReadTo( CDataStream &dst, unsigned int nSize )
{
	dst.Seek(0);
	unsigned char *pBuf = dst.ReserveW( nSize );
	unsigned int nRes = Read( pBuf, nSize );
	dst.Free( pBuf + nSize );
	return nRes;
}
inline unsigned int CDataStream::WriteFrom( CDataStream &src )
{
	src.Seek(0);
	int nSize = src.GetSize();
	unsigned char *pBuf = src.ReserveR( nSize );
	unsigned int nRes = Write( pBuf, nSize );
	src.Free( pBuf + nSize );
	return nRes;
}
inline void CBitStream::Init( unsigned char *pData, Mode _mode, int nSize )
{
	pCurrent = pData; nBitsCount = 0; pBitPtr = 0;
#ifdef _DEBUG
	mode = _mode;
	pReservedEnd = pCurrent + nSize;
#endif
}
inline void CBitStream::WriteBits( unsigned int _nBits, unsigned int _nBitsCount )
{
	if ( nBitsCount != 0 )
	{
		nBits += ( _nBits << nBitsCount );
		nBitsCount += _nBitsCount;
	}
	else
	{
		pBitPtr = pCurrent++;
		nBits = _nBits;
		nBitsCount = _nBitsCount;
	}
	while ( nBitsCount > 8 )
	{
		pBitPtr[0] = (unsigned char)nBits; //( nBits & 0xff );
		nBits >>= 8; nBitsCount -= 8;
		pBitPtr = pCurrent++;
	}
	CheckCurrentW();
}
inline void CBitStream::WriteBit( unsigned int _nBits )
{
	if ( nBitsCount == 0 )
	{
		pBitPtr = pCurrent++;
		nBits = _nBits;
		nBitsCount = 1;
	}
	else
	{
		nBits += ( _nBits << nBitsCount );
		nBitsCount++;
	}
	if ( nBitsCount > 8 )
	{
		pBitPtr[0] = (unsigned char)nBits; //( nBits & 0xff );
		nBits >>= 8; nBitsCount -= 8;
		pBitPtr = pCurrent++;
	}
	CheckCurrentW();
}
inline unsigned int CBitStream::ReadBits( unsigned int _nBitsCount )
{
	while ( nBitsCount < _nBitsCount )
	{
		nBits += ((unsigned int)*pCurrent++) << nBitsCount;
		nBitsCount += 8;
	}
	int nRes = nBits & nBitsMask[ _nBitsCount - 1 ];
	nBits >>= _nBitsCount;
	nBitsCount -= _nBitsCount;
	CheckCurrentR();
	return nRes;
}
inline unsigned int CBitStream::ReadBit()
{
	if ( nBitsCount < 1 )
	{
		nBits = ((unsigned int)*pCurrent++);
		nBitsCount = 8;
	}
	int nRes = nBits & 1;
	nBits >>= 1;
	nBitsCount--;
	CheckCurrentR();
	return nRes;
}
#endif // __STREAMS_H__
