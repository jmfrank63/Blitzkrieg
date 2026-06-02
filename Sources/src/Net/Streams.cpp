#include "StdAfx.h"
static const char LOCAL_FILE[] = __FILE__;
#include <stdio.h>
#include "Streams.h"
unsigned int CBitStream::nBitsMask[32] = {
	0x01,       0x03,       0x07,       0x0F,        0x1F,       0x3F,       0x7F,       0xFF,
	0x01FF,     0x03FF,     0x07FF,     0x0FFF,      0x1FFF,     0x3FFF,     0x7FFF,     0xFFFF,
	0x01FFFF,   0x03FFFF,   0x07FFFF,   0x0FFFFF,    0x1FFFFF,   0x3FFFFF,   0x7FFFFF,   0xFFFFFF,
	0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,  0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
};
unsigned int CDataStream::ReadOverflow( void *pDest, unsigned int nSize )
{
	SetFailed();
	memset( pDest, 0, nSize );
	if ( pCurrent < pFileEnd )
	{
		int nRes = pFileEnd - pCurrent;
		return Read( pDest, nRes );
	}
	pCurrent = pFileEnd;
	return 0;
}
bool CDataStream::ReadString( std::string &res, int nMaxSize )
{
	int nSize = 0;
	Read( &nSize, 1 );
	if ( nSize & 1 )
		Read( ((char*)&nSize) + 1, 3 );
	nSize >>= 1;
	if ( nMaxSize > 0 && nSize > nMaxSize )
	{
		SetFailed();
		return false;
	}
	unsigned char *pData = ReserveR( nSize );
	res.assign( (const char*)pData, nSize );
	Free( pData + nSize );
	return true;
}
void CDataStream::WriteString( const std::string &res )
{
	int nSize = res.size(), nVal;
	if ( nSize >= 128 )
	{
		nVal = nSize * 2 + 1;
		Write( &nVal, 4 );
	}
	else
	{
		nVal = nSize * 2;
		Write( &nVal, 1 );
	}
	Write( res.data(), nSize );
}
void CBitLocker::LockRead( CDataStream &data, unsigned int nSize )
{
	ASSERT(!pData);
	pData = &data;
	pBuffer = data.ReserveR( nSize );
	Init( pBuffer, read, nSize );
}
void CBitLocker::LockWrite( CDataStream &data, unsigned int nSize )
{
	ASSERT(!pData);
	pData = &data;
	pBuffer = data.ReserveW( nSize );
	Init( pBuffer, write, nSize );
}
void CBitLocker::ReserveRead( unsigned int nSize )
{
	ASSERT(pData);
	int nNewSize = pCurrent - pBuffer + nSize;
	unsigned char *pNewBuf;
	pNewBuf = pData->ReserveR( nNewSize );
#ifdef _DEBUG
	pReservedEnd = pNewBuf + nNewSize;
#endif
	int nFixup = pNewBuf - pBuffer;
	pCurrent += nFixup;
	pBitPtr += nFixup;
	pBuffer = pNewBuf;
}
void CBitLocker::ReserveWrite( unsigned int nSize )
{
	ASSERT(pData);
	int nNewSize = pCurrent - pBuffer + nSize;
	unsigned char *pNewBuf;
	pNewBuf = pData->ReserveW( nNewSize );
#ifdef _DEBUG
	pReservedEnd = pNewBuf + nNewSize;
#endif
	int nFixup = pNewBuf - pBuffer;
	pCurrent += nFixup;
	pBitPtr += nFixup;
	pBuffer = pNewBuf;
}
void CFixedMemStream::AllocForDirectReadAccess( unsigned int nSize )
{
	ASSERT(0);
}
void CFixedMemStream::AllocForDirectWriteAccess( unsigned int nSize )
{
	ASSERT(0);
}
unsigned int CFixedMemStream::DirectRead( void *pDest, unsigned int nSize )
{
	ASSERT(0);
	return 0;
}
unsigned int CFixedMemStream::DirectWrite( const void *pSrc, unsigned int nSize )
{
	ASSERT(0); 
	return 0;
}
void CMemoryStream::FixupBufferSize( int nNewSize )
{
	nNewSize = nNewSize * 2 + 64;
	unsigned char *pNewBuf = new unsigned char[ nNewSize ];
	NI_ASSERT_T( pReservedEnd - pBuffer <= nNewSize, NStr::Format("Can't fixup buffer - can't copy %d bytes to %d bytes buffer", pReservedEnd - pBuffer, nNewSize) );
	memcpy( pNewBuf, pBuffer, pReservedEnd - pBuffer );
	delete []pBuffer;
	pCurrent = pNewBuf + ( pCurrent - pBuffer );
	pFileEnd = pNewBuf + ( pFileEnd - pBuffer );
	pReservedEnd = pNewBuf + nNewSize;
	pBuffer = pNewBuf;
}
void CMemoryStream::AllocForDirectReadAccess( unsigned int nSize )
{
	FixupBufferSize( GetPosition() + nSize );
}
void CMemoryStream::AllocForDirectWriteAccess( unsigned int nSize )
{
	FixupBufferSize( GetPosition() + nSize );
}
unsigned int CMemoryStream::DirectRead( void *pDest, unsigned int nSize )
{
	ASSERT(0);
	return 0;
}
unsigned int CMemoryStream::DirectWrite( const void *pSrc, unsigned int nSize )
{
	FixupBufferSize( GetPosition() + nSize );
	RWrite( pSrc, nSize );
	return nSize;
}
CMemoryStream::CMemoryStream() { 
	nFlags = F_CanRead | F_CanWrite; 
	pBuffer = new unsigned char[32];
	pReservedEnd = pBuffer + 32;
	pFileEnd = pBuffer;
	pCurrent = pBuffer;
}
CMemoryStream::~CMemoryStream()
{
	delete[] pBuffer;
}
void CMemoryStream::SetSizeDiscard( int nSize )
{
	int nBufSize = nSize + 64;
	delete[] pBuffer;
	pBuffer = new unsigned char[nBufSize];
	pReservedEnd = pBuffer + nBufSize;
	pFileEnd = pBuffer + nSize;
	pCurrent = pBuffer;
}
void CMemoryStream::CopyMemoryStream( const CMemoryStream &src )
{
	int nBufSize = src.pReservedEnd - src.pBuffer;
	pBuffer = new unsigned char[ nBufSize ];
	pReservedEnd = pBuffer + nBufSize;
	memcpy( pBuffer, src.pBuffer, nBufSize );
	pFileEnd = pBuffer + ( src.pFileEnd - src.pBuffer );
	pCurrent = pBuffer + ( src.pCurrent - src.pBuffer );
	nFlags = src.nFlags;
}
CMemoryStream::CMemoryStream( const CMemoryStream &src )
{
	CopyMemoryStream( src );
}
CMemoryStream& CMemoryStream::operator=( const CMemoryStream &src )
{
	delete[] pBuffer;
	CopyMemoryStream( src );
	return *this;
}
void CBufferedStream::FlushBuffer()
{
	if ( IsWasted() )
	{
		ASSERT( pBuffer );
		ClearWasted();
		if ( !pBuffer )
			return;
		int nSize = pCurrent - pBuffer;
		if ( DoWrite( nBufferStart, pBuffer, nSize ) != nSize )
			SetFailed(); // throw
		FixupSize();
	} 
}
void CBufferedStream::SetNewBufferSize( unsigned int nSize )
{
	int nNewSize = nSize + 1024;
	if ( nNewSize < pReservedEnd - pBuffer )
		return;
	unsigned char *pNewBuf = new unsigned char [ nNewSize ];
	if ( pBuffer )
	{
		memcpy( pNewBuf, pBuffer, pReservedEnd - pBuffer );
		delete[] pBuffer;
	}
	int nFixup = pNewBuf - pBuffer;
	pCurrent += nFixup;
	pFileEnd += nFixup;
	pBuffer = pNewBuf;
	pReservedEnd = pNewBuf + nNewSize;
}
void CBufferedStream::StartAccess( unsigned int nFileSize, unsigned int nSize )
{
	ClearWasted();
	pBuffer = new unsigned char [ nSize ];
	pCurrent = pBuffer;
	pFileEnd = pBuffer + nFileSize;
	pReservedEnd = pBuffer + nSize;
	nBufferStart = 0;
	LoadBufferForced( 0 );
}
void CBufferedStream::FinishAccess()
{
	FlushBuffer();
	if ( pBuffer ) delete[] pBuffer; pBuffer = 0;
}
template<class T>
T __Min( const T &a, const T &b ) { return a < b ? a : b; }
void CBufferedStream::LoadBufferForced( int nPos )
{
	FlushBuffer();
	int nRead = __Min( pReservedEnd - pBuffer, pFileEnd - pCurrent );
	if ( DoRead( nPos, pBuffer, nRead ) != nRead )
		SetFailed(); // throw
	pCurrent += nBufferStart - nPos;
	pFileEnd += nBufferStart - nPos;
	nBufferStart = nPos;
}
void CBufferedStream::ShiftBuffer()
{
	int nPos = GetPosition();
	if ( nBufferStart == nPos )
		return;
	LoadBufferForced( nPos ); // also shifts pCurrent to pBuffer
}
void CBufferedStream::AllocForDirectReadAccess( unsigned int nSize )
{
	if ( nSize > pReservedEnd - pBuffer )
	{
		SetNewBufferSize( nSize );
		LoadBufferForced( nBufferStart );
	}
	else
		ShiftBuffer();
}
void CBufferedStream::AllocForDirectWriteAccess( unsigned int nSize )
{
	SetNewBufferSize( nSize );
}
void CBufferedStream::NotifyFinishDirectAccess()
{
	ShiftBuffer();
}
void CBufferedStream::Seek( unsigned int nPos )
{
	FixupSize();
	unsigned char *pNewCurrent = pBuffer + nPos - nBufferStart; 
	if ( pNewCurrent >= pBuffer && pNewCurrent < pReservedEnd )
	{
		pCurrent = pNewCurrent;
		return;
	}
	FlushBuffer(); 
	pCurrent = pNewCurrent;
	ShiftBuffer();
}
unsigned int CBufferedStream::DirectRead( void *pDest, unsigned int nSize )
{
	if ( nSize > pReservedEnd - pBuffer )
	{
		FlushBuffer();
		unsigned int nRes = DoRead( GetPosition(), pDest, nSize );
		if ( nRes != nSize )
			SetFailed(); // throw
		pCurrent += nSize;
		ShiftBuffer();
		return nRes;
	}
	ShiftBuffer();
	return Read( pDest, nSize );
}
unsigned int CBufferedStream::DirectWrite( const void *pSrc, unsigned int nSize )
{
	if ( nSize > pReservedEnd - pBuffer )
	{
		FlushBuffer();
		unsigned int nRes = DoWrite( GetPosition(), pSrc, nSize );
		if ( nRes != nSize )
			SetFailed(); // throw
		pCurrent += nSize;
		FixupSize();
		ShiftBuffer();
		return nRes;
	}
	ShiftBuffer();
	return Write( pSrc, nSize );
}
