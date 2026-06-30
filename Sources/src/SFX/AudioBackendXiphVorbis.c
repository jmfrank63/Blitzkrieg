#include "AudioBackendXiphVorbis.h"

#include <stdlib.h>
#include <string.h>

#include <vorbis/vorbisfile.h>

typedef struct SXiphMemoryStream
{
	const unsigned char *pData;
	long nSize;
	long nPosition;
} SXiphMemoryStream;

struct SXiphVorbisStream
{
	SXiphMemoryStream memoryStream;
	OggVorbis_File vorbisFile;
	unsigned int nSampleRate;
	unsigned int nChannels;
	unsigned int nBlockAlign;
	unsigned long long nTotalFrames;
	int bOpen;
};

static size_t XiphRead( void *pBuffer, size_t nSize, size_t nCount, void *pUserData )
{
	SXiphMemoryStream *pStream = (SXiphMemoryStream*)pUserData;
	long nRequested = (long)( nSize * nCount );
	long nRemaining;

	if ( !pStream || !pBuffer || nSize == 0 || nCount == 0 )
		return 0;

	nRemaining = pStream->nSize - pStream->nPosition;
	if ( nRequested > nRemaining )
		nRequested = nRemaining;

	if ( nRequested <= 0 )
		return 0;

	memcpy( pBuffer, pStream->pData + pStream->nPosition, nRequested );
	pStream->nPosition += nRequested;
	return nRequested / nSize;
}

static int XiphSeek( void *pUserData, ogg_int64_t nOffset, int nWhence )
{
	SXiphMemoryStream *pStream = (SXiphMemoryStream*)pUserData;
	long nNewPosition;

	if ( !pStream )
		return -1;

	switch ( nWhence )
	{
	case SEEK_SET:
		nNewPosition = (long)nOffset;
		break;
	case SEEK_CUR:
		nNewPosition = pStream->nPosition + (long)nOffset;
		break;
	case SEEK_END:
		nNewPosition = pStream->nSize + (long)nOffset;
		break;
	default:
		return -1;
	}

	if ( nNewPosition < 0 || nNewPosition > pStream->nSize )
		return -1;

	pStream->nPosition = nNewPosition;
	return 0;
}

static int XiphClose( void *pUserData )
{
	(void)pUserData;
	return 0;
}

static long XiphTell( void *pUserData )
{
	SXiphMemoryStream *pStream = (SXiphMemoryStream*)pUserData;
	return pStream ? pStream->nPosition : -1;
}

static int AppendPcm( SXiphDecodedVorbis *pDecoded, const char *pBuffer, long nBytes )
{
	char *pNewData;

	if ( nBytes <= 0 )
		return 1;

	pNewData = (char*)realloc( pDecoded->pPcmData, pDecoded->nPcmBytes + nBytes );
	if ( !pNewData )
		return 0;

	pDecoded->pPcmData = pNewData;
	memcpy( pDecoded->pPcmData + pDecoded->nPcmBytes, pBuffer, nBytes );
	pDecoded->nPcmBytes += nBytes;
	return 1;
}

int DecodeXiphVorbisMemory( const char *pData, int nDataSize, SXiphDecodedVorbis *pDecoded )
{
	SXiphVorbisStream *pStream = 0;
	vorbis_info *pInfo;
	int nCurrentSection;
	char buffer[32768];

	if ( !pData || nDataSize <= 0 || !pDecoded )
		return 0;

	if ( !OpenXiphVorbisStreamMemory( pData, nDataSize, &pStream ) )
		return 0;

	memset( pDecoded, 0, sizeof( *pDecoded ) );
	pInfo = ov_info( &pStream->vorbisFile, -1 );
	if ( !pInfo || pInfo->channels <= 0 || pInfo->rate <= 0 )
	{
		CloseXiphVorbisStream( pStream );
		return 0;
	}

	pDecoded->nSampleRate = (unsigned int)pInfo->rate;
	pDecoded->nChannels = (unsigned int)pInfo->channels;
	pDecoded->nBitsPerSample = 16;
	pDecoded->nBlockAlign = pDecoded->nChannels * 2;

	for ( ;; )
	{
		long nRead = ov_read( &pStream->vorbisFile, buffer, sizeof( buffer ), 0, 2, 1, &nCurrentSection );
		if ( nRead == 0 )
			break;
		if ( nRead < 0 )
		{
			FreeXiphDecodedVorbis( pDecoded );
			CloseXiphVorbisStream( pStream );
			return 0;
		}
		if ( !AppendPcm( pDecoded, buffer, nRead ) )
		{
			FreeXiphDecodedVorbis( pDecoded );
			CloseXiphVorbisStream( pStream );
			return 0;
		}
	}

	CloseXiphVorbisStream( pStream );
	return pDecoded->pPcmData && pDecoded->nPcmBytes > 0;
}

