#include "StdAfx.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include "../Platform/LegacyText.h"

#include "MOBuilding.h"

#include "../Common/Actions.h"
#include "../Common/Icons.h"
#include "../GameTT/iMission.h"
#include "../Formats/fmtTerrain.h"
#include "../Input/Input.h"
#include "PlayEffect.h"
#include "../Misc/Win32Random.h"
#include "../Main/CameraTurn.h"
#include "../Main/Remastered.h"
CMOBuilding::~CMOBuilding()
{
	for ( CPassangersList::iterator it = passangers.begin(); it != passangers.end(); ++it )
		it->pUnit->SetContainer( 0 );
}
bool CMOBuilding::Create( IRefCount *pAIObjLocal, const SGDBObjectDesc *pDescLocal, int _nSeason, int nFrameIndex, 
												  float fNewHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	nSeason = _nSeason;
	pDesc = pDescLocal;
	nCameraTurn = NCameraTurn::Step();
	nDrawnTurn = GetDrawnTurn();
	pRPG = NGDB::GetRPGStats<SHPObjectRPGStats>( pGDB, pDesc );
	NI_ASSERT_TF( pRPG != 0, NStr::Format("Can't find RPG stats for building \"%s\"", pDesc->szKey.c_str()), return 0 );
	if ( pRPG == 0 )
		return false;
	UpdateModelWithHP( fNewHP / pRPG->fMaxHP, pVOB, true );
	if ( IsDOT() ) 
	{
		checked_cast<IObjVisObj*>(pVisObj.GetPtr())->SetPriority( 91 );
	}
	NI_ASSERT_T( pVisObj != 0, NStr::Format("Can't create building \"%s\" from path \"%s\"", pDesc->szKey.c_str(), pDesc->szPath.c_str()) );
	ISpriteAnimation *pAnim = static_cast<ISpriteAnimation*>( static_cast<IObjVisObj*>( pVisObj.GetPtr() )->GetAnimation() );
	pAnim->SetFrameIndex( nFrameIndex );
	if ( pShadow )
	{
		pAnim = static_cast<ISpriteAnimation*>( static_cast<IObjVisObj*>( pShadow.GetPtr() )->GetAnimation() );
		pAnim->SetFrameIndex( nFrameIndex );
	}
	this->nFrameIndex = nFrameIndex;
	pAIObj = pAIObjLocal;
	fHP = fNewHP / pRPG->fMaxHP;
	ISceneIconBar *pBar;
	if ( GetGlobalVar("MultiplayerGame", 0) == 1 )
		pBar = static_cast<ISceneIconBar*>( pVOB->BuildSceneObject( "icons\\mechhpmp", SCENE_OBJECT_TYPE_ICON, ICON_HP_BAR ) );
	else
		pBar = static_cast<ISceneIconBar*>( pVOB->BuildSceneObject( "icons\\mechhp", SCENE_OBJECT_TYPE_ICON, ICON_HP_BAR ) );
	pBar->SetColor( 0xff00ff00 );
	pBar->SetSize( CVec2(50, 2) );
	GetVisObj()->AddIcon( pBar, ICON_HP_BAR, VNULL3, VNULL3, ICON_HP_BAR, ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	pBar->SetLength( fHP );
	pBar->Enable( false );
	fTraceProbabilityCoeff = GetGlobalVar( "Scene.GunTrace.ProbabilityCoeff", 1.0f );
	fTraceSpeedCoeff = GetGlobalVar( "Scene.GunTrace.SpeedCoeff", 1.0f );
	return true;
}
void CMOBuilding::Visit( IMapObjVisitor *pVisitor )
{
	pVisitor->VisitSprite( pVisObj, IsDOT() ? SGVOGT_FORTIFICATION : pDesc->eGameType, SGVOT_SPRITE );
	if ( pShadow ) 
		pVisitor->VisitSprite( pShadow, SGVOGT_SHADOW, SGVOT_SPRITE );
	if ( pGarbage ) 
		pVisitor->VisitSprite( pGarbage, SGVOGT_TERRAOBJ, SGVOT_SPRITE );
}
int CMOBuilding::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SMapObject*>(this) );
	saver.Add( 2, &passangers );
	saver.Add( 3, &pLocalName );
	saver.Add( 4, &pGarbage );
	saver.Add( 5, &nSeason );
	// The side it is drawn with, so a save made with the camera turned (Remastered)
	// loads knowing it; at the 2003 view nothing is written, as before.
	if ( saver.IsReading() || nCameraTurn != 0 )
		saver.Add( 6, &nCameraTurn );
	if ( saver.IsReading() || nFacing != 0 )
		saver.Add( 7, &nFacing );
	if ( saver.IsReading() || nDrawnTurn != 0 )
		saver.Add( 8, &nFrameIndex );
	if ( saver.IsReading() )
		nDrawnTurn = GetDrawnTurn();
	return 0;
}
bool CMOBuilding::CanEnterOrBoard() const
{
	int nCounter = 0;
	for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
		nCounter += int( it->pUnit->IsFriend() );
	return GetNumTotalSlots() > nCounter;
}
void CMOBuilding::GetActions( CUserActions *pActions, EActionsType eActions ) const
{
	CUserActions actions;
	if ( eActions == IMapObj::ACTIONS_BY ) 
	{
		for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
		{
			if ( it->pUnit->CanSelect() ) 
			{
				actions.SetAction( USER_ACTION_LEAVE );
				break;
			}
		}
	}
	else if ( eActions == IMapObj::ACTIONS_WITH ) 
	{
		if ( ( (GetRPGStats()->eType == SBuildingRPGStats::TYPE_MAIN_RU_STORAGE) || 
			     (GetRPGStats()->eType == SBuildingRPGStats::TYPE_TEMP_RU_STORAGE) ) &&
				 (fHP > 0) )
			actions.SetAction( USER_ACTION_FILL_RU );
		if ( (fHP > 0) && CanEnterOrBoard() )
			actions.SetAction( USER_ACTION_BOARD );
		if ( fHP < 1 ) 
			actions.SetAction( USER_ACTION_ENGINEER_REPAIR_BUILDING );
		if ( fHP > 0 ) 
			actions.SetAction( USER_ACTION_ATTACK );
	}
	actions.SetAction( USER_ACTION_UNKNOWN );
	*pActions |= actions;
}
bool CMOBuilding::Load( IMOUnit *pMO, bool bEnter )
{
	if ( bEnter )
	{
		CVec3 vAdd, vStep;
		GetPassangerIconPlacement( &vAdd, &vStep );
		AddPassanger( passangers, this, pMO, GetVisObj(), vAdd, vStep, ICON_ALIGNMENT_BOTTOM | ICON_ALIGNMENT_HCENTER | ICON_PLACEMENT_VERTICAL );
		// static_cast, the way every other ICON_HP_BAR access reads that slot
		// (MOUnit.cpp, MOUnitInfantry.cpp, WorldBase.cpp): the icon is created
		// in Scene, and a dynamic_cast on it crosses the module boundary. The
		// MSVC ABI compares RTTI by name string, so it worked on Windows; the
		// Itanium ABI (macOS/Linux) compares typeinfo identity, the copies in
		// the two modules do not unify, and the cast returned null - the lock
		// was silently skipped and a garrisoned enemy's bar kept the HP
		// gradient: green at full health, in every mission.
		if( ISceneIconBar *pIcon = static_cast<ISceneIconBar*>( static_cast_ptr<IObjVisObj*>( pMO->pVisObj )->GetIcon(ICON_HP_BAR) ) )
			pIcon->LockBarColor();
	}
	else
	{
		RemovePassanger( passangers, pMO, GetVisObj() );
		// static_cast for the same cross-module reason as the lock above.
		if( ISceneIconBar *pIcon = static_cast<ISceneIconBar*>( static_cast_ptr<IObjVisObj*>( pMO->pVisObj )->GetIcon(ICON_HP_BAR) ) )
			pIcon->UnlockBarColor();
	}

	if ( getenv("BK_AI_TRACE") ) fprintf( stderr, "BK_AI_TRACE: building passengers changed, container %p\n", (void*)static_cast<IMOContainer*>(this) );
	GetSingleton<IInput>()->AddMessage( SGameMessage(MC_UPDATE_WHO_IN_CONTAINER, reinterpret_cast<std::intptr_t>( static_cast<IMOContainer*>(this) )) );
	UpdatePassangers();
	return true;
}
// Where the passengers' HP bars stand: above the sprite's nearest depth.
void CMOBuilding::GetPassangerIconPlacement( CVec3 *pvAdd, CVec3 *pvStep )
{
	*pvAdd = CalcPassangerHPAdd( GetAnim()->GetRect().fDepthLeft + 1 );
	*pvStep = CVec3( 0, 0, GetGlobalVar("MultiplayerGame", 0) == 1 ? 4 : 2 );
}
void CMOBuilding::UpdatePassangers()
{
	const bool bPassangersVisible = IsPassangersVisible( passangers );
	EnablePassangersIcons( passangers, GetVisObj(), bPassangersVisible );
	if ( ISceneIcon *pIcon = GetVisObj()->GetIcon(ICON_HP_BAR) ) 
		pIcon->Enable( IsAlive() && (bPassangersVisible || IsEnemy() || (pVisObj->GetSelectionState() == SGVOSS_SELECTED)) );
}
int CMOBuilding::GetPassangers( IMOUnit **pBuffer, const bool bCanSelectOnly ) const
{
	if ( bCanSelectOnly )
	{
		// count with an integer: the original advanced the NULL pBuffer to
		// count in the counting-mode call (null pointer arithmetic)
		int nCount = 0;
		if ( pBuffer != 0 )
		{
			for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
			{
				if ( it->pUnit->CanSelect() )
				{
					*pBuffer++ = it->pUnit;
					++nCount;
				}
			}
		}
		else
		{
			for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
			{
				if ( it->pUnit->CanSelect() )
					++nCount;
			}
		}
		return nCount;
	}
	else
	{
		if ( pBuffer != 0 ) 
		{
			for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
				*pBuffer++ = it->pUnit;
		}
		return passangers.size();
	}
}
// wDir is the building's map Direction, which the AI passes on
// (CBuilding::SetMapDirection; the 2003 AI passed 0). With Remastered on it is
// the facing the side is drawn by (NCameraTurn::Facing); the sprites stay at
// direction 0, as the 2003 game had them: the side is in the picture.
void CMOBuilding::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	nFacing = NCameraTurn::Facing( wDir );
	// The world places a new object before it adds it to the scene.
	if ( GetDrawnTurn() != nDrawnTurn )
		Redraw( GetSingleton<IVisObjBuilder>(), false );
	pVisObj->SetPlacement( vPos, 0 );
	if ( pShadow )
		pShadow->SetPlacement( vPos, 0 );
	if ( pGarbage ) 
		pGarbage->SetPlacement( vPos, 0 );
}
void CMOBuilding::GetPlacement( CVec3 *pvPos, WORD *pwDir )
{
	*pvPos = pVisObj->GetPosition();
	*pwDir = pVisObj->GetDirection();
}
void CMOBuilding::GetStatus( struct SMissionStatusObject *pStatus ) const
{
	pStatus->nScenarioIndex = -1;
	pStatus->dwIconsStatus = 0;
	pStatus->dwPlayer = -1;
	pStatus->params[0] = PackParams( MINT(fHP * pRPG->fMaxHP), MINT(pRPG->fMaxHP) );
	pStatus->params[1] = 0;
	pStatus->params[2] = 0;
	pStatus->params[3] = 0;
	pStatus->armors[0] = GetRPGStats()->GetArmor( RPG_FRONT );
	pStatus->armors[1] = ( GetRPGStats()->GetArmor( RPG_LEFT ) + GetRPGStats()->GetArmor( RPG_RIGHT ) ) / 2;
	pStatus->armors[2] = GetRPGStats()->GetArmor( RPG_BACK );
	pStatus->armors[3] = GetRPGStats()->GetArmor( RPG_TOP );
	if ( GetRPGStats()->pPrimaryGun && !GetRPGStats()->pPrimaryGun->pWeapon->shells.empty() ) 
	{
		const SWeaponRPGStats::SShell &shell = GetRPGStats()->pPrimaryGun->pWeapon->shells[0];
		pStatus->weaponstats[0] = shell.fDamagePower;
		pStatus->weaponstats[1] = shell.nPiercing;
		pStatus->weaponstats[2] = GetRPGStats()->pPrimaryGun->pWeapon->nCeiling;
	}
	else
		Zero( pStatus->weaponstats );
	if ( IText *pName = GetLocalName() ) 
		NPlatform::CopyWordStringToWide( pStatus->pszName, sizeof(pStatus->pszName) / sizeof(pStatus->pszName[0]), pName->GetString() );
	else
	{
		static std::wstring szName;
		NStr::ToUnicode( &szName, pDesc->szKey );
		NPlatform::CopyWideToWide( pStatus->pszName, sizeof(pStatus->pszName) / sizeof(pStatus->pszName[0]), szName.c_str() );
	}
}
// placement.dir is the map Direction, as for SetPlacement: the Map Editor's
// turn (CGivenPassabilityStObject::TurnForEditor) redraws the side here.
void CMOBuilding::AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	nFacing = NCameraTurn::Facing( placement.dir );
	if ( GetDrawnTurn() != nDrawnTurn )
		Redraw( GetSingleton<IVisObjBuilder>(), true );
	CVec3 vPos;
	AI2Vis( &vPos, placement.center.x, placement.center.y, placement.z );
	pVisObj->SetDirection( 0 );
	pScene->MoveObject( pVisObj, vPos );
	pVisObj->Update( currTime, true );
	if ( pShadow )
	{
		pScene->MoveObject( pShadow, vPos );
		pShadow->Update( currTime, true );
	}
	if ( pGarbage ) 
	{
		pScene->MoveObject( pGarbage, vPos );
		pGarbage->Update( currTime, true );
	}
}
inline int GetBuildingDamageState( float fHP )
{
	if ( fHP > 0.5f )
		return 0;
	else if ( fHP > 0.0f )
		return 1;
	else
		return 2;
}
IVisObj* UpdateVisObj( IVisObj *pVisObj, const std::string &szBaseName, const char *pszSeasonApp, 
											 const char *pszShadowApp, IVisObjBuilder *pVOB )
{
	if ( pVisObj == 0 ) 
	{
		pVisObj = pVOB->BuildObject( (szBaseName + pszSeasonApp + pszShadowApp).c_str(), 0, SGVOT_SPRITE );
		if ( pVisObj == 0 ) 
			pVisObj = pVOB->BuildObject( (szBaseName + pszShadowApp).c_str(), 0, SGVOT_SPRITE );
	}
	else
	{
		if ( pVOB->ChangeObject(pVisObj, (szBaseName + pszSeasonApp + pszShadowApp).c_str(), 0, SGVOT_SPRITE) == false )
		{
			if ( pVOB->ChangeObject(pVisObj, (szBaseName + pszShadowApp).c_str(), 0, SGVOT_SPRITE) == false )
				pVisObj = 0;
		}
	}
	return pVisObj;
}
bool CMOBuilding::UpdateModelWithHP( const float fNewHP, IVisObjBuilder *pVOB, const bool bForced )
{
	const NTimer::STime currTime = GetSingleton<IGameTimer>()->GetGameTime();
	const int nOldState = GetBuildingDamageState( fHP );
	const int nNewState = GetBuildingDamageState( fNewHP );
	if ( (nNewState != nOldState) || bForced )
	{
		const std::string szBaseModelName = NStr::Format( "%s\\%d", GetModelPath().c_str(), nNewState + 1 );
		CPtr<IVisObj> pTempVO = UpdateVisObj( pVisObj, szBaseModelName, GetSeasonApp(nSeason), "", pVOB );
		if ( pTempVO == 0 ) 
			GetSingleton<IScene>()->RemoveObject( pVisObj );
		pVisObj = pTempVO;
		pTempVO = UpdateVisObj( pShadow, szBaseModelName, GetSeasonApp(nSeason), "s", pVOB );
		if ( pTempVO == 0 ) 
			GetSingleton<IScene>()->RemoveObject( pShadow );
		pShadow = pTempVO;
		pTempVO = UpdateVisObj( pGarbage, szBaseModelName, GetSeasonApp(nSeason), "g", pVOB );
		if ( pTempVO == 0 ) 
			GetSingleton<IScene>()->RemoveObject( pGarbage );
		pGarbage = pTempVO;
		return true;
	}
	return false;
}
// The turn of its variant it is drawn with: its own facing (Remastered and
// the Map Editor only; the 2003 game ignored it) plus the camera's step
// (NCameraTurn::DrawnTurn, the rule the Map Editor shares).
int CMOBuilding::GetDrawnTurn() const
{
	return NCameraTurn::DrawnTurn( NCameraTurn::FacingCounts() ? nFacing : 0, nCameraTurn );
}
// A turn the data holds no pictures of is drawn as its own side, and the log
// says so once per variant and turn (the camera turn mod missing or not made
// for this variant would otherwise look like a building that does not turn).
std::string CMOBuilding::GetModelPath() const
{
	const std::string szPath = NCameraTurn::ModelPath( pDesc->szPath, nDrawnTurn );
	if ( nDrawnTurn != 0 && szPath == pDesc->szPath )
	{
		static std::set<std::string> told;
		const std::string szTold = NStr::Format( "%s_r%d", pDesc->szPath.c_str(), nDrawnTurn );
		if ( told.insert( szTold ).second )
			fprintf( stderr, "camera turn: no pictures of %s (its turn %d); drawn as its own side\n", szTold.c_str(), nDrawnTurn );
	}
	return szPath;
}
void CMOBuilding::SetCameraTurn( const int nTurn, IVisObjBuilder *pVOB )
{
	nCameraTurn = nTurn;
	if ( GetDrawnTurn() != nDrawnTurn )
		Redraw( pVOB, true );
}
// Draws it again from GetModelPath. The pictures change in place, so the scene
// keeps the objects it holds; a shadow or ground picture the other side has
// and this one had not is new (added to the scene if it is in it). Its frame
// and its passengers' HP bars, which stand on the sprite's depth, are set
// again for the new pictures.
void CMOBuilding::Redraw( IVisObjBuilder *pVOB, const bool bInScene )
{
	if ( pVisObj == 0 )
		return;
	const bool bHadShadow = pShadow != 0, bHadGarbage = pGarbage != 0;
	nDrawnTurn = GetDrawnTurn();
	UpdateModelWithHP( fHP, pVOB, true );
	if ( pVisObj == 0 )
		return;
	IScene *pScene = GetSingleton<IScene>();
	if ( pShadow != 0 && !bHadShadow && bInScene )
	{
		pShadow->SetPosition( pVisObj->GetPosition() );
		pScene->AddObject( pShadow, SGVOGT_SHADOW );
	}
	if ( pGarbage != 0 && !bHadGarbage && bInScene )
	{
		pGarbage->SetPosition( pVisObj->GetPosition() );
		pScene->AddObject( pGarbage, SGVOGT_TERRAOBJ );
	}
	if ( nFrameIndex >= 0 )
	{
		GetAnim()->SetFrameIndex( nFrameIndex );
		if ( pShadow )
			static_cast<ISpriteAnimation*>( static_cast<IObjVisObj*>( pShadow.GetPtr() )->GetAnimation() )->SetFrameIndex( nFrameIndex );
	}
	CVec3 vAdd, vStep;
	GetPassangerIconPlacement( &vAdd, &vStep );
	for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		if ( ISceneIcon *pIcon = GetVisObj()->GetIcon( it->nHPIconID ) )
			GetVisObj()->AddIcon( pIcon, it->nHPIconID, vAdd, vStep, 0, ICON_ALIGNMENT_BOTTOM | ICON_ALIGNMENT_HCENTER | ICON_PLACEMENT_VERTICAL, false );
	}
	GetVisObj()->AddIcon( 0, 0, VNULL3, VNULL3, 0, 0 );
}
IVisObj* AddDirEffect( const std::string &szEffect, const CVec3 &vPos, const float fHDir, const float fVDir, 
											const NTimer::STime &timeEffect, const NTimer::STime &timePassed, IVisObjBuilder *pVOB, IScene *pScene )
{
	if ( IEffectVisObj *pObj = PlayEffect(szEffect, vPos, timeEffect, false, pScene, pVOB, timePassed) ) 
	{
		SHMatrix matDirection;
		MakeMatrixFromDirection( &matDirection, fHDir, fVDir );
		static_cast<IEffectVisObj*>(pObj)->SetEffectDirection( matDirection );
		return pObj;
	}
	return 0;
}
// The quarter turns it stands turned by, its footprint and points with its
// picture (NCameraTurn::StandingTurn, as the AI turns it): 0 but for a
// facing other than 0 with Remastered on, or in the Map Editor.
int CMOBuilding::GetStandingTurn() const
{
	return NCameraTurn::StandingTurn( pDesc->szPath, true, nFacing * 16384 );
}
// A point of its 1.xml (fire, smoke, a slot; world units from its place) as
// it stands: turned by its standing turn, (x, y) -> (-y, x) each. The camera
// step needs nothing: the points are 3D, so the turned view draws them where
// its turned picture shows that part of the building.
CVec3 CMOBuilding::TurnPoint( const CVec3 &vPoint ) const
{
	CVec3 v = vPoint;
	for ( int i = GetStandingTurn(); i > 0; --i )
		v = CVec3( -v.y, v.x, v.z );
	return v;
}
void CMOBuilding::AddEffectsAtDamagePoints( const int nDamageState, const NTimer::STime &timeEffect, 
																					  const NTimer::STime &timePassed, IVisObjBuilder *pVOB, IScene *pScene )
{
	const SBuildingRPGStats *pRPGStats = GetRPGStats();
	const CVec3 vPos = pVisObj->GetPosition();
	const float fTurn = 90.0f * GetStandingTurn();			// the points' directions are degrees
	if ( !pRPGStats->szSmokeEffect.empty() && !pRPGStats->smokePoints.empty() ) 
	{
		for ( std::vector<SBuildingRPGStats::SFirePoint>::const_iterator it = pRPGStats->smokePoints.begin(); it != pRPGStats->smokePoints.end(); ++it )
		{
			AddDirEffect( pRPGStats->szSmokeEffect, vPos + TurnPoint( it->vWorldPosition ), it->fDirection + fTurn, 
				            it->fVerticalAngle, timeEffect, timePassed, pVOB, pScene );
		}
	}
	if ( (nDamageState == 1) && !pRPGStats->firePoints.empty() ) 
	{
		for ( std::vector<SBuildingRPGStats::SFirePoint>::const_iterator it = pRPGStats->firePoints.begin(); it != pRPGStats->firePoints.end(); ++it )
		{
			if ( !it->szFireEffect.empty() ) 
			{
				AddDirEffect( it->szFireEffect, vPos + TurnPoint( it->vWorldPosition ), it->fDirection + fTurn, 
					            it->fVerticalAngle, timeEffect, timePassed, pVOB, pScene );
			}
		}
	}
}
bool CMOBuilding::AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene *pScene )
{
	const float fNewHP = stats.fHitPoints / pRPG->fMaxHP;
	if ( (UpdateModelWithHP(fNewHP, pVOB) != false) && (fNewHP < fHP) )
	{
		const NTimer::STime currTime = GetSingleton<IGameTimer>()->GetGameTime();
		const NTimer::STime timeEffect = Min( stats.time, currTime );
		const NTimer::STime timePassed = currTime - timeEffect;
		AddEffectsAtDamagePoints( GetBuildingDamageState(fNewHP), timeEffect, timePassed, pVOB, pScene );
		if ( pGarbage ) 
		{
			pScene->RemoveObject( pGarbage );
			pGarbage->SetPosition( pVisObj->GetPosition() );
			pScene->AddObject( pGarbage, SGVOGT_TERRAOBJ );
		}
	}
	if ( fHP != fNewHP ) 
	{
		ISceneIconBar *pBar = static_cast<ISceneIconBar*>( static_cast_ptr<IObjVisObj*>(pVisObj)->GetIcon( ICON_HP_BAR ) );
		if ( pBar )
		{
			pBar->SetLength( fNewHP );
			pBar->SetColor( MakeHPBarColor(fNewHP) );
			if ( fNewHP <= 0 ) 
				pBar->Enable( false );
		}
	}
	fHP = fNewHP;
	return fNewHP > 0;
}
void CMOBuilding::SetIcon( const int nType, IVisObjBuilder *pVOB )
{
	const int nTypeID = nType & 0xffff;
	const char *pszIconName = GetIconName( nTypeID );
	if ( ISceneIcon *pIcon = static_cast<ISceneIcon*>(pVOB->BuildSceneObject(pszIconName, SCENE_OBJECT_TYPE_ICON)) )
	{
		pIcon->Enable( true );
		static_cast_ptr<IObjVisObj*>(pVisObj)->AddIcon( pIcon, nTypeID, VNULL3, VNULL3, nTypeID, ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL );
	}
}
void CMOBuilding::RemoveIcon( const int nType )
{
	static_cast_ptr<IObjVisObj*>(pVisObj)->RemoveIcon( nType & 0xffff );
}
bool CMOBuilding::AIUpdateDiplomacy( const SAINotifyDiplomacy &_diplomacy )
{
	SetDiplomacy( _diplomacy.eDiplomacy );
	if ( GetRPGStats()->eType == SBuildingRPGStats::TYPE_TEMP_RU_STORAGE ) 
	{
		if ( IsFriend() )
		{
			RemoveIcon( ICON_STORAGE_FOE );
			SetIcon( ICON_STORAGE_FRIEND, GetSingleton<IVisObjBuilder>() );
		}
		else
		{
			RemoveIcon( ICON_STORAGE_FRIEND );
			SetIcon( ICON_STORAGE_FOE, GetSingleton<IVisObjBuilder>() );
		}
	}
	if ( ISceneIconBar *pBar = static_cast<ISceneIconBar*>(static_cast_ptr<IObjVisObj*>(pVisObj)->GetIcon(ICON_HP_BAR)) )
		pBar->SetBorderColor( GetGlobalVar(NStr::Format("Scene.PlayerColors.Player%d", _diplomacy.nPlayer), int(0xff000000)) );
	UpdatePassangers();
	return IsFriend();
}
int CMOBuilding::AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager )
{
	switch ( action.typeID ) 
	{
		case ACTION_NOTIFY_SELECTABLE_CHANGED:
			bCanSelect = action.nParam;
			break;
		case ACTION_NOTIFY_STORAGE_CONNECTED:
			if ( action.nParam == 0 ) 
				SetIcon( ICON_STORAGE_NO_SUPPLY, pVOB );
			else
				RemoveIcon( ICON_STORAGE_NO_SUPPLY );
			break;
	}
	return 0;
}
void CMOBuilding::AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB )
{
	if ( (hit.wShell >= hit.pWeapon->shells.size()) || GetRPGStats()->dirExplosions.empty() || GetRPGStats()->szDirExplosionEffect.empty() )
		return;
	if ( hit.pWeapon->shells[hit.wShell].fArea == 0 )
		return;
	const float fAngle = float( hit.wDir ) / 65535.0f * FP_2PI;
	float fMinDiff = 1e5f;
	const SBuildingRPGStats::SDirectionExplosion *pDir = 0;
	for ( std::vector<SBuildingRPGStats::SDirectionExplosion>::const_iterator it = GetRPGStats()->dirExplosions.begin(); it != GetRPGStats()->dirExplosions.end(); ++it )
	{
		const float fDiff = fabsf( it->fDirection - fAngle );
		if ( fDiff < fMinDiff ) 
		{
			fMinDiff = fDiff;
			pDir = &( *it );
		}
	}
	if ( pDir ) 
	{
		AddDirEffect( GetRPGStats()->szDirExplosionEffect, pVisObj->GetPosition() + pDir->vWorldPosition, 
		              pDir->fDirection, pDir->fVerticalAngle, currTime, 0, pVOB, pScene );
	}
}
void CMOBuilding::Select( ISelector *pSelector, bool bSelect, bool bSelectSuper )
{
	pVisObj->Select( bSelect ? SGVOSS_SELECTED : SGVOSS_UNSELECTED );
	if ( ISceneIcon *pIcon = GetVisObj()->GetIcon(ICON_HP_BAR) ) 
		pIcon->Enable( bSelect || IsPassangersVisible(passangers) || IsEnemy() );
}
void CMOBuilding::AIUpdateShot( const struct SAINotifyBaseShot &_shot, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene )
{
	const SAINotifyInfantryShot &shot = *( static_cast<const SAINotifyInfantryShot*>(&_shot) );
	if ( shot.pWeapon && !shot.pWeapon->shells.empty() )
	{
		const NTimer::STime timeEffect = Min( shot.time, currTime );
		const NTimer::STime timePassed = currTime - timeEffect;
		const SWeaponRPGStats::SShell &shell = shot.pWeapon->shells[0];
		const CVec3 &vPos = pVisObj->GetPosition();
		if ( !shell.szFireSound.empty() ) 
			pScene->AddSound( shell.szFireSound.c_str(), vPos, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT, 1, 100, timePassed  );
		const SBuildingRPGStats *pRPGStats = GetRPGStats();
		if ( (shot.nSlot >= 0) && (shot.nSlot < pRPGStats->slots.size()) ) 
		{
			const SBuildingRPGStats::SSlot &slot = pRPGStats->slots[shot.nSlot];
			const CVec3 vSlot = TurnPoint( slot.vWorldPosition );
			// The slot's direction as the view sees it: turned with the building
			// and by the camera step (NCameraTurn::ViewDirection), so a flash
			// facing away from the screen is heard only, as in 2003.
			const WORD wSlotDir = NCameraTurn::ViewDirection( slot.wDirection + GetStandingTurn() * 16384, nCameraTurn );
			if ( !shell.szEffectGunFire.empty() )
			{
				if ( (wSlotDir >= 57344) || (wSlotDir <= 24576) )	// only sound effect
				{
					if ( const char *pszSoundName = pVOB->GetEffectSound(shell.szEffectGunFire) ) 
					{
						pScene->AddSound( pszSoundName, vPos + vSlot, 
															SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT, 1, 100, timePassed );
					}
				}
				else														// visual + sound effect
				{
					PlayEffect( shell.szEffectGunFire, vPos + vSlot, timeEffect, false, pScene, pVOB, timePassed,
											SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT );
				}
			}
			if ( shell.trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE && NWin32Random::Random( 100 ) + 1 <= shell.fTraceProbability  * fTraceProbabilityCoeff * 100.0f )
			{
				pVisObj->Update( currTime );
				const CVec3 vStart = vPos + vSlot;
				CVec3 vEnd;
				AI2Vis( &vEnd, shot.vDestPos );
				UpdateGunTraces( vStart, vEnd, AI2VisX(shell.fSpeed) * shell.fTraceSpeedCoeff * fTraceSpeedCoeff, shot.time, pScene );
			}
		}
	}
}
void CMOBuilding::UpdateGunTraces( const CVec3 &vStart, const CVec3 &vEnd, float fSpeed, NTimer::STime nCurrTime, IScene *pScene )
{
	SGunTrace trace;
	trace.birthTime = nCurrTime;
	trace.vStart = vStart;
	trace.vDir = vEnd - vStart;
	trace.vPoints[0] = vStart;
	trace.vPoints[1] = vStart;
	trace.vPoints[2] = vStart;
	trace.vPoints[3] = vStart;
	// near-zero shell trace speed makes the lifetime exceed int range; MSVC's
	// _ftol yielded 0x80000000 there (deathTime in the past, trace dropped) —
	// keep that behavior, UBSan traps the out-of-range float->int conversion
	const float fLifeTime = fabs( trace.vDir ) / fSpeed;
	// [x64-diag] TEMP: flag traces with absurd geometry (vertical-line artifact hunt)
	if ( !(fabsf( trace.vDir.z ) < 500.0f) || !(fabs( trace.vDir ) < 10000.0f) )
	{
		fprintf( stderr, "[x64-diag] building guntrace start=(%g,%g,%g) end=(%g,%g,%g) speed=%g\n",
						 vStart.x, vStart.y, vStart.z, vEnd.x, vEnd.y, vEnd.z, fSpeed );
		fflush( stderr );
	}
	trace.deathTime = ( fLifeTime >= -2147483648.0f && fLifeTime < 2147483648.0f ? int( fLifeTime ) : ( -2147483647 - 1 ) ) + trace.birthTime;
	pScene->AddGunTrace( trace );
}
