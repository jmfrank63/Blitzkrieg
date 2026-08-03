#include "StdAfx.h"

#include "PointChecking.h"
#include "StaticObject.h"
bool CAttackPointChecking::IsGoodTile( const SVector &curTile ) const
{
	const float fDist = SquareOfDistance( curTile, targetTile );

	return 
		fDist <= sqr( long( fRangeMax / SConsts::TILE_SIZE ) ) &&
		fDist >= sqr( long( fRangeMin / SConsts::TILE_SIZE ) );
}
bool CGoToDistance::IsGoodTile( const SVector &curTile ) const
{
	return SquareOfDistance( curTile, targetTile ) <= tileDistance2;
}
bool CAttackSideChecking::IsGoodTile( const SVector &curTile ) const
{
	const WORD wCurDir = GetDirectionByVector( (curTile - targetTile).ToCVec2() );
	const float fDist = SquareOfDistance( curTile, targetTile );

	return
		DirsDifference( wCurDir, wAttackDir ) <= wHalfAngle &&
		fDist <= sqr( long( fRangeMax / SConsts::TILE_SIZE ) ) &&
		fDist >= sqr( long( fRangeMin / SConsts::TILE_SIZE ) );
}
CAttackStObjectChecking::CAttackStObjectChecking( const float _fRangeMin, const float _fRangeMax, CStaticObject *pObj, const CVec2 vAttackingUnitCenter )
: fRangeMin( _fRangeMin ), fRangeMax( _fRangeMax ), targetTile( AICellsTiles::GetTile( pObj->GetAttackCenter( vAttackingUnitCenter ) ) )
{
}
bool CAttackStObjectChecking::IsGoodTile( const SVector &curTile ) const
{
	const float fDist = SquareOfDistance( curTile, targetTile );

	return 
		fDist <= sqr( long( fRangeMax / SConsts::TILE_SIZE ) ) &&
		fDist >= sqr( long( fRangeMin / SConsts::TILE_SIZE ) );
}
