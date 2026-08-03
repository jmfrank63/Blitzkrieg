#include "StdAfx.h"

#include "Mine.h"
#include "StaticObjects.h"
#include "Shell.h"
#include "Updater.h"
#include "UnitsIterators2.h"
#include "Diplomacy.h"
#include "AIUnit.h"
#include "Cheats.h"
extern CStaticObjects theStatObjs;
extern CUpdater updater;
extern CDiplomacy theDipl;
extern NTimer::STime curTime;
extern CShellsStore theShellsStore;
extern SCheats theCheats;
BASIC_REGISTER_CLASS( CMineStaticObject );
CMineStaticObject::CMineStaticObject()
{
}
CMineStaticObject::CMineStaticObject( const SMineRPGStats *_pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, int _player ) 
: pStats( _pStats ), bIfWillBeDeleted( false ), bAlive( true ), CGivenPassabilityStObject( center, dbID, fHP, nFrameIndex ), player( _player ) 
{ 
}
void CMineStaticObject::Init()
{
	CGivenPassabilityStObject::Init(); 	
	nextSegmTime = curTime + 4 * SConsts::AI_SEGMENT_DURATION + Random( 0, 3 * SConsts::AI_SEGMENT_DURATION );

	theStatObjs.RegisterSegment( this );
}
void CMineStaticObject::Detonate()
{
	theShellsStore.AddShell
	( new CInvisShell( curTime, new CBurstExpl( 0, pStats->pWeapon, GetCenter(), 0, VNULL2, 0, false, 1 ), 0 ) );

	Delete();
	bAlive = false;
}
bool CMineStaticObject::IsRegisteredInWorld() const 
{
	return bIfRegisteredInCWorld;
}
void CMineStaticObject::SetBeingDisarmed( bool bStartDisarm )
{ 
	bIfWillBeDeleted = bStartDisarm; 
}
void CMineStaticObject::RegisterInWorld()
{
	if ( !IsRegisteredInWorld() )
	{
		bIfRegisteredInCWorld = true;
		updater.Update( ACTION_NOTIFY_NEW_ST_OBJ, this );
	}
}
bool CMineStaticObject::WillExplodeUnder( CAIUnit *pUnit )
{
	return pUnit->GetStats()->fWeight >= pStats->fWeight && // weight is enough
			(!IsVisible( pUnit->GetParty() ) || pUnit->GetStats()->type != RPG_TYPE_ENGINEER );
}
bool CMineStaticObject::CheckToDetonate( CAIUnit *pUnit )
{
	if ( pUnit->GetZ() == 0 )
	{
		bool bMatchTiles = pUnit->GetTile() == AICellsTiles::GetTile( GetCenter() );
		SRect rect( pUnit->GetUnitRect() );
		const bool bGoodUnitToExplode = WillExplodeUnder( pUnit );
		if ( bGoodUnitToExplode && ( bMatchTiles || rect.IsPointInside( GetCenter() ) ) )
		{
			Detonate();
			return true;
		}
	}

	return false;
}
void CMineStaticObject::Segment()
{
	nextSegmTime = curTime + 4 * SConsts::AI_SEGMENT_DURATION + Random( 0, 3 * SConsts::AI_SEGMENT_DURATION );

	CUnitsIter<0,0> it( theDipl.GetNParty( player ), EDI_ENEMY, GetCenter(), 3 * SConsts::TILE_SIZE );
	while ( !it.IsFinished() )
	{
		if ( CheckToDetonate( *it ) )
			return;

		it.Iterate();
	}
}
void CMineStaticObject::Die( const float fDamage )
{
	Detonate();
}
void CMineStaticObject::TakeDamage( const float fDamage, const bool bFromExplosion, const int nPlayerOfShoot, CAIUnit *pShotUnit )
{
	if ( bFromExplosion && fHP > 0 )
	{
		fHP -= fDamage;
		if ( fHP <= 0 || theCheats.GetFirstShoot( theDipl.GetNParty( nPlayerOfShoot ) ) == 1 )
		{
			fHP = 0;
			Detonate();
		}
		else
			updater.Update( ACTION_NOTIFY_RPG_CHANGED, this );
	}
}
void CMineStaticObject::ClearVisibleStatus()
{
	mVisibleStatus = 0;
	bIfRegisteredInCWorld = false; // изначально у ёрика нет мины. 
}
void CMineStaticObject::SetVisible( int nParty, bool bVis ) 
{ 
	if ( nParty != theDipl.GetNeutralParty() )
	{
		mVisibleStatus = ( mVisibleStatus & ~( 1UL << nParty ) ) | ( DWORD(bVis) << nParty );

		if ( theDipl.GetDiplStatusForParties( nParty, theDipl.GetMyParty() ) == EDI_FRIEND ) 
			RegisterInWorld();
	}
}
const bool CMineStaticObject::IsVisible( const BYTE nParty ) const
{ 
	return mVisibleStatus & ( 1 << nParty ); 
}
