#include "StdAfx.h"

#include "CellsConglomerateContainer.h"
template < class TEnumerator>
		void EnumConglomerates( const int nMaxRank, const SIntPair &vCenter, const int nRadius, TEnumerator enumerator )
{
	const int nMinX = Clamp( vCenter.x - nRadius, 0, enumerator.GetSizeX() );
	const int nMaxX = Clamp( vCenter.x + nRadius, 0, enumerator.GetSizeX() );
	
	const int nMinY =	Clamp( vCenter.y - nRadius, 0, enumerator.GetSizeY() );
	const int nMaxY = Clamp( vCenter.y + nRadius, 0, enumerator.GetSizeY() );

	if ( nMinX == nMaxX || nMinY == nMaxY ) return ;		// nothing to do
	
	const int nAreaUnderSound = ( nMaxX - nMinX ) * (  nMaxY - nMinY );
	int nAreaSoFar = 0;

	int nCurXMin, nCurYMin, nCurXMax, nCurYMax;
		
	int nCurRank = nMaxRank;
	for ( ; nCurRank >= 0 && nAreaSoFar < nAreaUnderSound; --nCurRank )
	{
		const int nConglomerateSize = 1 << nCurRank;
	
		nCurXMin = nMinX / nConglomerateSize + (nMinX % nConglomerateSize == 0 ? 0 : 1);
		nCurYMin = nMinY / nConglomerateSize + (nMinY % nConglomerateSize == 0 ? 0 : 1);
		nCurXMax = (nMaxX / nConglomerateSize );
		nCurYMax = (nMaxY / nConglomerateSize );
			
		if ( nCurXMin < nCurXMax && nCurYMin < nCurYMax )
		{
			const int nConglomerateArea = nConglomerateSize * nConglomerateSize;
			for ( int nX = nCurXMin; nX < nCurXMax; ++nX )
			{
				for ( int nY = nCurYMin; nY < nCurYMax; ++nY )
				{
					enumerator( nCurRank, nX, nY );
					nAreaSoFar += nConglomerateArea;
				}
			}
			--nCurRank;
			break;
		}
	}

	nCurXMin *= 2;
	nCurYMin *= 2;
	nCurXMax *= 2;
	nCurYMax *= 2;
	
	for ( ; nCurRank >= 0 && nAreaSoFar < nAreaUnderSound; --nCurRank )
	{
		const int nConglomerateSize = 1 << nCurRank;
		const int nConglomerateArea = nConglomerateSize * nConglomerateSize;
		
		
		const int nRankMinX = nMinX / nConglomerateSize + (nMinX % nConglomerateSize == 0 ? 0 : 1);
		const int nRankMinY = nMinY / nConglomerateSize + (nMinY % nConglomerateSize == 0 ? 0 : 1);
		const int nRankMaxX = (nMaxX / nConglomerateSize );
		const int nRankMaxY = (nMaxY / nConglomerateSize );

		const bool bTopExists = nRankMinY < nCurYMin;
		if ( bTopExists )
		{
			for ( int nX = nRankMinX; nX < nRankMaxX; ++nX )
			{
				nAreaSoFar += nConglomerateArea;
				enumerator( nCurRank, nX, nRankMinY );
			}
		}

		const bool bBottomExists = nRankMaxY > nCurYMax;
		if ( bBottomExists )
		{
			for ( int nX = nRankMinX; nX < nRankMaxX; ++nX )
			{
				nAreaSoFar += nConglomerateArea;
				enumerator( nCurRank, nX, nCurYMax );
			}
		}

		const bool bLeftExists = nRankMinX < nCurXMin;
		if ( bLeftExists )					// left column exists
		{
			for ( int nY = nRankMinY + bTopExists; nY < nRankMaxY - bBottomExists; ++nY )
			{
				nAreaSoFar += nConglomerateArea;
				enumerator( nCurRank, nRankMinX, nY );
			}
		}
		
		const bool bRightExists = nRankMaxX > nCurXMax;					// rigth column exists
		if ( bRightExists )
		{
			for ( int nY = nRankMinY + bTopExists; nY < nRankMaxY - bBottomExists; ++nY )
			{
				nAreaSoFar += nConglomerateArea;
				enumerator( nCurRank, nCurXMax, nY );
			}
		}
		nCurXMin = nRankMinX * 2;
		nCurYMin = nRankMinY * 2;

		nCurXMax = ( bRightExists ? nRankMaxX : nCurXMax ) * 2;
		nCurYMax = ( bBottomExists ? nRankMaxY : nCurYMax ) * 2;
	}
}
const int CCellsConglomerateContainer::MAX_RANK = 4;
void CCellsConglomerateContainer::Clear() 
{ 
	bInitted = false; 
	conglomerates.clear(); 
}
void CCellsConglomerateContainer::Init( const int nMaxX, const int nMaxY )
{
	bInitted = true;
	nMaxRank = MAX_RANK;
	conglomerates.reserve( MAX_RANK + 1 );
	
	for ( int i = 0; i <= MAX_RANK; ++i )
	{
		const int nConglomerateSize = 1<<i;
		if ( 0 == nMaxX / nConglomerateSize || 0 == nMaxY / nConglomerateSize ) // cannot make rank  this high.
		{
			nMaxRank = i - 1;
			break;
		}
		conglomerates.resize( i + 1 );
		conglomerates[i].SetSizes( nMaxX / nConglomerateSize, nMaxY / nConglomerateSize );
	}
}
void CCellsConglomerateContainer::AddHearCell( const SIntPair &vCenter, const int nRadius )
{
	if ( nRadius )
	{
		SAddCellEnumerator en( vCenter, conglomerates );
		EnumConglomerates( nMaxRank, vCenter, nRadius, en );
	}
}
void CCellsConglomerateContainer::RemoveHearCell( const SIntPair &vCenter, const int nRadius )
{
	if ( nRadius )
	{
		SRemoveCellEnumerator en( vCenter, conglomerates );
		EnumConglomerates( nMaxRank, vCenter, nRadius, en );
	}
}