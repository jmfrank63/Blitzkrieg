#pragma once
// Portable data-only port of the MFC-era S3TC encoder/decoder used by ResourceEditor before the
// NDxt rewrite (see Sources/src/Image/S3TC.{h,cpp} at commit 40645ad29^). Kept bit-identical to the
// legacy algorithm so T04 can measure the tolerance between it and the modern NDxt encoder; this is
// a measurement reference, not a shipping path.
//
// Differences from NDxt that make a tolerance non-zero:
//   - Pack565 truncates (r >> 3) where NDxt rounds ((r*31 + 127)/255).
//   - DXT3 alpha truncates (a >> 4) where NDxt rounds ((a*15 + 127)/255).
//   - Endpoint selection ranks by luma = R + G + B instead of 299/587/114.
//   - When min and max endpoints collide the legacy encoder nudges c1/c0 to force two distinct
//     endpoints, so a block keeps the palette mode it asked for.
//   - DXT1 runs in S3TC's colour-key mode, as MFC's CompressDXTN called it
//     (S3TC_ENCODE_RGB_COLOR_KEY, alpha reference 0): every block uses the 3-colour palette and
//     alpha-0 pixels are punch-through. NDxt uses the 4-colour palette for opaque blocks.
//   - DXT5 alpha with min == max is nudged the same way to keep the 8-step interpolation.
#include <cstdint>

namespace NLegacyDxt
{
	enum class Format
	{
		DXT1,
		DXT3,
		DXT5
	};

	struct SurfaceDesc
	{
		int width;
		int height;
		int pitch; // bytes per row of the ARGB8888 source (input) or encoded surface (decode)
		const void *data;
	};

	int GetEncodedSize( int width, int height, Format format );
	int GetDecodedSize( int width, int height );

	// Encodes an ARGB8888 image (pitch in bytes) into DXT1/3/5 blocks.
	void Encode( const SurfaceDesc &input, Format format, void *output );

	// Decodes DXT1/3/5 blocks back into ARGB8888 pixels (tightly packed, width*4 bytes per row).
	void Decode( const SurfaceDesc &input, Format format, void *outputPixels );
}
