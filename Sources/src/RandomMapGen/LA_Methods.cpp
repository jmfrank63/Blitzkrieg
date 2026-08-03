#include "StdAfx.h"

#include "LA_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const BYTE RMGC_UNLOCKED							= 0;
const BYTE RMGC_LOCKED								= 1;

const BYTE RMGC_START_POINT						= 2;
const BYTE RMGC_FINISH_POINT					= 3;

const BYTE RMGC_INVALID_DIRECTION			= 4;
const BYTE RMGC_HORIZONTAL_TO_ZERO		= 5;
const BYTE RMGC_HORIZONTAL_FROM_ZERO	= 6;
const BYTE RMGC_VERTICAL_TO_ZERO			= 7;
const BYTE RMGC_VERTICAL_FROM_ZERO		= 8;

extern const CTPoint<int> RMGC_SHIFT_POINTS[4] = { CTPoint<int>( -1,  0 ),
																									 CTPoint<int>(  1,  0 ),
																									 CTPoint<int>(  0, -1 ),
																									 CTPoint<int>(  0,  1 ), };

extern const BYTE RMGC_NEGATIVE_DIRECTIONS[4] = { RMGC_HORIZONTAL_FROM_ZERO,
																									RMGC_HORIZONTAL_TO_ZERO,
																									RMGC_VERTICAL_FROM_ZERO,
																									RMGC_VERTICAL_TO_ZERO, };

int FindPath( const CTPoint<int> &rStartPoint,
						  const CTPoint<int> &rFinishPoint,
						  CArray2D<BYTE> *pLockArray,
						  std::vector<CTPoint<int> > *pPointList )
{
	NI_ASSERT_T( pLockArray != 0,
							 NStr::Format( "Wrong parameter: %x\n", pLockArray ) );
	NI_ASSERT_T( pPointList != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPointList ) );
	NI_ASSERT_T( IsValidIndices( (*pLockArray), rStartPoint ) &&
		           IsValidIndices( (*pLockArray), rFinishPoint ),
							 NStr::Format( "Invalid parameters!" ) );

	int nPointCount = 0;
	if ( rStartPoint == rFinishPoint )
	{
		pPointList->push_back( rFinishPoint );
		 ++nPointCount;
		return nPointCount;
	}


	(*pLockArray)[rStartPoint.y][rStartPoint.x] = RMGC_FINISH_POINT;
	(*pLockArray)[rFinishPoint.y][rFinishPoint.x] = RMGC_START_POINT;
	
	std::list<CTPoint<int> > pointList;
	pointList.push_back( rFinishPoint );
	CTPoint<int> pointToAdd;
	bool isFinished = false;
	while ( !pointList.empty() )
	{
		CTPoint<int> pointToRemove = pointList.front();
		pointList.pop_front();

		for ( BYTE nDirection = 0; nDirection < 4; ++nDirection )
		{
			pointToAdd = pointToRemove - RMGC_SHIFT_POINTS[nDirection];
			if ( IsValidIndices( (*pLockArray), pointToAdd ) )
			{
				if ( (*pLockArray)[pointToAdd.y][pointToAdd.x] == RMGC_FINISH_POINT )
				{
					(*pLockArray)[pointToAdd.y][pointToAdd.x] = RMGC_HORIZONTAL_TO_ZERO + nDirection;
					isFinished = true;
					break;
				}
				if ( (*pLockArray)[pointToAdd.y][pointToAdd.x] == RMGC_UNLOCKED )
				{
					(*pLockArray)[pointToAdd.y][pointToAdd.x] = RMGC_HORIZONTAL_TO_ZERO + nDirection;
					pointList.push_back( pointToAdd );
				}
			}
		}
		if ( isFinished )
		{
			break;
		}
	}
	
	if ( isFinished )
	{
		pPointList->push_back( pointToAdd );
		while ( (*pLockArray)[pointToAdd.y][pointToAdd.x] != RMGC_START_POINT )
		{
			NI_ASSERT_TF( ( (*pLockArray)[pointToAdd.y][pointToAdd.x] >= RMGC_HORIZONTAL_TO_ZERO ) &&
				            ( (*pLockArray)[pointToAdd.y][pointToAdd.x] <= RMGC_VERTICAL_FROM_ZERO ),
										"Wrong algorithm!",
										return 0 );
			CTPoint<int> point = pointToAdd + RMGC_SHIFT_POINTS[ (*pLockArray)[pointToAdd.y][pointToAdd.x] -  RMGC_HORIZONTAL_TO_ZERO];
			if ( (*pLockArray)[point.y][point.x] != (*pLockArray)[pointToAdd.y][pointToAdd.x] )
			{
				pPointList->push_back( point );
				++nPointCount;
			}
			pointToAdd = point;
		}
	}
	
	return nPointCount;
}
