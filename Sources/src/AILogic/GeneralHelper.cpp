#include "StdAfx.h"

#include "GeneralHelper.h"
#include "GeneralInternalInterfaces.h"
#include "CommonUnit.h"
#include "..\Formats\fmtMap.h"
#include "UnitStates.h"
#include "Guns.h"
#include "Formation.h"
#include "Soldier.h"
#include "AIUnit.h"
extern NTimer::STime curTime;
void SGeneralHelper::SSeverityCountPredicate::operator()( const CCommonUnit *pUnit )
{
	fCount += CalcUnitSeverity( pUnit );
}
void SGeneralHelper::SFindBestByEnumeratorPredicate::operator()( class CCommonUnit *pU1 )
{
	if ( pEn->EvaluateWorker( pU1, eType ) )
	{
		const float fNewRating = pEn->EvaluateWorkerRating( pU1, eType );
	
		if ( !pBest || fNewRating > fRating )
		{
			pBest = pU1;
			fRating = fNewRating;
		}
	}
}
bool SGeneralHelper::SDeadPredicate::operator() ( CCommonUnit * pUnit )
{
	return !pUnit || !pUnit->IsValid() || !pUnit->IsAlive();
}
bool SGeneralHelper::SFindByEnumeratorPredicate::operator()( class CCommonUnit *pU1 )
{ 
	return pEn->EvaluateWorker( pU1, eType ); 
}

bool SGeneralHelper::IsUnitNearParcel( const CCommonUnit *pUnit, const struct SAIGeneralParcelInfo &parcel ) 
{
	if ( !pUnit->IsValid() || !pUnit->IsAlive() ) return false;
	const CVec2 vDiff1( ( pUnit->GetCenter() - parcel.vCenter ) );
	return  fabs( vDiff1 ) <=  parcel.fRadius + SConsts::SPY_GLASS_RADIUS / 2;
}
bool SGeneralHelper::IsUnitInParcel( const CCommonUnit *pUnit, const struct SAIGeneralParcelInfo &parcel ) 
{
	if ( !pUnit->IsValid() || !pUnit->IsAlive() ) return false;
	const CVec2 vDiff( pUnit->GetCenter() - parcel.vCenter );
	return fabs( vDiff ) <= parcel.fRadius;
}
float SGeneralHelper::CalcUnitSeverity( const CCommonUnit *pUnit )
{
	if ( !pUnit->IsValid() || !pUnit->IsAlive() ) return 0;
	return pUnit->GetPriceMax();
}
bool SGeneralHelper::RemoveDead( CommonUnits *pUnits )
{
	SGeneralHelper::SDeadPredicate deadPred;
	CommonUnits::iterator firstDead = std::remove_if( pUnits->begin(), pUnits->end(), deadPred );
	const bool bDeleted = firstDead != pUnits->end();
	pUnits->erase( firstDead, pUnits->end() );
	return bDeleted;
}
