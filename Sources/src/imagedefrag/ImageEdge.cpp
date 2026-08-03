#include "StdAfx.h"
#include "..//Image//Image.h"
#include "2DVarArray.h"

#include "ImageEdge.h"

bool SImageEdge::In( const CTPoint<int> &rPoint )
{
  if(edges.GetSize() == 0 ) return false;
  int offset = 0;
  short index = 0; 
  short count = 0;
  if ( isHorizontal )
  {
    if ( ( rPoint.y < originalLeftTop.y ) ||
         ( rPoint.y >= originalLeftTop.y + edges.GetLineCount() ) ) return false;
    index = rPoint.y - originalLeftTop.y; 
    offset = rPoint.x - originalLeftTop.x;
  }
  else
  {
    if ( ( rPoint.x < originalLeftTop.x ) ||
         ( rPoint.x >= originalLeftTop.x + edges.GetLineCount() ) ) return false;
    index = rPoint.x - originalLeftTop.x; 
    offset = rPoint.y - originalLeftTop.y;
  }
  while( count < edges.GetLineSize(index) )
  {
    if ( edges[index][count] == offset ) return true;
    else if ( edges[index][count] < offset ) ++count;
    else return ( ( count % 2 ) > 0 );
  }
  return false;
}

bool SImageEdge::CreateImageEdge( IImage *pImage,
                                  const CTPoint<int> &rOriginalLeftTop,
                                  DWORD alpha )
{
  if ( pImage == 0 ) return false;

  edges.Clear();
  originalLeftTop = rOriginalLeftTop;

  CTPoint<int> dim( pImage->GetSizeX(), pImage->GetSizeY() );
	int nXindex = 0;
	int nYindex = 0;
  CImageAccessor imageAccessor = pImage;
  int elements = 0;
  isHorizontal = dim.y <= dim.x;

  if ( isHorizontal )
  {
    for ( nYindex = 0; nYindex < dim.y; ++nYindex )
	  {
      bool isOdd = false;
      for ( nXindex = 0; nXindex < dim.x; ++nXindex )
		  {
        if ( ( imageAccessor[nYindex][nXindex].a < alpha ) == isOdd )
        {
          if ( ( nXindex != ( dim.x - 1 ) ) &&
               ( ( imageAccessor[nYindex][nXindex + 1].a < alpha ) == isOdd ) )
          {
            ++elements;
            isOdd = !isOdd;
          }
        }
		  }
      if ( isOdd )
      {
        ++elements;
      }
	  }
    if( ( elements > 0 ) && ( dim.y > 0 ) )
    {
      edges.SetSizes( elements, dim.y );
    }
  }
  else
  {
    for ( nXindex = 0; nXindex < dim.x; ++nXindex )
	  {
      bool isOdd = false;
      std::vector< short > edge;
      for ( nYindex = 0; nYindex < dim.y; ++nYindex )
		  {
        if ( ( imageAccessor[nYindex][nXindex].a < alpha ) == isOdd )
        {
          if ( ( nYindex != ( dim.y - 1 ) ) &&
               ( ( imageAccessor[nYindex + 1][nXindex].a < alpha ) == isOdd ) )
          {
            ++elements;
            isOdd = !isOdd;
          }
        }
		  }
      if ( isOdd )
      { 
        ++elements;
      }
	  }
    if( ( elements > 0 ) && ( dim.x > 0 ) )
    {
      edges.SetSizes( elements, dim.x );
    }
  }
  if( edges.IsEmpty() ) return true;

  if ( isHorizontal )
  {
    for ( nYindex = 0; nYindex < dim.y; ++nYindex )
	  {
      elements = 0;
      bool isOdd = false;
      for ( nXindex = 0; nXindex < dim.x; ++nXindex )
		  {
        if ( ( imageAccessor[nYindex][nXindex].a < alpha ) == isOdd )
        {
          if ( ( nXindex != ( dim.x - 1 ) ) &&
               ( ( imageAccessor[nYindex][nXindex + 1].a < alpha ) == isOdd ) )
          {
            ++elements;
            isOdd = !isOdd;
          }
        }
		  }
      if ( isOdd )
      {
        ++elements;      
      }
      edges.SetLineLength( nYindex, elements );
      elements = 0;
      isOdd = false;
      for ( nXindex = 0; nXindex < dim.x; ++nXindex )
		  {
        if ( ( imageAccessor[nYindex][nXindex].a < alpha ) == isOdd )
        {
          if ( ( nXindex != ( dim.x - 1 ) ) &&
               ( ( imageAccessor[nYindex][nXindex + 1].a < alpha ) == isOdd ) )
          {
            edges[nYindex][elements] = short( nXindex  - ( isOdd ? 1 : 0 ) );
            ++elements;
            isOdd = !isOdd;
          }
        }
		  }
      if ( isOdd )
      {
        edges[nYindex][elements] = short( dim.x - 1 );
      }
	  }
  }
  else
  {
    for ( nXindex = 0; nXindex < dim.x; ++nXindex )
	  {
      elements = 0;
      bool isOdd = false;
      for ( nYindex = 0; nYindex < dim.y; ++nYindex )
		  {
        if ( ( imageAccessor[nYindex][ nXindex].a < alpha ) == isOdd )
        {
          if ( ( nYindex != ( dim.y - 1 ) ) &&
               ( ( imageAccessor[nYindex + 1][nXindex].a < alpha ) == isOdd ) )
          {
            ++elements;
            isOdd = !isOdd;
          }
        }
		  }
      if ( isOdd )
      {
        ++elements;
      }
      edges.SetLineLength( nXindex, elements );
      elements = 0;
      isOdd = false;
      for ( nYindex = 0; nYindex < dim.y; ++nYindex )
		  {
        if ( ( imageAccessor[nYindex][nXindex].a < alpha ) == isOdd )
        {
          if ( ( nYindex != ( dim.y - 1 ) ) &&
               ( ( imageAccessor[nYindex + 1][nXindex].a < alpha ) == isOdd ) )
          {
            edges[nXindex][elements] = short( nYindex  - ( isOdd ? 1 : 0 ) );
            ++elements;
            isOdd = !isOdd;
          }
        }
		  }
      if ( isOdd )
      {
        edges[nXindex][elements] = short( dim.y - 1 );
      }
	  }
  }
  return true;
}

