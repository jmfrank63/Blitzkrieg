#include "StdAfx.h"

#include "MOUnitInfantry.h"

#include "../Common/Actions.h"
#include "../Common/Icons.h"
#include "../GameTT/iMission.h"
#include "../AILogic/AILogic.h"
#include "../Formats/fmtTerrain.h"
#include "MOProjectile.h"
#include "PlayEffect.h"
#include "../StreamIO/OptionSystem.h"
#include "../Input/Input.h"
#include "../Misc/Win32Random.h"
#include "ObjectStatus.h"
#include "../Misc/Checker.h"
inline const int GetHPIconState( const float fHP )
{
	if ( fHP <= 0.7f ) 
		return 2;
	else if ( fHP <= 0.9f )
		return 1;
	else
		return 0;
}
inline const char* GetNameWithSeason( const int nSeason, const bool bTakeBloodyInAccount )
{
	const bool bBloody = bTakeBloodyInAccount ? GetGlobalVar( "Options.GFX.Blood", 0 ) : false;

	if ( bBloody ) 
	{
		switch ( nSeason ) 
		{
			case 0: return "\\1b";
			case 1: return "\\1bw";
			case 2: return "\\1ba";
		}
		return "\\1b";
	}
	else
	{
		switch ( nSeason ) 
		{
			case 0: return "\\1";
			case 1: return "\\1w";
			case 2: return "\\1a";
		}
		return "\\1";
	}
}
CMOUnitInfantry::CMOUnitInfantry()
: nDeadCounter( 0 )
{
	fTraceProbabilityCoeff = GetGlobalVar( "Scene.GunTrace.ProbabilityCoeff", 1.0f );
	fTraceSpeedCoeff = GetGlobalVar( "Scene.GunTrace.SpeedCoeff", 1.0f );
}
bool CMOUnitInfantry::Create( IRefCount *pAIObjLocal, const SGDBObjectDesc *pDescLocal, int _nSeason, int nFrameIndex, 
														  float fNewHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	SetSeason( _nSeason );
	pDesc = pDescLocal;
	pRPG = NGDB::GetRPGStats<SHPObjectRPGStats>( pGDB, pDesc );
	NI_ASSERT_TF( pRPG != 0, NStr::Format("Can't find RPG stats for object \"%s\"", pDesc->szKey.c_str()), return 0 );
	if ( pRPG == 0 )
		return false;
	{
		const std::string szModelName = GetNameWithSeason( GetSeason(), true );
		pVisObj = pVOB->BuildObject( (pDesc->szPath + szModelName).c_str(), 0, pDesc->eVisType );
	}
	if ( pVisObj == 0 ) 
	{
		const std::string szModelName = GetNameWithSeason( GetSeason(), false );
		pVisObj = pVOB->BuildObject( (pDesc->szPath + szModelName).c_str(), 0, pDesc->eVisType );
		NI_ASSERT_T( pVisObj != 0, NStr::Format("Can't create object \"%s\" from path \"%s\"", pDesc->szKey.c_str(), pDesc->szPath.c_str()) );
	}
	SetScenarioIndex( nFrameIndex );
	CommonUpdateHP( fNewHP / pRPG->fMaxHP );
	pAIObj = pAIObjLocal;
	ISceneIconBar *pBar;
	if ( GetGlobalVar("MultiplayerGame", 0) == 1 )
		pBar = static_cast<ISceneIconBar*>( pVOB->BuildSceneObject( "icons\\infhpmp", SCENE_OBJECT_TYPE_ICON, ICON_HP_BAR ) );
	else
		pBar = static_cast<ISceneIconBar*>( pVOB->BuildSceneObject( "icons\\infhp", SCENE_OBJECT_TYPE_ICON, ICON_HP_BAR ) );
	pBar->SetSize( CVec2(11, 2) );
	GetVisObj()->AddIcon( pBar, ICON_HP_BAR, VNULL3, VNULL3, ICON_HP_BAR, ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	pBar->SetLength( fHP );
	pBar->SetColor( MakeHPBarColor(fNewHP) );
	pBar->Enable( GetHPIconState(fHP) != 0 );
	std::string szVarName = "Scene.SunLight.";
	szVarName = szVarName + GetGlobalVar( "World.Season", "Summer" ) + ".Direction.";
	vSunDir = CVec3( GetGlobalVar((szVarName + "X").c_str(), 1), GetGlobalVar((szVarName + "Y").c_str(), 1), GetGlobalVar((szVarName + "Z").c_str(), -2) );
	Vis2AI( &vSunDir, vSunDir );
	UpdateVisibility();
	CMOUnit::OnCreate();
	return pVisObj != 0;
}
void CMOUnitInfantry::Visit( IMapObjVisitor *pVisitor )
{
	pVisitor->VisitSprite( pVisObj, pDesc->eGameType, pDesc->eVisType );
	if ( pShadow ) 
		pVisitor->VisitSprite( pShadow, SGVOGT_SHADOW, SGVOT_SPRITE );
}
void CMOUnitInfantry::GetStatus( struct SMissionStatusObject *pStatus ) const
{
	CMOUnit::GetStatus( pStatus );
	GetStatusFromRPGStats( pStatus, GetRPGStats(), IsEnemy() );
}
void CMOUnitInfantry::GetActions( CUserActions *pActions, EActionsType eActions ) const
{
	CUserActions acts;
	GetActionsLocal( eActions, &acts );
	if ( IsValid() && pSquad )
	{
		CUserActions acts2;
		pSquad->GetActions( &acts2, eActions );
		acts |= acts2;
	}
	if ( GetContainer() ) 
	{
		if ( acts.HasAction(USER_ACTION_MOVE) ) 
			pActions->SetAction( USER_ACTION_MOVE );
		if ( acts.HasAction(USER_ACTION_BOARD) ) 
			pActions->SetAction( USER_ACTION_BOARD );
		if ( acts.HasAction(USER_ACTION_LEAVE) ) 
			pActions->SetAction( USER_ACTION_LEAVE );
	}
	else
	{
		acts.RemoveAction( USER_ACTION_LEAVE );
		*pActions |= acts;
	}
}
void CMOUnitInfantry::UpdateVisibility()
{
	if ( GetContainer() ) 
	{
		GetVisObj()->SetVisible( false );
		GetContainer()->UpdatePassangers();
	}
	else
		GetVisObj()->SetVisible( IsVisibleLocal() );
	if ( pShadow )
	{
		if ( !IsVisibleLocal() )
			GetSingleton<IScene>()->RemoveObject( pShadow );
		else
		{
			pShadow->SetPosition( pVisObj->GetPosition() );
			GetSingleton<IScene>()->AddObject( pShadow, SGVOGT_SHADOW );
		}
	}
}
void CMOUnitInfantry::UpdateHPBarVisibility( const float fHP )
{
	if ( ISceneIcon *pIcon = GetVisObj()->GetIcon(ICON_HP_BAR) )
	{
		if ( IsVisibleLocal() && ((GetHPIconState(fHP) > 0) || IsSelected()) )
			pIcon->Enable( true );
		else if ( GetContainer() != 0 ) 
			GetContainer()->UpdatePassangers();
		else
			pIcon->Enable( false );
	}
}
bool CMOUnitInfantry::AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene )
{
	const float fNewHP = stats.fHitPoints / GetRPG()->fMaxHP;

	const int nOldHPIconState = GetHPIconState( fHP );
	const int nNewHPIconState = GetHPIconState( fNewHP );
	if ( nOldHPIconState != nNewHPIconState ) 
	{
		if ( ISceneIcon *pIcon = GetVisObj()->GetIcon(ICON_HP_BAR) )
			UpdateHPBarVisibility( fNewHP );
	}
	CommonUpdateRPGStats( fNewHP, stats, pVOB );
	return fNewHP > 0;
}
void CMOUnitInfantry::AIUpdatePlacement( const struct SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	CMOUnit::AIUpdatePlacement( placement, currTime, pScene );
	if ( ISpriteAnimation *pAnim = GetAnim() )
	{
		if ( (placement.fSpeed > 0) && (pAnim->GetAnimation() > 0) ) 
		{
			const float fAnimSpeed = pAnim->GetSpeed() * float( ( 1000.0 * double( SAIConsts::TILE_SIZE ) ) / ( 3600.0 * 1000.0 ) );
			pAnim->SetAnimSpeedCoeff( placement.fSpeed / fAnimSpeed );
		}
	}
	if ( pShadow )
	{
		CVec3 vPos( placement.center.x, placement.center.y, placement.z );
		CVec3 vShadowPos = vPos;
		float fMultiplier = - 2 * vPos.z / vSunDir.z;
		if ( fMultiplier != 0 )
		{
			if ( !GetSingleton<IAILogic>()->GetIntersectionWithTerrain( &vShadowPos, vPos, vPos + fMultiplier * vSunDir ) )
			{
				vShadowPos.Set( -100.0f, -100.0f, -100.0f );
			}
		}
		AI2Vis( &vShadowPos, vShadowPos );
		pScene->MoveObject( pShadow, vShadowPos );
		pShadow->Update( currTime );
	}
}
IMapObj* CMOUnitInfantry::AIUpdateFireWithProjectile( const SAINotifyNewProjectile &projectile,
																										  const NTimer::STime &currTime, interface IVisObjBuilder *pVOB )
{
	CheckRange( GetRPGStats()->guns, projectile.nGun );
	const SInfantryRPGStats::SGun &gun = GetRPGStats()->guns[projectile.nGun];	
	NI_ASSERT_SLOW_T( gun.pWeapon != 0, NStr::Format("Weapon \"%s\" in gun %d of the soldier \"%s\" are empty", gun.szWeapon.c_str(), projectile.nGun, pDesc->szKey.c_str()) );
	CheckRange( gun.pWeapon->shells, projectile.nShell );
	const SWeaponRPGStats::SShell &shell = gun.pWeapon->shells[projectile.nShell];
	IMOEffect *pMO = 0;
	if ( !shell.szEffectTrajectory.empty() )
	{
		pMO = CreateObject<IMOEffect>( MISSION_MO_PROJECTILE );
		if ( pMO->Create( projectile.pObj, ("effects\\effects\\" + shell.szEffectTrajectory).c_str(), pVOB ) == false )
		{
			pMO->AddRef();
			pMO->Release();
			pMO = 0;
		}
	}
	if ( pMO == 0 ) 
		return 0;
	pMO->SetPlacement( VNULL3, 0 );
	static_cast<CMOProjectile*>(pMO)->Init( projectile.startTime, projectile.flyingTime, CVec3(0, 0, 15) );
	return pMO;
}
int CMOUnitInfantry::AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager )
{
	bool bDieAnimation = false;
	int nAnimation = -1;
	switch ( action.typeID )
	{
		case ACTION_NOTIFY_DIE:
			GetVisObj()->SetDirection( ( rand() % 8 ) * 8192 );
			if ( pShadow )
			{
				pScene->RemoveObject( pShadow );
				pShadow = 0;
			}
		case ACTION_NOTIFY_DIE_LYING:
			nAnimation = GetAnimationFromAction( action.typeID );
		case ACTION_NOTIFY_DIE_TRENCH:
		case ACTION_NOTIFY_DIE_BUILDING:
		case ACTION_NOTIFY_DIE_TRANSPORT:
			SendDeathAcknowledgement( pAckManager, currTime - Min( action.time, currTime )  );
			pAckManager->UnitDead( this, pScene );
			if ( GetContainer() )
				GetContainer()->Load( this, false );
			GetVisObj()->RemoveIcon( -1 );
			SetVisible( !((action.typeID == ACTION_NOTIFY_DIE_BUILDING) || (action.typeID == ACTION_NOTIFY_DIE_TRANSPORT)) );
			bDieAnimation = true;
			break;
		case ACTION_NOTIFY_MOVE:
		case ACTION_NOTIFY_CRAWL:
			break;
		case ACTION_NOTIFY_ANIMATION_CHANGED:
			nAnimation = GetAnimationFromAction( action.nParam >> 16 );
			NI_ASSERT_SLOW_T( nAnimation != -1, NStr::Format("Wrong animation change (%d) for infantry unit", action.nParam) );
			break;
		case ACTION_NOTIFY_CHANGE_VISIBILITY:
			SetVisible( action.nParam );
			UpdateVisibility();
			UpdateHPBarVisibility( fHP );
			break;
		case ACTION_NOTIFY_CHANGE_DBID:
			{
				IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
				pDesc = pGDB->GetDesc( action.nParam );
				pRPG = NGDB::GetRPGStats<SHPObjectRPGStats>( pGDB, pDesc );
				ChangeWithBlood( pVOB );
				ClearLocalName();
				GetVisObj()->AddIcon( 0, 0, VNULL3, VNULL3, 0, 0 );
				if ( pShadow )
				{
					pScene->RemoveObject( pShadow );
					pShadow = 0;
				}
			}
			pScene->RemoveObject( pVisObj );
			pScene->AddObject( pVisObj, SGVOGT_UNIT, pDesc );
			break;
		case ACTION_NOTIFY_OPEN_PARASHUTE:
		case ACTION_NOTIFY_PARASHUTE:
		case ACTION_NOTIFY_FALLING:
		case ACTION_NOTIFY_CLOSE_PARASHUTE:
			if ( pShadow == 0 )
			{
				if ( (pShadow = pVOB->BuildObject("units\\humans\\ussr\\paratroopershadow\\1", 0 , SGVOT_SPRITE)) && IsVisible() )
				{
					pShadow->SetPosition( pVisObj->GetPosition() );
					pScene->AddObject( pShadow, SGVOGT_SHADOW );
				}
				pScene->RemoveObject( pVisObj );
				pScene->AddOutboundObject( pVisObj, SGVOGT_UNIT );
			}
			break;
		default:
			return CMOUnit::AIUpdateActions( action, currTime, pVOB, pScene, pAckManager );
	}
	if ( (nAnimation != -1) && (nDeadCounter < 100) ) 
	{
		IAnimation *pAnimation = GetAnim();
		pAnimation->SetAnimation( nAnimation );
		pAnimation->SetStartTime( Min(action.time, currTime) );
		if ( pShadow )
		{
			IAnimation *pAnim = static_cast_ptr<IObjVisObj*>(pShadow)->GetAnimation();
			pAnim->SetAnimation( nAnimation );
			pAnim->SetStartTime( Min(action.time, currTime) );
		}
		if ( CanShowIcons() ) 
			GetVisObj()->AddIcon( 0, 0, GetIconAddValue(), VNULL3, 0, 0 );
		if ( bDieAnimation ) 
			nDeadCounter = 100;
	}

	return 0;
}
void CMOUnitInfantry::AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB )
{
	if ( hit.wShell >= hit.pWeapon->shells.size() )
		return;
	const SWeaponRPGStats::SShell &shell = hit.pWeapon->shells[hit.wShell];
	const CVec3 &vPos = pVisObj->GetPosition();
	if ( (hit.eHitType != SAINotifyHitInfo::EHT_HIT) || (shell.fArea > 0) ) 
		PlayEffect( shell.szEffectHitGround, vPos, currTime, false, pScene, pVOB, 0, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT );
	if ( shell.HasCraters() ) 
		SetCraterEffect( shell.GetRandomCrater(), GetSeason(), vPos, 110, pScene, pVOB );
	if ( shell.flashExplosion.HasFlash() ) 
		SetFlashEffect( shell.flashExplosion, currTime, vPos, GetFlashExpColor(), pScene, pVOB );
}
void CMOUnitInfantry::Select( ISelector *pSelector, bool bSelect, bool bSelectSuper )
{
	if ( bSelectSuper && pSquad )
		pSquad->Select( pSelector, bSelect, bSelectSuper );
	else
	{
		pVisObj->Select( bSelect ? SGVOSS_SELECTED : SGVOSS_UNSELECTED );
		UpdateHPBarVisibility( fHP );
	}
}
void CMOUnitInfantry::AIUpdateShot( const struct SAINotifyBaseShot &_shot, const NTimer::STime &currTime, 
																		  IVisObjBuilder *pVOB, IScene *pScene )
{
	const SAINotifyInfantryShot &shot = *( static_cast<const SAINotifyInfantryShot*>(&_shot) );
	const SWeaponRPGStats::SShell &shell = shot.pWeapon->shells[shot.cShell];
	if ( shell.trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE && NWin32Random::Random( 100 ) + 1 <= shell.fTraceProbability  * fTraceProbabilityCoeff * 100.0f )
	{
		pVisObj->Update( currTime );
		const CVec3 vStart = pVisObj->GetPosition() + CVec3(0,0,15);
		CVec3 vEnd;
		AI2Vis( &vEnd, shot.vDestPos );
		UpdateGunTraces( vStart, vEnd, AI2VisX(shell.fSpeed) * shell.fTraceSpeedCoeff * fTraceSpeedCoeff, shot.time, pScene );
	}
	if ( !shell.szFireSound.empty() )
	{
		pScene->AddSound( shell.szFireSound.c_str(), pVisObj->GetPosition(),
											SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT, 1, 100 );
	}
}
void CMOUnitInfantry::MakeVisible( const bool bVisible )
{
	if ( GetContainer() ) 
	{
		GetVisObj()->SetVisible( false );
		GetContainer()->UpdatePassangers();
	}
	else
		GetVisObj()->SetVisible( bVisible );
}
void CMOUnitInfantry::SetSquad( interface IMOSquad *_pSquad )
{
	if ( pSquad )
		pSquad->Load( this, false );
	pSquad = _pSquad;
	if ( pSquad )
	{
		pSquad->Load( this, true );
		GetSingleton<IInput>()->AddMessage( SGameMessage(MC_UPDATE_WHO_IN_CONTAINER, static_cast<int>( reinterpret_cast<std::uintptr_t>( GetContainer() ) )) );
	}
	else
	{
		if ( GetObserver() )
			GetObserver()->RemoveUnit();
		SetObserver( 0 );
	}
}
void CMOUnitInfantry::SetContainer( IMOContainer *_pContainer )
{
	CMOUnit::SetContainer( _pContainer );
	if ( _pContainer == 0 ) 
	{
		IObjVisObj *pVisObj = GetVisObj();
		for ( int i = 1; i < ICON_NUM_ICONS; ++i )
		{
			if ( ISceneIcon *pIcon = pVisObj->GetIcon(i) )
				pIcon->Enable( true );
		}
	}
}
bool CMOUnitInfantry::ChangeWithBlood( IVisObjBuilder *pVOB )
{
	bool bChanged = false;
	{
		const std::string szModelName = GetNameWithSeason( GetSeason(), true );
		bChanged = pVOB->ChangeObject( pVisObj, (pDesc->szPath + szModelName).c_str(), 0, pDesc->eVisType );
	}
	if ( !bChanged ) 
	{
		const std::string szModelName = GetNameWithSeason( GetSeason(), false );
		bChanged = pVOB->ChangeObject( pVisObj, (pDesc->szPath + szModelName).c_str(), 0, pDesc->eVisType );
	}
	return bChanged;
}
int CMOUnitInfantry::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMOUnit*>(this) );
	saver.Add( 2, &pSquad );
	saver.Add( 3, &nDeadCounter );
	saver.Add( 4, &vSunDir );
	return 0;
}
void CMOUnitInfantry::SetHPSimpleBar( bool bSimple )
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	RemoveIcon( ICON_HP_BAR );
	ISceneIconBar *pBar;
	if ( bSimple )
		pBar = static_cast<ISceneIconBar*>( pVOB->BuildSceneObject( "icons\\infhp", SCENE_OBJECT_TYPE_ICON, ICON_HP_BAR ) );
	else
		pBar = static_cast<ISceneIconBar*>( pVOB->BuildSceneObject( "icons\\infhpmp", SCENE_OBJECT_TYPE_ICON, ICON_HP_BAR ) );
	if ( bSimple )
		pBar->ForceThinIcon();
	pBar->SetSize( CVec2(11, 2) );
	GetVisObj()->AddIcon( pBar, ICON_HP_BAR, VNULL3, VNULL3, ICON_HP_BAR, ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	pBar->SetLength( fHP );
	pBar->SetColor( MakeHPBarColor(fHP) );
	pBar->Enable( GetHPIconState(fHP) != 0 );
	pBar->SetBorderColor( GetGlobalVar(NStr::Format("Scene.PlayerColors.Player%d", GetPlayerIndex()), int(0xff000000)) );
}
