#ifndef __IMAGE_PNG_H__
#define __IMAGE_PNG_H__
#include "ImageReal.h"
namespace NImage
{
	bool RecognizeFormatPNG( IDataStream *pStream );
	CImage* LoadImagePNG( IDataStream *pStream );
	bool SaveImageAsPNG( IDataStream *pStream, const IImage *pImage );
};
#endif // __IMAGE_PNG_H__