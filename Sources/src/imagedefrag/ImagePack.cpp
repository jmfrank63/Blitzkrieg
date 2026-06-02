#include "StdAfx.h"
#include "..\\Image\\Image.h"
#include "ImagePack.h"

const int SImagePack::LARGE_SIDE = 32;
const int SImagePack::SMALL_SIDE = 16;
const int SImagePack::GRANULARITY = 2;

void SImagePack::GetSquaresCount( int &nLargeSquaresCount, int &nSmallSquaresCount ) const
{
  nLargeSquaresCount = 0;
  nSmallSquaresCount = 0;
  for ( int imageIndex = 0; imageIndex < vPackedImages.size(); ++imageIndex )
  {
    for( int nodeIndex = 0; nodeIndex < vPackedImages[imageIndex].vPackedImageNodes.size(); ++nodeIndex )
    {
      if ( vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].isLarge ) ++nLargeSquaresCount;
      else ++nSmallSquaresCount;
    }
  }
}

bool SImagePack::GetMinimalDimensions( CTPoint<int> &rDim ) const
{
  int nLargeSquaresCount = 0;
  int nSmallSquaresCount = 0;
  GetSquaresCount( nLargeSquaresCount, nSmallSquaresCount );

  rDim.x = 0;
  rDim.y = 0;
  int nTotalLargeSquaresCount = nLargeSquaresCount + 
                                ( ( nSmallSquaresCount + square ( GRANULARITY ) - 1 ) / square ( GRANULARITY ) );
  int nFitSide = 1;
  while ( ( nFitSide * nFitSide ) < nTotalLargeSquaresCount )
  {
    nFitSide *= 2;
  }
  if ( ( nFitSide * nFitSide / 2 ) > nTotalLargeSquaresCount )
  {
    rDim.x = nFitSide;
    rDim.y = nFitSide / 2;
  }
  else
  {
    rDim.x = nFitSide;
    rDim.y = nFitSide;
  }
  return true;
}

bool SImagePack::FindEdges( IImage *pImage, CTRect<int> &rEdges , DWORD dwMinAlpha) const
{
	if( pImage == 0 ) return false;

  CTPoint<int> dim( pImage->GetSizeX(), pImage->GetSizeY() );
	int nXindex = 0;
	int nYindex = 0;

	rEdges.left = -1;
	rEdges.top = -1;
	rEdges.right =  dim.x;
	rEdges.bottom = dim.y;

  CImageAccessor imageAccessor = pImage;

	for ( nYindex = 0; nYindex < dim.y; ++nYindex )
	{
		for ( nXindex = 0; nXindex < dim.x; ++nXindex )
		{
			if ( imageAccessor[nYindex][nXindex].a >= dwMinAlpha )
			{
				rEdges.top = nYindex;
				break;
			}
		}
		if( rEdges.top >= 0 ) break;
	}
	if ( rEdges.top < 0 ) return false;

	for ( nYindex = dim.y - 1; nYindex >= rEdges.top; --nYindex )
	{
		for ( nXindex = 0; nXindex < dim.x; ++nXindex )
		{
			if ( imageAccessor[nYindex][nXindex].a >= dwMinAlpha )
			{
				rEdges.bottom = nYindex;
				break;
			}
		}
		if ( rEdges.bottom < dim.y ) break;
	}

	for ( nXindex = 0; nXindex < dim.x; ++nXindex )
	{
		for ( nYindex = rEdges.top; nYindex <= rEdges.bottom; ++nYindex )
		{
			if ( imageAccessor[nYindex][nXindex].a >= dwMinAlpha )
			{
				rEdges.left = nXindex;
				break;
			}
		}
		if ( rEdges.left >= 0 ) break;
	}

	for ( nXindex = dim.x - 1; nXindex >= rEdges.left; --nXindex )
	{
		for ( nYindex = rEdges.top; nYindex <= rEdges.bottom; ++nYindex )
		{
			if ( imageAccessor[nYindex][nXindex].a >= dwMinAlpha )
			{
				rEdges.right = nXindex;
				break;
			}
		}
		if ( rEdges.right < dim.x ) break;
	}
  rEdges.right += 1;
  rEdges.bottom += 1;
	return true;
}

