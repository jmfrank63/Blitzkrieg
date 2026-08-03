#include "StdAfx.h"

#include "StaticObjectsIters.h"
#include "StaticObject.h"
#include "Mine.h"
CMinesIter::CMinesIter( const CVec2 &vCenter, float fR, const int _nParty, const bool _bAllMines )
: CStObjCircleIter<false>( vCenter, fR ), nParty( _nParty ), bAllMines( _bAllMines )
{
	Reset();
}
void CMinesIter::Iterate()
{
	CStObjCircleIter<false>::Iterate();
	IterateToNextMine();
}
void CMinesIter::IterateToNextMine()
{
	CMineStaticObject *pMineMine = 0;
	for ( CExistingObject *pMine = 0; !CStObjCircleIter<false>::IsFinished(); CStObjCircleIter<false>::Iterate() )
	{
		pMine = CStObjCircleIter<false>::operator*();
		if (	pMine->GetObjectType() == ESOT_MINE  && 
					(pMineMine = static_cast<CMineStaticObject*>( pMine )) &&
					( bAllMines || !pMineMine->IsVisible( nParty ) ) )
		{
			break;
		}
	}
}
class CMineStaticObject* CMinesIter::operator->()
{
	return checked_cast<CMineStaticObject*>(CStObjCircleIter<false>::operator*());
};
class CMineStaticObject* CMinesIter::operator*()
{
	return checked_cast<CMineStaticObject*>(CStObjCircleIter<false>::operator*());
};
bool CMinesIter::IsFinished() const
{
	return CStObjCircleIter<false>::IsFinished();
}
void CMinesIter::Reset() 
{
	CStObjCircleIter<false>::Reset();
	IterateToNextMine();
}
