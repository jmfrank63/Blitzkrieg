#include "StdAfx.h"

#include "UnitCreation.h"
#include "Units.h"
#include "GroupLogic.h"
#include "AIStaticMap.h"
#include "Formation.h"
#include "CommonUnit.h"
#include "Soldier.h"
#include "updater.h"
#include "Artillery.h"
#include "AIWarFog.h"
#include "Diplomacy.h"
#include "Technics.h"
#include "AIClassesID.h"
#include "Path.h"
#include "StaticObjects.h"
#include "PlanePath.h"
#include "Weather.h"
#include "Aviation.h"
#include "Scripts/scripts.h"

#include "../Input/Input.h"
#include "../GameTT/iMission.h"
#include "../Scene/Scene.h"
#include "../Common/World.h"

#include "../Formats/fmtMap.h"
#include "../RandomMapGen/Polygons_Types.h"
#include "../Misc/Checker.h"
extern CWeather theWeather;
extern CScripts *pScripts;
extern CStaticObjects theStatObjs;
extern CUnits units;
extern CGroupLogic theGroupLogic;
extern CStaticMap theStaticMap;
extern CUpdater updater;
CUnitCreation theUnitCreation;
extern NTimer::STime curTime;
extern CDiplomacy theDipl;
void CUnitCreation::CLightPlaneCreation::CalcPositions( const int nMax, const CVec2 & box, const CVec2 & direction, std::vector<CVec2> * positions, CVec2 * offset, const bool bRandom )
{
	/* таким образом
		1
		 2

		3
		 4

		5
		 6

		..
			..
	*/

	NI_ASSERT_T( nMax , "рассчеты делаются для положительного числа только" );
	positions->clear();

	CVec2 vStartOffset( 0, 0 );
	const float resize = SConsts::PLANES_SMALL_FORMATION_SIZE;

	for ( int i=0; i< nMax; ++i )
	{
		if ( i % 2 == 0 )
			positions->push_back( vStartOffset /*^ direction*/ );
		else
		{
			positions->push_back( (vStartOffset + CVec2( -resize * box.y, resize / 2 * box.x ) ) ^ direction ) ;

			vStartOffset += CVec2( -resize * 3 * box.y, 0);
		}
	}

	*offset = ( CVec2( resize * box.y, 0 ) * nMax / 2 )/*^direction*/;
}
void CUnitCreation::CHeavyPlaneCreation::CalcPositions( const int nMax, const CVec2 &box, const CVec2 &direction, std::vector<CVec2> *positions, CVec2 *offset, bool bRandom )
{
	/*
	таким образом

					1
				5		6
			11	2		12
		..	7		8		..
			..	3		..
				9		10
					4
	*/
	NI_ASSERT_T( nMax , "рассчеты делаются для положительного числа только" );
	int nSize = sqrt( nMax-1 ) + 1;
	positions->clear();

	int iLimit = nSize;
	CVec2 vStartOffset( 0, 0 );// смещение первой точки ( 1, 5, 11 )
	
	const float resize = SConsts::PLANES_HEAVY_FORMATION_SIZE;

	*offset = ( CVec2( resize * box.y, 0 ) * int(nSize/2) )^direction;

	while ( iLimit > 0 )
	{
		for ( int i = 0; i < iLimit; ++i )
		{
			CVec2 res ( CVec2( vStartOffset.x, vStartOffset.y)+ CVec2( -resize * i * box.y, 0 ) );
			if ( bRandom )
				res.x -= box.y * Random( 0.0f, SConsts::PLANES_START_RANDOM );

			positions->push_back( res/*^direction*/ );
			if ( positions->size() == nMax )
				return ;
			if ( iLimit != nSize )
			{
				CVec2 res ( CVec2( vStartOffset.x, -vStartOffset.y )+ CVec2( -resize * i * box.y, 0 ) );
				if ( bRandom )
					res.x -= box.y * Random( 0.0f, SConsts::PLANES_START_RANDOM );
				positions->push_back( res /*^ direction*/ );
				if ( positions->size() == nMax )
					return ;
			}
		}
		vStartOffset += CVec2( -resize / 2 * box.y, resize / 2 * box.x );
		--iLimit;
	}
}
const char * CUnitCreation::STankPitInfo::GetRandomTankPit( const class CVec2 &vSize, const bool bCanDig, float *pfResize ) const
{
	const std::vector<std::string> * pPits = bCanDig ? &digTankPits : &sandBagTankPits;

	int nBestIndex = -1;
	float fDiff;

	for ( int i = 0; i < pPits->size(); ++i )
	{
		CGDBPtr<SGDBObjectDesc> pDesc = theUnitCreation.GetObjectDB()->GetDesc( (*pPits)[i].c_str() );
		NI_ASSERT_T( pDesc!=0, NStr::Format( "Unregistered object %s", (*pPits)[i].c_str() ) );
		const SMechUnitRPGStats * pStats = static_cast<const SMechUnitRPGStats*>( theUnitCreation.GetObjectDB()->GetRPGStats( pDesc ) );
		const float fResize = vSize.x / pStats->vAABBHalfSize.x ;
		const float fCurDiff = fabs( fResize - 1);
		if ( -1 == nBestIndex || fDiff > fCurDiff )
		{
			*pfResize = fResize;
			fDiff = fCurDiff;
			nBestIndex = i;
		}
	}

	NI_ASSERT_T( nBestIndex != -1, "cannot find any tankpit in tankpits.xml" );
	return (*pPits)[nBestIndex].c_str();
}
CUnitCreation::CUnitCreation()
{
	bInit = false;
	nAviationCallNumeber = 0;
	feedbacks.push_back( SFeedBack(EFB_SCOUT_ENABLED,				EFB_SCOUT_DISABLED) );
	feedbacks.push_back( SFeedBack(EFB_FIGHTERS_ENABLED,		EFB_FIGHTERS_DISABLED) );
	feedbacks.push_back( SFeedBack(EFB_PARADROPS_ENABLED,		EFB_PARADROPERS_DISABLED) );
	feedbacks.push_back( SFeedBack(EFB_BOMBERS_ENABLED,			EFB_BOMBERS_DISABLED) );
	feedbacks.push_back( SFeedBack(EFB_SHTURMOVIKS_ENABLED,	EFB_SHTURMOVIKS_DISABLED) );
}
void CUnitCreation::DisableMainAviationButton( NTimer::STime time )
{
	updater.AddFeedBack( SAIFeedBack(EFB_AVIA_DISABLED, time ) );
	bMainButtonDisabled = true;
}
void CUnitCreation::EnableAviationButtons( const bool bInit )
{
	const int nPlayer = theDipl.GetMyNumber();

	if ( inGameUnits.size() < nPlayer ) return;

	bool bSomePlaneEnabled = false;

	for ( int nAvia = 0; nAvia < SUCAviation::AT_COUNT; ++nAvia )
	{
		if ( IsAviaEnabledMapParameters( nPlayer, nAvia ) &&
					IsAviaEnabledScript( nPlayer, nAvia ) &&
					!theWeather.IsActive() )
		{
			EFeedBack eFeed = static_cast<EFeedBack>( feedbacks[nAvia].eEnable );
			updater.AddFeedBack( eFeed );
			bSomePlaneEnabled = true;
		}
		else
		{
			EFeedBack eFeed = static_cast<EFeedBack>( feedbacks[nAvia].eDisable );
			updater.AddFeedBack( eFeed );
		}
	}	
	if ( bSomePlaneEnabled )
	{
		if ( !bInit && bMainButtonDisabled )												// say about aviation 
			updater.AddFeedBack( SAIFeedBack(EFB_AVIA_ENABLED) );
		bMainButtonDisabled = false;
	}
	else
	{
		bMainButtonDisabled = true;
		updater.AddFeedBack( SAIFeedBack(EFB_AVIA_DISABLED) );
	}
}
void CUnitCreation::BadWeatherStarted()
{
	const int nPlayer = theDipl.GetMyNumber();
	bool bEnabled = false;
	for ( int nAvia = 0; nAvia < SUCAviation::AT_COUNT; ++nAvia )
	{
		if ( IsAviaEnabledMapParameters( nPlayer, nAvia ) )
		{
			bEnabled = true;
			break;
		}
	}
	if ( bEnabled ) 
		DisableMainAviationButton( 0 );
}
void CUnitCreation::EnableAviationScript( const int nPlayer, const int nAvia )
{
	if ( inGameUnits.size() < nPlayer ) return;
	
	if ( nAvia == -1 )
	{
		bForceDisabled[nPlayer] = false;
		for ( int i = 0; i < SUCAviation::AT_COUNT; ++i )
		{
			EnableAviationScript( nPlayer, i );
		}
		bForceDisabled[nPlayer] = false;
		if ( bMainButtonDisabled )
		{
			bMainButtonDisabled = false;
			updater.AddFeedBack( SAIFeedBack( EFB_AVIA_ENABLED ) );
		}
		return;
	}

	if ( IsAviaEnabledMapParameters( nPlayer, nAvia ) )
		inGameUnits[nPlayer].planes[nAvia].bEnabledScript = true;

	if ( theDipl.GetMyNumber() == nPlayer )
		updater.AddFeedBack( static_cast<EFeedBack>(feedbacks[nAvia].eEnable) );
}
void CUnitCreation::DisableAviationScript( const int nPlayer, const int nAvia )
{
	CheckRange( inGameUnits, nPlayer );
	
	if ( inGameUnits.size() < nPlayer ) return;

	if ( nAvia == -1 )
	{
		bForceDisabled[nPlayer] = true;
		for ( int i = 0; i < SUCAviation::AT_COUNT; ++i )
		{
			DisableAviationScript( nPlayer, i );
		}
		return;
	}

	CheckRange( inGameUnits[nPlayer].planes,  nAvia );

	SLocalInGameUnitCreationInfo &unitsInfo = inGameUnits[nPlayer];
	SLocalInGameUnitCreationInfo::SPlaneInfo &planeInfo = unitsInfo.planes[nAvia];
	if ( IsAviaEnabledMapParameters( nPlayer, nAvia ) && IsAviaEnabledScript( nPlayer, nAvia ) )
		planeInfo.bEnabledScript = false;


	if ( theDipl.GetMyNumber() == nPlayer )
	{
		updater.AddFeedBack( static_cast<EFeedBack>(feedbacks[nAvia].eDisable) );
		bool bMainButtonStillEnable = false;
		for ( int nAviaTemp = 0; nAviaTemp < SUCAviation::AT_COUNT; ++nAviaTemp )
		{	
			if ( IsAviaEnabledMapParameters( nPlayer, nAviaTemp ) && IsAviaEnabledScript( nPlayer, nAviaTemp ) )
			{
				bMainButtonStillEnable = true;
				break;
			}
		}

		if ( !bMainButtonStillEnable )
			DisableMainAviationButton( 0 );
	}
}