IImage* SImagePack::CreateImagePack( IImage **pImages, CTPoint<int> *pImageLeftTops, int nImageCount, DWORD dwMinAlpha )
{
  if ( ( pImages == 0 ) || ( pImageLeftTops == 0 ) || ( nImageCount == 0 ) )
  {
    return 0;
  }

  vPackedImages.clear();
  IImageProcessor *pImageProcessor = GetImageProcessor();
  
  int imageIndex = 0;
  for( imageIndex = 0; imageIndex < nImageCount; ++imageIndex )
  {
    vPackedImages.push_back( SPackedImage() );
    vPackedImages[imageIndex].originalLeftTop = pImageLeftTops[imageIndex];

    CTRect<int> edges( 0, 0, 0, 0 );	
    if( !FindEdges( pImages[imageIndex], edges , dwMinAlpha ) )
    {
      break;
    }
    CPtr<IImage> pEdgedImage = pImageProcessor->CreateImage( pImages[imageIndex]->GetSizeX(),
                                                             pImages[imageIndex]->GetSizeY() );
    pEdgedImage->Set( 0 );
    pEdgedImage->CopyFrom( pImages[imageIndex], &( ( RECT )( edges ) ), 0, 0 );

    int nXSize = 0; 
    int nYSize = 0;
    if ( ( edges.GetSizeX() % LARGE_SIDE ) > 0 )
    {
      nXSize = edges.GetSizeX() + LARGE_SIDE - ( edges.GetSizeX() % LARGE_SIDE );
    }
    else
    {
      nXSize = edges.GetSizeX();
    }
    if ( ( edges.GetSizeY() % LARGE_SIDE ) > 0 )
    {
      nYSize = edges.GetSizeY() + LARGE_SIDE - ( edges.GetSizeY() % LARGE_SIDE );
    }
    else
    {
      nYSize = edges.GetSizeY();
    }

    CImageAccessor edgedImageAccessor = pEdgedImage;
    for ( int nLargeYIndex = 0; nLargeYIndex < ( nYSize / LARGE_SIDE ); ++nLargeYIndex )
    {
      for ( int nLargeXIndex = 0; nLargeXIndex < ( nXSize / LARGE_SIDE ); ++nLargeXIndex )
      {
        int nSmallSquaresCount = 0;
        for ( int nSmallYIndex = 0; nSmallYIndex < GRANULARITY;	++nSmallYIndex )
        {
          for ( int nSmallXIndex = 0; nSmallXIndex < GRANULARITY; ++nSmallXIndex )
          {
            bool isHandled = false;
            int nXStartIndex =  ( nLargeXIndex * LARGE_SIDE ) + ( nSmallXIndex * SMALL_SIDE );
            int nYStartIndex =  ( nLargeYIndex * LARGE_SIDE ) + ( nSmallYIndex * SMALL_SIDE );
            int nXFinishIndex = ( nLargeXIndex * LARGE_SIDE ) + ( ( nSmallXIndex + 1) * SMALL_SIDE );
            int nYFinishIndex = ( nLargeYIndex * LARGE_SIDE ) + ( ( nSmallYIndex + 1) * SMALL_SIDE );
            for ( int nYIndex = nYStartIndex; nYIndex < nYFinishIndex; ++nYIndex )
            {
              for ( int nXIndex = nXStartIndex; nXIndex < nXFinishIndex; ++nXIndex )
              {
                if ( edgedImageAccessor[nYIndex][nXIndex].a >= dwMinAlpha )
                {
                  ++nSmallSquaresCount;
                  SPackedImage::SPackedImageNode packedImageNode;
                  packedImageNode.original.x = edges.left +
                                               vPackedImages[imageIndex].originalLeftTop.x +
                                               nXStartIndex;
                  packedImageNode.original.y = edges.top +
                                               vPackedImages[imageIndex].originalLeftTop.y + 
                                               nYStartIndex;
                  packedImageNode.isLarge = false;
                  vPackedImages[imageIndex].vPackedImageNodes.push_back( packedImageNode );
                  isHandled = true;
                  break;
                }
              }
              if ( isHandled ) break;
            }
          }
        }
        if ( nSmallSquaresCount == square( GRANULARITY ) )
        {
          for ( int index = 0; index < nSmallSquaresCount; ++index )
          {
            vPackedImages[imageIndex].vPackedImageNodes.pop_back();
          }
          SPackedImage::SPackedImageNode packedImageNode;
          packedImageNode.original.x = edges.left + 
                                       vPackedImages[imageIndex].originalLeftTop.x + 
                                       ( nLargeXIndex * LARGE_SIDE );
          packedImageNode.original.y = edges.top +
                                       vPackedImages[imageIndex].originalLeftTop.y +
                                       ( nLargeYIndex * LARGE_SIDE );
          packedImageNode.isLarge = true;
          vPackedImages[imageIndex].vPackedImageNodes.push_back( packedImageNode );
        }
      }
    }
  }
  if( vPackedImages.size() == 0 ) return 0;

  CTPoint<int> packDim( 0, 0 );
  GetMinimalDimensions( packDim );

  int nLargeCurrentSquare = 0;
  int nSmallCurrentSquare = 0;
  int nLargeSquaresCount = 0;
  int nSmallSquaresCount = 0;

  GetSquaresCount(nLargeSquaresCount, nSmallSquaresCount);

  int nodeIndex = 0;
  for( imageIndex = 0; imageIndex < nImageCount; ++imageIndex )
  {
    for( nodeIndex = 0; nodeIndex < vPackedImages[imageIndex].vPackedImageNodes.size(); ++nodeIndex )
    {
	    int nXposition = 0;
		  int nYposition = 0;
		  if(vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].isLarge)
		  {
			  nXposition = ( nLargeCurrentSquare % packDim.x ) * LARGE_SIDE;
			  nYposition = ( nLargeCurrentSquare / packDim.x ) * LARGE_SIDE;
        ++nLargeCurrentSquare;
		  }
		  else
		  {
			  int nActualLargeSquare = nLargeSquaresCount + ( nSmallCurrentSquare / square( GRANULARITY ) );
			  nXposition = ( nActualLargeSquare % packDim.x ) * LARGE_SIDE;
        nXposition += ( ( nSmallCurrentSquare % square( GRANULARITY ) ) % GRANULARITY ) * SMALL_SIDE;
			  nYposition = ( nActualLargeSquare / packDim.x ) * LARGE_SIDE;
			  nYposition += ( ( nSmallCurrentSquare % square( GRANULARITY ) ) / GRANULARITY ) * SMALL_SIDE;
			  ++nSmallCurrentSquare;
		  }
      vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.left = ( 0.5f + nXposition ) / ( packDim.x * LARGE_SIDE );
		  vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.top = ( 0.5f + nYposition ) / ( packDim.y * LARGE_SIDE );
		  if(vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].isLarge)
      {
        vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.right = ( 0.5f + nXposition + LARGE_SIDE ) / ( packDim.x * LARGE_SIDE );
		    vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.bottom = ( 0.5f + nYposition + LARGE_SIDE ) / ( packDim.y * LARGE_SIDE );
      }
      else
      {
        vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.right = ( 0.5f + nXposition + SMALL_SIDE ) / ( packDim.x * LARGE_SIDE );
		    vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.bottom = ( 0.5f + nYposition + SMALL_SIDE ) / ( packDim.y * LARGE_SIDE );
      }
    }
	}
	IImage *pPackedImage = pImageProcessor->CreateImage( packDim.x * LARGE_SIDE, packDim.y * LARGE_SIDE );
