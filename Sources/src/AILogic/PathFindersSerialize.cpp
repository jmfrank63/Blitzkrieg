#include "StdAfx.h"

#include "PathFinderInternal.h"
#include "TrainPathFinder.h"
// A path finder keeps nothing between searches but the marks in mapBuf, which every search
// takes off again: a load starts from none, as a fresh process does.
int CStandartPathFinder::operator&( IStructureSaver &ss )
{
	if ( ss.IsReading() )
		memset( *mapBuf, 0, sizeof( mapBuf ) );
	return 0;
}
int CPlanePathFinder::operator&( IStructureSaver &ss )
{
	return 0;
}
int CTrainPathFinder::operator&( IStructureSaver &ss )
{
	return 0;
}
