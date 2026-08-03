#include "StdAfx.h"

#include "IB_Types.h"
#include "LA_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void CSpritesPackBuilder::GetActualRect( SSpritesPack::CSpritesList::const_iterator spritesListIterator, CTRect<int> *pActualRect )
{
	CTRect<int> bounds( INT_MAX, INT_MAX, INT_MIN, INT_MIN );
	for ( SSpritesPack::SSprite::CSquaresList::const_iterator squareIterator = spritesListIterator->squares.begin(); squareIterator != spritesListIterator->squares.end(); ++squareIterator )
	{
		CTRect<int> actualSquareRect( static_cast<int>( squareIterator->vLeftTop.x ),
																	static_cast<int>( squareIterator->vLeftTop.y ),
																	static_cast<int>( squareIterator->vLeftTop.x + squareIterator->fSize ),
																	static_cast<int>( squareIterator->vLeftTop.y + squareIterator->fSize ) );
		
		if( actualSquareRect.minx < bounds.minx )
		{
			bounds.minx = actualSquareRect.minx;
		}
		if( actualSquareRect.miny < bounds.miny )
		{
			bounds.miny = actualSquareRect.miny;
		}
		if( actualSquareRect.maxx > bounds.maxx )
		{
			bounds.maxx = actualSquareRect.maxx;
		}
		if( actualSquareRect.maxy > bounds.maxy )
		{
			bounds.maxy = actualSquareRect.maxy;
		}
	}
	if ( pActualRect )
	{
		( *pActualRect ) = bounds;
	}
}

bool CSpritesPackBuilder::GetMinimalImageSize( const SSpritesPack *pSpritesPack, CTPoint<int> *pMinimalImageSize, CTPoint<int> *pCollectedSquareSideSize, int *pnSquaresCount )
{
	int nPackedSquareSize = 0;
	( *pnSquaresCount ) = 0;
	
	pCollectedSquareSideSize->min = INT_MAX;
	pCollectedSquareSideSize->max = 0;

	for ( SSpritesPack::CSpritesList::const_iterator spritesListIterator = pSpritesPack->sprites.begin(); spritesListIterator != pSpritesPack->sprites.end(); ++spritesListIterator )
	{
		for ( SSpritesPack::SSprite::CSquaresList::const_iterator squareIterator = spritesListIterator->squares.begin(); squareIterator != spritesListIterator->squares.end(); ++squareIterator )
		{
			if ( pCollectedSquareSideSize->max < static_cast<int>( squareIterator->fSize ) )
			{
				pCollectedSquareSideSize->max = static_cast<int>( squareIterator->fSize );
			}
			if ( pCollectedSquareSideSize->min > static_cast<int>( squareIterator->fSize ) )
			{
				pCollectedSquareSideSize->min = static_cast<int>( squareIterator->fSize );
			}
			++( *pnSquaresCount );
			nPackedSquareSize += static_cast<int>( squareIterator->fSize * squareIterator->fSize );
		}
	}

	/**
	NI_ASSERT_T( ( pCollectedSquareSideSize->min > 0 ) && ( pCollectedSquareSideSize->max > 0 ),
							 NStr::Format( "Invalid collected sizes: %d %d\n", pCollectedSquareSideSize->min, pCollectedSquareSideSize->max ) );
	/**/
	
	if ( ( ( *pnSquaresCount ) == 0 ) )
	{
		pCollectedSquareSideSize->min = 0;
		pCollectedSquareSideSize->max = 0;
		pMinimalImageSize->x = 1;
		pMinimalImageSize->y = 1;
		return false;
	}

	pMinimalImageSize->x = 1;
	pMinimalImageSize->y = 1;

	while ( ( pMinimalImageSize->x * pMinimalImageSize->y ) < nPackedSquareSize )
	{
		pMinimalImageSize->x *= 2;
		if ( ( pMinimalImageSize->x * pMinimalImageSize->y ) < nPackedSquareSize )
		{
			pMinimalImageSize->y *= 2;
		}
	}

	return true;
}

bool CSpritesPackBuilder::CollectSquares( int nCurrentDepth, const CTPoint<int> &rLeftTop, int nSquareSideSize, SCollectSquaresParameter *pParameter )
{
	int nSquaresCount = 0;
	for ( int nSquareYIndex = 0; nSquareYIndex < 2;	++nSquareYIndex )
	{
		for ( int nSquareXIndex = 0; nSquareXIndex < 2; ++nSquareXIndex )
		{
			bool bResult = false;
			if ( nCurrentDepth < pParameter->nMaxDepth )
			{
				bResult = CollectSquares( nCurrentDepth + 1,
																	CTPoint<int>( rLeftTop.x + nSquareXIndex * ( nSquareSideSize / 2 ),
																								rLeftTop.y + nSquareYIndex * ( nSquareSideSize / 2 ) ),
																	nSquareSideSize / 2,
																	pParameter );
			}
			else
			{
				CTRect<int> indices( rLeftTop.x - pParameter->shift.x + nSquareXIndex * ( nSquareSideSize / 2 ),
														 rLeftTop.y - pParameter->shift.y + nSquareYIndex * ( nSquareSideSize / 2 ),
														 rLeftTop.x - pParameter->shift.x + ( nSquareXIndex + 1 ) * ( nSquareSideSize / 2 ),
														 rLeftTop.y - pParameter->shift.y + ( nSquareYIndex + 1) * ( nSquareSideSize / 2 ) );

				if ( ValidateIndices( CTRect<int>( 0, 0, pParameter->pImage->GetSizeX(), pParameter->pImage->GetSizeY() ), &indices ) >= 0 )
				{
					CUnsafeImageAccessor imageAccessor = pParameter->pImage;
					for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
					{
						for ( int nXIndex = indices.minx; nXIndex < indices.maxx; ++nXIndex )
						{
							if ( imageAccessor[nYIndex][nXIndex].a >= pParameter->dwMinAlpha )
							{
								bResult = true;
								break;
							}
						}
						if ( bResult )
						{
							break;
						}
					}
				}
			}
			if ( bResult )
			{
				int nSize = nSquareSideSize / 2;
				CTPoint<int> leftTop( rLeftTop.x + nSquareXIndex * nSize, rLeftTop.y + nSquareYIndex * nSize );
				leftTop -= ( pParameter->pSprite->center + pParameter->shift );

				SSpritesPack::SSprite::SSquare square;
				square.vLeftTop.x = leftTop.x;
				square.vLeftTop.y = leftTop.y;
				square.fSize = nSize;
				square.fDepthLeft = 0;
				square.fDepthRight = 0;

				pParameter->pSprite->squares.push_back( square );
				nSquaresCount++;
			}
		}
	}
	if ( nSquaresCount == 4 )
	{
		for ( int index = 0; index < nSquaresCount; ++index )
		{
			pParameter->pSprite->squares.pop_back();
		}
		return true;
	}
	
	return false;
}							

bool CSpritesPackBuilder::CollectSquares( SSpritesPack *pSpritesPack, const CPackParameters &rPackParameters, int nMaxSquareSideSize, int nMaxDepth, int nMinDepth )
{
	pSpritesPack->sprites.clear();

	SCollectSquaresParameter collectSquaresParameter;
	collectSquaresParameter.nMaxDepth = nMaxDepth;
	for ( CPackParameters::const_iterator packParameterIterator = rPackParameters.begin(); packParameterIterator != rPackParameters.end(); ++packParameterIterator )
	{
		pSpritesPack->sprites.push_back( SSpritesPack::SSprite() );
		pSpritesPack->sprites.back().center = packParameterIterator->center;

		collectSquaresParameter.dwMinAlpha = packParameterIterator->dwMinAlpha;
		collectSquaresParameter.pImage = packParameterIterator->pImage;
		collectSquaresParameter.pSprite = &( pSpritesPack->sprites.back() );

		CTPoint<int> shiftedImageSize( 0, 0 );
		CTPoint<int> mainSquaresCount( 0, 0 );

		if ( ( pSpritesPack->sprites.back().center.x % nMaxSquareSideSize ) != 0 )
		{
			collectSquaresParameter.shift.x = nMaxSquareSideSize - ( pSpritesPack->sprites.back().center.x % nMaxSquareSideSize );
		}
		else
		{
			collectSquaresParameter.shift.x = 0;
		}
		if ( ( pSpritesPack->sprites.back().center.y % nMaxSquareSideSize ) != 0 )
		{
			collectSquaresParameter.shift.y = nMaxSquareSideSize - ( pSpritesPack->sprites.back().center.y % nMaxSquareSideSize );
		}
		else
		{
			collectSquaresParameter.shift.y = 0;
		}

		shiftedImageSize.x = packParameterIterator->pImage->GetSizeX() + collectSquaresParameter.shift.x;
		shiftedImageSize.y = packParameterIterator->pImage->GetSizeY() + collectSquaresParameter.shift.y;

		if ( ( shiftedImageSize.x % nMaxSquareSideSize ) != 0 )
		{
			shiftedImageSize.x += nMaxSquareSideSize - ( shiftedImageSize.x % nMaxSquareSideSize );
		}
		if ( ( shiftedImageSize.y % nMaxSquareSideSize ) != 0 )
		{
			shiftedImageSize.y += nMaxSquareSideSize - ( shiftedImageSize.y % nMaxSquareSideSize );
		}
		
		mainSquaresCount.x = ( shiftedImageSize.x / nMaxSquareSideSize );
		mainSquaresCount.y = ( shiftedImageSize.y / nMaxSquareSideSize );

		for ( int nMainYIndex = 0; nMainYIndex < mainSquaresCount.y; ++nMainYIndex )
		{
			for ( int nMainXIndex = 0; nMainXIndex < mainSquaresCount.x ; ++nMainXIndex )
			{
				if ( CollectSquares( nMinDepth,
														 CTPoint<int>( nMainXIndex * nMaxSquareSideSize,
																					 nMainYIndex * nMaxSquareSideSize ),
														 nMaxSquareSideSize,
														 &collectSquaresParameter ) )
				{
					CTPoint<int> leftTop( nMainXIndex * nMaxSquareSideSize, nMainYIndex * nMaxSquareSideSize );
					leftTop -= ( pSpritesPack->sprites.back().center + collectSquaresParameter.shift );

					SSpritesPack::SSprite::SSquare square;
					square.vLeftTop.x = leftTop.x;
					square.vLeftTop.y = leftTop.y;
					square.fSize = nMaxSquareSideSize;
					square.fDepthLeft = 0;
					square.fDepthRight = 0;

					pSpritesPack->sprites.back().squares.push_back( square );
				}
			}
		}
	}
	return true;
}

