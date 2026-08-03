#if !defined(__ImageBuilder__Types__)
#define __ImageBuilder__Types__

#include "..\Image\Image.h"
#include "..\Formats\FmtSprite.h"
inline std::string GetDDSImageExtention( ECompressionType compressionType )
{
	if ( compressionType == COMPRESSION_DXT )
	{
		return "_c.dds";
	}
	else if ( compressionType == COMPRESSION_LOW_QUALITY )
	{
		return "_l.dds";
	}
	else if ( compressionType == COMPRESSION_HIGH_QUALITY )
	{
		return "_h.dds";
	}
	return "";
}
class CUnsafeImageAccessor
{
	IImage *pImage;
	std::vector<SColor*> rows;
	void Set( IImage *_pImage )
	{
		pImage = _pImage;
		if ( pImage )
		{
			rows.resize( pImage->GetSizeY() );
			SColor *pColors = pImage->GetLFB();
			int nSizeX = pImage->GetSizeX();
			for ( int i=0; i<rows.size(); ++i )
				rows[i] = pColors + i*nSizeX;
		}
	}
public:
	CUnsafeImageAccessor() {  }
	CUnsafeImageAccessor( const CPtr<IImage> &_pImage ) { Set( _pImage.GetPtr() ); }
	CUnsafeImageAccessor( IImage *_pImage ) { Set( _pImage ); }
	const CUnsafeImageAccessor& operator=( const CPtr<IImage> &_pImage ) { Set( _pImage.GetPtr() ); return *this; }
	const CUnsafeImageAccessor& operator=( IImage *_pImage ) { Set( _pImage ); return *this; }
	const CUnsafeImageAccessor& operator=( const CUnsafeImageAccessor &accessor ) { Set( accessor.pImage ); return *this; }
	operator IImage*() const { return pImage; }
	IImage* operator->() const { return pImage; }
	bool operator==( const CUnsafeImageAccessor &ptr ) const { return ( pImage == ptr.pImage ); }
	bool operator==( IImage *pNewObject ) const { return ( pImage == pNewObject ); }
	bool operator!=( const CUnsafeImageAccessor &ptr ) const { return ( pImage != ptr.pImage ); }
	bool operator!=( IImage *pNewObject ) const { return ( pImage != pNewObject ); }
	const SColor* operator[]( int nY ) const { return rows[nY]; }
	SColor* operator[]( int nY ) { return rows[nY]; }
	const SColor& operator()( int nX, int nY ) const { return rows[nY][nX]; }
	SColor& operator()( int nX, int nY ) { return rows[nY][nX]; }
};

class CSpritesPackBuilder
{
	CSpritesPackBuilder() {}	

	static const int nCellSizeY;
	static const int nCellSizeX;

/**
	void GetSquareRect( const TSquares::const_iterator &rSquareIterator, CTRect<int> *pSquareRect ) const
	{
		pSquareRect->minx = rSquareIterator->leftTop.x + leftTop.x;
		pSquareRect->miny = rSquareIterator->leftTop.y + leftTop.y;
		pSquareRect->maxx = pSquareRect->minx + rSquareIterator->nSize;
		pSquareRect->maxy = pSquareRect->miny + rSquareIterator->nSize;
	}
/**/
	struct SCollectSquaresParameter
	{
		friend class CSpritesPackBuilder;

		private:
		int nMaxDepth;
		CTPoint<int> shift;
		CPtr<IImage> pImage;
		DWORD dwMinAlpha;
		SSpritesPack::SSprite *pSprite;
	};

	public:
	struct SPackParameter
	{
		CPtr<IImage> pImage;
		CTPoint<int> center;
		DWORD dwMinAlpha;
		
		CArray2D<BYTE> lockedTiles;
		CTPoint<int> lockedTilesCenter;
		
