#ifndef __IMAGETGA_H__
#define __IMAGETGA_H__
#include "ImageReal.h"
namespace NImage
{
	bool RecognizeFormatTGA( IDataStream *pStream );
	CImage* LoadImageTGA( IDataStream *pStream );
	bool SaveImageAsTGA( IDataStream *pStream, const IImage *pImage );
};
#endif // __IMAGETGA_H__
