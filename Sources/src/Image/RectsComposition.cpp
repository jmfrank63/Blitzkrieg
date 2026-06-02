#include "StdAfx.h"

#include "ImageProcessor.h"

#include "ImageReal.h"
static const int MAX_TEXTURE_SIZE = 2048;
struct SRectOptimizeStructure
{
	int nNum;
	GRect rRect;
	SRectOptimizeStructure() {  }
	SRectOptimizeStructure( GRect rect, int num ) { nNum = num; rRect = rect; }
};
class CFindFillRectHelper
{
public:	
	int dx;
	int dy;
	CFindFillRectHelper() :  dx(0), dy(0) {}
	void operator ()( const SRectOptimizeStructure &r ) 
	{ 
		if ( r.rRect.bottom() > dy )
			dy = r.rRect.bottom();
		if ( r.rRect.right()  > dx )
			dx = r.rRect.right();
	}
};
GRect GetFillRect( std::vector<SRectOptimizeStructure> &rects )
{
	CFindFillRectHelper hlp;
	hlp = std::for_each( rects.begin(), rects.end(), hlp );
	hlp.dx = GetNextPow2( hlp.dx + 1) - 1;
	hlp.dy = GetNextPow2( hlp.dy + 1) - 1;
	return GRect( 0, 0, hlp.dx, hlp.dy );
}
class CRectSortFunc
{
public:
	CRectSortFunc() {  }
	bool operator()( const SRectOptimizeStructure &r1, const SRectOptimizeStructure &r2 ) const 
	{
		if ( r1.rRect.height() == r2.rRect.height() )
			return r1.rRect.width() > r2.rRect.width();
		else
			return r1.rRect.height() > r2.rRect.height();
	}
};
class CFindIntersectRectHelper
{
public:	
	mutable bool intersect;
	GRect rRect;
	explicit CFindIntersectRectHelper( const GRect &r ) : intersect( false ), rRect( r ) {  }
	void operator()( const SRectOptimizeStructure &r ) const
	{ 
		GRect rRectTemp = rRect;
		rRectTemp.intersect( r.rRect );
		if ( !rRectTemp.isEmpty() )
			intersect = true;
	}
};
void SliceRectToDown( const std::vector<SRectOptimizeStructure> &rects, SRectOptimizeStructure &rect )
{
	bool intersect = true;
	std::set<int> yCorners;
	for ( std::vector<SRectOptimizeStructure>::const_iterator it = rects.begin(); it != rects.end(); ++it )
		yCorners.insert( (*it).rRect.bottom() + 1);				

	for ( std::set<int>::iterator it = yCorners.begin(); it != yCorners.end(); ++it )
	{
		CFindIntersectRectHelper hlp( rect.rRect );	
		hlp = std::for_each( rects.begin(), rects.end(), hlp );
		if ( hlp.intersect == true )
		{
			intersect  = true;
			rect.rRect.origin.y = (*it);
		}
		else
		{
			intersect = false;
			break;
		}
	}
}
void OptimizeRectsByWidth( std::vector<SRectOptimizeStructure> &rects, int textureWidth )
{
	const int TEXTUREWIDTH = textureWidth;
	std::vector<SRectOptimizeStructure> vTemp;

	int nCurrentX = 0;
	enum { toLeft, toRight } curDirection;
	curDirection = toRight;
	for ( std::vector<SRectOptimizeStructure>::const_iterator it = rects.begin(); it != rects.end(); ++it )
	{
		GRect newRect = (*it).rRect;
		if ( curDirection == toRight )
		{
			newRect.move_origin( nCurrentX, 0 );
			if ( newRect.right() >= TEXTUREWIDTH )
			{
				newRect.move( -( newRect.width() - (TEXTUREWIDTH - nCurrentX)), 0 );
				curDirection = toLeft;
				nCurrentX = newRect.left() - 1;
			}
			else
				nCurrentX = newRect.right() + 1;
		}
		else
		{
			newRect.move_origin( nCurrentX - newRect.width(), 0 );
			if ( newRect.left() <= 0 )
			{
				newRect.move( (newRect.width() - nCurrentX), 0 );
				curDirection = toRight;
				nCurrentX = newRect.right() + 1;
			}
			else
				nCurrentX = newRect.left() - 1;
		}
		SRectOptimizeStructure tempRct( newRect, it->nNum );
		SliceRectToDown( vTemp, tempRct );
		vTemp.push_back( tempRct );
	}
	rects.clear();
	rects = vTemp;
}
void OptimizeRects( std::vector< SRectOptimizeStructure > &rects )
{
	CFindFillRectHelper hlp;
	hlp = std::for_each( rects.begin(), rects.end(), hlp );
	hlp.dx = GetNextPow2 ( hlp.dx + 1) ;
	int TEXTUREWIDTH = hlp.dx;
	float procent = 0;
	int lastGoodWidth = hlp.dx;
	GRect rcSquareRect( 0, 0, 0, 0 );
	GRect rcCloseSquareRect( 0, 0, 0, 0 );
	GRect rcLastGoodRect( 0, 0, 0, 0 );
	float fLastCloserRespect = 1000.0f;
	for ( ; TEXTUREWIDTH <= MAX_TEXTURE_SIZE; )
	{
		std::vector<SRectOptimizeStructure> vTmp = rects;
		OptimizeRectsByWidth( vTmp, TEXTUREWIDTH );
		int size = 0;
		const GRect rcTempRect = GetFillRect( vTmp );
		if ( rcTempRect.width() == rcTempRect.height() ) 
			rcSquareRect = rcTempRect;
		const float fRespect = Max( 1.0f, float( GetNextPow2(rcTempRect.height()) ) / float( GetNextPow2(rcTempRect.width()) ) );
		if ( fRespect < fLastCloserRespect ) 
		{
			fLastCloserRespect = fRespect;
			rcCloseSquareRect = rcTempRect;
		}
		int maxRectSize = rcTempRect.width() * rcTempRect.height();

		for ( std::vector< SRectOptimizeStructure >::const_iterator it = vTmp.begin(); it != vTmp.end(); ++it )
			size += (*it).rRect.width()*(*it).rRect.height();

		if ( procent < (size / float(maxRectSize + 0.001)) && rcTempRect.height() < MAX_TEXTURE_SIZE )
		{
			procent = size / float( maxRectSize + 0.001);
			lastGoodWidth = TEXTUREWIDTH;
			rcLastGoodRect = rcTempRect;
		}
		TEXTUREWIDTH = GetNextPow2( TEXTUREWIDTH + 1 ) ;
	}
	const int nSqrWidth = GetNextPow2( rcSquareRect.width() );
	const int nSqrHeight = GetNextPow2( rcSquareRect.height() );
	const int nOptWidth = GetNextPow2( rcLastGoodRect.width() );
	const int nOptHeight = GetNextPow2( rcLastGoodRect.height() );
	const int nCloseSqrWidth = GetNextPow2( rcCloseSquareRect.width() );
	const int nCloseSqrHeight = GetNextPow2( rcCloseSquareRect.height() );

	if ( nSqrWidth*nSqrHeight == nOptWidth*nOptHeight ) 
		OptimizeRectsByWidth( rects, nSqrWidth );
	else if ( nCloseSqrWidth*nCloseSqrHeight == nOptWidth*nOptHeight ) 
		OptimizeRectsByWidth( rects, nCloseSqrWidth );
	else
		OptimizeRectsByWidth( rects, nOptWidth );
}
const int nBoundValue = 1000000;
RECT AnalyzeSubrect( const CImageAccessor &image, const RECT &rect )
{
	int minx = nBoundValue, miny = nBoundValue, maxx = -nBoundValue, maxy = -nBoundValue;
	bool bNoYBefore = true;
	for ( int i=rect.top; i <= rect.bottom; ++i )
	{
		bool bEmptyLine = true;
		bool bNoXBefore = true;
		for ( int j=rect.left; j <= rect.right; ++j )
		{
			BYTE a = image[i][j].a;
			if ( image[i][j].a != 0 )
			{
				if ( bNoXBefore )
					minx = Min( minx, j ); 
				bNoXBefore = false;
				maxx = Max( maxx, j );
				bEmptyLine = false;
			}
		}
		if ( !bEmptyLine )
		{
			if ( bNoYBefore )
				miny = i;
			bNoYBefore = false;
			maxy = Max( maxy, i );
		}
	}
	minx = minx ==  nBoundValue ? 0 : minx;
	maxx = maxx == -nBoundValue ? 0 : maxx;
	miny = miny ==  nBoundValue ? 0 : miny;
	maxy = maxy == -nBoundValue ? 0 : maxy;

	RECT ret = { minx, miny, maxx, maxy };
	return ret;
}
IImage* CImageProcessor::ComposeImages( IImage **pImages, RECT *pRects, RECT *pRectsMain, int nNumImages, int nSizeX, int nSizeY ) const
{
	std::vector< SRectOptimizeStructure > vTemp;
	std::vector< SRectOptimizeStructure > vTemp2;

	std::vector<RECT> rectsTemp( nNumImages );
	for ( int i =0 ; i != nNumImages; ++i )
	{
		CImageAccessor image = pImages[i];
		pRects[ i ] = AnalyzeSubrect( image, GRect(0, 0, pImages[ i ]->GetSizeX() - 1, pImages[ i ]->GetSizeY() - 1) );
		pRectsMain[ i ] = GRect( 0, 0, pImages[ i ]->GetSizeX() - 1, pImages[ i ]->GetSizeY() - 1 );		
		if ( (pRects[i].left == pRects[i].right) && (pRects[i].left != 0) )
			pRects[i].right += 1;
		if ( (pRects[i].top == pRects[i].bottom) && (pRects[i].top != 0) )
			pRects[i].bottom += 1;
	}
	memcpy( &rectsTemp[0], pRects ,sizeof(RECT) * nNumImages ) ;

	for ( int i = 0; i != nNumImages; ++i )
		vTemp.push_back( SRectOptimizeStructure( pRects[i], i ) );	

	vTemp2 = vTemp; 
	std::sort(vTemp.begin(), vTemp.end(), CRectSortFunc());
	OptimizeRects( vTemp );
	for ( int i = 0; i != nNumImages; ++i )
		pRects[ vTemp[ i ].nNum ]  = vTemp[ i ].rRect;  

	GRect texRect = GetFillRect( vTemp );
	CImage *pImage = new CImage( texRect.width(), texRect.height() );
	pImage->Set( 0 );
	for ( int i=0; i<nNumImages; ++i )
	{
		int nOriginY = pRects[ i ].top;
		int nOriginX = pRects[ i ].left;
		SColor *pDst = (*pImage)[ nOriginY ] + nOriginX;
		for ( int j = vTemp2[ i ].rRect.top(); j <= vTemp2[ i ].rRect.bottom() ; ++j )
		{
			const SColor *pSrc = pImages[i]->GetLine( j ) + vTemp2[ i ].rRect.left();
			memcpy( pDst, pSrc,  ( vTemp2[ i ].rRect.width() ) * sizeof(SColor) );
			pDst +=  texRect.width();
		}
	}
	for ( int i =0 ; i != nNumImages; ++i )
	{
		pRectsMain[ i ].top = pRects[ i ].top - ( rectsTemp[ i ].top - pRectsMain[ i ].top );
		pRectsMain[ i ].left = pRects[ i ].left - ( rectsTemp[ i ].left - pRectsMain[ i ].left );
		pRectsMain[ i ].bottom = pRects[ i ].bottom + ( pRectsMain[ i ].bottom - rectsTemp[ i ].bottom );
		pRectsMain[ i ].right = pRects[ i ].right - ( pRectsMain[ i ].right - rectsTemp[ i ].right );
	}
	return pImage;
}