#ifdef _DEBUG
bool SImageEdge::MarkEdge( IImage *pImage )
{
  if ( pImage == 0 ) return false;

  CTPoint<int> dim( pImage->GetSizeX(), pImage->GetSizeY() );
	int nXindex = 0;
	int nYindex = 0;
  CImageAccessor imageAccessor = pImage;

  if(isHorizontal)
  {
    for ( nYindex = 0; nYindex < edges.GetLineCount(); ++nYindex )
	  {
      for ( nXindex = 0; nXindex < edges.GetLineSize(nYindex); ++nXindex )
		  {
        if( ( edges[nYindex][nXindex] < dim.x ) && 
            ( nYindex < dim.y ) )
        {
          SColor &rColor = imageAccessor[nYindex][static_cast<int>( edges[nYindex][nXindex] )];
          rColor.r = 255;
          rColor.g = 255;
          rColor.b = 255;
        }
      }
    }
  }
  else
  {
    for ( nXindex = 0; nXindex < edges.GetLineCount(); ++nXindex )
	  {
      for ( nYindex = 0; nYindex < edges.GetLineSize(nXindex); ++nYindex )
		  {
        if( ( nXindex < dim.x ) && 
            ( edges[nXindex][nYindex] < dim.y ) )
        {
          SColor &rColor = imageAccessor[static_cast<int>( edges[nXindex][nYindex] )][nXindex];
          rColor.r = 255;
          rColor.g = 255;
          rColor.b = 255;
        }
      }
    }
  }
  return true;
}

bool SImageEdge::MarkInEdge( IImage *pImage )
{
  if ( pImage == 0 ) return false;

  CTPoint<int> dim( pImage->GetSizeX(), pImage->GetSizeY() );
	int nXindex = 0;
	int nYindex = 0;
  CImageAccessor imageAccessor = pImage;

  for ( nYindex = 0; nYindex < dim.y; ++nYindex )
	{
    for ( nXindex = 0; nXindex < dim.x; ++nXindex )
		{
      if( In( CTPoint<int>( nXindex, nYindex ) ) )
      {
        SColor &rColor = imageAccessor[nYindex][nXindex];
        rColor.r = 255;
        rColor.g = 255;
        rColor.b = 255;
      }
    }
  }
  return true;
}

bool SImageEdge::MarkAlpha( IImage *pImage , DWORD dwMinAlpha, DWORD dwMaxAlpha )
{
  if ( pImage == 0 ) return false;

  CTPoint<int> dim( pImage->GetSizeX(), pImage->GetSizeY() );
	int nXindex = 0;
	int nYindex = 0;
  CImageAccessor imageAccessor = pImage;

	for ( nYindex = 0; nYindex < dim.y; ++nYindex )
	{
    for ( nXindex = 0; nXindex < dim.x; ++nXindex )
		{
      if ( ( imageAccessor[nYindex][nXindex].a >= dwMinAlpha ) &&
           ( imageAccessor[nYindex][nXindex].a <= dwMaxAlpha ) )
      {
        SColor &rColor = imageAccessor[nYindex][nXindex];
        rColor.r = 255;
        rColor.g = 255;
        rColor.b = 255;
      }
    }
  }
  return true;
}
#endif //#ifdef _DEBUG