		SPackParameter()
			: pImage( 0 ), center( 0, 0 ), dwMinAlpha( 1 ), lockedTilesCenter( 0, 0 ) {}
		SPackParameter( const CPtr<IImage> &rpImage, const CTPoint<int> &rCenter, DWORD _dwMinAlpha, const CArray2D<BYTE> &rLockedTiles, const CTPoint<int> &rLockedTilesCenter )
			: pImage( rpImage ), center( rCenter ), dwMinAlpha( _dwMinAlpha ), lockedTiles( rLockedTiles ), lockedTilesCenter( rLockedTilesCenter ) {}
		SPackParameter( const SPackParameter &rPackParameter )
			: pImage( rPackParameter.pImage ), center( rPackParameter.center ), dwMinAlpha( rPackParameter.dwMinAlpha ), lockedTiles( rPackParameter.lockedTiles ), lockedTilesCenter( rPackParameter.lockedTilesCenter ) {}
		SPackParameter& operator=( const SPackParameter &rPackParameter )
		{
			if( &rPackParameter != this )
			{
				pImage = rPackParameter.pImage;
				center = rPackParameter.center;
				dwMinAlpha = rPackParameter.dwMinAlpha;
				lockedTiles = rPackParameter.lockedTiles;
				lockedTilesCenter = rPackParameter.lockedTilesCenter;
			}
			return *this;
		}
	};
	typedef std::vector<SPackParameter> CPackParameters;

	static void GetActualRect( SSpritesPack::CSpritesList::const_iterator spritesListIterator, CTRect<int> *pActualRect );
	static bool GetMinimalImageSize( const SSpritesPack *pSpritesPack, CTPoint<int> *pMinimalImageSize, CTPoint<int> *pCollectedSquareSideSize, int *pnSquaresCount );

	static bool CollectSquares( int nCurrentDepth, const CTPoint<int> &rLeftTop, int nSquareSideSize, SCollectSquaresParameter *pParameter );
	static bool CollectSquares( SSpritesPack *pSpritesPack, const CPackParameters &rPackParameters, int nMaxSquareSideSize, int nMaxDepth, int nMinDepth );

	static void MarkLockedTile( IImage *pImage, const CTPoint<int> &rPoint );
	static IImage* CreateLockArrayImage( const CArray2D<BYTE> &rLockedTiles, const CTPoint<int> &rLockedTilesCenter, const CTRect<int> rActualRect );

	static IImage* Pack( SSpritesPack *pSpritesPack, const CPackParameters &rPackParameters, int nMaxSquareSideSize, int nDepth );
	static IImage* Pack( SSpritesPack *pSpritesPack, const SPackParameter &rPackParameter, int nMaxSquareSideSize, int nDepth );

	static IImage* Unpack							( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect = 0 );
	static IImage* UnpackAndMarkEdge	( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect = 0 );
	static IImage* UnpackAndMarkBounds( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect = 0 );
	static IImage* UnpackAndMarkInEdge( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect = 0 );
	static IImage* UnpackAndMarkDepth	( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect = 0 );
	static IImage* UnpackAndMarkAlpha	( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, DWORD dwMinAlpha, DWORD dwMaxAlpha, CTRect<int> *pActualRect = 0 );
};

class CRMImageBuilder
{
	CRMImageBuilder() {}

public:
	static const SColor BLACK_COLOR;
	static const SColor WHITE_COLOR;
	static const SColor GRAY_LIGHTER_COLOR;
	static const SColor GRAY_DARKER_COLOR;
	static const SColor BASE_EMBOSS_COLOR;

	static bool ApplyFilter( IImage *pImage, const CArray2D<int> &rFilter, DWORD dwMinAlpha );
	static bool Emboss( IImage *pImage, const CTPoint<int> &rShiftPoint, const CArray2D<int> &rFilter, DWORD dwMinAlpha );
	static bool Noise( IImage *pImage, IImage *pNoise, DWORD dwMinAlpha );
	static bool FastAddImageByAlpha( IImage *pImage, IImage *pImageToAdd, DWORD dwMinAlpha );
	static bool FastAddImageByColor( IImage *pImage, IImage *pImageToAdd, SColor color );

	static IImage* GetEdge( IImage *pImage, SColor edgeColor, SColor nonEdgeColor, DWORD dwMinAlpha );
	static IImage* GetShadow( IImage *pImage, const CTPoint<int> &rShiftPoint, SColor shadowColor, SColor nonShadowColor, DWORD dwMinAlpha );
	static IImage* GetAlphaEmboss( IImage *pImage, const CTPoint<int> &rShiftPoint, int nFilterSize, DWORD dwMinAlpha );
	static IImage* FastComposeImagesByAlpha( const std::vector<CPtr<IImage> > &rImages, DWORD dwMinAlpha );
	static IImage* FastComposeImagesByColor( const std::vector<CPtr<IImage> > &rImages, SColor color );
};

