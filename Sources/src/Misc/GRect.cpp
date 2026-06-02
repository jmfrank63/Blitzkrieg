#include "StdAfx.h"

void GRect::set( GPoint p0, GPoint p1 )
{
  if( p0.x > p1.x )
  {
    GSPos t = p0.x;  p0.x = p1.x;  p1.x = t;
  }
  if( p0.y > p1.y )
  {
    GSPos t = p0.y;  p0.y = p1.y;  p1.y = t;
  }
  set( p0.x, p0.y, p1.x, p1.y );
}


void GRect::left ( GSPos n )
{
  GSPos r  = right();
  origin.x = n;
  size.x   = (GSPos)(r - n + 1);
}

void GRect::top ( GSPos n )
{
  GSPos b  = bottom();
  origin.y = n;
  size.y   = (GSPos)(b - n + 1);
}


void GRect::center( const GPoint& new_center )
{
  origin += new_center - center();
}


bool GRect::contains( GSPos x, GSPos y ) const
{
  return origin.x <= x && x < origin.x + size.x &&
         origin.y <= y && y < origin.y + size.y;
}

bool GRect::contains( const GRect& r ) const
{
  return left()   <= r.left()  &&
         top()    <= r.top()   &&
         right()  >= r.right() &&
         bottom() >= r.bottom();
}


void GRect::grow( GSPos x, GSPos y )
{
  origin.x -=   x;
  origin.y -=   y;
  size.x   += (GSPos)(2*x);
  size.y   += (GSPos)(2*y);
}

void GRect::intersect( const GRect& r )
{
  GPoint B  =   right_bottom();
  GPoint rb = r.right_bottom();

  origin.x = Max( origin.x, r.origin.x );
  origin.y = Max( origin.y, r.origin.y );
  B.x      = Min( B.x, rb.x );
  B.y      = Min( B.y, rb.y );

  right_bottom( B );
}

void GRect::Union( const GRect& r )
{
  if( isEmpty() )
    *this = r;
  else if( !r.isEmpty() )
  {
    GPoint B  =   right_bottom();
    GPoint rb = r.right_bottom();

    origin.x = Min( origin.x, r.origin.x );
    origin.y = Min( origin.y, r.origin.y );
    B.x      = Max( B.x, rb.x );
    B.y      = Max( B.y, rb.y );

    right_bottom( B );
  }
}

void GRect::split_vertically  ( GSPos shift_from_left, GRect& l, GRect& r  ) const
{
  l = *this;
  r = *this;
  shift_from_left += left();
  l.right( shift_from_left   );
  r.left ( (GSPos)(shift_from_left+1) );
}

void GRect::split_horizontally( GSPos shift_from_top,  GRect& t,  GRect& b ) const
{
  t = *this;
  b = *this;
  shift_from_top += top();
  t.bottom( shift_from_top   );
  b.top   ( (GSPos)(shift_from_top+1) );
}


/*  An implementation of the Sutherland-Cohen clipping algorithm. */

union OutcodeUnion      /* outcodes are represented as bit fields */
{
  struct
  {
    unsigned code0 : 1;         /* x < Xul */
    unsigned code1 : 1;         /* y < Yul */
    unsigned code2 : 1;         /* x > Xlr */
    unsigned code3 : 1;         /* y > Ylr */
  }
    ocs;

  unsigned outcodes;
};

#define Swap(type,a,b) { type t=a; a=b; b=t; }

#define SetOutcodes( u, x, y ) \
{                              \
  u.outcodes  = 0;             \
  u.ocs.code0 = ((x) < XUL);   \
  u.ocs.code1 = ((y) < YUL);   \
  u.ocs.code2 = ((x) > XLR);   \
  u.ocs.code3 = ((y) > YLR);   \
}

bool GRect::clip_line( GSPos& X1, GSPos& Y1, GSPos& X2, GSPos& Y2 ) const
{
  const GSPos XUL = left(),
              YUL = top(),
              XLR = right(),
              YLR = bottom();
  union OutcodeUnion  ocu1, ocu2;

  /* initialize 4-bit codes */

  SetOutcodes( ocu1, X1, Y1 );        /* initial 4-bit codes */
  SetOutcodes( ocu2, X2, Y2 );

  bool Inside  = (ocu1.outcodes | ocu2.outcodes) == 0;
  bool Outside = (ocu1.outcodes & ocu2.outcodes) != 0;

  while( !Outside && !Inside )
  {
    if( ocu1.outcodes==0 )         /* swap endpoints if necessary so */
    {                              /* that (x1,y1) needs to be clipped */
      Swap( GSPos, X1, X2 );
      Swap( GSPos, Y1, Y2 );
      Swap( unsigned, ocu1.outcodes, ocu2.outcodes );
    }


    if( ocu1.ocs.code0 )                   /* clip left */
    {
      Y1 += GSPos((long)(Y2-Y1)*(XUL-X1)/(X2-X1));
      X1 = XUL;
    }
    else if( ocu1.ocs.code1 )              /* clip above */
    {
      X1 += GSPos((long)(X2-X1)*(YUL-Y1)/(Y2-Y1));
      Y1 = YUL;
    }
    else if( ocu1.ocs.code2 )              /* clip right */
    {
      Y1 += GSPos((long)(Y2-Y1)*(XLR-X1)/(X2-X1));
      X1 = XLR;
    }
    else if( ocu1.ocs.code3 )              /* clip below */
    {
      X1 += GSPos((long)(X2-X1)*(YLR-Y1)/(Y2-Y1));
      Y1 = YLR;
    }

    SetOutcodes( ocu1, X1, Y1 );            /* update for (x1,y1) */

    Inside  = (ocu1.outcodes | ocu2.outcodes) == 0; // update
    Outside = (ocu1.outcodes & ocu2.outcodes) != 0; //  4-bit codes
  }

  return Inside;
}

#undef Swap
#undef SetOutCodes

bool GRect::clip_hline( GSPos& x0, GSPos y, GSPos& x1 ) const
{
  const GSPos ax = left(),
              bx = right();

  if( y  < top() || y  > bottom() || x0 > bx || x1 < ax )
    return false;
  if( x0 <= ax )  x0 = ax;
  if( x1 >= bx )  x1 = bx;
  return true;
}

bool GRect::clip_vline( GSPos x0, GSPos& y0, GSPos& y1 ) const
{
  const GSPos ay = top(),
              by = bottom();

  if( x0 < left() || x0 > right() || y0 > by || y1 < ay )
    return false;
  if( y0 <= ay )  y0 = ay;
  if( y1 >= by )  y1 = by;
  return true;
}




GRect::GRect( const RECT& r )
  : origin( (GSPos)r.left, (GSPos)r.top ),
    size  ( (GSPos)(r.right  - r.left + 1), (GSPos)(r.bottom - r.top + 1) )
{
}

GRect::operator RECT() const
{
  RECT r;
  r.left   = origin.x;
  r.top    = origin.y;
  r.right  = origin.x + size.x - 1;
  r.bottom = origin.y + size.y - 1;
  return r;
}
