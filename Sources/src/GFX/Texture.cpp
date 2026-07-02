#include "StdAfx.h"

#include "Texture.h"
#include "..\Image\Image.h"
#include "..\Formats\fmtTexture.h"
EGFXPixelFormat GetGFXFormat( const SDDSPixelFormat &format )
{
	if ( format.dwFlags & DDS_FOURCC ) 
	{
		switch ( format.dwFourCC ) 
		{
			case MAKEFOURCC('D','X','T','1'):	return GFXPF_DXT1;
			case MAKEFOURCC('D','X','T','2'):	return GFXPF_DXT2;
			case MAKEFOURCC('D','X','T','3'):	return GFXPF_DXT3;
			case MAKEFOURCC('D','X','T','4'):	return GFXPF_DXT4;
			case MAKEFOURCC('D','X','T','5'):	return GFXPF_DXT5;
		}
	}
	else if ( (format.dwFlags & DDS_ARGB) == DDS_ARGB )
	{
		switch ( format.dwRGBBitCount ) 
		{
			case 32: return GFXPF_ARGB8888;
			case 16: return format.dwRBitMask == 0x00007c00 ? GFXPF_ARGB1555 : GFXPF_ARGB4444;
		}
	}
	else if ( (format.dwFlags & DDS_RGB) == DDS_RGB )
	{
		if ( format.dwRBitMask == 0x0000f800 ) 
			return GFXPF_ARGB0565;
	}
	return GFXPF_UNKNOWN;
}
bool LoadMipLevel( const SSurfaceLockInfo &lockinfo, IDataStream *pStream, 
									 const int nSizeX, const int nSizeY, const int nBPP, const int nCurrMipLevel, const int nLineModifier )
{
	if ( (nLineModifier * nSizeX * nBPP / 8) == lockinfo.nPitch )
	{
		const int nDataSize = nSizeX * nSizeY * nBPP / 8;
		const int nCheck = pStream->Read( lockinfo.pData, nDataSize );
		NI_ASSERT_SLOW_TF( nCheck == nDataSize, NStr::Format("Wrong texture read - read %d bytes instead of %d in %d mip level", nCheck, nDataSize, nCurrMipLevel), return false );
	}
	else
	{
		const int nDataSize = nLineModifier * nSizeX * nBPP / 8;
		BYTE *pDstData = (BYTE*)lockinfo.pData;
		for ( int j = 0; j < nSizeY/nLineModifier; ++j )
		{
			const int nCheck = pStream->Read( pDstData, nDataSize );
			NI_ASSERT_SLOW_TF( nCheck == nDataSize, NStr::Format("Wrong texture read - read %d bytes instead of %d in %d mip level in line %d", nCheck, nDataSize, nCurrMipLevel, j), return false );
			pDstData += lockinfo.nPitch;
		}
	}
	return true;
}
bool CTexture::Load( const bool bPreLoad )
{
	if ( pTexture != 0 ) 
		return true;
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	const int nStartMipLevel = 0;
	if ( pStream != 0 )										// load DDS image
	{
		DWORD dwSignature = 0;
		pStream->Read( &dwSignature, sizeof(dwSignature) );
		NI_ASSERT_SLOW_T( dwSignature == SDDSHeader::SIGNATURE, NStr::Format("Texture \"%s\" is not a DDS file!", szStreamName.c_str()) );
		if ( dwSignature != SDDSHeader::SIGNATURE ) 
			return false;
		SDDSHeader hdr;
		pStream->Read( &hdr, sizeof(hdr) );
		nSizeX = hdr.dwWidth >> nStartMipLevel;
		nSizeY = hdr.dwHeight >> nStartMipLevel;
		format = GetGFXFormat( hdr.ddspf );
		if ( bPreLoad ) 
			return true;
		const int nNumMipLevels = hdr.dwHeaderFlags & DDS_HEADER_FLAGS_MIPMAP ? (hdr.dwMipMapCount == 0 ? 1 : hdr.dwMipMapCount) : 1;
		const int nNumMips = nNumMipLevels - nStartMipLevel;
		const int nBPP = ::GetBPP( format );
		GetSingleton<IGFX>()->CreateTexture( nSizeX, nSizeY, nNumMips, format, GFXD_STATIC, this );
		if ( nStartMipLevel > 0 ) 
		{
			int nToSeek = 0;
			for ( int i = 0; i < nStartMipLevel; ++i )
				nToSeek += (hdr.dwWidth >> i) * (hdr.dwHeight >> i) * nBPP / 8;
			pStream->Seek( nToSeek, STREAM_SEEK_CUR );
		}
		const int nLineModifier = (format >= GFXPF_DXT1) && (format <= GFXPF_DXT5) ? 4 : 1;
		for ( int i = 0; i < nNumMips; ++i )
		{
			SSurfaceLockInfo lockinfo;
			this->Lock( i, &lockinfo );
			LoadMipLevel( lockinfo, pStream, nSizeX >> i, nSizeY >> i, nBPP, nStartMipLevel + i, nLineModifier );
			this->Unlock( i );
		}
	}
	else																	// create checker texture
	{
		nSizeX = 32;
		nSizeY = 32;
		format = GFXPF_ARGB8888;
		if ( bPreLoad ) 
			return true;
		GetSingleton<IGFX>()->CreateTexture( nSizeX, nSizeY, 1, format, GFXD_STATIC, this );
		CPtr<IImage> pImage = GetSingleton<IImageProcessor>()->GenerateImage( 32, 32, IGT_CHECKER );

		SSurfaceLockInfo lockinfo;
		this->Lock( 0, &lockinfo );
		const SColor *pData = pImage->GetLFB();
		if ( (nSizeX * sizeof(SColor)) == lockinfo.nPitch )
		{
			const int nCopySize = nSizeX * nSizeY * sizeof( SColor );
			memcpy( lockinfo.pData, pData, nCopySize );
			pData += nCopySize / sizeof( SColor );
		}
		else                                // copy line by line
		{
			const int nCopySize = nSizeX * sizeof( SColor );
			for ( int j = 0; j < nSizeY; ++j )
			{
				memcpy( lockinfo.pData, pData, nCopySize );
				pData += nCopySize / sizeof( SColor );
				lockinfo.pData = (void*)( DWORD(lockinfo.pData) + lockinfo.nPitch );
			}
		}
		this->Unlock( 0 );
	}

	return true;	
}
void CTexture::Init( IDirect3DTexture8 *_pTexture, const int _nMemUsage )
{
	NI_ASSERT_T( _pTexture != 0, "Can't init texture from NULL D3D object" );
	pTexture = _pTexture;
	if ( (nSizeX == -1) || (nSizeY == -1) ) 
	{
		D3DSURFACE_DESC desc;
		pTexture->GetLevelDesc( 0, &desc );
		nSizeX = desc.Width;
		nSizeY = desc.Height;
		format = D3DToGFXPixelFormat( desc.Format );
	}
	nMemUsage = _nMemUsage;
}
IDirect3DTexture8* CTexture::GetInternalContainer()
{ 
	if ( pTexture == 0 ) 
		Load();
	return pTexture;
}
bool CTexture::Lock( int nLevel, SSurfaceLockInfo *pLockInfo )
{
	D3DLOCKED_RECT lr;
  HRESULT dxrval = pTexture->LockRect( nLevel, &lr, 0, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Failed to lock rect on the level %d of the texture", nLevel), return false );
	pLockInfo->nPitch = lr.Pitch;
	pLockInfo->pData = lr.pBits;
  return true;
} 
bool CTexture::Unlock( int nLevel )
{
  HRESULT dxrval = pTexture->UnlockRect( nLevel );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Faild to unlock rect on the level %d of the texture", nLevel), return false );
  return true;
}
bool CTexture::AddDirtyRect( const RECT *pRect )
{
  HRESULT dxrval = pTexture->AddDirtyRect( pRect );
	NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Failed to add dirty rect (%d,%d : %d,%d) to the texture", pRect->left, pRect->top, pRect->right, pRect->bottom), return false );
  return true;
}
int CTexture::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nSharedResourceLastUsage.a );
	return 0;
}
void CRenderTargetTexture::Init( IDirect3DTexture8 *_pTexture, IDirect3DSurface8 *_pDepth, int _nMemUsage )
{
	NI_ASSERT_T( _pTexture != 0, "Can't init texture from NULL D3D object" );
	pTexture = _pTexture;
	if ( (nSizeX == -1) || (nSizeY == -1) ) 
	{
		D3DSURFACE_DESC desc;
		pTexture->GetLevelDesc( 0, &desc );
		nSizeX = desc.Width;
		nSizeY = desc.Height;
	}
	nMemUsage = _nMemUsage;
	IDirect3DSurface8 *pSurface = 0;
	HRESULT dxrval = pTexture->GetSurfaceLevel( 0, &pSurface );
	NI_ASSERTHR_T( dxrval, "Can't retrieve 0th surface level from render-target texture" );
	pColor = pSurface;
	pSurface->Release();
	pDepth = _pDepth;
}
void CSurface::Init( IDirect3DSurface8 *_pSurface )
{
	NI_ASSERT_T( _pSurface != 0, "Can't init surface from NULL D3D object" );
	pSurface = _pSurface;
	D3DSURFACE_DESC desc;
	pSurface->GetDesc( &desc );
	nSizeX = desc.Width;
	nSizeY = desc.Height;
	format = D3DToGFXPixelFormat( desc.Format );
}
bool CSurface::Lock( SSurfaceLockInfo *pLockInfo )
{
	D3DLOCKED_RECT lr;
  HRESULT dxrval = pSurface->LockRect( &lr, 0, D3DLOCK_NOSYSLOCK );
	NI_ASSERTHR_SLOW_TF( dxrval, "Failed to lock rect on the surfaces", return false );
	pLockInfo->nPitch = lr.Pitch;
	pLockInfo->pData = lr.pBits;
  return true;
} 
bool CSurface::Unlock()
{
  HRESULT dxrval = pSurface->UnlockRect();
	NI_ASSERTHR_SLOW_TF( dxrval, "Faild to unlock surface's rect", return false );
  return true;
}