struct SRMImageApplyFilterFunctional
{
	friend class CRMImageBuilder;
private:
	IImage *pSourceImage;
	IImage *pDestImage;
	const CArray2D<int> *pFilter;
	int nDivider;
	CImageAccessor sourceImageAccessor;
	CImageAccessor destImageAccessor;

	SRMImageApplyFilterFunctional( IImage *_pSourceImage, IImage *_pDestImage, const CArray2D<int> *_pFilter, int _nDivider ) 
		: pSourceImage( _pSourceImage ), pDestImage( _pDestImage ), pFilter( _pFilter ), nDivider( _nDivider )
	{
		NI_ASSERT_T( ( pSourceImage != 0 ) &&
								 ( pDestImage != 0 ) &&
								 ( pFilter != 0 ) &&
								 ( nDivider != 0 ),
								 NStr::Format( "Wrong parameter: %x, %x, %x, %d\n", pSourceImage, pDestImage, pFilter, nDivider ) );
		NI_ASSERT_T( ( pSourceImage->GetSizeX() == pDestImage->GetSizeX() ) &&
								 ( pSourceImage->GetSizeY() == pDestImage->GetSizeY() ) && 
								 ( ( pFilter->GetSizeX() & 0x1 ) != 0 ) &&
								 ( ( pFilter->GetSizeY() & 0x1 ) != 0 ),
								 NStr::Format( "Invalid size: source:(%d, %d), dest:(%d, %d), filter:(%d %d)\n", pSourceImage->GetSizeX(), pSourceImage->GetSizeY(), pDestImage->GetSizeX(), pDestImage->GetSizeY(), pFilter->GetSizeX(), pFilter->GetSizeY() ) );
		sourceImageAccessor = pSourceImage;
		destImageAccessor = pDestImage;
	}

	bool operator()( int nXIndex, int nYIndex )
	{ 
		CTRect<int> localIndices( nXIndex - ( pFilter->GetSizeX() / 2 ),
															nYIndex - ( pFilter->GetSizeY() / 2 ),
															nXIndex + ( pFilter->GetSizeX() / 2 ),
															nYIndex + ( pFilter->GetSizeY() / 2 ) );
		DWORD dwRed = 0;
		DWORD dwGreen = 0;
		DWORD dwBlue = 0;

		for ( int nFilterXIndex = 0, nLocalXIndex = localIndices.minx; 
					nLocalXIndex <= localIndices.maxx;
					++nFilterXIndex, ++nLocalXIndex )
		{
			for ( int nFilterYIndex = 0, nLocalYIndex = localIndices.miny;
						nLocalYIndex <= localIndices.maxy;
						++nFilterYIndex, ++nLocalYIndex )
			{
				int nValue = ( *pFilter )[nFilterYIndex][nFilterXIndex];
				const SColor &rSourceColor = sourceImageAccessor[nLocalYIndex][nLocalXIndex];
				dwRed += rSourceColor.r * nValue;
				dwGreen += rSourceColor.g * nValue;
				dwBlue += rSourceColor.b * nValue;
			}
		}
		
		dwRed /= nDivider;
		dwGreen /= nDivider;
		dwBlue /= nDivider;

		if ( dwRed < 0 ) dwRed = 0;
		if ( dwRed > 0xFF ) dwRed = 0xFF;
		if ( dwGreen < 0 ) dwGreen = 0;
		if ( dwGreen > 0xFF ) dwGreen = 0xFF;
		if ( dwBlue < 0 ) dwBlue = 0;
		if ( dwBlue > 0xFF ) dwBlue = 0xFF;

		SColor destColor( sourceImageAccessor[nYIndex][nXIndex].a, dwRed, dwGreen, dwBlue );
		destImageAccessor[nYIndex][nXIndex] = destColor;
		return true;
	}
};

struct SRMImageApplyFilterInBoundsFunctional
{
	friend class CRMImageBuilder;
private:
	IImage *pSourceImage;
	IImage *pDestImage;
	const CArray2D<int> *pFilter;
	int nDivider;
	CImageAccessor sourceImageAccessor;
	CImageAccessor destImageAccessor;