void CSpritesPackBuilder::MarkLockedTile( IImage *pImage, const CTPoint<int> &rPoint )
{
	CUnsafeImageAccessor imageAccessor = pImage;
	
	for ( int nYIndex = 0; nYIndex < ( nCellSizeY / 2 ); ++nYIndex )
	{
		for ( int nXIndex = ( nCellSizeX / 2 ) - ( nYIndex + 1 ) * 2; nXIndex < ( nCellSizeX / 2 ) + ( nYIndex + 1 ) * 2; ++nXIndex )
		{
			int nXPos = nXIndex + rPoint.x;
			int nYPos = nYIndex + rPoint.y;
			if ( ( nXPos >= 0 ) &&
					 ( nYPos >= 0 ) &&
					 ( nXPos < pImage->GetSizeX() ) &&
					 ( nYPos < pImage->GetSizeY() ) )
			{
				SColor &rColor = imageAccessor[nYPos][nXPos];
				rColor.a = 255;
				rColor.r = 255;
				rColor.g = 255;
				rColor.b = 255;
			}
		}
	}
	for ( int nYIndex = 0; nYIndex < ( nCellSizeY / 2 ); ++nYIndex )
	{
		for ( int nXIndex = nYIndex * 2; nXIndex < nCellSizeX - ( nYIndex * 2 ); ++nXIndex )
		{
			int nXPos = nXIndex + rPoint.x;
			int nYPos = ( nCellSizeY / 2 ) + nYIndex + rPoint.y;
			if ( ( nXPos >= 0 ) &&
					 ( nYPos >= 0 ) &&
					 ( nXPos < pImage->GetSizeX() ) &&
					 ( nYPos < pImage->GetSizeY() ) )
			{
				SColor &rColor = imageAccessor[nYPos][nXPos];
				rColor.a = 255;
				rColor.r = 255;
				rColor.g = 255;
				rColor.b = 255;
			}
		}
	}
}

IImage* CSpritesPackBuilder::CreateLockArrayImage( const CArray2D<BYTE> &rLockedTiles, const CTPoint<int> &rLockedTilesCenter, const CTRect<int> rActualRect )
{
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get IImageProcessor: %x\n", pImageProcessor ),
							  return 0 );

	IImage *pLockedArrayImage = pImageProcessor->CreateImage( rActualRect.Width(), rActualRect.Height() );
	if ( pLockedArrayImage != 0 )
	{
		pLockedArrayImage->Set( 0 );	

		for ( int nLockedArrayYIndex = 0; nLockedArrayYIndex < rLockedTiles.GetSizeY(); ++nLockedArrayYIndex )
		{
			for ( int nLockedArrayXIndex = 0; nLockedArrayXIndex < rLockedTiles.GetSizeX(); ++nLockedArrayXIndex )
			{
				if ( rLockedTiles[nLockedArrayYIndex][nLockedArrayXIndex] != RMGC_UNLOCKED )
				{
					int nXPos = rLockedTilesCenter.x + ( nLockedArrayXIndex + nLockedArrayYIndex ) * ( nCellSizeX / 2 ) - rActualRect.minx;
					int nYPos = rLockedTilesCenter.y + ( nLockedArrayXIndex - nLockedArrayYIndex - 1 ) * ( nCellSizeY / 2 ) - rActualRect.miny;
					MarkLockedTile( pLockedArrayImage, CTPoint<int>( nXPos, nYPos ) );
				}
			}
		}
	}

	for ( int nLockedArrayXIndex = 1; nLockedArrayXIndex < ( rLockedTiles.GetSizeX() - 1 ); ++nLockedArrayXIndex )
	{
		int nLockedArrayYIndex = 0;
		if ( rLockedTiles[nLockedArrayYIndex][nLockedArrayXIndex] != RMGC_LOCKED )
		{
		
			bool bForwardExists = false;
			for ( int nLockedArrayForwardXIndex = ( nLockedArrayXIndex + 1 );
						nLockedArrayForwardXIndex < rLockedTiles.GetSizeX();
						++nLockedArrayForwardXIndex )
			{
				if ( rLockedTiles[nLockedArrayYIndex][nLockedArrayForwardXIndex] != RMGC_UNLOCKED )
				{
					bForwardExists = true;
					break;
				}			
			}
			
			bool bBackwardExists = false;
			for ( int nLockedArrayBackwardXIndex = ( nLockedArrayXIndex - 1 );
						nLockedArrayBackwardXIndex >= 0;
						--nLockedArrayBackwardXIndex )
			{
				if ( rLockedTiles[nLockedArrayYIndex][nLockedArrayBackwardXIndex] != RMGC_UNLOCKED )
				{
					bBackwardExists = true;
					break;
				}			
			}

			if ( bForwardExists && bBackwardExists )
			{
				int nXPos = rLockedTilesCenter.x + ( nLockedArrayXIndex + nLockedArrayYIndex ) * ( nCellSizeX / 2 ) - rActualRect.minx;
				int nYPos = rLockedTilesCenter.y + ( nLockedArrayXIndex - nLockedArrayYIndex - 1 ) * ( nCellSizeY / 2 ) - rActualRect.miny;
				MarkLockedTile( pLockedArrayImage, CTPoint<int>( nXPos, nYPos ) );
			}
		}
	}

	for ( int nLockedArrayYIndex = 1; nLockedArrayYIndex < ( rLockedTiles.GetSizeY() - 1 ); ++nLockedArrayYIndex )
	{
		int nLockedArrayXIndex = ( rLockedTiles.GetSizeX() - 1 );
		if ( rLockedTiles[nLockedArrayYIndex][nLockedArrayXIndex] != RMGC_LOCKED )
		{
			
			bool bForwardExists = false;
			for ( int nLockedArrayForwardYIndex = ( nLockedArrayYIndex + 1 );
						nLockedArrayForwardYIndex < rLockedTiles.GetSizeY();
						++nLockedArrayForwardYIndex )
			{
				if ( rLockedTiles[nLockedArrayForwardYIndex][nLockedArrayXIndex] != RMGC_UNLOCKED )
				{
					bForwardExists = true;
					break;
				}			
			}
			
			bool bBackwardExists = false;
			for ( int nLockedArrayBackwardYIndex = ( nLockedArrayYIndex - 1 );
						nLockedArrayBackwardYIndex >= 0;
						--nLockedArrayBackwardYIndex )
			{
				if ( rLockedTiles[nLockedArrayBackwardYIndex][nLockedArrayXIndex] != RMGC_UNLOCKED )
				{
					bBackwardExists = true;
					break;
				}			
			}

			if ( bForwardExists && bBackwardExists )
			{
				int nXPos = rLockedTilesCenter.x + ( nLockedArrayXIndex + nLockedArrayYIndex ) * ( nCellSizeX / 2 ) - rActualRect.minx;
				int nYPos = rLockedTilesCenter.y + ( nLockedArrayXIndex - nLockedArrayYIndex - 1 ) * ( nCellSizeY / 2 ) - rActualRect.miny;
				MarkLockedTile( pLockedArrayImage, CTPoint<int>( nXPos, nYPos ) );
			}
		}
	}

	CUnsafeImageAccessor lockedArrayImageAccessor = pLockedArrayImage;
	for ( int nXIndex = 0; nXIndex < lockedArrayImageAccessor->GetSizeX(); ++nXIndex )
	{
		bool bFilled = false;
		for ( int nYIndex = ( lockedArrayImageAccessor->GetSizeY() - 1 ); nYIndex >= 0; --nYIndex )
		{
			if ( lockedArrayImageAccessor[nYIndex][nXIndex].a > 0 )
			{
				if ( nXIndex > 0 )
				{
					for ( int nInnerXIndex = 0; nInnerXIndex <= nXIndex; ++nInnerXIndex )
					{
						SColor &rColor = lockedArrayImageAccessor[nYIndex][nInnerXIndex];
						rColor.a = 255;
						rColor.r = 255;
						rColor.g = 255;
						rColor.b = 255;
					}
				}
				bFilled = true;
				break;
			}
		}
		if ( bFilled )
		{
			break;
		}
	}
	
	for ( int nXIndex = ( lockedArrayImageAccessor->GetSizeX() - 1 ); nXIndex >= 0; --nXIndex )
	{
		bool bFilled = false;
		for ( int nYIndex = ( lockedArrayImageAccessor->GetSizeY() - 1 ); nYIndex >= 0; --nYIndex )
		{
			if ( lockedArrayImageAccessor[nYIndex][nXIndex].a > 0 )
			{
				if ( nXIndex < ( lockedArrayImageAccessor->GetSizeX() - 1 ) )
				{
					for ( int nInnerXIndex = nXIndex; nInnerXIndex < lockedArrayImageAccessor->GetSizeX(); ++nInnerXIndex )
					{
						SColor &rColor = lockedArrayImageAccessor[nYIndex][nInnerXIndex];
						rColor.a = 255;
						rColor.r = 255;
						rColor.g = 255;
						rColor.b = 255;
					}
				}
				bFilled = true;
				break;
			}
		}
		if ( bFilled )
		{
			break;
		}
	}

	return pLockedArrayImage;
}

