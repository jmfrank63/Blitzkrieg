#ifndef __S3TC_H__
#define __S3TC_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Formats\fmtTexture.h"

#ifndef DDPF_ALPHAPIXELS
#define DDPF_ALPHAPIXELS 0x00000001
#endif
#ifndef DDPF_FOURCC
#define DDPF_FOURCC      0x00000004
#endif
#ifndef DDPF_RGB
#define DDPF_RGB         0x00000040
#endif

#ifndef DDSD_HEIGHT
#define DDSD_HEIGHT      0x00000002
#endif
#ifndef DDSD_WIDTH
#define DDSD_WIDTH       0x00000004
#endif
#ifndef DDSD_PITCH
#define DDSD_PITCH       0x00000008
#endif
#ifndef DDSD_PIXELFORMAT
#define DDSD_PIXELFORMAT 0x00001000
#endif
#ifndef DDSD_LPSURFACE
#define DDSD_LPSURFACE   0x00000800
#endif
#ifndef DDSD_LINEARSIZE
#define DDSD_LINEARSIZE  0x00080000
#endif

struct DDPIXELFORMAT
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	union
	{
		DWORD dwABitMask;
		DWORD dwRGBAlphaBitMask;
	};
};

struct DDSURFACEDESC
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwHeight;
	DWORD dwWidth;
	LONG lPitch;
	void *lpSurface;
	DDPIXELFORMAT ddpfPixelFormat;
};

enum
{
	S3TC_ENCODE_RGB_FULL = 0x00000001,
	S3TC_ENCODE_RGB_ALPHA_COMPARE = 0x00000002,
	S3TC_ENCODE_RGB_COLOR_KEY = 0x00000004,
	S3TC_ENCODE_ALPHA_EXPLICIT = 0x00000010,
	S3TC_ENCODE_ALPHA_INTERPOLATED = 0x00000020
};

#ifdef __cplusplus
extern "C" {
#endif

int S3TCgetEncodeSize( const DDSURFACEDESC *pIn, DWORD dwEncodeType );
int S3TCgetDecodeSize( const DDSURFACEDESC *pIn );
void S3TCsetAlphaReference( DWORD dwAlphaReference );
void S3TCencode( const DDSURFACEDESC *pIn, const void *pReserved, DDSURFACEDESC *pOut, void *pOutBits, DWORD dwEncodeType, const float *pWeights );
void S3TCdecode( const DDSURFACEDESC *pIn, DDSURFACEDESC *pOut, void *pOutBits );

#ifdef __cplusplus
}
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __S3TC_H__
