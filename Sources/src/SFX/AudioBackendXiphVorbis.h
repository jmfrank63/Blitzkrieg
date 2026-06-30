#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SXiphDecodedVorbis
{
	unsigned int nSampleRate;
	unsigned int nChannels;
	unsigned int nBitsPerSample;
	unsigned int nBlockAlign;
	unsigned int nPcmBytes;
	char *pPcmData;
} SXiphDecodedVorbis;

typedef struct SXiphVorbisStream SXiphVorbisStream;

int DecodeXiphVorbisMemory( const char *pData, int nDataSize, SXiphDecodedVorbis *pDecoded );
void FreeXiphDecodedVorbis( SXiphDecodedVorbis *pDecoded );

int OpenXiphVorbisStreamMemory( const char *pData, int nDataSize, SXiphVorbisStream **ppStream );
void CloseXiphVorbisStream( SXiphVorbisStream *pStream );
long ReadXiphVorbisStream( SXiphVorbisStream *pStream, char *pBuffer, long nBytes );
int SeekXiphVorbisStream( SXiphVorbisStream *pStream, unsigned long long nFrame );
unsigned long long TellXiphVorbisStream( SXiphVorbisStream *pStream );
unsigned long long GetXiphVorbisStreamLength( SXiphVorbisStream *pStream );
unsigned int GetXiphVorbisStreamSampleRate( SXiphVorbisStream *pStream );
unsigned int GetXiphVorbisStreamChannels( SXiphVorbisStream *pStream );
unsigned int GetXiphVorbisStreamBlockAlign( SXiphVorbisStream *pStream );

#ifdef __cplusplus
}
#endif
