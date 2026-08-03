#include "StdAfx.h"

#include "RP_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int SRoadPoint::INVALID_WIDTH = 0;

int SRoadPoint::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &x );	
	saver.Add( 2, &y );	
	saver.Add( 3, &nDirection );
	saver.Add( 4, &nWidth );
	saver.Add( 5, &nRoadType );

	return 0;
}

int SRoadPoint::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "x", &x );	
	saver.Add( "y", &y );	
	saver.Add( "Direction", &nDirection );
	saver.Add( "nWidth", &nWidth );	
	saver.Add( "RoadType", &nRoadType );

	return 0;
}

bool SRoadPoint::IsRoadPoint( const SRoadPoint &rRoadPoint, int *pnLength ) const
{ 
	if ( HasHorizontalDirection() )
	{
		if( y == rRoadPoint.y )
		{
			if ( pnLength )
			{
				*pnLength = ( ( rRoadPoint.x - x ) * RMGC_SHIFT_POINTS[nDirection - RMGC_HORIZONTAL_TO_ZERO].x );
			}
			return true;
		}
	}
	else
	{
		if( x == rRoadPoint.x )
		{
			if ( pnLength )
			{
				*pnLength = ( ( rRoadPoint.y - y ) * RMGC_SHIFT_POINTS[nDirection - RMGC_HORIZONTAL_TO_ZERO].y );
			}
			return  true;
		}
	}
	return false;
}

int SRoadMakeParameter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nMinMiddleDistance );	
	saver.Add( 2, &lockedRects );

	return 0;
}

int SRoadMakeParameter::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "MinMiddleDistance", &nMinMiddleDistance );	
	saver.Add( "LockedRects", &lockedRects );

	return 0;
}