	SRMImageApplyFilterInBoundsFunctional( IImage *_pSourceImage, IImage *_pDestImage, const CArray2D<int> *_pFilter, int _nDivider ) 
		: pSourceImage( _pSourceImage ), pDestImage( _pDestImage ), pFilter( _pFilter ), nDivider( _nDivider )
	{
		NI_ASSERT_T( ( pSourceImage != 0 ) &&
								 ( pDestImage != 0 ) &&
								 ( pFilter != 0 ) &&
								 ( nDivider != 0 ),
								 NStr::Format( "Wrong parameter: %x, %x, %x, %d\n", pSourceImage, pDestImage, pFilter, nDivider ) );
		NI_ASSERT_T( ( pSourceImage->GetSizeX() == pDestImage->GetSizeX() ) &&
								 ( pSourceImage->GetSizeY() == pDestImage->GetSizeY() ) && 
								 ( ( pFilter->GetSizeX() & 0x1 ) != 0 ) &&
								 ( ( pFilter->GetSizeY() & 0x1 ) != 0 ),
								 NStr::Format( "Invalid size: source:(%d, %d), dest:(%d, %d), filter:(%d %d)\n", pSourceImage->GetSizeX(), pSourceImage->GetSizeY(), pDestImage->GetSizeX(), pDestImage->GetSizeY(), pFilter->GetSizeX(), pFilter->GetSizeY() ) );
		sourceImageAccessor = pSourceImage;
		destImageAccessor = pDestImage;
	}

	bool operator()( int nXIndex, int nYIndex )
	{ 
		CTRect<int> localIndices( nXIndex - ( pFilter->GetSizeX() / 2 ),
															nYIndex - ( pFilter->GetSizeY() / 2 ),
															nXIndex + ( pFilter->GetSizeX() / 2 ),
															nYIndex + ( pFilter->GetSizeY() / 2 ) );
		DWORD dwRed = 0;
		DWORD dwGreen = 0;
		DWORD dwBlue = 0;

		for ( int nFilterXIndex = 0, nLocalXIndex = localIndices.minx; 
					nLocalXIndex <= localIndices.maxx;
					++nFilterXIndex, ++nLocalXIndex )
		{
			for ( int nFilterYIndex = 0, nLocalYIndex = localIndices.miny;
						nLocalYIndex <= localIndices.maxy;
						++nFilterYIndex, ++nLocalYIndex )
			{
				int nXPos = nLocalXIndex;
				int nYPos = nLocalYIndex;

				if ( nXPos < 0 )
				{
					nXPos = 0;
				}
				if ( nXPos >= pSourceImage->GetSizeX() )
				{
					nXPos = pSourceImage->GetSizeX() - 1;
				}
				if ( nYPos < 0 )
				{
					nYPos = 0;
				}
				if ( nYPos >= pSourceImage->GetSizeY() )
				{
					nYPos = pSourceImage->GetSizeY() - 1;
				}
				
				int nValue = ( *pFilter )[nFilterYIndex][nFilterXIndex];
				const SColor &rSourceColor = sourceImageAccessor[nYPos][nXPos];
				dwRed += rSourceColor.r * nValue;
				dwGreen += rSourceColor.g * nValue;
				dwBlue += rSourceColor.b * nValue;
			}
		}
		
		dwRed /= nDivider;
		dwGreen /= nDivider;
		dwBlue /= nDivider;

		if ( dwRed < 0 ) dwRed = 0;
		if ( dwRed > 0xFF ) dwRed = 0xFF;
		if ( dwGreen < 0 ) dwGreen = 0;
		if ( dwGreen > 0xFF ) dwGreen = 0xFF;
		if ( dwBlue < 0 ) dwBlue = 0;
		if ( dwBlue > 0xFF ) dwBlue = 0xFF;

		SColor destColor( sourceImageAccessor[nYIndex][nXIndex].a, dwRed, dwGreen, dwBlue );
		destImageAccessor[nYIndex][nXIndex] = destColor;
		return true;
	}
};

struct SRMGetImageEdgeFunctional
{
	friend class CRMImageBuilder;
private:
	IImage *pImage;
	IImage *pEdgedImage;
	SColor edgedColor;
	DWORD dwMinAlpha;
		
	CImageAccessor imageAccessor;
	CUnsafeImageAccessor edgedImageAccessor;

