#ifndef __IMAGE_HELPER_H__
#define __IMAGE_HELPER_H__
inline const EGFXPixelFormat ChooseBestFormat( IImage *pImage, const ECompressionType eType )
{
	SColor *pColors = pImage->GetLFB();
	const int nNumColors = pImage->GetSizeX() * pImage->GetSizeY();
	float alphas[256];
	memset( alphas, 0, sizeof(alphas) );
	for ( int i = 0; i < pImage->GetSizeX()*pImage->GetSizeY(); ++i )
		alphas[ pColors[i].a ]++;
	int nMin = 0;
	for ( int i = 0; i < 256; ++i )
	{
		alphas[i] /= float( nNumColors );
		if ( alphas[i] < alphas[nMin] ) 
			nMin = i;
	}
	int nMax1 = nMin, nMax2 = nMin, nMax3 = nMin, nMax4 = nMin;
	for ( int i = 0; i < 256; ++i )
	{
		if ( alphas[i] > alphas[nMax1] ) 
			nMax1 = i;
	}
	for ( int i = 0; i < 256; ++i )
	{
		if ( (alphas[i] > alphas[nMax2]) && (i != nMax1) ) 
			nMax2 = i;
	}
	for ( int i = 0; i < 256; ++i )
	{
		if ( (alphas[i] > alphas[nMax3]) && (i != nMax1) && (i != nMax2) ) 
			nMax3 = i;
	}
	for ( int i = 0; i < 256; ++i )
	{
		if ( (alphas[i] > alphas[nMax4]) && (i != nMax1) && (i != nMax2) && (i != nMax3) ) 
			nMax4 = i;
	}
	switch ( eType ) 
	{
		case COMPRESSION_DXT:
			if ( alphas[255] == 1 ) 
				return GFXPF_DXT1;
			else if ( ( ((nMax1 == 255) && (nMax2 == 0)) || ((nMax2 == 255) && (nMax1 == 0)) ) && (alphas[0] + alphas[255] > 0.999f) )
				return GFXPF_DXT1;
			else
				return GFXPF_DXT5;
		case COMPRESSION_HIGH_QUALITY:
			return GFXPF_ARGB8888;
		case COMPRESSION_LOW_QUALITY:
			if ( alphas[255] == 1 ) 
				return GFXPF_ARGB0565;
			else if ( ( ((nMax1 == 255) && (nMax2 == 0)) || ((nMax2 == 255) && (nMax1 == 0)) ) && (alphas[0] + alphas[255] > 0.999f) )
				return GFXPF_ARGB1555;
			else
				return GFXPF_ARGB4444;
	}
	return GFXPF_UNKNOWN;
}
#endif // __IMAGE_HELPER_H__