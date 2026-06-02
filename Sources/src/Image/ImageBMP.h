#ifndef __IMAGE_BMP_H__
#define __IMAGE_BMP_H__
#include "ImageReal.h"
namespace NImage
{
	bool RecognizeFormatBMP( IDataStream *pStream );
	CImage* LoadImageBMP( IDataStream *pStream );
};
#endif // __IMAGE_BMP_H__