#if !defined(_DEBUG)
  pPackedImage->Set( 0 );
#endif //#if !defined(_DEBUG)
  for( imageIndex = 0; imageIndex < nImageCount; ++imageIndex )
  {
    for( nodeIndex = 0; nodeIndex < vPackedImages[imageIndex].vPackedImageNodes.size(); ++nodeIndex )
    {
      CTRect<int> originalSquare;
      originalSquare.left = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].original.x - 
                            vPackedImages[imageIndex].originalLeftTop.x;
      originalSquare.top = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].original.y - 
                           vPackedImages[imageIndex].originalLeftTop.y;
      originalSquare.right = originalSquare.left + 
                             ( vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].isLarge ?
                               LARGE_SIDE :
                               SMALL_SIDE );
      if( originalSquare.right > pImages[imageIndex]->GetSizeX() )
      {
        originalSquare.right = pImages[imageIndex]->GetSizeX();
      }
      originalSquare.bottom = originalSquare.top + 
                             ( vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].isLarge ?
                               LARGE_SIDE :
                               SMALL_SIDE );
      if( originalSquare.bottom > pImages[imageIndex]->GetSizeY() )
      {
        originalSquare.bottom = pImages[imageIndex]->GetSizeY();
      }
#ifdef _DEBUG
      originalSquare.bottom -= 1;
      originalSquare.right -= 1;
