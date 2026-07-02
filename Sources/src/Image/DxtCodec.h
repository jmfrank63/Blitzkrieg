#ifndef __DXTCODEC_H__
#define __DXTCODEC_H__
#pragma once

#include <cstdint>

namespace NDxt
{
	enum class Format
	{
		DXT1,
		DXT2,
		DXT3,
		DXT4,
		DXT5
	};

	struct DxtSurfaceDesc
	{
		int width;
		int height;
		int pitch;
		const void *data;
	};

	int GetEncodedSize( int width, int height, Format format );
	int GetDecodedSize( int width, int height );
	void Encode( const DxtSurfaceDesc &input, Format format, void *output );
	void Decode( const DxtSurfaceDesc &input, Format format, void *outputPixels );
}

#endif // __DXTCODEC_H__
