#ifndef __CHECK_SUMS_H__
#define __CHECK_SUMS_H__
#pragma ONCE
#include "../zlib/zlib.h"
namespace NCheckSums
{
struct SCheckSumBufferStorage
{
	std::vector<BYTE> buf;
	int nCnt;
	SCheckSumBufferStorage() : buf( 10 ), nCnt( 0 ) {  }
	SCheckSumBufferStorage( const int nBufSize ) : buf( nBufSize ), nCnt( 0 ) {  }
	void Clear() { buf.clear(); nCnt = 0; }
	template <class T>
	void CopyToBuf( const T &data )
	{
		const int nRequiredSize = nCnt + sizeof( data );
		if ( nRequiredSize >= buf.size() )
			buf.resize( nRequiredSize * 1.5 );
		memcpy( &(buf[0]) + nCnt, &data, sizeof( data ) );
		nCnt += sizeof( data );
	}
	void CopyToBufRaw( const void *pData, const int nDataSize )
	{
		const int nRequiredSize = nCnt + nDataSize;
		if ( nRequiredSize >= buf.size() )
			buf.resize( nRequiredSize * 1.5 );
		memcpy( &(buf[0]) + nCnt, pData, nDataSize );
		nCnt += nDataSize;
	}
	const uLong GetCRC() const
	{
		return nCnt > 0 ? crc32( 0, &(buf[0]), nCnt ) : 0;
	}
	int GetSize() const { return nCnt; }
};
template <class T>
inline void CopyToBuf( SCheckSumBufferStorage *pStorage, const T &data )
{
	pStorage->CopyToBuf( data );
}
inline void CopyToBufRaw( SCheckSumBufferStorage *pStorage, const void *pData, const int nDataSize )
{
	pStorage->CopyToBufRaw( pData, nDataSize );
}
inline const uLong GetCRC( SCheckSumBufferStorage *pStorage )
{
	return pStorage->GetCRC();
}
inline const uLong GetCRC( const uLong newCheckSum, const uLong oldCheckSum )
{
	static union { uLong checkSum; BYTE array[ sizeof(uLong) ]; };
	checkSum = oldCheckSum;
	return crc32( newCheckSum, array, sizeof(uLong) );
}
};
#endif // __CHECK_SUMS_H__