#endif //#ifdef _DEBUG
      pPackedImage->CopyFrom( pImages[imageIndex],
                              &( ( RECT )( originalSquare ) ),
                              vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.left * packDim.x * LARGE_SIDE,
                              vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.top * packDim.y * LARGE_SIDE );
    }
  }
  return pPackedImage;
}

IImage* SImagePack::UnpackImage( IImage *pPackedImage, int imageIndex ) const
{
  if ( ( pPackedImage == 0 ) ||
       ( imageIndex >= vPackedImages.size() ) ||
       ( vPackedImages[imageIndex].vPackedImageNodes.size() == 0 ) ) return 0;

  int nMaxXSize = 0;
  int nMaxYSize = 0;
  int nodeIndex = 0;
  for( nodeIndex = 0; nodeIndex < vPackedImages[imageIndex].vPackedImageNodes.size(); ++nodeIndex )
  {
    CTRect<int> originalSquare;
    originalSquare.left = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].original.x - vPackedImages[imageIndex].originalLeftTop.x;
    originalSquare.top = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].original.y - vPackedImages[imageIndex].originalLeftTop.y;
    if( originalSquare.left > nMaxXSize )
    {
      nMaxXSize = originalSquare.left;
    }
    if( originalSquare.top > nMaxYSize )
    {
      nMaxYSize = originalSquare.top;
    }
  }
  if( nMaxXSize < 32 )
  {
    nMaxXSize = 32;
  }
  else
  {
    ++nMaxXSize;
  }
  if( nMaxYSize < 32 )
  {
    nMaxYSize = 32;
  }
  else
  {
    ++nMaxYSize;
  }
#ifdef _DEBUG
  printf("nMaxSize (%dx%d)\n", nMaxXSize, nMaxYSize );
#endif //#ifdef _DEBUG
  int power = 1;
  while( power < nMaxXSize ) power *= 2;
  nMaxXSize = power;
  power = 1;
  while( power < nMaxYSize ) power *= 2;
  nMaxYSize = power;

  IImageProcessor *pImageProcessor = GetImageProcessor();
	IImage *pOriginalImage = pImageProcessor->CreateImage( nMaxXSize, nMaxYSize );
#if !defined(_DEBUG)
  pOriginalImage->Set( 0 );
#endif //#if !defined(_DEBUG)
  CTPoint<int> packedDim( pPackedImage->GetSizeX(), pPackedImage->GetSizeY() );
  for( nodeIndex = 0; nodeIndex < vPackedImages[imageIndex].vPackedImageNodes.size(); ++nodeIndex )
  {
    CTRect<int> packedSquare;
    packedSquare.left = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.left * packedDim.x;
    packedSquare.top = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.top * packedDim.y;
    packedSquare.right = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.right * packedDim.x;
    packedSquare.bottom = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].packed.bottom * packedDim.y;
    
    int actualRight = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].original.x - 
                      vPackedImages[imageIndex].originalLeftTop.x + 
                      ( vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].isLarge ?
                        LARGE_SIDE :
                        SMALL_SIDE );
    
    int actualBottom = vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].original.y - 
                       vPackedImages[imageIndex].originalLeftTop.y + 
                       ( vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].isLarge ?
                         LARGE_SIDE :
                         SMALL_SIDE );
    if( actualRight > nMaxXSize )
    {
      packedSquare.right -= actualRight - nMaxXSize;
    }
    if( actualBottom > nMaxYSize )
    {
      packedSquare.bottom -= actualBottom - nMaxYSize;
    }
    
    pOriginalImage->CopyFrom( pPackedImage,
                              &( ( RECT )( packedSquare ) ),
                              vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].original.x - vPackedImages[imageIndex].originalLeftTop.x,
                              vPackedImages[imageIndex].vPackedImageNodes[nodeIndex].original.y - vPackedImages[imageIndex].originalLeftTop.y );
  }
  return pOriginalImage;
}