void FreeXiphDecodedVorbis( SXiphDecodedVorbis *pDecoded )
{
	if ( pDecoded )
	{
		free( pDecoded->pPcmData );
		memset( pDecoded, 0, sizeof( *pDecoded ) );
	}
}

int OpenXiphVorbisStreamMemory( const char *pData, int nDataSize, SXiphVorbisStream **ppStream )
{
	SXiphVorbisStream *pStream;
	ov_callbacks callbacks;
	vorbis_info *pInfo;
	ogg_int64_t nTotalFrames;

	if ( !pData || nDataSize <= 0 || !ppStream )
		return 0;

	*ppStream = 0;
	pStream = (SXiphVorbisStream*)calloc( 1, sizeof( *pStream ) );
	if ( !pStream )
		return 0;

	pStream->memoryStream.pData = (const unsigned char*)pData;
	pStream->memoryStream.nSize = nDataSize;

	callbacks.read_func = XiphRead;
	callbacks.seek_func = XiphSeek;
	callbacks.close_func = XiphClose;
	callbacks.tell_func = XiphTell;

	if ( ov_open_callbacks( &pStream->memoryStream, &pStream->vorbisFile, 0, 0, callbacks ) < 0 )
	{
		free( pStream );
		return 0;
	}
	pStream->bOpen = 1;

	pInfo = ov_info( &pStream->vorbisFile, -1 );
	if ( !pInfo || pInfo->channels <= 0 || pInfo->rate <= 0 )
	{
		CloseXiphVorbisStream( pStream );
		return 0;
	}

	pStream->nSampleRate = (unsigned int)pInfo->rate;
	pStream->nChannels = (unsigned int)pInfo->channels;
	pStream->nBlockAlign = pStream->nChannels * 2;
	nTotalFrames = ov_pcm_total( &pStream->vorbisFile, -1 );
	pStream->nTotalFrames = nTotalFrames > 0 ? (unsigned long long)nTotalFrames : 0;
	*ppStream = pStream;
	return 1;
}

void CloseXiphVorbisStream( SXiphVorbisStream *pStream )
{
	if ( pStream )
	{
		if ( pStream->bOpen )
			ov_clear( &pStream->vorbisFile );
		free( pStream );
	}
}

long ReadXiphVorbisStream( SXiphVorbisStream *pStream, char *pBuffer, long nBytes )
{
	int nCurrentSection;
	long nTotalRead = 0;

	if ( !pStream || !pStream->bOpen || !pBuffer || nBytes <= 0 )
		return 0;

	while ( nTotalRead < nBytes )
	{
		long nRead = ov_read( &pStream->vorbisFile, pBuffer + nTotalRead, nBytes - nTotalRead, 0, 2, 1, &nCurrentSection );
		if ( nRead <= 0 )
			break;
		nTotalRead += nRead;
	}

	return nTotalRead;
}

int SeekXiphVorbisStream( SXiphVorbisStream *pStream, unsigned long long nFrame )
{
	if ( !pStream || !pStream->bOpen )
		return 0;
	return ov_pcm_seek( &pStream->vorbisFile, (ogg_int64_t)nFrame ) == 0;
}

unsigned long long TellXiphVorbisStream( SXiphVorbisStream *pStream )
{
	ogg_int64_t nFrame;
	if ( !pStream || !pStream->bOpen )
		return 0;
	nFrame = ov_pcm_tell( &pStream->vorbisFile );
	return nFrame > 0 ? (unsigned long long)nFrame : 0;
}

unsigned long long GetXiphVorbisStreamLength( SXiphVorbisStream *pStream )
{
	return pStream ? pStream->nTotalFrames : 0;
}

unsigned int GetXiphVorbisStreamSampleRate( SXiphVorbisStream *pStream )
{
	return pStream ? pStream->nSampleRate : 0;
}

unsigned int GetXiphVorbisStreamChannels( SXiphVorbisStream *pStream )
{
	return pStream ? pStream->nChannels : 0;
}

unsigned int GetXiphVorbisStreamBlockAlign( SXiphVorbisStream *pStream )
{
	return pStream ? pStream->nBlockAlign : 0;
}