void CUnitCreation::Segment()
{
	const int nDipl = theDipl.GetMyNumber();

	if ( bInit )
	{
		if ( ( nDipl >= inGameUnits.size() ) ||	// нас нет в спискe
				 ( 0 == inGameUnits[nDipl].vAppearPoints.size() ) ) //нет аэродромов
		{
			bForceDisabled[nDipl] = true;
			DisableMainAviationButton( 0 ); // запретить вызов авиации совсем
		}
		else
		{
			bForceDisabled[nDipl] = false;
			EnableAviationButtons( bInit );
		}
		bInit = false;
	}

	if ( !bForceDisabled[nDipl] &&				// если самолеты в принципе возможны
				bMainButtonDisabled  &&					// но в данный момент запрешены
				!theWeather.IsActive() )				// weater is suitable for planes
	{
		if (	curTime >= inGameUnits[nDipl].timeLastCall + inGameUnits[nDipl].timeRelax || 
					inGameUnits[nDipl].timeLastCall == 0 )
		{ //время подошло, разрешить самолеты
			EnableAviationButtons();
		}
	}
}
void CUnitCreation::SLocalInGameUnitCreationInfo::Copy( const SUnitCreation  &rSUnitCreation )
{
	timeLastCall = 0;
	timeRelax = 0;
	nLastCalledAviaType = -1;
	
	planes.resize( SUCAviation::AT_COUNT );
	szPartyName = rSUnitCreation.szPartyName;

	for ( int i = 0; i < SUCAviation::AT_COUNT; ++i )
	{
		SPlaneInfo & plane = planes[i];
		planes[i].nFormation = 	rSUnitCreation.aviation.aircrafts[i].nFormationSize;
		planes[i].szName = rSUnitCreation.aviation.aircrafts[i].szName;
		planes[i].nPlanes = Max( rSUnitCreation.aviation.aircrafts[i].nPlanes,
														 rSUnitCreation.aviation.aircrafts[i].nFormationSize );

		planes[i].bEnabledScript = !planes[i].szName.empty() &&
												planes[i].nFormation &&
												planes[i].nPlanes;
	}
	timeRelax = rSUnitCreation.aviation.nRelaxTime * 1000;
	szParatrooper = rSUnitCreation.aviation.szParadropSquadName;
	nParadropSquadCount = rSUnitCreation.aviation.nParadropSquadCount;

	for ( std::list<CVec3>::const_iterator appearPointIterator = rSUnitCreation.aviation.vAppearPoints.begin();
				appearPointIterator != rSUnitCreation.aviation.vAppearPoints.end(); ++appearPointIterator )
	{
		vAppearPoints.push_back( CVec2( appearPointIterator->x, appearPointIterator->y ) );
	}
}
CUnitCreation::SLocalInGameUnitCreationInfo::SLocalInGameUnitCreationInfo( const SUnitCreation &rSUnitCreation )
{
	Copy( rSUnitCreation  );
}
CUnitCreation::SLocalInGameUnitCreationInfo & CUnitCreation::SLocalInGameUnitCreationInfo::operator=( const SUnitCreation &rSUnitCreation  )
{
	Copy( rSUnitCreation );
	return *this;
}
void CUnitCreation::InitConsts()
{
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "units\\technics\\common\\tankpit.xml" , STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "TankPits", &tankPitInfo );
	}
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "partys.xml" , STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "PartyInfo", &partyDependentInfo );
	}
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "objects\\objectcreation.xml" , STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "Objects", &commonInfo );
	}
}
void CUnitCreation::Init()
{
	pIDB = GetSingleton<IObjectsDB>();
	InitConsts();
	bInit = false;
}
void CUnitCreation::Init( const struct SUnitCreationInfo &rUnitCreationInfo )
{

	pIDB = GetSingleton<IObjectsDB>();
	InitConsts();

	inGameUnits.clear();

	for ( std::vector<SUnitCreation>::const_iterator unitCreationIterator = rUnitCreationInfo.units.begin();
				unitCreationIterator != rUnitCreationInfo.units.end();
				++unitCreationIterator )
	{
		inGameUnits.push_back( SLocalInGameUnitCreationInfo( (*unitCreationIterator) ) );
	}

	bForceDisabled.resize( rUnitCreationInfo.units.size() );
	std::fill( bForceDisabled.begin(), bForceDisabled.end(), 0 );

	bInit = true;
	
	bLockedFlags.resize( rUnitCreationInfo.units.size() );
	std::fill( bLockedFlags.begin(), bLockedFlags.end(), false );
	vLockedAppearPoints.resize( rUnitCreationInfo.units.size() );
	nAviationCallNumeber = Random( 0, 100 );
}
void CUnitCreation::Clear()
{
	pIDB = 0;
	inGameUnits.clear();
}
void CUnitCreation::InitPlanePath( class CCommonUnit * pUnit, const CVec3 &vAppearPoint, const CVec3 &vGoToPoint )
{
	CVec2 vDir( vGoToPoint.x- vAppearPoint.x, vGoToPoint.y- vAppearPoint.y );
	pUnit->UpdateDirection( vDir );
	pUnit->GetCurPath()->Init( pUnit, new CPlanePath( vAppearPoint,vGoToPoint), true );
}
const SMechUnitRPGStats * CUnitCreation::GetPlaneStats( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/ nAvia ) const
{
	const std::string &name = inGameUnits[nPlayer].planes[nAvia].szName;
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( name.c_str() );
	return static_cast<const SMechUnitRPGStats*>( pIDB->GetRPGStats( pDesc ) );
}
const NTimer::STime CUnitCreation::GetPlaneRegenerateTime( const int nPlayer ) const
{
	return inGameUnits[nPlayer].timeRelax;
}
const float CUnitCreation::GetPlaneFlyHeight( const int nPlayer, const int nAvia )
{
	return GetPlaneStats( nPlayer, nAvia )->fMaxHeight;
}
void CUnitCreation::RegisterAviationCall( const int nPlayer, const int nAviaType )
{
	CheckRange( inGameUnits, nPlayer );
	CheckRange( inGameUnits[nPlayer].planes,  nAviaType );

	NI_ASSERT_T( inGameUnits.size() > nPlayer, "n player is too large" );
	if ( nPlayer == theDipl.GetMyNumber() )
	{
		if ( bMainButtonDisabled || bForceDisabled[theDipl.GetMyNumber()] ) return;
		DisableMainAviationButton( inGameUnits[nPlayer].timeRelax );
		if ( 0 == inGameUnits[nPlayer].planes[nAviaType].nPlanes ) // кончились самолеты
		{
			EFeedBack eFeed = static_cast<EFeedBack>(feedbacks[nAviaType].eDisable);
			updater.AddFeedBack( eFeed );
		}
	}
	inGameUnits[nPlayer].timeLastCall = curTime + 1;	// to dissalow aviation call with timeLastCall == 0
	inGameUnits[nPlayer].nLastCalledAviaType = nAviaType;
}
void CUnitCreation::CallPlane( const int nPlayer,
															 const int /*SUCAviation::AIRCRAFT_TYPE*/ nAviaType,
															 const WORD wGroupID,
															 CUnitCreation::IPlaneCreation * pCreation )
{
	CheckRange( inGameUnits, nPlayer );
	CheckRange( inGameUnits[nPlayer].planes,  nAviaType );

	if ( !IsAviaEnabled( nPlayer, nAviaType ) )
	{
		theGroupLogic.RegisterGroup( 0, 0, wGroupID );
		return;
	}
	const std::string &szPlaneName = inGameUnits[nPlayer].planes[nAviaType].szName;
	const int nAvia = Min( inGameUnits[nPlayer].planes[nAviaType].nFormation,
												 inGameUnits[nPlayer].planes[nAviaType].nPlanes );

	if ( 0 == nAvia ) 
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, "wrong call from script(time isn' reserved)", 0xffff0000 );

		theGroupLogic.RegisterGroup( 0, 0, wGroupID );
		return;
	}
	inGameUnits[nPlayer].planes[nAviaType].nPlanes -= nAvia;

	LockAppearPoint( nPlayer, true );
	const CVec2 vAppearPoint( GetRandomAppearPoint(nPlayer) );
	
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( szPlaneName.c_str() );
	const SMechUnitRPGStats *pStats = static_cast<const SMechUnitRPGStats*>( pIDB->GetRPGStats( pDesc ) );
	const int dbID = pIDB->GetIndex( szPlaneName.c_str() );
	const EObjVisType eVisType = SGVOT_MESH;
	const CVec2 vGoToPoint( pCreation->GetDestPoint() );

	IRefCount **arUnits = GetTempBuffer<IRefCount*>( nAvia );
	std::vector<CVec2> positions;
	CVec2 vBombPointOffset;
	const CVec2 box ( pStats->vAABBHalfSize * 2 );
	CVec2 vUitDir( vGoToPoint - vAppearPoint );
	Normalize( &vUitDir );

	pCreation->CalcPositions( nAvia, box, vUitDir, &positions, &vBombPointOffset );
	NI_ASSERT_T( positions.size() == nAvia, "wrong result" );
	
	CVec2 vDirection( vGoToPoint - vAppearPoint );
	Normalize( &vDirection );
	const WORD wDirection( GetDirectionByVector( vDirection ) );
	
	const float fAppearHeight = pStats->fMaxHeight ;

	CPtr<CPlanesFormation> pFormation;
	const bool bNeedFormation = (nAviaType == SUCAviation::AT_BOMBER && pStats->wDivingAngle <= SConsts::ANGLE_DIVEBOMBER_MIN_DIVE) ||
															 nAviaType == SUCAviation::AT_PARADROPER;
	if ( bNeedFormation )
	{
		float fMaxOffset = 0;								// to adjust turn radius acording to plane's formation
		for ( int i = 0; i < positions.size(); ++i )
			fMaxOffset = Max( fMaxOffset, fabs( positions[i].y ) );
		pFormation = new CPlanesFormation;
		pFormation->Init( vAppearPoint, fAppearHeight, pStats->fTurnRadius + fMaxOffset, pStats->fTurnRadius * 2 + fMaxOffset, wDirection, pStats->fSpeed, fabs(vBombPointOffset.y) );
		pFormation->SendAlongPath( new CPlanePath( CVec3( vAppearPoint, fAppearHeight ), CVec3( vGoToPoint, fAppearHeight ) ) );
	}

	for ( int i = 0; i < nAvia; ++i )
	{
		const CVec2 pointOfAppear( vAppearPoint + (positions[i]^vDirection) );
		const CVec2 pointOfBombing( vGoToPoint + ((positions[i]+ vBombPointOffset)^vDirection) );
		const int id = AddNewUnit( pStats, 1, pointOfAppear.x, pointOfAppear.y, fAppearHeight, dbID, wDirection, nPlayer, eVisType );
		
		NI_ASSERT_T( dynamic_cast<CAviation*>( units[id] ) != 0, NStr::Format( "not aviation created %s", szPlaneName.c_str()) );
		CAviation * pUnit = static_cast<CAviation*>( units[id] );
		arUnits[i] = pUnit;
		pUnit->SendAcknowledgement( ACK_PLANE_TAKING_OFF, true );
		pUnit->SetAviationType( nAviaType );

		if ( pFormation )
		{
			theGroupLogic.UnitCommand( SAIUnitCmd( pCreation->GetCommand(), vGoToPoint, pCreation->GetNParam() ), units[id], false );
			pUnit->SetPlanesFormation( pFormation, positions[i] );
			CPtr<CPlaneInFormationSmoothPath> pPath = new CPlaneInFormationSmoothPath;
			pPath->Init( pUnit );
			pUnit->SetCurPath( pPath );
		}
		else
		{
			theGroupLogic.UnitCommand( SAIUnitCmd( pCreation->GetCommand(), pointOfBombing, pCreation->GetNParam() ), units[id], false );
			InitPlanePath( units[id], CVec3( pointOfAppear, pStats->fMaxHeight ), CVec3( pointOfBombing, pStats->fMaxHeight ) );
		}
	}
	theGroupLogic.RegisterGroup( arUnits, nAvia, wGroupID );
	RegisterAviationCall( nPlayer, nAviaType );

	if ( EDI_ENEMY == theDipl.GetDiplStatus( theDipl.GetMyNumber(), nPlayer ) )
	{
		const CVec2 vIntercect = GetFirstIntercectWithMap( nPlayer );
		NI_ASSERT_T( vIntercect.x >= 0 && vIntercect.y >= 0, "wrong algo" );
		updater.AddFeedBack( SAIFeedBack(EFB_ENEMY_AVIATION_CALLED, MAKELONG( vIntercect.x, vIntercect.y ) ) );
	}
	LockAppearPoint( nPlayer, false );
	++nAviationCallNumeber;
}
void CUnitCreation::CallScout( const SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl )
{
	CLightPlaneCreation cr( unitCmd.vPos, ACTION_MOVE_PLANE_SCOUT_POINT );
	CallPlane( nDipl, SUCAviation::AT_SCOUT, wGroupID, &cr );
}
void CUnitCreation::CallFighters( const SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl )
{
	CLightPlaneCreation cr( unitCmd.vPos, ACTION_MOVE_FIGHTER_PATROL );
	CallPlane( nDipl, SUCAviation::AT_FIGHTER, wGroupID, &cr );
}
void CUnitCreation::CallShturmoviks( const SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl )
{
	CLightPlaneCreation cr( unitCmd.vPos, ACTION_MOVE_SHTURMOVIK_PATROL, EGCA_GUNPLANE );
	CallPlane( nDipl, SUCAviation::AT_BATTLEPLANE, wGroupID, &cr );
}
void CUnitCreation::CallParadroppers( const SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl )
{
	CHeavyPlaneCreation cr( unitCmd.vPos, ACTION_MOVE_DROP_PARATROOPERS );
	CallPlane( nDipl, SUCAviation::AT_PARADROPER, wGroupID, &cr );
}
void CUnitCreation::CallBombers( const SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl )
{
	const std::string &szPlaneName = inGameUnits[nDipl].planes[SUCAviation::AT_BOMBER].szName;
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( szPlaneName.c_str() );
	const SMechUnitRPGStats *pStats = static_cast<const SMechUnitRPGStats*>( pIDB->GetRPGStats( pDesc ) );

	if ( pStats->wDivingAngle <= SConsts::ANGLE_DIVEBOMBER_MIN_DIVE )
	{
		CHeavyPlaneCreation cr( unitCmd.vPos, ACTION_MOVE_PLANE_BOMB_POINT, true );
		CallPlane( nDipl, SUCAviation::AT_BOMBER, wGroupID, &cr );
	}
	else
	{
		CLightPlaneCreation cr( unitCmd.vPos, ACTION_MOVE_SHTURMOVIK_PATROL, EGCA_DIVEBOMBER );
		CallPlane( nDipl, SUCAviation::AT_BOMBER, wGroupID, &cr );
	}
}
const CUnitCreation::SPartyDependentInfo & CUnitCreation::GetPartyDependentInfo( const int nDipl ) const 
{
	std::string szPartyName = inGameUnits[nDipl].szPartyName;
	if ( szPartyName == "" )
		szPartyName = "USSR";
	int i = 0;
	for( ; i < partyDependentInfo.size() && szPartyName != partyDependentInfo[i].szPartyName; ++i )
	{
	}
	NI_ASSERT_T( i < partyDependentInfo.size(), NStr::Format("wrong party name %s for party Number %i",szPartyName, nDipl ) );
	return partyDependentInfo[i];
}
void CUnitCreation::CreateMine( const enum SMineRPGStats::EType nType, const class CVec2 &vPoint, const int nDipl )
{
	NI_ASSERT_T( nDipl < inGameUnits.size(), NStr::Format( "no mines for player %d", nDipl ) );
	
	const SPartyDependentInfo & info = GetPartyDependentInfo( nDipl );

	const std::string *pszName=0;
	switch ( nType )
	{
	case SMineRPGStats::INFANTRY:
		pszName = &commonInfo.szMineAP;
		break;
	case SMineRPGStats::TECHNICS:
		pszName = &commonInfo.szMineAT;
		break;
	default: 
		NI_ASSERT_T( false, NStr::Format( "wrong mine type %d", nType ) );
		return;
	}

	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( pszName->c_str() );
	CGDBPtr<SMineRPGStats> pStats = static_cast<const SMineRPGStats *>( pIDB->GetRPGStats( pDesc ) );
	const int nDBIndex = pIDB->GetIndex( pszName->c_str() );
	theStatObjs.AddNewMine( pStats, 1, nDBIndex, vPoint, -1, nDipl );
}
void CUnitCreation::SendFormationToWorld( CFormation * pUnit ) const
{
	updater.Update( ACTION_NOTIFY_NEW_UNIT, pUnit );
	for ( int j = 0; j < pUnit->Size(); ++j )
		updater.Update( ACTION_NOTIFY_NEW_FORMATION, (*pUnit)[j] );
	pUnit->SetSelectable( true );
}
CFormation* CUnitCreation::CreateParatroopers( const CVec3 &where, CAIUnit *pPlane, const int nScriptID )const
{
	const int nDipl = pPlane->GetPlayer();
	
	NI_ASSERT_T( nDipl < inGameUnits.size(), NStr::Format( "wrong player %d", nDipl ) );

	const std::string &name = inGameUnits[nDipl].szParatrooper;
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( name.c_str() );

	CGDBPtr<SSquadRPGStats> pStats = static_cast<const SSquadRPGStats*>( pIDB->GetRPGStats( pDesc ) );

	const WORD wDir = pPlane->GetDir();
	const int nFormation = 0;
	float fHP = 1.0f;

	CPtr<CFormation> formation = static_cast<CFormation*>( AddNewFormation( pStats, nFormation, fHP, where.x, where.y, where.z, wDir, nDipl,false,false ) );
		
	for ( int i = 0; i < formation->Size(); ++i )
	{
		(*formation)[i]->SetToSolidPlace();
		pScripts->AddObjToScriptGroup( (*formation)[i], nScriptID );
	}
	
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_MOVE_PARACHUTE ), formation, false );
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_PARADE, -1.0f ), formation, true );
	

	return formation;
}
CFormation* CUnitCreation::CreateResupplyEngineers( CAITransportUnit *pWithUnit )const
{
	NI_ASSERT_T( pWithUnit->GetPlayer() < inGameUnits.size(), NStr::Format( "wrong player %d",pWithUnit->GetPlayer()) );
	
	const char * pszName = GetPartyDependentInfo(pWithUnit->GetPlayer()).szResupplyEngineerSquad.c_str();
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( pszName );
	
	if ( !pDesc )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CHAT, "bad resupply engineers in map, created default instead", 0xffff0000 );
		std::string szName  = "USSR_reload";
		pDesc = pIDB->GetDesc( szName.c_str() );
	}

	NI_ASSERT_T( pDesc!=0, "default engineers are not valid" );

	CGDBPtr<SSquadRPGStats> pStats = static_cast<const SSquadRPGStats*>( pIDB->GetRPGStats( pDesc ) );

	const WORD wDir = 0;
	const int nFormation = 0;
	float fHP = 1.0f;
	const int nPlayer = pWithUnit->GetPlayer();
	const CVec2 where = pWithUnit->GetEntrancePoint();

	CPtr<CFormation> pFormation = static_cast<CFormation*>( AddNewFormation( pStats, nFormation, fHP, where.x, where.y, theStaticMap.GetZ( AICellsTiles::GetTile(where) ), wDir, nPlayer, false, false ) );
	pFormation->SetSelectable( false );

	for ( int i = 0; i < pFormation->Size(); ++i )
	{
		CPtr<CSoldier> pS = (*pFormation)[i];
		pS->SetToSolidPlace();
	}
	pFormation->SetInTransport( pWithUnit );
	theGroupLogic.UnitCommand( SAIUnitCmd( ACTION_COMMAND_IDLE_TRANSPORT, pWithUnit ), pFormation, false );
	return pFormation ;
}
CFormation * CUnitCreation::CreateCrew( CArtillery *pUnit, IObjectsDB *_pIDB, const int nUnits, const CVec3 vPos, const int _nPlayer, const bool bImmidiateAttach )const
{
	if ( 0 == _pIDB )
		_pIDB = pIDB;

	NI_ASSERT_T( (_nPlayer == -1 ? pUnit->GetPlayer() : _nPlayer ) < inGameUnits.size(), NStr::Format( "wrong player GetPlayer = %d, nPlayer = %d", pUnit->GetPlayer(), _nPlayer ) );
	
	CVec3 vCreatePos (vPos);
	if ( -1 == vPos.x )
		vCreatePos = CVec3( pUnit->GetCenter(), pUnit->GetZ() );

	const int nPlayer = ( _nPlayer == -1 ) ? pUnit->GetPlayer() : _nPlayer;

	const char * pszName = RPG_TYPE_ART_HEAVY_MG == pUnit->GetStats()->type ? 
													GetPartyDependentInfo(nPlayer).szHeavyMGSquad.c_str():
													GetPartyDependentInfo(nPlayer).szGunCrewSquad.c_str();

	CGDBPtr<SGDBObjectDesc> pDesc = _pIDB->GetDesc( pszName );
	const int nDBIndex = _pIDB->GetIndex( pszName );
	NI_ASSERT_T( dynamic_cast<const SSquadRPGStats*>(_pIDB->GetRPGStats( pDesc )) != 0, "" );
	CGDBPtr<SSquadRPGStats> pStats = static_cast<const SSquadRPGStats*>( _pIDB->GetRPGStats( pDesc ) );
	
	CFormation * pForm = static_cast<CFormation*>
											 ( theUnitCreation.AddNewFormation(	pStats,
																		0, 1.0f, vCreatePos.x, vCreatePos.y, vCreatePos.z,
																		0, nPlayer, false, true, nUnits )
										   );

	theGroupLogic.UnitCommand( SAIUnitCmd(ACTION_COMMAND_CATCH_ARTILLERY, pUnit), pForm, false );
	pUnit->SetCapturingUnit( pForm );
	if ( bImmidiateAttach )
		pUnit->SetCrew( pForm );
	pForm->SetSelectable( false );
	return pForm;
}
int CUnitCreation::AddNewUnit( const std::string &name, IObjectsDB *_pIDB, const float fHPFactor, const int x, const int y, const int z, const WORD dir, const BYTE player, bool bInitialization, bool IsEditor, bool bSendToWorld ) const
{
	CGDBPtr<SGDBObjectDesc> pDesc = _pIDB->GetDesc( name.c_str() );
	const int nDBIndex = _pIDB->GetIndex( name.c_str() );
	NI_ASSERT_T( dynamic_cast<const SUnitBaseRPGStats*>(_pIDB->GetRPGStats( pDesc )) != 0, "" );
	CGDBPtr<SUnitBaseRPGStats> pStats = static_cast<const SUnitBaseRPGStats*>( _pIDB->GetRPGStats( pDesc ) );

	return AddNewUnit( pStats, fHPFactor, x, y, z, nDBIndex, dir, player, pDesc->eVisType, bInitialization, bSendToWorld, IsEditor );
}
int CUnitCreation::AddNewUnit( const SUnitBaseRPGStats *pStats, const float fHPFactor, const int x, const int y, const int z, const WORD dbID, const WORD dir, const BYTE player, const EObjVisType eVisType, bool bInitialization, bool bSendToWorld, bool IsEditor ) const
{
	CPtr<CAIUnit> pUnit;
	if ( pStats->IsInfantry() )
	{
		pUnit = pStats->type == RPG_TYPE_SNIPER ?
						CreateObject<CAIUnit>( AI_SNIPER ) : CreateObject<CAIUnit>( AI_INFANTRY );
	}
	else if ( pStats->IsAviation() )
		pUnit = CreateObject<CAIUnit>( AI_AVIATION );
	else if ( pStats->IsTransport() )
		pUnit = CreateObject<CAIUnit>( AI_TRANSPORT_UNIT );
	else if ( pStats->IsArtillery() || pStats->type == RPG_TYPE_TRAIN_SUPER )
		pUnit = CreateObject<CAIUnit>( AI_ARTILLERY );
	else if ( pStats->IsArmor() || pStats->IsSPG() || pStats->IsTrain() )
		pUnit = CreateObject<CAIUnit>( AI_TANK );
	else
		NI_ASSERT_T( false, "Unknown unit's type" );
	NI_ASSERT_T( pUnit != 0, "cannot create unit" );

	const int newId = units.AddUnitToUnits( pUnit, player, pStats->type );
	pUnit->Init( CVec2( x, y ), z, pStats, pStats->fMaxHP * fHPFactor, dir, player, newId, eVisType, dbID );
	pUnit->Mem2UniqueIdObjs();

	units.AddUnitToMap( pUnit );

	CAIUnit::CheckCmdsSize( newId );

	if ( bSendToWorld)
		updater.Update( ACTION_NOTIFY_NEW_UNIT, pUnit );
	pUnit->LockTiles( !bInitialization );

	SFogInfo fogInfo;
	pUnit->GetFogInfo( &fogInfo );
	theWarFog.AddUnit( newId, pUnit->GetParty(), fogInfo );
	
	pUnit->SetSelectable( player == theDipl.GetMyNumber() );
	
	if ( pStats->IsArtillery() && theDipl.GetNeutralPlayer() != player )
	{
		CArtillery *pArt = static_cast_ptr<CArtillery*>( pUnit );
		if ( pArt->MustHaveCrewToOperate() && !IsEditor )
			CreateCrew( pArt, pIDB );
		else
			pArt->SetOperable( 1.0f );
	}

	return newId;
}
bool CUnitCreation::IsAviaEnabledScript( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/nAvia ) const
{
	const SLocalInGameUnitCreationInfo &unitsInfo = inGameUnits[nPlayer];
	const SLocalInGameUnitCreationInfo::SPlaneInfo &planeInfo = unitsInfo.planes[nAvia];
	return planeInfo.bEnabledScript;
}
bool CUnitCreation::IsAviaEnabledMapParameters( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/nAvia ) const
{
	const SLocalInGameUnitCreationInfo &unitsInfo = inGameUnits[nPlayer];
	const SLocalInGameUnitCreationInfo::SPlaneInfo &planeInfo = unitsInfo.planes[nAvia];
	
	return !planeInfo.szName.empty() && 0 < planeInfo.nFormation &&
			0 < planeInfo.nPlanes && !unitsInfo.vAppearPoints.empty();
}
bool CUnitCreation::IsAviaEnabled( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/nAvia ) const
{
	return 	!theWeather.IsActive() &&
				 ( 0 == inGameUnits[nPlayer].timeLastCall || 
					 ( curTime > inGameUnits[nPlayer].timeRelax + inGameUnits[nPlayer].timeLastCall )
				 ) &&
				 ( IsAviaEnabledMapParameters( nPlayer, nAvia ) && IsAviaEnabledScript( nPlayer, nAvia ) );
}
void CUnitCreation::LockAppearPoint( const int nPlayer, const bool bLocked )
{
	if ( !bLockedFlags[nPlayer] && bLocked )
		vLockedAppearPoints[nPlayer] = GetRandomAppearPoint( nPlayer );
	bLockedFlags[nPlayer] += bLocked ? 1 : -1;
}
void CUnitCreation::PlaneLandedSafely( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/ nAvia )
{
	if ( nPlayer >= inGameUnits.size() ) 
		return;

	++inGameUnits[nPlayer].planes[nAvia].nPlanes;
	if ( nPlayer == theDipl.GetMyNumber() && 
			 curTime >= inGameUnits[nPlayer].timeLastCall + inGameUnits[nPlayer].timeRelax &&
			 !theWeather.IsActive() )
	{
		EnableAviationButtons();
	}
}
CVec2 CUnitCreation::GetRandomAppearPoint( const int _nPlayer, const bool bLeave ) const
{
	int nPlayer = _nPlayer;												// neutral planes leave to player  any player's appear point.
	if ( _nPlayer >= inGameUnits.size() )
	{
		for ( int i = 0; i < inGameUnits.size(); ++i )
			if ( inGameUnits[i].vAppearPoints.size() )
			{
				nPlayer = i;
				break;
			}
	}
	NI_ASSERT_T( 0 != inGameUnits[nPlayer].vAppearPoints.size(), NStr::Format("WRONG CALL for player (initial) %d, calculated %d", _nPlayer, nPlayer ) );
	const int nCurrentRandom = nAviationCallNumeber%inGameUnits[nPlayer].vAppearPoints.size();
	if ( bLockedFlags[nPlayer] )
		return vLockedAppearPoints[nPlayer];
	return inGameUnits[nPlayer].vAppearPoints[nCurrentRandom];
}
const char *CUnitCreation::GetRandomAntitankObjectName() const 
{
	return commonInfo.antitankObjects[Random(commonInfo.antitankObjects.size())].c_str();
}
const char * CUnitCreation::GetEntrenchmentName() const 
{
	return commonInfo.szEntrenchment.c_str();
}
const char *CUnitCreation::GetWireFenceName() const 
{
	return commonInfo.szAPFence.c_str();
}
int CUnitCreation::GetNParadropers( const int nPlayer ) const
{
	return inGameUnits[nPlayer].nParadropSquadCount;
}
int CUnitCreation::GetParadropperDBID( const int nPlayer ) const
{
	return pIDB->GetIndex( GetPartyDependentInfo(nPlayer).szParatroopSoldierName.c_str() );
}
const char * CUnitCreation::GetRandomTankPit( const class CVec2 &vSize, const bool bCanDig, float *pfResize ) const
{
	return tankPitInfo.GetRandomTankPit( vSize, bCanDig, pfResize  );	
}
void CUnitCreation::GetCentersOfAllFormationUnits( const SSquadRPGStats *pStats, const CVec2 &vFormCenter, const WORD wFormDir, const int nFormation, const int nUnits, std::list<CVec2> *pCenters ) const
{
	const SSquadRPGStats::SFormation &formation = pStats->formations[nFormation];
	const int nSizeOfFormation = (nUnits == -1) ? formation.order.size() : nUnits;

	CVec2 vRelFormDir = GetVectorByDirection( wFormDir );
	std::swap( vRelFormDir.x, vRelFormDir.y );
	vRelFormDir.y = -vRelFormDir.y;
	
	for ( int j = 0; j < nSizeOfFormation; ++j )
	{
		CVec2 vUnitCenter( vFormCenter + ((formation.order[j].vPos) ^ vRelFormDir) );
		vUnitCenter.x = Clamp( vUnitCenter.x, 0.0f, (float)(theStaticMap.GetSizeX() * SConsts::TILE_SIZE - 1) );
		vUnitCenter.y = Clamp( vUnitCenter.y, 0.0f, (float)(theStaticMap.GetSizeY() * SConsts::TILE_SIZE - 1) );

		pCenters->push_back( vUnitCenter );
	}
}
int CUnitCreation::GetLastCalledAviation( const int nPlayer ) const
{
	if ( nPlayer >= inGameUnits.size() )
		return -1;
	else
		return inGameUnits[nPlayer].nLastCalledAviaType;
}
const CVec2 CUnitCreation::GetFirstIntercectWithMap( const int nPlayer )
{
	const CVec2 vAppearPoint = theUnitCreation.GetRandomAppearPoint( nPlayer, false );

	if ( !theStaticMap.IsPointInside( vAppearPoint ) )
	{
		SRect rect;
		rect.InitRect( CVec2(0,0), 
									 CVec2(theStaticMap.GetSizeX()*SConsts::TILE_SIZE, 0 ),
									 CVec2(theStaticMap.GetSizeX()*SConsts::TILE_SIZE, theStaticMap.GetSizeY()*SConsts::TILE_SIZE),
									 CVec2(0, theStaticMap.GetSizeY()*SConsts::TILE_SIZE) ); 
		
		
		float fIntercect = 0;
		for ( int i = 0; i < 4; ++i )
		{
			const CVec2 vBegin = rect.v[i];
			const CVec2 vEnd = rect.v[(i+1)%4];
			if ( CI_SKEW_CROSS == ClassifyCross( vBegin, vEnd, vAppearPoint, rect.center, &fIntercect ) )
			{
				return vBegin + (vEnd - vBegin) * fIntercect;
			}
		}
	}
	return vAppearPoint;
}
CCommonUnit* CUnitCreation::AddNewFormation( const SSquadRPGStats *pStats, const int nFormation, const float fHP, const float x, const float y, const float z, const WORD wDir, const int nDiplomacy, bool bInitialization, bool bSendToWorld, const int nUnits ) const
{
	const SSquadRPGStats::SFormation &formation = pStats->formations[nFormation];
	CObj<CFormation> pFormation = new CFormation();
	const CVec2 vFormCenter( x, y );
	pFormation->Init( pStats, vFormCenter, z, wDir, pIDB->GetIndex( pStats->szParentName.c_str() ) );
	
	if ( bSendToWorld )
		updater.Update( ACTION_NOTIFY_NEW_UNIT, pFormation );

	std::list<CVec2> centers;
	GetCentersOfAllFormationUnits( pStats, vFormCenter, wDir, nFormation, nUnits, &centers );

	const int nSizeOfFormation = Min( formation.order.size(), (nUnits == -1) ? formation.order.size() : nUnits );
	std::list<CVec2>::iterator iter = centers.begin();
	for ( int j = 0; j < nSizeOfFormation; ++j, ++iter )
	{
		NI_ASSERT_T( iter != centers.end(), "Centers of units of formation incorrectly initialized" );

		const CVec2 unitCoord( *iter );
		const WORD wUnitDir = wDir + formation.order[j].nDir;

		const int id = AddNewUnit( formation.order[j].pSoldier, fHP, unitCoord.x, unitCoord.y, z, pIDB->GetIndex( formation.order[j].szSoldier.c_str() ), wUnitDir, nDiplomacy, pIDB->GetDesc( formation.order[j].szSoldier.c_str() )->eVisType, bInitialization, bSendToWorld );
		IRefCount *pUnit = units[id];
		NI_ASSERT_T( dynamic_cast<CSoldier*>( pUnit ) != 0, "Wrong unit in formation" );
		pFormation->AddNewUnitToSlot( static_cast<CSoldier*>( pUnit ), j );
		if ( bSendToWorld )
			updater.Update( ACTION_NOTIFY_NEW_FORMATION, static_cast<CSoldier*>( pUnit ) );
	}

	NI_ASSERT_T( iter == centers.end(), "Centers of units of formation incorrectly initialized" );

	pFormation->ChangeGeometry( nFormation );
	pFormation->MoveGeometries2Center();
	pFormation->TurnToDir( wDir, false );

	pFormation->AddAvailCmd( ACTION_COMMAND_FOLLOW );

	for ( int i = 0; i < pFormation->Size(); ++i )
	{
		CVec3 vNewCoord = CVec3( (*pFormation)[i]->GetUnitPointInFormation(), 0.0f );
		vNewCoord.x = Clamp( vNewCoord.x, 0.0f, (float)(theStaticMap.GetSizeX() * SConsts::TILE_SIZE - 1) );
		vNewCoord.y = Clamp( vNewCoord.y, 0.0f, (float)(theStaticMap.GetSizeY() * SConsts::TILE_SIZE - 1) );

		(*pFormation)[i]->SetNewCoordinates( vNewCoord );
	}
	
	pFormation->SetSelectable( nDiplomacy == theDipl.GetMyNumber() );

	return pFormation;
}
CCommonUnit* CUnitCreation::CreateSingleUnitFormation( CSoldier *pSoldier ) const
{
	CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( "AISingleUnitFormation" );
	CGDBPtr<SSquadRPGStats> pStats = static_cast<const SSquadRPGStats*>( pIDB->GetRPGStats( pDesc ) );

	CObj<CFormation> pFormation = new CFormation();
	pFormation->Init( pStats, pSoldier->GetCenter(), pSoldier->GetZ(), pSoldier->GetFrontDir(), pIDB->GetIndex( pStats->szParentName.c_str() ) );
	pFormation->Mem2UniqueIdObjs();
	updater.Update( ACTION_NOTIFY_NEW_UNIT, pFormation );

	pFormation->AddUnit( pSoldier, 0 );

	pFormation->ChangeGeometry( 0 );
	pFormation->MoveGeometries2Center();
	pFormation->AddAvailCmd( ACTION_COMMAND_FORM_FORMATION );
	pFormation->AddAvailCmd( ACTION_COMMAND_FOLLOW );

	updater.Update( ACTION_NOTIFY_NEW_FORMATION, pSoldier );

	return pFormation;
}
bool CUnitCreation::IsAntiTank( const SHPObjectRPGStats *pStats ) const
{
	for ( int i = 0; i < commonInfo.antitankObjects.size(); ++i )
	{
		if ( pStats->szParentName == commonInfo.antitankObjects[i] )
			return true;
	}

	return false;
}
bool CUnitCreation::IsAPFence( const SHPObjectRPGStats *pStats ) const
{
	return pStats->szParentName == commonInfo.szAPFence;
}