	SRMGetImageEdgeFunctional( IImage *_pImage, IImage *_pEdgedImage, SColor _edgedColor, DWORD _dwMinAlpha ) 
		: pImage( _pImage ), pEdgedImage( _pEdgedImage ), edgedColor( _edgedColor ), dwMinAlpha( _dwMinAlpha )
	{
		NI_ASSERT_T( ( pImage != 0 ) &&
								 ( pEdgedImage != 0 ),
								 NStr::Format( "Wrong parameter: %x, %x\n", pImage, pEdgedImage ) );
		NI_ASSERT_T( ( pImage->GetSizeX() == pEdgedImage->GetSizeX() ) &&
								 ( pImage->GetSizeY() == pEdgedImage->GetSizeY() ),
								 NStr::Format( "Invalid size: source:(%d, %d), dest:(%d, %d)\n", pImage->GetSizeX(), pImage->GetSizeY(), pEdgedImage->GetSizeX(), pEdgedImage->GetSizeY() ) );
		imageAccessor = pImage;
		edgedImageAccessor = pEdgedImage;
	}

	bool operator()( int nXIndex, int nYIndex )
	{ 
		if ( imageAccessor[nYIndex][nXIndex].a < dwMinAlpha )
		{
			int nValidPixelsCount = 0;
			for ( int nLocalYIndex = nYIndex - 1; nLocalYIndex <= nYIndex + 1; ++nLocalYIndex )
			{
				for ( int nLocalXIndex = nXIndex - 1; nLocalXIndex <= nXIndex + 1; ++nLocalXIndex )
				{
					nValidPixelsCount += ( imageAccessor[nLocalYIndex][nLocalXIndex] >= dwMinAlpha ) ? 1 : 0;
				}
			}
			if ( nValidPixelsCount > 0 )
			{
				edgedImageAccessor[nYIndex][nXIndex] = edgedColor;
			}
		}
		return true;
	}
};

struct SRMGetImageEdgeInBoundsFunctional
{
	friend class CRMImageBuilder;
private:
	IImage *pImage;
	IImage *pEdgedImage;
	SColor edgedColor;
	DWORD dwMinAlpha;
		
	CImageAccessor imageAccessor;
	CUnsafeImageAccessor edgedImageAccessor;

	SRMGetImageEdgeInBoundsFunctional( IImage *_pImage, IImage *_pEdgedImage, SColor _edgedColor, DWORD _dwMinAlpha ) 
		: pImage( _pImage ), pEdgedImage( _pEdgedImage ), edgedColor( _edgedColor ), dwMinAlpha( _dwMinAlpha )
	{
		NI_ASSERT_T( ( pImage != 0 ) &&
								 ( pEdgedImage != 0 ),
								 NStr::Format( "Wrong parameter: %x, %x\n", pImage, pEdgedImage ) );
		NI_ASSERT_T( ( pImage->GetSizeX() == pEdgedImage->GetSizeX() ) &&
								 ( pImage->GetSizeY() == pEdgedImage->GetSizeY() ),
								 NStr::Format( "Invalid size: source:(%d, %d), dest:(%d, %d)\n", pImage->GetSizeX(), pImage->GetSizeY(), pEdgedImage->GetSizeX(), pEdgedImage->GetSizeY() ) );
		imageAccessor = pImage;
		edgedImageAccessor = pEdgedImage;
	}

	bool operator()( int nXIndex, int nYIndex )
	{ 
		if ( imageAccessor[nYIndex][nXIndex].a < dwMinAlpha )
		{
			int nValidPixelsCount = 0;
			for ( int nLocalYIndex = nYIndex - 1; nLocalYIndex <= nYIndex + 1; ++nLocalYIndex )
			{
				for ( int nLocalXIndex = nXIndex - 1; nLocalXIndex <= nXIndex + 1; ++nLocalXIndex )
				{
					if ( ( nLocalXIndex >= 0 ) && 
							 ( nLocalYIndex >= 0 ) &&
							 ( nLocalXIndex < pImage->GetSizeX() ) &&
							 ( nLocalYIndex < pImage->GetSizeY() )  )
					{
						nValidPixelsCount += ( imageAccessor[nLocalYIndex][nLocalXIndex] >= dwMinAlpha ) ? 1 : 0;
					}
				}
			}
			if ( nValidPixelsCount > 0 )
			{
				edgedImageAccessor[nYIndex][nXIndex] = edgedColor;
			}
		}
		return true;
	}
};
#endif // __ImageBuilder__Types__
