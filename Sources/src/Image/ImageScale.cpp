#include "StdAfx.h"

#include "ImageReal.h"

#include <stdio.h>
#include <math.h>
static const int WHITE_PIXEL = 255;
static const int BLACK_PIXEL = 0;
#pragma pack( 1 )
struct SARGB
{
	BYTE a, r, g, b;
};
#pragma pack()
struct CONTRIB
{
  int pixel;
  double weight;
	CONTRIB() 
		: pixel( 0 ), weight( 0 ) {  }
};
struct CLIST
{
  int n;         /* number of contributors */
  CONTRIB *p;    /* pointer to list of contributions */
	CLIST()
		: n( 0 ), p( 0 ) {  }
};
inline SARGB get_pixel( const CImage *pImg, int x, int y )
{
	return bit_cast<SARGB>( pImg->Get( x, y ) );
}

inline void put_pixel( CImage *pImg, int x, int y, SARGB pixel )
{
	pImg->Set( x, y, bit_cast<DWORD>( pixel ) );
}

inline void get_row( SARGB *row, const CImage *pImg, int y )
{
  if ( (y < 0) || (y >= pImg->GetSizeY()) )
		return;
  memcpy( row, (*pImg)[y], pImg->GetSizeX() * sizeof(SARGB) );
}

inline void get_column(SARGB *column, const CImage *pImg, int x)
{
  if ( (x < 0) || (x >= pImg->GetSizeX()) )
		return;
  for ( int i=0; i<pImg->GetSizeY(); ++i ) 
    column[i] = bit_cast<SARGB>( pImg->Get( x, i ) );
}
static const double point_support = 0.0;

double point_filter( double t )
{
	if ( t == 0.0 )
		return 1.0;
	else
		return 0.0;
}

static const double filter_support = 1.0;

double filter( double t )
{
	/* f(t) = 2|t|^3 - 3|t|^2 + 1, -1 <= t <= 1 */
	if ( t < 0.0 ) 
		t = -t;
	if ( t < 1.0 ) 
		return ( (2.0*t - 3.0)*t*t + 1.0 );
	else
		return 0.0;
}
static const double box_support = 0.5;

double box_filter( double t )
{
	if ( (t > -0.5) && (t <= 0.5) ) 
		return 1.0;
	else
		return 0.0;
}
static const double triangle_support = 1.0;

double triangle_filter( double t )
{
	if ( t < 0.0 ) 
		t = -t;
	if ( t < 1.0 ) 
		return (1.0 - t);
	else
		return 0.0;
}
static const double bell_support = 1.5;

double bell_filter( double t )      /* box (*) box (*) box */
{
	if ( t < 0 ) 
		t = -t;
	if ( t < 0.5 ) 
		return ( 0.75 - (t*t) );
	else if ( t < 1.5 ) 
	{
		t -= 1.5;
		return (0.5*t*t);
	}
	else
		return 0.0;
}
static const double B_spline_support = 2.0;

double B_spline_filter( double t )  /* box (*) box (*) box (*) box */
{
	double tt;

	if ( t < 0 ) 
		t = -t;
	if ( t < 1 ) 
	{
		tt = t*t;
		return ( (0.5*tt*t) - tt + (2.0/3.0) );
	} 
	else if ( t < 2 ) 
	{
		t = 2 - t;
		return ( (1.0/6.0)*(t*t*t) );
	}
	else
		return 0.0;
}
double sinc( double x )
{
	x *= PI;
	if ( x != 0 ) 
		return sin(x) / x;
	else
		return 1.0;
}

static const double Lanczos3_support = 3.0;

double Lanczos3_filter( double t )
{
	if ( t < 0 ) 
		t = -t;
	if ( t < 3.0 ) 
		return sinc(t) * sinc(t/3.0);
	else
		return 0.0;
}
static const double Mitchell_support = 2.0;

static const double B = (1.0/3.0);
static const double C	= (1.0/3.0);

double Mitchell_filter( double t )
{
	double tt = t * t;

	if ( t < 0 ) 
		t = -t;
	if ( t < 1.0 ) 
	{
		t = ( (12.0 - 9.0*B - 6.0*C) * (t*tt) ) + ( (-18.0 + 12.0*B + 6.0*C) * tt ) + (6.0 - 2.0*B);
		return (t / 6.0);
	} 
	else if ( t < 2.0 ) 
	{
		t = ( (-1.0*B - 6.0*C) * (t*tt) ) + ( (6.0*B + 30.0*C) * tt ) + ( (-12.0*B - 48.0*C) * t ) + ( 8.0*B + 24*C );
		return (t / 6.0);
	}
	else
		return 0.0;
}