IImage* CSpritesPackBuilder::Pack( SSpritesPack *pSpritesPack, const CPackParameters &rPackParameters, int nMaxSquareSideSize, int nDepth )
{
	NI_ASSERT_TF( pSpritesPack != 0,
							  NStr::Format( "Wrong parameter: %x\n", pSpritesPack ),
							  return 0 );
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get IImageProcessor: %x\n", pImageProcessor ),
							  return 0 );

	pSpritesPack->sprites.clear();
	CTPoint<int> collectedSquareSideSize( 0, 0 );
	CTPoint<int> packedImageSize( 0, 0 );
	int nSquaresCount = 0;
	
	{
		SSpritesPack spritesPackMaxDepth;
		CTPoint<int> collectedSquareSideSizeMaxDepth( 0, 0 );
		CTPoint<int> packedImageSizeMaxDepth( 0, 0 );
		int nSquaresCountMaxDepth = 0;
		bool bSquaresNotFilled = true;

		if ( !CollectSquares( &spritesPackMaxDepth, rPackParameters, nMaxSquareSideSize, nDepth, 1 ) )
		{
			return 0;
		}
		GetMinimalImageSize( &spritesPackMaxDepth, &packedImageSizeMaxDepth, &collectedSquareSideSizeMaxDepth, &nSquaresCountMaxDepth );

		if ( nDepth > 1 )
		{
			SSpritesPack spritesPackSubDepth;
			CTPoint<int> collectedSquareSideSizeSubDepth( 0, 0 );
			CTPoint<int> packedImageSizeSubDepth( 0, 0 );
			int nSquaresCountSubDepth = 0;

			if ( !CollectSquares( &spritesPackSubDepth, rPackParameters, nMaxSquareSideSize, nDepth - 1, 1 ) )
			{
				return 0;
			}
			GetMinimalImageSize( &spritesPackSubDepth, &packedImageSizeSubDepth, &collectedSquareSideSizeSubDepth, &nSquaresCountSubDepth );

			if ( ( packedImageSizeSubDepth.x * packedImageSizeSubDepth.y ) <= ( packedImageSizeMaxDepth.x * packedImageSizeMaxDepth.y ) )
			{
				pSpritesPack->sprites = spritesPackSubDepth.sprites;
				collectedSquareSideSize = collectedSquareSideSizeSubDepth;
				packedImageSize = packedImageSizeSubDepth;
				nSquaresCount = nSquaresCountSubDepth;
				
				bSquaresNotFilled = false;
			}
		}

		if ( bSquaresNotFilled )
		{
			pSpritesPack->sprites = spritesPackMaxDepth.sprites;
			collectedSquareSideSize = collectedSquareSideSizeMaxDepth;
			packedImageSize = packedImageSizeMaxDepth;
			nSquaresCount = nSquaresCountMaxDepth;
		}
	}

	if ( nSquaresCount == 0 )
	{
		return 0;
	}

	CArray2D<BYTE> lockedSquares( packedImageSize.x / collectedSquareSideSize.min,
																packedImageSize.y / collectedSquareSideSize.min );
	
	lockedSquares.SetZero();

	int nGranularity = static_cast<int>( collectedSquareSideSize.max ) /
										 static_cast<int>( collectedSquareSideSize.min );						//сколько локать минимальных квадратов при записи квадрата
	int nFilledSquaresCount = 0;																									//сколько всего записано квадратов
	float fFilledSquareSideSize = collectedSquareSideSize.max;										//текущий размер записываемого квадрата
	CTPoint<int> filledLockedSquare( 0, 0 );																			//текущая координата записываемого квадрата в массиве залоканных квадратов
	while ( nFilledSquaresCount < nSquaresCount )
	{
		NI_ASSERT_T( ( nGranularity > 0 ),
								 NStr::Format( "Invalid prediction: (%d) in [%d]", nGranularity, static_cast<int>( collectedSquareSideSize.max ) / static_cast<int>( collectedSquareSideSize.min ) ) );
		
		for ( SSpritesPack::CSpritesList::iterator spritesListIterator = pSpritesPack->sprites.begin(); spritesListIterator != pSpritesPack->sprites.end(); ++spritesListIterator )
		{
			for ( SSpritesPack::SSprite::CSquaresList::iterator squareIterator = spritesListIterator->squares.begin(); squareIterator != spritesListIterator->squares.end(); ++squareIterator )
			{
				if ( squareIterator->fSize == fFilledSquareSideSize )
				{
					while( true )
					{
						while ( ( filledLockedSquare.x < lockedSquares.GetSizeX() ) && 
										( lockedSquares[filledLockedSquare.y][filledLockedSquare.x] == RMGC_LOCKED ) )
						{
							filledLockedSquare.x += nGranularity;
						}
						if ( ( filledLockedSquare.x < lockedSquares.GetSizeX() ) )
						{
							break;
						}
						
						filledLockedSquare.y += nGranularity;
						filledLockedSquare.x = 0;

						NI_ASSERT_T( ( filledLockedSquare.y < lockedSquares.GetSizeY() ),
												 NStr::Format( "Invalid prediction: (%d, %d) in [%d, %d]", filledLockedSquare.x, filledLockedSquare.y, lockedSquares.GetSizeX(), lockedSquares.GetSizeY() ) );
					}
					
					int nXPosition = filledLockedSquare.x * static_cast<int>( collectedSquareSideSize.min );
					int nYPosition = filledLockedSquare.y * static_cast<int>( collectedSquareSideSize.min );

					squareIterator->rcMaps.minx = ( 0.5f + nXPosition ) / ( packedImageSize.x * 1.0f );
					squareIterator->rcMaps.miny = ( 0.5f + nYPosition ) / ( packedImageSize.y * 1.0f );
					squareIterator->rcMaps.maxx = ( 0.5f + nXPosition + squareIterator->fSize ) / ( packedImageSize.x * 1.0f );
					squareIterator->rcMaps.maxy = ( 0.5f + nYPosition + squareIterator->fSize ) / ( packedImageSize.y * 1.0f );
					
					for ( int nYIndex = 0; nYIndex < nGranularity; ++nYIndex )
					{
						for ( int nXIndex = 0; nXIndex < nGranularity; ++nXIndex )
						{
							lockedSquares[filledLockedSquare.y + nYIndex][filledLockedSquare.x + nXIndex] = RMGC_LOCKED;
						}
					}
					++nFilledSquaresCount;
				}
			}
		}
		fFilledSquareSideSize = static_cast<int>( fFilledSquareSideSize ) / 2;
		nGranularity /= 2;
	}

	IImage *pPackedImage = pImageProcessor->CreateImage( packedImageSize.x, packedImageSize.y );
	if ( pPackedImage == 0 )
	{
		return 0;
	}
	pPackedImage->Set( 0 );
	
	CPackParameters::const_iterator packParameterIterator = rPackParameters.begin();
	for ( SSpritesPack::CSpritesList::iterator spritesListIterator = pSpritesPack->sprites.begin(); spritesListIterator != pSpritesPack->sprites.end(); ++spritesListIterator )
	{
		if ( packParameterIterator != rPackParameters.end() )
		{
			CTRect<int> imageActualSquareRect( 0, 0, 0, 0 );
			GetActualRect( spritesListIterator, &imageActualSquareRect );
			CPtr<IImage> pLockArrayImage = CreateLockArrayImage( packParameterIterator->lockedTiles, packParameterIterator->lockedTilesCenter, imageActualSquareRect );
			CUnsafeImageAccessor imageAccessor = packParameterIterator->pImage;
			for ( SSpritesPack::SSprite::CSquaresList::iterator squareIterator = spritesListIterator->squares.begin(); squareIterator != spritesListIterator->squares.end(); ++squareIterator )
			{
				CTRect<int> actualSquareRect( static_cast<int>( squareIterator->vLeftTop.x ),
																			static_cast<int>( squareIterator->vLeftTop.y ),
																			static_cast<int>( squareIterator->vLeftTop.x + squareIterator->fSize ),
																			static_cast<int>( squareIterator->vLeftTop.y + squareIterator->fSize ) );

				if ( pLockArrayImage )
				{
					CUnsafeImageAccessor lockArrayImageAccessor = pLockArrayImage;
					bool bLockArrayLeftFilled = false;
					bool bLockArrayRightFilled = false;
					int nLockArrayLeftIndex = 0;
					int nLockArrayRightIndex = 0;
					int nLockArrayMinXIndex = actualSquareRect.minx - imageActualSquareRect.minx;
					int nLockArrayMaxXIndex = ( actualSquareRect.maxx - 1 ) - imageActualSquareRect.minx;

					if ( ( nLockArrayMinXIndex >= 0 ) && ( nLockArrayMinXIndex < pLockArrayImage->GetSizeX() ) )
					{
						for ( int nYIndex = ( pLockArrayImage->GetSizeY() - 1); nYIndex >= 0; --nYIndex )
						{
							if (  lockArrayImageAccessor[nYIndex][nLockArrayMinXIndex].a > 0 )
							{
								nLockArrayLeftIndex = ( -1 ) * ( nYIndex + imageActualSquareRect.miny ); //перевернуто
								bLockArrayLeftFilled = true;
								break;
							}
						}
					}
					if ( ( nLockArrayMaxXIndex >= 0 ) && ( nLockArrayMaxXIndex < pLockArrayImage->GetSizeX() ) )
					{
						for ( int nYIndex = ( pLockArrayImage->GetSizeY() - 1); nYIndex >= 0; --nYIndex )
						{
							if (  lockArrayImageAccessor[nYIndex][nLockArrayMaxXIndex].a > 0 )
							{
								nLockArrayRightIndex = ( -1 ) * ( nYIndex + imageActualSquareRect.miny ); //перевернуто
								bLockArrayRightFilled = true;
								break;
							}
						}
					}

					if ( bLockArrayLeftFilled && !bLockArrayRightFilled )
					{
						nLockArrayRightIndex = nLockArrayLeftIndex;
						bLockArrayRightFilled = true;
					}

					if ( bLockArrayRightFilled && !bLockArrayLeftFilled )
					{
						nLockArrayLeftIndex = nLockArrayRightIndex;
						bLockArrayLeftFilled = true;
					}

					bool bLeftFilled = false;
					bool bRightFilled = false;
					int nLeftIndex = 0;
					int nRightIndex = 0;
					int nMinXIndex = actualSquareRect.minx + spritesListIterator->center.x;
					int nMaxXIndex = ( actualSquareRect.maxx - 1 ) + spritesListIterator->center.x;

					if ( ( nMinXIndex >= 0 ) && ( nMinXIndex < packParameterIterator->pImage->GetSizeX() ) )
					{
						for ( int nYIndex = ( packParameterIterator->pImage->GetSizeY() - 1 ); nYIndex >= 0; --nYIndex )
						{
							if ( imageAccessor[nYIndex][nMinXIndex].a >= packParameterIterator->dwMinAlpha )
							{
								nLeftIndex = spritesListIterator->center.y - nYIndex; //перевернуто
								bLeftFilled = true;
								break;
							}
						}
					}
					if ( ( nMaxXIndex >= 0 ) && ( nMaxXIndex < packParameterIterator->pImage->GetSizeX() ) )
					{
						for ( int nYIndex = ( packParameterIterator->pImage->GetSizeY() - 1 ); nYIndex >= 0; --nYIndex )
						{
							if ( imageAccessor[nYIndex][nMaxXIndex].a >= packParameterIterator->dwMinAlpha )
							{
								nRightIndex = spritesListIterator->center.y - nYIndex; //перевернуто
								bRightFilled = true;
								break;
							}
						}
					}
					
					if ( bLeftFilled && !bRightFilled )
					{
						nRightIndex = nLeftIndex;
					}

					if ( bRightFilled && !bLeftFilled )
					{
						nLeftIndex = nRightIndex;
					}

					if ( bLockArrayLeftFilled &&
							 ( ( ( nLeftIndex - nLockArrayLeftIndex ) > ( nCellSizeY / 2 ) ) ||
								 true /*( nLeftIndex < nLockArrayLeftIndex )*/ ) )
					{
						nLeftIndex = nLockArrayLeftIndex;
					}

					if ( bLockArrayRightFilled &&
							 ( ( ( nRightIndex - nLockArrayRightIndex ) > ( nCellSizeY / 2 ) ) ||
								 true /*( nRightIndex < nLockArrayRightIndex )*/ ) )
					{
						nRightIndex = nLockArrayRightIndex;
					}

					squareIterator->fDepthLeft = nLeftIndex;
					squareIterator->fDepthRight = nRightIndex;
				}
					
				actualSquareRect.minx += spritesListIterator->center.x;
				actualSquareRect.miny += spritesListIterator->center.y;
				actualSquareRect.maxx += spritesListIterator->center.x;
				actualSquareRect.maxy += spritesListIterator->center.y;

				CTPoint<int> shift( actualSquareRect.minx, actualSquareRect.miny );
				if ( ValidateIndices( CTRect<int>( 0,
																					 0,
																					 packParameterIterator->pImage->GetSizeX(),
																					 packParameterIterator->pImage->GetSizeY() ), &actualSquareRect ) >= 0 )
				{
					shift.x -= actualSquareRect.minx;
					shift.y -= actualSquareRect.miny;

					pPackedImage->CopyFrom( packParameterIterator->pImage,
																	&( static_cast<RECT>( actualSquareRect ) ),
																	squareIterator->rcMaps.minx * packedImageSize.x - shift.x,
																	squareIterator->rcMaps.miny * packedImageSize.y - shift.y );

				}
			}

			spritesListIterator->edge.edges.Clear();

			spritesListIterator->edge.rcBoundBox.minx = 0.0f;
			spritesListIterator->edge.rcBoundBox.miny = 0.0f;
			spritesListIterator->edge.rcBoundBox.maxx = 0.0f;
			spritesListIterator->edge.rcBoundBox.maxy = 0.0f;
			
			spritesListIterator->edge.bHorizontal = true;

			if ( !spritesListIterator->squares.empty() )
			{
				CTPoint<int> imageSize( packParameterIterator->pImage->GetSizeX(), packParameterIterator->pImage->GetSizeY() );
				CTRect<int> bounds( imageSize.x, imageSize.y, -1, -1 );
				
				for ( int nYindex = 0; nYindex < imageSize.y; ++nYindex )
				{
					for ( int nXindex = 0; nXindex < imageSize.x; ++nXindex )
					{
						if ( imageAccessor[nYindex][nXindex].a >= packParameterIterator->dwMinAlpha )
						{
							if ( bounds.minx > nXindex )
							{
								bounds.minx = nXindex;
							}
							if ( bounds.maxx < nXindex )
							{
								bounds.maxx = nXindex;
							}
							if ( bounds.miny > nYindex )
							{
								bounds.miny = nYindex;
							}
							if ( bounds.maxy < nYindex )
							{
								bounds.maxy = nYindex;
							}
						}
					}
				}
				
				spritesListIterator->edge.rcBoundBox.minx = bounds.minx - spritesListIterator->center.x;
				spritesListIterator->edge.rcBoundBox.miny = bounds.miny - spritesListIterator->center.y;
				spritesListIterator->edge.rcBoundBox.maxx = bounds.maxx - spritesListIterator->center.x;
				spritesListIterator->edge.rcBoundBox.maxy = bounds.maxy - spritesListIterator->center.y;

				int nHorizontalElements = 0;
				{
					for ( int nYindex = bounds.miny; nYindex <= bounds.maxy; ++nYindex )
					{
						bool isOdd = false;
						for ( int nXindex = bounds.minx; nXindex <= bounds.maxx; ++nXindex )
						{
							if ( ( imageAccessor[nYindex][nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd )
							{
								if ( ( nXindex != bounds.maxx ) &&
										 ( ( imageAccessor[nYindex][nXindex + 1].a < packParameterIterator->dwMinAlpha ) == isOdd ) )
								{
									++nHorizontalElements;
									isOdd = !isOdd;
								}
							}
						}
						if ( isOdd )
						{
							++nHorizontalElements;
						}
					}
				}
				
				int nVerticalElements = 0;
				{
					for ( int nXindex = bounds.minx; nXindex <= bounds.maxx; ++nXindex )
					{
						bool isOdd = false;
						std::vector< short > edge;
						for ( int nYindex = bounds.miny; nYindex <= bounds.maxy; ++nYindex )
						{
							if ( ( imageAccessor[nYindex][nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd )
							{
								if ( ( nYindex != bounds.maxy ) &&
										 ( ( imageAccessor[nYindex + 1][nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd ) )
								{
									++nVerticalElements;
									isOdd = !isOdd;
								}
							}
						}
						if ( isOdd )
						{ 
							++nVerticalElements;
						}
					}
				}

				if ( ( nHorizontalElements > 0 ) || ( nVerticalElements > 0 ) )
				{
				
					spritesListIterator->edge.bHorizontal = ( ( ( nHorizontalElements * 2 ) + ( bounds.Height()  * 3 ) ) <= 
																										( ( nVerticalElements * 2 ) + ( bounds.Width() * 3 ) ) );
					if ( nHorizontalElements == 0 )
					{
						spritesListIterator->edge.bHorizontal = false;
					}
					else if ( nVerticalElements == 0 )
					{
						spritesListIterator->edge.bHorizontal = true;
					}
					
					if ( spritesListIterator->edge.bHorizontal )
					{
						spritesListIterator->edge.edges.SetSizes( nHorizontalElements, bounds.Height() + 1 );
						for ( int nYindex = bounds.miny; nYindex <= bounds.maxy; ++nYindex )
						{
							int nElements = 0;
							bool isOdd = false;
							for ( int nXindex = bounds.minx; nXindex <= bounds.maxx; ++nXindex )
							{
								if ( ( imageAccessor[nYindex][nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd )
								{
									if ( ( nXindex != bounds.maxx ) &&
											 ( ( imageAccessor[nYindex][nXindex + 1].a < packParameterIterator->dwMinAlpha ) == isOdd ) )
									{
										++nElements;
										isOdd = !isOdd;
									}
								}
							}
							if ( isOdd )
							{
								++nElements;      
							}
							spritesListIterator->edge.edges.SetLineLength( nYindex - bounds.miny, nElements );
							
							nElements = 0;
							isOdd = false;
							for ( int nXindex = bounds.minx; nXindex <= bounds.maxx; ++nXindex )
							{
								if ( ( imageAccessor[nYindex][nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd )
								{
									if ( ( nXindex != bounds.maxx  ) &&
											 ( ( imageAccessor[nYindex][nXindex + 1].a < packParameterIterator->dwMinAlpha ) == isOdd ) )
									{
										spritesListIterator->edge.edges[nYindex - bounds.miny][nElements] = short( ( nXindex - bounds.minx ) - ( isOdd ? 1 : 0 ) );
										++nElements;
										isOdd = !isOdd;
									}
								}
							}
							if ( isOdd )
							{
								spritesListIterator->edge.edges[nYindex - bounds.miny][nElements] = short( bounds.maxx - bounds.minx );
							}
						}
					}
					else
					{
						spritesListIterator->edge.edges.SetSizes( nVerticalElements, bounds.Width() + 1 );
						for ( int nXindex = bounds.minx; nXindex <= bounds.maxx; ++nXindex )
						{
							int nElements = 0;
							bool isOdd = false;
							for ( int nYindex = bounds.miny; nYindex <= bounds.maxy; ++nYindex )
							{
								if ( ( imageAccessor[nYindex][ nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd )
								{
									if ( ( nYindex != bounds.maxy ) &&
											 ( ( imageAccessor[nYindex + 1][nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd ) )
									{
										++nElements;
										isOdd = !isOdd;
									}
								}
							}
							if ( isOdd )
							{
								++nElements;
							}
							spritesListIterator->edge.edges.SetLineLength( nXindex - bounds.minx, nElements );
							
							nElements = 0;
							isOdd = false;
							for ( int nYindex = bounds.miny; nYindex <= bounds.maxy; ++nYindex )
							{
								if ( ( imageAccessor[nYindex][nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd )
								{
									if ( ( nYindex != bounds.maxy ) &&
											 ( ( imageAccessor[nYindex + 1][nXindex].a < packParameterIterator->dwMinAlpha ) == isOdd ) )
									{
										spritesListIterator->edge.edges[nXindex - bounds.minx][nElements] = short( nYindex - bounds.miny - ( isOdd ? 1 : 0 ) );
										++nElements;
										isOdd = !isOdd;
									}
								}
							}
							if ( isOdd )
							{
								spritesListIterator->edge.edges[nXindex - bounds.minx][nElements] = short( bounds.maxy - bounds.miny );
							}
						}
					}
				}
			}
			++packParameterIterator;
		}
	}

	return pPackedImage;
}

IImage* CSpritesPackBuilder::Pack( SSpritesPack *pSpritesPack, const SPackParameter &rPackParameter, int nMaxSquareSideSize, int nDepth )
{
	CPackParameters packParameters;
	packParameters.push_back( rPackParameter );
	return Pack( pSpritesPack, packParameters, nMaxSquareSideSize, nDepth );
}

IImage* CSpritesPackBuilder::Unpack( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect )
{
	NI_ASSERT_TF( ( pSpritesPack != 0 ) && ( pPackedImage != 0 ),
							  NStr::Format( "Wrong parameter: %x or %x\n", pSpritesPack, pPackedImage ),
							  return 0 );
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get IImageProcessor: %x\n", pImageProcessor ),
							  return 0 );

	if ( nSpriteIndex >= pSpritesPack->sprites.size() )
	{
		return 0;
	}

	SSpritesPack::CSpritesList::const_iterator spritesListIterator = pSpritesPack->sprites.begin();
	for ( int nSpritePackIndex = 0; nSpritePackIndex < nSpriteIndex; ++nSpritePackIndex )
	{
		++spritesListIterator;
	}
	
	CTRect<int> bounds( 0, 0, 0, 0 );
	GetActualRect( spritesListIterator, &bounds );

	IImage *pOriginalImage = pImageProcessor->CreateImage( bounds.Width(), bounds.Height() );
	if ( pOriginalImage )
	{
		pOriginalImage->Set( 0 );
		
		for ( SSpritesPack::SSprite::CSquaresList::const_iterator squareIterator = spritesListIterator->squares.begin(); squareIterator != spritesListIterator->squares.end(); ++squareIterator )
		{
			CTRect<int> actualSquareRect( static_cast<int>( squareIterator->vLeftTop.x ),
																		static_cast<int>( squareIterator->vLeftTop.y ),
																		static_cast<int>( squareIterator->vLeftTop.x + squareIterator->fSize ),
																		static_cast<int>( squareIterator->vLeftTop.y + squareIterator->fSize ) );

			CTRect<int> packedSquareRect( squareIterator->rcMaps.minx * pPackedImage->GetSizeX(),
																		squareIterator->rcMaps.miny * pPackedImage->GetSizeY(),
																		squareIterator->rcMaps.maxx * pPackedImage->GetSizeX(),
																		squareIterator->rcMaps.maxy * pPackedImage->GetSizeY() );
			
			actualSquareRect.minx -= bounds.minx;
			actualSquareRect.miny -= bounds.miny;

			pOriginalImage->CopyFrom( pPackedImage, &( static_cast<RECT>( packedSquareRect ) ), actualSquareRect.minx, actualSquareRect.miny );
		}
	}

	if (  pActualRect )
	{
		( *pActualRect ) = bounds;
	}
	return pOriginalImage;
}

IImage* CSpritesPackBuilder::UnpackAndMarkEdge( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect )
{
	CTRect<int> actualRect( 0, 0, 0, 0 );
	IImage *pUnpackedImage = Unpack( pSpritesPack, pPackedImage, nSpriteIndex, &actualRect );
	if ( pUnpackedImage == 0 )
	{
		return 0;
	}
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get IImageProcessor: %x\n", pImageProcessor ),
							  return 0 );

	CUnsafeImageAccessor imageAccessor = pUnpackedImage;

	SSpritesPack::CSpritesList::const_iterator spritesListIterator = pSpritesPack->sprites.begin();
	for ( int nSpritePackIndex = 0; nSpritePackIndex < nSpriteIndex; ++nSpritePackIndex )
	{
		++spritesListIterator;
	}

	if( spritesListIterator->edge.bHorizontal )
	{
		for ( int nYindex = 0; nYindex < spritesListIterator->edge.edges.GetLineCount(); ++nYindex )
		{
			for ( int nXindex = 0; nXindex < spritesListIterator->edge.edges.GetLineSize( nYindex ); ++nXindex )
			{
				SColor &rColor = imageAccessor[nYindex + static_cast<int>( spritesListIterator->edge.rcBoundBox.miny ) - actualRect.miny][static_cast<int>( spritesListIterator->edge.edges[nYindex][nXindex] ) + static_cast<int>( spritesListIterator->edge.rcBoundBox.minx ) - actualRect.minx];
				rColor.r = 255;
				rColor.g = 255;
				rColor.b = 255;
			}
		}
	}
	else
	{
		for ( int nXindex = 0; nXindex < spritesListIterator->edge.edges.GetLineCount(); ++nXindex )
		{
			for ( int nYindex = 0; nYindex < spritesListIterator->edge.edges.GetLineSize( nXindex ); ++nYindex )
			{
				SColor &rColor = imageAccessor[static_cast<int>( spritesListIterator->edge.edges[nXindex][nYindex] ) + static_cast<int>( spritesListIterator->edge.rcBoundBox.miny ) - actualRect.miny][nXindex + static_cast<int>( spritesListIterator->edge.rcBoundBox.minx ) - actualRect.minx];
				rColor.r = 255;
				rColor.g = 255;
				rColor.b = 255;
			}
		}
	}

	if (  pActualRect )
	{
		( *pActualRect ) = actualRect;
	}
	return pUnpackedImage;
}

IImage* CSpritesPackBuilder::UnpackAndMarkBounds( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect )
{
	CTRect<int> actualRect( 0, 0, 0, 0 );
	IImage *pUnpackedImage = Unpack( pSpritesPack, pPackedImage, nSpriteIndex, &actualRect );
	if ( pUnpackedImage == 0 )
	{
		return 0;
	}
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get IImageProcessor: %x\n", pImageProcessor ),
							  return 0 );

	CUnsafeImageAccessor imageAccessor = pUnpackedImage;

	SSpritesPack::CSpritesList::const_iterator spritesListIterator = pSpritesPack->sprites.begin();
	for ( int nSpritePackIndex = 0; nSpritePackIndex < nSpriteIndex; ++nSpritePackIndex )
	{
		++spritesListIterator;
	}

	for ( int nYindex = static_cast<int>( spritesListIterator->edge.rcBoundBox.miny );
				nYindex <= static_cast<int>( spritesListIterator->edge.rcBoundBox.maxy );
				++nYindex )
	{
		{
			SColor &rColor = imageAccessor[nYindex - actualRect.miny][static_cast<int>( spritesListIterator->edge.rcBoundBox.minx ) - actualRect.minx];
			rColor.r = 255;
			rColor.g = 255;
			rColor.b = 255;
		}
		{
			SColor &rColor = imageAccessor[nYindex - actualRect.miny][static_cast<int>( spritesListIterator->edge.rcBoundBox.maxx ) - actualRect.minx];
			rColor.r = 255;
			rColor.g = 255;
			rColor.b = 255;
		}
	}
	for ( int nXindex = static_cast<int>( spritesListIterator->edge.rcBoundBox.minx );
				nXindex <= static_cast<int>( spritesListIterator->edge.rcBoundBox.maxx );
				++nXindex )
	{
		{
			SColor &rColor = imageAccessor[static_cast<int>( spritesListIterator->edge.rcBoundBox.miny ) - actualRect.miny][nXindex - actualRect.minx];
			rColor.r = 255;
			rColor.g = 255;
			rColor.b = 255;
		}
		{
			SColor &rColor = imageAccessor[static_cast<int>( spritesListIterator->edge.rcBoundBox.maxy ) - actualRect.miny][nXindex - actualRect.minx];
			rColor.r = 255;
			rColor.g = 255;
			rColor.b = 255;
		}
	}

	if (  pActualRect )
	{
		( *pActualRect ) = actualRect;
	}
	return pUnpackedImage;
}

IImage* CSpritesPackBuilder::UnpackAndMarkInEdge( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect )
{
	CTRect<int> actualRect( 0, 0, 0, 0 );
	IImage *pUnpackedImage = Unpack( pSpritesPack, pPackedImage, nSpriteIndex, &actualRect );
	if ( pUnpackedImage == 0)
	{
		return 0;
	}
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get IImageProcessor: %x\n", pImageProcessor ),
							  return 0 );

	CUnsafeImageAccessor imageAccessor = pUnpackedImage;

	SSpritesPack::CSpritesList::const_iterator spritesListIterator = pSpritesPack->sprites.begin();
	for ( int nSpritePackIndex = 0; nSpritePackIndex < nSpriteIndex; ++nSpritePackIndex )
	{
		++spritesListIterator;
	}

	for ( int nYindex = actualRect.miny; nYindex < actualRect.maxy; ++nYindex )
	{
		for ( int nXindex = actualRect.minx; nXindex < actualRect.maxx; ++nXindex )
		{
			if ( spritesListIterator->IsInside( CVec2( nXindex, nYindex ) ) )
			{
				SColor &rColor = imageAccessor[nYindex - actualRect.miny][nXindex - actualRect.minx];
				rColor.r = 255;
				rColor.g = 255;
				rColor.b = 255;
			}
		}
	}

	if (  pActualRect )
	{
		( *pActualRect ) = actualRect;
	}
	return pUnpackedImage;
}

IImage* CSpritesPackBuilder::UnpackAndMarkDepth( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, CTRect<int> *pActualRect )
{
	CTRect<int> actualRect( 0, 0, 0, 0 );
	IImage *pUnpackedImage = Unpack( pSpritesPack, pPackedImage, nSpriteIndex, &actualRect );
	if ( pUnpackedImage == 0)
	{
		return 0;
	}
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get IImageProcessor: %x\n", pImageProcessor ),
							  return 0 );

	CUnsafeImageAccessor imageAccessor = pUnpackedImage;

	SSpritesPack::CSpritesList::const_iterator spritesListIterator = pSpritesPack->sprites.begin();
	for ( int nSpritePackIndex = 0; nSpritePackIndex < nSpriteIndex; ++nSpritePackIndex )
	{
		++spritesListIterator;
	}

	for ( SSpritesPack::SSprite::CSquaresList::const_iterator squareIterator = spritesListIterator->squares.begin(); squareIterator != spritesListIterator->squares.end(); ++squareIterator )
	{
		CTRect<int> actualSquareRect( static_cast<int>( squareIterator->vLeftTop.x ),
																	static_cast<int>( squareIterator->vLeftTop.y ),
																	static_cast<int>( squareIterator->vLeftTop.x + squareIterator->fSize ),
																	static_cast<int>( squareIterator->vLeftTop.y + squareIterator->fSize ) );
		
		{
			int nXPos = actualSquareRect.minx - actualRect.minx;
			int nYPos = ( squareIterator->fDepthLeft + actualRect.miny ) * ( -1 );
			if ( ( nXPos >= 0 ) &&
					 ( nYPos >= 0 ) &&
					 ( nXPos < pUnpackedImage->GetSizeX() ) &&
					 ( nYPos < pUnpackedImage->GetSizeY() ) )
			{
				SColor &rColor = imageAccessor[nYPos][nXPos];
				rColor.r = 255;
				rColor.g = 255;
				rColor.b = 255;
			}
		}
		{
			int nXPos = ( actualSquareRect.maxx - 1 ) - actualRect.minx;
			int nYPos = ( squareIterator->fDepthRight + actualRect.miny ) * ( -1 );
			if ( ( nXPos >= 0 ) &&
					 ( nYPos >= 0 ) &&
					 ( nXPos < pUnpackedImage->GetSizeX() ) &&
					 ( nYPos < pUnpackedImage->GetSizeY() ) )
			{
				SColor &rColor = imageAccessor[nYPos][nXPos];
				rColor.r = 255;
				rColor.g = 255;
				rColor.b = 255;
			}
		}
	}

	if (  pActualRect )
	{
		( *pActualRect ) = actualRect;
	}
	return pUnpackedImage;
}

IImage* CSpritesPackBuilder::UnpackAndMarkAlpha( SSpritesPack *pSpritesPack, IImage *pPackedImage, int nSpriteIndex, DWORD dwMinAlpha, DWORD dwMaxAlpha, CTRect<int> *pActualRect )
{
	CTRect<int> actualRect( 0, 0, 0, 0 );
	IImage *pUnpackedImage = Unpack( pSpritesPack, pPackedImage, nSpriteIndex, &actualRect );
	if ( pUnpackedImage == 0)
	{
		return 0;
	}
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( pImageProcessor != 0,
							  NStr::Format( "Can't get IImageProcessor: %x\n", pImageProcessor ),
							  return 0 );

	CUnsafeImageAccessor imageAccessor = pUnpackedImage;

	SSpritesPack::CSpritesList::const_iterator spritesListIterator = pSpritesPack->sprites.begin();
	for ( int nSpritePackIndex = 0; nSpritePackIndex < nSpriteIndex; ++nSpritePackIndex )
	{
		++spritesListIterator;
	}

	for ( int nYindex = actualRect.miny; nYindex < actualRect.maxy; ++nYindex )
	{
		for ( int nXindex = actualRect.minx; nXindex < actualRect.maxx; ++nXindex )
		{
			if ( ( imageAccessor[nYindex - actualRect.miny][nXindex - actualRect.minx].a >= dwMinAlpha ) &&
					 ( imageAccessor[nYindex - actualRect.miny][nXindex - actualRect.minx].a <= dwMaxAlpha ) )
			{
				SColor &rColor = imageAccessor[nYindex - actualRect.miny][nXindex - actualRect.minx];
				rColor.r = 255;
				rColor.g = 255;
				rColor.b = 255;
			}
		}
	}

	if ( pActualRect )
	{
		( *pActualRect ) = actualRect;
	}
	return pUnpackedImage;
}

bool CRMImageBuilder::ApplyFilter( IImage *pImage, const CArray2D<int> &rFilter, DWORD dwMinAlpha )
{
	NI_ASSERT_T( pImage != 0,
							 NStr::Format( "Wrong parameter: %x\n", pImage ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return false;
	}
	
	CPtr<IImage> pDestImage = pImageProcessor->CreateImage( pImage->GetSizeX(), pImage->GetSizeY() );
	if ( !pDestImage )
	{
		return false;
	}
	
	int nDivider = 0;
	for ( int nXIndex = 0; nXIndex < rFilter.GetSizeX(); ++ nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < rFilter.GetSizeY(); ++ nYIndex )
		{
			nDivider += rFilter[nYIndex][nXIndex];
		}
	}

	SRMImageApplyFilterInBoundsFunctional filterInBoundsFunctional( pImage, pDestImage, &rFilter, nDivider );
	SRMImageApplyFilterFunctional filterFunctional( pImage, pDestImage, &rFilter, nDivider );
	
	CTRect<int> indices( ( rFilter.GetSizeX() / 2 ),
											 ( rFilter.GetSizeY() / 2 ),
											 pImage->GetSizeX() - ( rFilter.GetSizeX() / 2 ),
											 pImage->GetSizeY() - ( rFilter.GetSizeY() / 2 ) );
	for ( int nXIndex = indices.minx; nXIndex < indices.maxx; ++nXIndex )
	{
		for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
		{
			filterFunctional( nXIndex, nYIndex );
		}
	}
	for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
	{
		for ( int nXIndex = 0; nXIndex < indices.minx; ++nXIndex )
		{
			filterInBoundsFunctional( nXIndex, nYIndex );
		}
		for ( int nXIndex = indices.maxx; nXIndex < pImage->GetSizeX(); ++nXIndex )
		{
			filterInBoundsFunctional( nXIndex, nYIndex );
		}
	}
	for ( int nXIndex = indices.minx; nXIndex < indices.maxx; ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < indices.miny; ++nYIndex )
		{
			filterInBoundsFunctional( nXIndex, nYIndex );
		}
		for ( int nYIndex = indices.maxy; nYIndex < pImage->GetSizeY(); ++nYIndex )
		{
			filterInBoundsFunctional( nXIndex, nYIndex );
		}
	}

	return pImage->CopyFrom( pDestImage, 0, 0 ,0 );
}

bool CRMImageBuilder::Emboss( IImage *pImage, const CTPoint<int> &rShiftPoint, const CArray2D<int> &rFilter, DWORD dwMinAlpha )
{
	NI_ASSERT_T( pImage != 0,
							 NStr::Format( "Wrong parameter: %x\n", pImage ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return false;
	}
	
	CPtr<IImage> pHeightsImage = pImageProcessor->CreateImage( pImage->GetSizeX(), pImage->GetSizeY() );
	if ( !pHeightsImage )
	{
		return false;
	}

	CImageAccessor heightsImageAccessor = pHeightsImage;
	CImageAccessor imageAccessor = pImage;

	for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
		{
			heightsImageAccessor[nYIndex][nXIndex] = ( imageAccessor[nYIndex][nXIndex].a >= dwMinAlpha ) ? WHITE_COLOR : BLACK_COLOR;
		}
	}

	if ( !ApplyFilter( pHeightsImage, rFilter, 1 ) )
	{
		return false;
	}
	
	for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
		{
			const SColor &rImageColor = imageAccessor[nYIndex][nXIndex];
			const SColor &rHeightColor = heightsImageAccessor[nYIndex][nXIndex];
			SColor shiftedHeigthColor;
			if ( ( nXIndex < -( rShiftPoint.x ) ) ||
					 ( nYIndex < -( rShiftPoint.y ) ) ||
					 ( nXIndex >= ( pImage->GetSizeX() - rShiftPoint.x ) ) ||
					 ( nYIndex >= ( pImage->GetSizeY() - rShiftPoint.y ) ) )
			{
				shiftedHeigthColor = heightsImageAccessor[nYIndex][nXIndex];
			}
			else
			{
				shiftedHeigthColor = heightsImageAccessor[nYIndex + rShiftPoint.y][nXIndex + rShiftPoint.x];
			}
			
			DWORD dwRed		= rImageColor.r * ( ( rHeightColor.r / 2 ) + ( 0xFF - ( shiftedHeigthColor.r / 2 ) ) ) / 0xFF;
			DWORD dwGreen	= rImageColor.g * ( ( rHeightColor.g / 2 ) + ( 0xFF - ( shiftedHeigthColor.g / 2 ) ) ) / 0xFF;
			DWORD dwBlue	= rImageColor.b * ( ( rHeightColor.b / 2 ) + ( 0xFF - ( shiftedHeigthColor.b / 2 ) ) ) / 0xFF;
			
			if ( dwRed < 0 ) dwRed = 0;
			if ( dwRed > 0xFF ) dwRed = 0xFF;
			if ( dwGreen < 0 ) dwGreen = 0;
			if ( dwGreen > 0xFF ) dwGreen = 0xFF;
			if ( dwBlue < 0 ) dwBlue = 0;
			if ( dwBlue > 0xFF ) dwBlue = 0xFF;
			
			imageAccessor[nYIndex][nXIndex].r = dwRed; 	
			imageAccessor[nYIndex][nXIndex].g = dwGreen; 	
			imageAccessor[nYIndex][nXIndex].b = dwBlue; 	
		}
	}

	return true;
}

bool CRMImageBuilder::Noise( IImage *pImage, IImage *pNoise, DWORD dwMinAlpha )
{
	NI_ASSERT_T( ( pImage != 0 ) && ( pNoise != 0 ),
							 NStr::Format( "Wrong parameter: %x, %x\n", pImage, pNoise ) );

	NI_ASSERT_T( ( pNoise->GetSizeX() * pNoise->GetSizeY() ) > 0,
							 NStr::Format( "Invalid Noise Size: (%d, %d)\n", pNoise->GetSizeX(), pNoise->GetSizeY() ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return false;
	}

	CImageAccessor imageAccessor = pImage;
	CImageAccessor noiseAccessor = pNoise;

	DWORD dwRedDivider = 0;
	DWORD dwGreenDivider = 0;
	DWORD dwBlueDivider = 0;

	for ( int nXIndex = 0; nXIndex < pNoise->GetSizeX(); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < pNoise->GetSizeY(); ++nYIndex )
		{
			dwRedDivider += noiseAccessor[nYIndex][nXIndex].r;
			dwGreenDivider += noiseAccessor[nYIndex][nXIndex].g;
			dwBlueDivider += noiseAccessor[nYIndex][nXIndex].b;
		}
	}
	dwRedDivider /= ( pNoise->GetSizeX() * pNoise->GetSizeY() );
	dwGreenDivider /= ( pNoise->GetSizeX() * pNoise->GetSizeY() );
	dwBlueDivider /= ( pNoise->GetSizeX() * pNoise->GetSizeY() );

	for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
		{
			if ( imageAccessor[nYIndex][nXIndex].a >= dwMinAlpha )
			{
				DWORD dwRed		= imageAccessor[nYIndex][nXIndex].r;
				DWORD dwGreen	= imageAccessor[nYIndex][nXIndex].g;
				DWORD dwBlue	= imageAccessor[nYIndex][nXIndex].b;
				
				if ( dwRedDivider > 0 )
				{
					dwRed = noiseAccessor[nYIndex % pNoise->GetSizeY()][nXIndex % pNoise->GetSizeX()].r * imageAccessor[nYIndex][nXIndex].r / dwRedDivider;
					if ( dwRed < 0 ) dwRed = 0;
					if ( dwRed > 0xFF ) dwRed = 0xFF;
				}
				if ( dwGreenDivider > 0 )
				{
					dwGreen = noiseAccessor[nYIndex % pNoise->GetSizeY()][nXIndex % pNoise->GetSizeX()].g * imageAccessor[nYIndex][nXIndex].g / dwGreenDivider;
					if ( dwGreen < 0 ) dwGreen = 0;
					if ( dwGreen > 0xFF ) dwGreen = 0xFF;
				}
				if ( dwBlueDivider > 0 )
				{
					dwBlue = noiseAccessor[nYIndex % pNoise->GetSizeY()][nXIndex % pNoise->GetSizeX()].b * imageAccessor[nYIndex][nXIndex].b / dwBlueDivider;
					if ( dwBlue < 0 ) dwBlue = 0;
					if ( dwBlue > 0xFF ) dwBlue = 0xFF;
				}

				imageAccessor[nYIndex][nXIndex].r = dwRed;
				imageAccessor[nYIndex][nXIndex].g = dwGreen;
				imageAccessor[nYIndex][nXIndex].b = dwBlue;
			}
		}
	}

	return true;	
}

bool CRMImageBuilder::FastAddImageByAlpha( IImage *pImage, IImage *pImageToAdd, DWORD dwMinAlpha )
{
	NI_ASSERT_T( ( pImage != 0 ) && ( pImageToAdd != 0 ),
							 NStr::Format( "Wrong parameter: %x, %x\n", pImage, pImageToAdd ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return false;
	}

	CImageAccessor imageAccessor = pImage;
	CImageAccessor imageToAddAccessor = pImageToAdd;
	
	for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
	{
		for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
		{
			SColor &rColor = imageToAddAccessor[nYIndex][nXIndex];
			if ( rColor.a >= dwMinAlpha )
			{
				imageAccessor[nYIndex][nXIndex] = rColor;
			}
		}
	}
	
	return true;
}

bool CRMImageBuilder::FastAddImageByColor( IImage *pImage, IImage *pImageToAdd, SColor color )
{
	NI_ASSERT_T( ( pImage != 0 ) && ( pImageToAdd != 0 ),
							 NStr::Format( "Wrong parameter: %x, %x\n", pImage, pImageToAdd ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return false;
	}

	CImageAccessor imageAccessor = pImage;
	CImageAccessor imageToAddAccessor = pImageToAdd;
	
	for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
	{
		for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
		{
			SColor &rColor = imageToAddAccessor[nYIndex][nXIndex];
			if ( rColor.a != color )
			{
				imageAccessor[nYIndex][nXIndex] = rColor;
			}
		}
	}
	
	return true;
}

IImage* CRMImageBuilder::GetEdge( IImage *pImage, SColor edgeColor,  SColor nonEdgeColor, DWORD dwMinAlpha )
{
	NI_ASSERT_T( pImage != 0,
							 NStr::Format( "Wrong parameter: %x\n", pImage ) );

	NI_ASSERT_T( ( pImage->GetSizeX() * pImage->GetSizeY() ) > 0,
							 NStr::Format( "Invalid Image Size: (%d, %d)\n", pImage->GetSizeX(), pImage->GetSizeY() ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return 0;
	}

	IImage *pEdgedImage = pImageProcessor->CreateImage( pImage->GetSizeX(), pImage->GetSizeY() );
	if ( pEdgedImage )
	{
		pEdgedImage->Set( nonEdgeColor );

		SRMGetImageEdgeFunctional getImageEdgeFunctional( pImage, pEdgedImage, edgeColor, dwMinAlpha );
		SRMGetImageEdgeInBoundsFunctional getImageEdgeInBoundsFunctional( pImage, pEdgedImage, edgeColor, dwMinAlpha );
		for ( int nXIndex = 1; nXIndex < ( pImage->GetSizeX() - 1 ); ++nXIndex )
		{
			for ( int nYIndex = 1; nYIndex < ( pImage->GetSizeY() - 1 ); ++nYIndex )
			{
				getImageEdgeFunctional( nXIndex, nYIndex );
			}
		}
		for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
		{
			int nXIndex = 0;
			getImageEdgeInBoundsFunctional( nXIndex, nYIndex );
			nXIndex = ( pImage->GetSizeX() - 1 );
			getImageEdgeInBoundsFunctional( nXIndex, nYIndex );
		}
		for ( int nXIndex = 1; nXIndex < ( pImage->GetSizeX() - 1 ); ++nXIndex )
		{
			int nYIndex = 0;
			getImageEdgeInBoundsFunctional( nXIndex, nYIndex );
			nYIndex = ( pImage->GetSizeY() - 1 );
			getImageEdgeInBoundsFunctional( nXIndex, nYIndex );
		}
	}
	return pEdgedImage;
}

IImage* CRMImageBuilder::GetShadow( IImage *pImage, const CTPoint<int> &rShiftPoint, SColor shadowColor, SColor nonShadowColor, DWORD dwMinAlpha )
{
	NI_ASSERT_T( pImage != 0,
							 NStr::Format( "Wrong parameter: %x\n", pImage ) );

	NI_ASSERT_T( ( pImage->GetSizeX() * pImage->GetSizeY() ) > 0,
							 NStr::Format( "Invalid Image Size: (%d, %d)\n", pImage->GetSizeX(), pImage->GetSizeY() ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return 0;
	}

	IImage *pShadowImage = pImageProcessor->CreateImage( pImage->GetSizeX(), pImage->GetSizeY() );
	if ( pShadowImage )
	{
		pShadowImage->Set( nonShadowColor );

		CImageAccessor imageAccessor = pImage;
		CUnsafeImageAccessor shadowImageAccessor = pShadowImage;

		for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
		{
			for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
			{
				if ( imageAccessor[nYIndex][nXIndex].a >= dwMinAlpha )
				{
					CTPoint<int> shiftPoint( nXIndex + rShiftPoint.x, nYIndex + rShiftPoint.y );
					if ( ( shiftPoint.x >= 0 ) && 
							 ( shiftPoint.y >= 0 ) &&
							 ( shiftPoint.x < pImage->GetSizeX() ) &&
							 ( shiftPoint.y < pImage->GetSizeY() )  )
					{
						if( imageAccessor[shiftPoint.y][shiftPoint.x] < dwMinAlpha )
						{
							shadowImageAccessor[shiftPoint.y][shiftPoint.x] = shadowColor;
						}
					}
				}
			}
		}
	}
	return pShadowImage;
}

IImage* CRMImageBuilder::GetAlphaEmboss( IImage *pImage, const CTPoint<int> &rShiftPoint, int nFilterSize, DWORD dwMinAlpha )
{
	NI_ASSERT_T( pImage != 0,
							 NStr::Format( "Wrong parameter: %x\n", pImage ) );

	NI_ASSERT_T( ( pImage->GetSizeX() * pImage->GetSizeY() ) > 0,
							 NStr::Format( "Invalid Image Size: (%d, %d)\n", pImage->GetSizeX(), pImage->GetSizeY() ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return 0;
	}
	
	CArray2D<int> embossFilter( nFilterSize, nFilterSize );
	embossFilter.Set( 1 );

	CPtr<IImage> pHeightsImage = pImageProcessor->CreateImage( pImage->GetSizeX(), pImage->GetSizeY() );
	if ( !pHeightsImage )
	{
		return 0;
	}

	CTPoint<int> frontShift( ( rShiftPoint.x / 2 ), ( rShiftPoint.y / 2 ) );
	CTPoint<int> backShift( frontShift * ( -1 ) );
	if ( ( rShiftPoint.x & 0x01 ) > 0 )
	{
		frontShift.x += ( rShiftPoint.x > 0 ) ? 1 : ( -1 );
	}
	if ( ( rShiftPoint.y & 0x01 ) > 0 )
	{
		frontShift.y += ( rShiftPoint.y > 0 ) ? 1 : ( -1 );
	}

	CImageAccessor imageAccessor = pImage;
	CImageAccessor heightsImageAccessor = pHeightsImage;

	for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
		{
			heightsImageAccessor[nYIndex][nXIndex] = ( imageAccessor[nYIndex][nXIndex].a < dwMinAlpha ) ? WHITE_COLOR : BLACK_COLOR;
		}
	}

	if ( !ApplyFilter( pHeightsImage, embossFilter, dwMinAlpha ) )
	{
		return 0;
	}
	
	IImage *pEmbossImage = pImageProcessor->CreateImage( pImage->GetSizeX(), pImage->GetSizeY() );
	if ( pEmbossImage )
	{
		CUnsafeImageAccessor embossImageAccessor = pEmbossImage;

		for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
		{
			for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
			{
				const SColor &rEmbossImageColor = embossImageAccessor[nYIndex][nXIndex];
				SColor frontShiftHeigthColor;
				if ( ( ( nXIndex + frontShift.x ) >= 0 ) &&
						 ( ( nYIndex + frontShift.y ) >= 0 ) &&
						 ( ( nXIndex + frontShift.x ) < pImage->GetSizeX() ) &&
						 ( ( nYIndex + frontShift.y ) < pImage->GetSizeY() ) )
				{
					frontShiftHeigthColor = heightsImageAccessor[nYIndex + frontShift.y][nXIndex + frontShift.x];
				}
				else
				{
					frontShiftHeigthColor = heightsImageAccessor[nYIndex][nXIndex];
				}
				
				SColor backShiftHeigthColor;
				if ( ( ( nXIndex + backShift.x ) >= 0 ) &&
						 ( ( nYIndex + backShift.y ) >= 0 ) &&
						 ( ( nXIndex + backShift.x ) < pImage->GetSizeX() ) &&
						 ( ( nYIndex + backShift.y ) < pImage->GetSizeY() ) )
				{
					backShiftHeigthColor = heightsImageAccessor[nYIndex + backShift.y][nXIndex + backShift.x];
				}
				else
				{
					backShiftHeigthColor = heightsImageAccessor[nYIndex][nXIndex];
				}
				
				DWORD dwRed		= ( ( frontShiftHeigthColor.r / 2 ) + ( 0xFF - ( backShiftHeigthColor.r / 2 ) ) ) / 2;
				DWORD dwGreen	= ( ( frontShiftHeigthColor.g / 2 ) + ( 0xFF - ( backShiftHeigthColor.g / 2 ) ) ) / 2;
				DWORD dwBlue	= ( ( frontShiftHeigthColor.b / 2 ) + ( 0xFF - ( backShiftHeigthColor.b / 2 ) ) ) / 2;
				
				if ( dwRed < 0 ) dwRed = 0;
				if ( dwRed > 0xFF ) dwRed = 0xFF;
				if ( dwGreen < 0 ) dwGreen = 0;
				if ( dwGreen > 0xFF ) dwGreen = 0xFF;
				if ( dwBlue < 0 ) dwBlue = 0;
				if ( dwBlue > 0xFF ) dwBlue = 0xFF;
				
				embossImageAccessor[nYIndex][nXIndex].a = 0xFF;
				embossImageAccessor[nYIndex][nXIndex].r = dwRed; 	
				embossImageAccessor[nYIndex][nXIndex].g = dwGreen; 	
				embossImageAccessor[nYIndex][nXIndex].b = dwBlue; 	
			}
		}
	}	
	return pEmbossImage;
}

IImage* CRMImageBuilder::FastComposeImagesByAlpha( const std::vector<CPtr<IImage> > &rImages, DWORD dwMinAlpha )
{
	NI_ASSERT_T( !rImages.empty(),
							 NStr::Format( "Invalid size: %d\n", rImages.size() ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return 0;
	}

	CTPoint<int> imageSize(  rImages[0]->GetSizeX(), rImages[0]->GetSizeY() );
	IImage *pComposeImage = pImageProcessor->CreateImage( imageSize.x, imageSize.y );
	if ( pComposeImage )
	{
		pComposeImage->Set( 0 );
		CUnsafeImageAccessor composeImageAccessor = pComposeImage;

		std::vector<CImageAccessor> imageAccessors;
		for ( int nImageIndex = 0; nImageIndex < rImages.size(); ++nImageIndex )
		{
			imageAccessors.push_back( CImageAccessor( rImages[nImageIndex] ) );
		}

		for ( int nXIndex = 0; nXIndex < imageSize.x; ++nXIndex )
		{
			for ( int nYIndex = 0; nYIndex < imageSize.y; ++nYIndex )
			{
				for ( int nImageIndex = 0; nImageIndex < rImages.size(); ++nImageIndex )
				{
					SColor &rColor = imageAccessors[nImageIndex][nYIndex][nXIndex];
					if ( rColor.a >= dwMinAlpha )
					{
						composeImageAccessor[nYIndex][nXIndex] = rColor;
						break;
					}
				}
			}
		}
	}

	return pComposeImage;
}

IImage* CRMImageBuilder::FastComposeImagesByColor( const std::vector<CPtr<IImage> > &rImages, SColor color )
{
	NI_ASSERT_T( !rImages.empty(),
							 NStr::Format( "Invalid size: %d\n", rImages.size() ) );

	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return 0;
	}

	CTPoint<int> imageSize(  rImages[0]->GetSizeX(), rImages[0]->GetSizeY() );
	IImage *pComposeImage = pImageProcessor->CreateImage( imageSize.x, imageSize.y );
	if ( pComposeImage )
	{
		pComposeImage->Set( color );
		CUnsafeImageAccessor composeImageAccessor = pComposeImage;

		std::vector<CImageAccessor> imageAccessors;
		for ( int nImageIndex = 0; nImageIndex < rImages.size(); ++nImageIndex )
		{
			imageAccessors.push_back( CImageAccessor( rImages[nImageIndex] ) );
		}

		for ( int nXIndex = 0; nXIndex < imageSize.x; ++nXIndex )
		{
			for ( int nYIndex = 0; nYIndex < imageSize.y; ++nYIndex )
			{
				for ( int nImageIndex = 0; nImageIndex < rImages.size(); ++nImageIndex )
				{
					SColor &rColor = imageAccessors[nImageIndex][nYIndex][nXIndex];
					if ( rColor != color )
					{
						composeImageAccessor[nYIndex][nXIndex] = rColor;
						break;
					}
				}
			}
		}
	}

	return pComposeImage;
}
