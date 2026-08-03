#include "StdAfx.h"
#include "UIMask.h"
#include "../Image/Image.h"
bool CUIMask::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	CPtr<IImageProcessor> pIP = GetSingleton<IImageProcessor>();
	CPtr<IImage> pImage = pIP->LoadImage( pStream );
	if ( pImage == 0 )
		return false;

	int nSizeX = pImage->GetSizeX();
	data.SetSizes( nSizeX, pImage->GetSizeY() );
	const SColor* pIB = pImage->GetLFB();			//image buffer
	BYTE *pMB = data.GetBuffer();		//mask buffer
	for ( int y = 0; y < pImage->GetSizeY(); ++y )
	{
		for ( int x = 0; x < nSizeX; ++x )
			pMB[y*nSizeX + x] = pIB[y*nSizeX + x].a;
	}
	return true;
}
void CUIMask::SwapData( ISharedResource *pResource )
{
	CUIMask *pRes = dynamic_cast<CUIMask*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a CUIMask", return );
	std::swap( data, pRes->data );
}