typedef double (*FILTERFUNC)( double t );
void ImageScale( const CImage *pSrcImg, CImage *pDstImg, EImageScaleMethod method )
{
	FILTERFUNC pfnFilterFunc = Lanczos3_filter;
	double filterwidth = Lanczos3_support;
	switch ( method )
	{
		case ISM_FILTER:
			pfnFilterFunc = filter, filterwidth = filter_support;
			break;
		case ISM_BOX:
			pfnFilterFunc = box_filter, filterwidth = box_support;
			break;
		case ISM_TRIANGLE:
			pfnFilterFunc = triangle_filter, filterwidth = triangle_support;
			break;
		case ISM_BELL:
			pfnFilterFunc = bell_filter, filterwidth = bell_support;
			break;
		case ISM_BSPLINE:
			pfnFilterFunc = B_spline_filter, filterwidth = B_spline_support;
			break;
		case ISM_LANCZOS3:
			pfnFilterFunc = Lanczos3_filter, filterwidth = Lanczos3_support;
			break;
		case ISM_MITCHELL:
			pfnFilterFunc = Mitchell_filter, filterwidth = Mitchell_support;
			break;
	}
	CImage *pTmpImg = new CImage( pDstImg->GetSizeX(), pSrcImg->GetSizeY() );
  double xscale = double( pDstImg->GetSizeX() ) / double( pSrcImg->GetSizeX() );
  double yscale = double( pDstImg->GetSizeY() ) / double( pSrcImg->GetSizeY() );
  CLIST *contrib = new CLIST[pDstImg->GetSizeX()];
  if ( contrib != 0 ) 
	{
    if ( xscale < 1.0 ) 
		{
      double width = filterwidth / xscale;
      double fscale = 1.0 / xscale;
      for ( int i=0; i<pDstImg->GetSizeX(); ++i ) 
			{
        contrib[i].n = 0;
        contrib[i].p = new CONTRIB[int(width*2 + 1)];
        double center = (double( i ) + 0.5) / xscale - 0.5; // shift center to (1/2 axis - 0.5 = 0.5/scale - 0.5) to reach an actual pixel center
        double left = ceil( center - width );
        double right = floor( center + width );
        for ( int j=left; j<=right; ++j ) 
				{
          double weight = center - double( j );
          weight = (*pfnFilterFunc)( weight / fscale ) / fscale;
					int n = j;
          if ( j < 0 ) 
	          n = -j;
					else if ( j >= pSrcImg->GetSizeX() ) 
            n = (pSrcImg->GetSizeX() - j) + pSrcImg->GetSizeX() - 1;
          int k = contrib[i].n++;
          contrib[i].p[k].pixel = n;
          contrib[i].p[k].weight = weight;
        }
      }
    } 
		else 
		{
      for ( int i=0; i<pDstImg->GetSizeX(); ++i ) 
			{
        contrib[i].n = 0;
        contrib[i].p = new CONTRIB[int(filterwidth*2 + 1)];
        double center = (double( i ) + 0.5) / xscale - 0.5; // shift center to (1/2 axis - 0.5 = 0.5/scale - 0.5) to reach an actual pixel center
        double left = ceil( center - filterwidth );
        double right = floor( center + filterwidth );
        for ( int j=left; j<=right; ++j ) 
				{
          double weight = center - double( j );
          weight = (*pfnFilterFunc)( weight );
					int n = j;
          if ( j < 0 ) 
            n = -j;
					else if ( j >= pSrcImg->GetSizeX() ) 
            n = (pSrcImg->GetSizeX() - j) + pSrcImg->GetSizeX() - 1;
          int k = contrib[i].n++;
          contrib[i].p[k].pixel = n;
          contrib[i].p[k].weight = weight;
        }
      }
    }

    SARGB *raster = new SARGB[pSrcImg->GetSizeX()];
    if ( raster != 0 ) 
		{
      for ( int k=0; k<pTmpImg->GetSizeY(); ++k ) 
			{
        get_row( raster, pSrcImg, k );
        for ( int i=0; i<pTmpImg->GetSizeX(); ++i ) 
				{
					double a = 0, r = 0, g = 0, b = 0;
          for ( int j=0; j<contrib[i].n; ++j ) 
					{
            a += raster[contrib[i].p[j].pixel].a * contrib[i].p[j].weight;
            r += raster[contrib[i].p[j].pixel].r * contrib[i].p[j].weight;
            g += raster[contrib[i].p[j].pixel].g * contrib[i].p[j].weight;
            b += raster[contrib[i].p[j].pixel].b * contrib[i].p[j].weight;
          }
          SARGB pix = {	(BYTE)Clamp( int(a + 0.5), BLACK_PIXEL, WHITE_PIXEL ),
												(BYTE)Clamp( int(r + 0.5), BLACK_PIXEL, WHITE_PIXEL ),
												(BYTE)Clamp( int(g + 0.5), BLACK_PIXEL, WHITE_PIXEL ),
												(BYTE)Clamp( int(b + 0.5), BLACK_PIXEL, WHITE_PIXEL )	};
          put_pixel( pTmpImg, i, k, pix );
        }
      }
      delete []raster;
    }

    for ( int i=0; i<pTmpImg->GetSizeX(); ++i ) 
      delete [](contrib[i].p);
    delete []contrib;
  }

  contrib = new CLIST[pDstImg->GetSizeY()];
  if ( contrib != 0 ) 
	{
    if ( yscale < 1.0 ) 
		{
      double width = filterwidth / yscale;
      double fscale = 1.0 / yscale;
      for ( int i=0; i<pDstImg->GetSizeY(); ++i ) 
			{
        contrib[i].n = 0;
        contrib[i].p = new CONTRIB[int(width*2 + 1)];
        double center = (double( i ) + 0.5) / yscale - 0.5; // shift center to (1/2 axis - 0.5 = 0.5/scale - 0.5) to reach an actual pixel center
        double left = ceil( center - width );
        double right = floor( center + width );
        for ( int j=left; j<=right; ++j ) 
				{
          double weight = center - double( j );
          weight = (*pfnFilterFunc)( weight / fscale ) / fscale;
					int n = j;
          if ( j < 0 )
            n = -j;
          else if ( j >= pTmpImg->GetSizeY() )
            n = (pTmpImg->GetSizeY() - j) + pTmpImg->GetSizeY() - 1;
          int k = contrib[i].n++;
          contrib[i].p[k].pixel = n;
          contrib[i].p[k].weight = weight;
        }
      }
    } 
		else 
		{
      for ( int i=0; i<pDstImg->GetSizeY(); ++i ) 
			{
        contrib[i].n = 0;
        contrib[i].p = new CONTRIB[int(filterwidth*2 + 1)];
        double center = (double( i ) + 0.5) / yscale - 0.5; // shift center to (1/2 axis - 0.5 = 0.5/scale - 0.5) to reach an actual pixel center
        double left = ceil( center - filterwidth );
        double right = floor( center + filterwidth );
        for ( int j=left; j<=right; ++j ) 
				{
          double weight = center - double( j );
          weight = (*pfnFilterFunc)( weight );
					int n = j;
          if ( j < 0 )
            n = -j;
          else if ( j >= pTmpImg->GetSizeY() )
            n = (pTmpImg->GetSizeY() - j) + pTmpImg->GetSizeY() - 1;
          int k = contrib[i].n++;
          contrib[i].p[k].pixel = n;
          contrib[i].p[k].weight = weight;
        }
      }
    }

    SARGB *raster = new SARGB[pTmpImg->GetSizeY()];
    if ( raster != 0 ) 
		{
      for ( int k=0; k<pDstImg->GetSizeX(); ++k ) 
			{
        get_column( raster, pTmpImg, k );
        for ( int i=0; i<pDstImg->GetSizeY(); ++i ) 
				{
					double a = 0, r = 0, g = 0, b = 0;
          for ( int j=0; j<contrib[i].n; ++j ) 
					{
            a += raster[contrib[i].p[j].pixel].a * contrib[i].p[j].weight;
            r += raster[contrib[i].p[j].pixel].r * contrib[i].p[j].weight;
            g += raster[contrib[i].p[j].pixel].g * contrib[i].p[j].weight;
            b += raster[contrib[i].p[j].pixel].b * contrib[i].p[j].weight;
          }
          SARGB pix = {	(BYTE)Clamp( int(a + 0.5), BLACK_PIXEL, WHITE_PIXEL ),
												(BYTE)Clamp( int(r + 0.5), BLACK_PIXEL, WHITE_PIXEL ),
												(BYTE)Clamp( int(g + 0.5), BLACK_PIXEL, WHITE_PIXEL ),
												(BYTE)Clamp( int(b + 0.5), BLACK_PIXEL, WHITE_PIXEL )	};
          put_pixel( pDstImg, k, i, pix );
        }
      }
    }
    delete []raster;

    for ( int i=0; i<pDstImg->GetSizeY(); ++i ) 
      delete [](contrib[i].p);
    delete []contrib;
  }
	delete pTmpImg;
}
