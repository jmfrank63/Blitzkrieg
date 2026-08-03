#include "StdAfx.h"

#include "Flag.h"
#include "StaticObjects.h"
#include "UnitsIterators2.h"
#include "AIUnit.h"
#include "MultiplayerInfo.h"
#include "Diplomacy.h"
#include "Scripts/scripts.h"
#include "Updater.h"
#include "MPLog.h"
extern CStaticObjects theStatObjs;
extern NTimer::STime curTime;
extern CScripts* pScripts;
extern CMultiplayerInfo theMPInfo;
extern CDiplomacy theDipl;
extern CUpdater updater;
CFlag::CFlag( const SStaticObjectRPGStats *_pStats, const CVec2 &center, const int dbID, const float fHP, const int nFrameIndex, const int _nPlayer, const EStaticObjType eType )
: CCommonStaticObject( center, dbID, fHP, nFrameIndex, eType ), 
	pStats( _pStats ), nParty( theDipl.GetNParty( _nPlayer ) ),
	bGoingToCapture( false ), nPartyToCapture( -1 ), nPlayerToCapture( -1 ), 
	timeOfStartCapturing( 0 ), lastSegmentTime( 0 ), nextSegmentTime( 0 ), bCapturingByNeutral( false ),
	timeOfStartNeutralPartyCapturing( 0 )
{
	theMPInfo.AddFlagAtTheMap();
}
void CFlag::Init()
{
	CCommonStaticObject::Init();
	
	lastSegmentTime = 0;
	nextSegmentTime = 0;
	theStatObjs.RegisterSegment( this );
}
void CFlag::Segment()
{
	if ( lastSegmentTime != 0 )
		theMPInfo.AddFlagPoints( nParty, SConsts::FLAG_POINTS_SPEED * float( curTime - lastSegmentTime ) / 1000 );
	
	nextSegmentTime = curTime;
	lastSegmentTime = curTime;

	bool bOnlyEnemyUnitsOfOneTypeInZone = false;
	bool bIsThereOurUnits = nParty == theDipl.GetNeutralParty();
	int nEnemyParty = theDipl.GetNeutralParty();
	int nEnemyPlayer = theDipl.GetNeutralPlayer();
	for ( CUnitsIter<0,1> iter( 0, ANY_PARTY, GetCenter(), SConsts::FLAG_RADIUS ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pUnit = *iter;
		if ( pUnit->IsAlive() && pUnit->GetZ() <= 0.0f && fabs2( pUnit->GetCenter() - GetCenter() ) < sqr( SConsts::FLAG_RADIUS ) )
		{
			const int nUnitParty = pUnit->GetParty();
			if ( nUnitParty != theDipl.GetNeutralParty() )
			{
				if ( nUnitParty == nParty )
				{
					bOnlyEnemyUnitsOfOneTypeInZone = false;
					bIsThereOurUnits = true;
					break;
				}
				else if ( !bOnlyEnemyUnitsOfOneTypeInZone )
				{
					bOnlyEnemyUnitsOfOneTypeInZone = true;
					nEnemyParty = nUnitParty;
					nEnemyPlayer = pUnit->GetPlayer();
				}
				else if ( nEnemyParty != nUnitParty )
				{
					bOnlyEnemyUnitsOfOneTypeInZone = false;
					break;
				}
			}
		}
	}

	if ( !bIsThereOurUnits )
	{
		if ( nParty != theDipl.GetNeutralParty() && !bCapturingByNeutral )
		{
			bCapturingByNeutral = true;
			timeOfStartNeutralPartyCapturing = curTime;
		}
	}

	if ( bCapturingByNeutral && curTime >= timeOfStartNeutralPartyCapturing + theMPInfo.GetTimeToCaptureObject() )
	{
		bCapturingByNeutral = false;
		nParty = theDipl.GetNeutralParty();

		if ( !theMPInfo.IsSabotage() )		
		{
			updater.Update( ACTION_NOTIFY_SIDE_CHANGED, this, nParty );
			theMPInfo.FlagCaptured( nParty, GetUniqueId() );
		}
	}

	if ( bOnlyEnemyUnitsOfOneTypeInZone && bGoingToCapture && nPartyToCapture != nEnemyParty )
		bGoingToCapture = false;

	if ( bOnlyEnemyUnitsOfOneTypeInZone )
	{
		if ( !bGoingToCapture )
		{
			bGoingToCapture = true;
			nPartyToCapture = nEnemyParty;
			nPlayerToCapture = nEnemyPlayer;

			timeOfStartCapturing = curTime;
		}
		else if ( curTime >= timeOfStartCapturing + theMPInfo.GetTimeToCaptureObject() )
		{
			bGoingToCapture = false;
			nParty = nPartyToCapture;
			bCapturingByNeutral = false;

			if ( theMPInfo.IsSabotage() && nParty == theMPInfo.GetAttackingParty() )
				updater.Update( ACTION_NOTIFY_SIDE_CHANGED, this, theDipl.GetNeutralParty() );
			else
				updater.Update( ACTION_NOTIFY_SIDE_CHANGED, this, nParty );

			theMPInfo.FlagCaptured( nParty, GetUniqueId() );
		}
	}
	else
		bGoingToCapture = false;
}
const BYTE CFlag::GetPlayer() const
{
	if ( nParty == theDipl.GetNeutralParty() )
		return theDipl.GetNeutralPlayer();
	else
	{
		int i = 0;
		while ( i < theDipl.GetNPlayers() && theDipl.GetNParty( i ) != nParty )
			++i;

		if ( i >= theDipl.GetNPlayers() )
			return theDipl.GetNeutralPlayer();
		else
			return i;
	}
}
