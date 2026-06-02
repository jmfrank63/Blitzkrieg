#include "stdafx.h"

#include "ArtilleryBulletStorage.h"
BASIC_REGISTER_CLASS( CArtilleryBulletStorage );
CArtilleryBulletStorage::CArtilleryBulletStorage( const SStaticObjectRPGStats * _pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, CAIUnit *_pOwner )
: CGivenPassabilityStObject( center, dbID, fHP, nFrameIndex ), pStats( _pStats ), pOwner( _pOwner )
{
}
void CArtilleryBulletStorage::MoveTo( const CVec2 &newCenter )
{
	SetNewPlaceWithoutMapUpdate( newCenter );
	Init();
}
