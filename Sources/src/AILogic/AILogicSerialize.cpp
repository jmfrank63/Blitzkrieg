#include "StdAfx.h"

#include "AILogicInternal.h"
#include "GlobalObjects.h"
#include "StaticMembers.h"
#include "UnitCreation.h"
#include "Updater.h"
#include "Trigonometry.h"
#include "UnitsIterators.h"
#include "Soldier.h"
#include "Diplomacy.h"
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
int CAILogic::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	if ( saver.IsReading() )
	{
		pGameSegment = GetSingleton<IGameTimer>()->GetGameSegmentTimer();
		curTime = GetAIGetSegmTime( pGameSegment );
		
		NTrg::Init();
	}

	NGlobalObjects::Serialize( 1, ss );

	CStaticMembers staticMembers;
	saver.Add( 2, &staticMembers );

	saver.Add( 3, &bSuspended );
	saver.Add( 5, &garbage );
	saver.Add( 7, &scripts );
	saver.Add( 8, &pGameSegment );
	saver.Add( 9, &bridges );
	saver.Add( 10, &eTypeOfAreasToShow );
	saver.Add( 12, &bFirstTime );
	saver.Add( 13, &startCmds );
	saver.Add( 14, &nextCheckSumTime );
	saver.Add( 15, &periodToCheckSum );
	saver.Add( 16, &checkSum );
	saver.Add( 17, &availableTrucks );
	saver.Add( 18, &bNetGameStarted );
	saver.Add( 19, &reservePositions );
	
	if ( saver.IsReading() )
	{
		// Saves written while a transport's load was flagged unselectable
		// keep that flag. The player's soldiers inside a transport are
		// selectable now: put the flag back and let the client hear of it.
		for ( CGlobalIter iter( 0, ANY_PARTY ); !iter.IsFinished(); iter.Iterate() )
		{
			CSoldier *pSoldier = dynamic_cast<CSoldier*>( *iter );
			if ( pSoldier && pSoldier->IsInTransport() && !pSoldier->IsSelectable() && pSoldier->GetPlayer() == theDipl.GetMyNumber() )
				pSoldier->SetSelectable( true );
		}
	}
	return 0;
}
int CUnitCreation::STankPitInfo::operator&( IDataTree  &ss )
{
	CTreeAccessor tree = &ss;
	tree.Add( "SandBag", &sandBagTankPits );
	tree.Add( "Dig", &digTankPits );
	return 0;
}
int CUnitCreation::SCommonInfo::operator&( IDataTree  &ss )
{
	CTreeAccessor tree = &ss;
	tree.Add( "AntitankObjects", &antitankObjects );
	tree.Add( "AntipersonFence", &szAPFence );
	tree.Add( "AntiPersonMine", &szMineAP );
	tree.Add( "AntiTankMine", &szMineAT );
	tree.Add( "Entrenchment", &szEntrenchment );
	return 0;
}
int CUnitCreation::SLocalInGameUnitCreationInfo::SPlaneInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szName );
	saver.Add( 2, &nFormation );
	saver.Add( 3, &bEnabledScript );
	saver.Add( 4, &nPlanes );
	return 0;
}
int CUnitCreation::SLocalInGameUnitCreationInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &planes );
	saver.Add( 2, &szParatrooper );
	saver.Add( 3, &timeLastCall );
	saver.Add( 4, &timeRelax );
	saver.Add( 5, &vAppearPoints );
	saver.Add( 6, &szPartyName );
	saver.Add( 7, &nParadropSquadCount );
	saver.Add( 8, &nLastCalledAviaType );
	return 0;
}
int CUnitCreation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		pIDB = GetSingleton<IObjectsDB>();
		InitConsts();
	}
	saver.Add( 2, &bMainButtonDisabled );
	saver.Add( 4, &inGameUnits );
	saver.Add( 6, &vLockedAppearPoints );
	saver.Add( 7, &inGameUnits );
	saver.Add( 9, &bForceDisabled );
	saver.Add( 10, &bLockedFlags );
	saver.Add( 11, &nAviationCallNumeber );
	return 0;
}
