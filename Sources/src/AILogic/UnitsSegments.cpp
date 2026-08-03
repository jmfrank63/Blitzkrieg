#include "StdAfx.h"

#include "UnitsSegments.h"
#include "CommonUnit.h"
#include "AIUnit.h"
#include "PathUnit.h"
#include "CollisionInternal.h"

#include "MPLog.h"
extern NTimer::STime curTime;
void CFreezeSegments::SetSegmentObject( CCommonUnit *_pUnit )
{
	pUnit = _pUnit;
}
bool CFreezeSegments::Check()
{ 
	return pUnit && pUnit->IsValid() && ( pUnit->IsAlive() || pUnit->IsTrain() );
}
const NTimer::STime CFreezeSegments::ProcessSegment()
{
	pUnit->FreezeSegment();
	return curTime + Random( 500, 1500 );
}
void CStateSegments::SetSegmentObject( CCommonUnit *_pUnit )
{
	pUnit = _pUnit;
}
bool CStateSegments::Check()
{ 
	bIsValid = pUnit && pUnit->IsValid() && ( pUnit->IsAlive() || pUnit->IsTrain() );
	bCheck = bIsValid && !pUnit->CanBeFrozen();

	return bCheck;
}
const NTimer::STime CStateSegments::ProcessSegment()
{
	pUnit->Segment();
	return pUnit->GetNextSegmTime();
}
bool CStateSegments::ShouldBeUnregistered() const
{
	return bIsValid && !bCheck;
}
void CFirstPathSegments::SetSegmentObject( CAIUnit *_pUnit )
{
	pUnit = _pUnit;
}
bool CFirstPathSegments::Check()
{
	return pUnit && pUnit->IsValid();
}
const NTimer::STime CFirstPathSegments::ProcessSegment()
{
	if ( ( pUnit->IsAlive() || pUnit->GetStats()->IsTrain() ) && pUnit->IsFree() )
		pUnit->FirstSegment();

	return pUnit->GetNextPathSegmTime();
}
void CSecondPathSegments::SetSegmentObject( CAIUnit *_pUnit )
{
	pUnit = _pUnit;
}
bool CSecondPathSegments::Check()
{
	return IsValidObj( pUnit );
}
const NTimer::STime CSecondPathSegments::ProcessSegment()
{
	if ( pUnit->IsFree() )
	{

		pUnit->SecondSegment();

		if ( !pUnit->IsInfantry() && pUnit->GetZ() == 0 && pUnit->GetDir() != pUnit->GetDirAtTheBeginning() )
			pUnit->GetPathUnit()->CheckForDestroyedObjects( pUnit->GetCenter() );
		
		if ( pUnit->IsMoving() || ( pUnit->GetDirAtTheBeginning() != pUnit->GetDir() && pUnit->GetRotateSpeed() != 0 ) )
		{
			pUnit->RemoveCamouflage( ECRR_SELF_MOVE );
			pUnit->Moved();
		}
		else
			pUnit->Stopped();

		pUnit->GetPathUnit()->UnsetDirUpdate();

	}

	return pUnit->GetNextSecondPathSegmTime();
}
void CStayTimeSegments::SetSegmentObject( CAIUnit *_pUnit )
{
	pUnit = _pUnit;
}
bool CStayTimeSegments::Check()
{
	return IsValidObj( pUnit );
}
void CStayTimeSegments::ProcessSegment()
{
	if ( ( pUnit->IsAlive() || pUnit->GetStats()->IsTrain() ) && !pUnit->IsIdle() && !pUnit->GetStats()->IsAviation() )
	{
		CPathUnit *pPathUnit = pUnit->GetPathUnit();
		const NTimer::STime collStayTime = pPathUnit->GetCollStayTime();
		if ( collStayTime != 0 )
		{
			const float fProb = ( 3000.0f + Random( 0.0f, 800.0f ) ) / float( collStayTime );
			if ( Random( 0.0f, 1.0f ) > fProb )
				pPathUnit->SetCollision( new CStopCollision( pPathUnit ) );
		}

		pPathUnit->NullCollStayTime();
		pPathUnit->NullCollisions();
	}
}
