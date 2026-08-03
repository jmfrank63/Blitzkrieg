#include "StdAfx.h"

#include "MOUnitMechanical.h"

#include "../Misc/Win32Random.h"
#include "../Input/Input.h"
#include "../Common/Actions.h"
#include "../Common/AdditionalActions.h"
#include "../Common/World.h"
#include "../GameTT/iMission.h"
#include "../Common/Icons.h"
#include "../AILogic/AILogic.h"
#include "../Formats/fmtTerrain.h"
#include "MOProjectile.h"
#include "PlayEffect.h"
#include "Season.h"
#include "ObjectStatus.h"
#include "../Misc/Checker.h"
#include "../GFX/GFX.H"
#include "MOUnitInfantry.h"
#define MAX_NUM_EXT_PASSANGERS 6
#define INTERIM_STEP 10
CMOUnitMechanical::CMOUnitMechanical()
{
	bInstalled = true;
	bArtilleryHooked = false;
	wMoveSoundID = wNonCycleSoundID = 0;
	bDiveMove = false;
	nNumExtPassangers = 0;
	fTraceProbabilityCoeff = GetGlobalVar( "Scene.GunTrace.ProbabilityCoeff", 1.0f );
	fTraceSpeedCoeff = GetGlobalVar( "Scene.GunTrace.SpeedCoeff", 1.0f );
	fTrackLifeCoeff = GetGlobalVar( "Options.GFX.DensityCoeff", 100.0f ) * 0.01f;
	bInEditor = ( GetGlobalVar( "editor", 0 ) == 1 );
	bSkipTrack = true;
}
CMOUnitMechanical::~CMOUnitMechanical()
{
	if ( 0 != wMoveSoundID )
	{
		GetSingleton<IScene>()->RemoveSound( wMoveSoundID );
		wMoveSoundID = 0;
	}
	if ( 0 != wNonCycleSoundID )
	{
		GetSingleton<IScene>()->RemoveSound( wNonCycleSoundID );
		wNonCycleSoundID = 0;
	}
	for ( CPassangersList::iterator it = passangers.begin(); it != passangers.end(); ++it )
		it->pUnit->SetContainer( 0 );
}
bool CMOUnitMechanical::Create( IRefCount *pAIObjLocal, const SGDBObjectDesc *pDescLocal, int _nSeason, int nFrameIndex, 
															  float fNewHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	SetSeason( _nSeason );
	pDesc = pDescLocal;
	pRPG = NGDB::GetRPGStats<SHPObjectRPGStats>( pGDB, pDesc );
	NI_ASSERT_TF( pRPG != 0, NStr::Format("Can't find RPG stats for object \"%s\"", pDesc->szKey.c_str()), return 0 );
	if ( pRPG == 0 )
		return false;
	const std::string szModelName = "\\1";
	const std::string szTextureName = GetSeason() == 0 ? "\\1" : (GetSeason() == 1 ? "\\1w" : "\\1a");
	pVisObj = pVOB->BuildObject( (pDesc->szPath + szModelName).c_str(), (pDesc->szPath + szTextureName).c_str(), pDesc->eVisType );
	NI_ASSERT_T( pVisObj != 0, NStr::Format("Can't create object \"%s\" from path \"%s\"", pDesc->szKey.c_str(), pDesc->szPath.c_str()) );
	SetScenarioIndex( nFrameIndex );
	UpdateModelWithHP( fNewHP / pRPG->fMaxHP, pVOB );
	CommonUpdateHP( fNewHP / pRPG->fMaxHP );
	pAIObj = pAIObjLocal;
	ISceneIconBar *pBar;
	if ( GetGlobalVar("MultiplayerGame", 0) == 1 )
		pBar = static_cast<ISceneIconBar*>( pVOB->BuildSceneObject( "icons\\mechhpmp", SCENE_OBJECT_TYPE_ICON, ICON_HP_BAR ) );
	else
		pBar = static_cast<ISceneIconBar*>( pVOB->BuildSceneObject( "icons\\mechhp", SCENE_OBJECT_TYPE_ICON, ICON_HP_BAR ) );
	pBar->SetSize( CVec2(50, 2) );
	GetVisObj()->AddIcon( pBar, ICON_HP_BAR, VNULL3, VNULL3, ICON_HP_BAR, ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
	pBar->SetLength( fHP );
	pBar->SetColor( MakeHPBarColor(fHP) );
	IMatrixEffectorLeveling *pEffector = CreateObject<IMatrixEffectorLeveling>( ANIM_EFFECTOR_LEVELING );
	pEffector->SetupTimes( 0, -1 );
	pEffector->SetupData( V3_AXIS_Z, 0 );
	GetAnim()->AddEffector( pEffector, ANIM_EFFECTOR_LEVELING, -2 );
	GetVisObj()->SetVisible( IsVisibleLocal() );
	if ( pExtPassangers ) 
		pExtPassangers->SetVisible( IsVisibleLocal() );
	CMOUnit::OnCreate();
	return pVisObj != 0;
}
void CMOUnitMechanical::PrepareToRemove()
{
	for ( CEffectsList::iterator it = effects.begin(); it != effects.end(); ++it )
	{
		if ( it->pEffect != 0 ) 
			it->pEffect->Stop();
	}
}
inline int GetUnitDamageState( float fNewHP ) { return fNewHP > 0 ? 0 : 1; }
void CMOUnitMechanical::UpdateModelWithHP( const float fNewHP, IVisObjBuilder *pVOB )
{
	const int nOldState = GetUnitDamageState( fHP );
	const int nNewState = GetUnitDamageState( fNewHP );
	if ( nNewState != nOldState )
	{
		pVOB->ChangeObject( pVisObj, 0, NStr::Format("%s\\%d%s", pDesc->szPath.c_str(), nNewState + 1, GetSeasonApp(GetSeason())), pDesc->eVisType );
		if ( nNewState == 1 ) 
			GetVisObj()->RemoveEffector( -1, -1 );
	}
}
void CMOUnitMechanical::Visit( IMapObjVisitor *pVisitor )
{
	const bool bOutbound = GetRPGStats()->type == RPG_TYPE_ART_SUPER;
	pVisitor->VisitMesh( pVisObj, pDesc->eGameType, pDesc->eVisType, bOutbound );
}
void CMOUnitMechanical::GetStatus( struct SMissionStatusObject *pStatus ) const
{
	CMOUnit::GetStatus( pStatus );
	if ( GetRPGStats()->IsTransport() ) 
		pStatus->params[1] = PackParams( GetAmmo(0), 1000 );
	GetStatusFromRPGStats( pStatus, GetRPGStats(), IsEnemy() );
}
void CMOUnitMechanical::GetActions( CUserActions *pActions, EActionsType eActions ) const
{
	CUserActions actions;
	GetActionsLocal( eActions, &actions );
	if ( IsValid() )
	{
		if ( eActions == IMapObj::ACTIONS_BY ) 
		{
			if ( GetNumFreeSlots() == GetNumTotalSlots() ) 
				actions.RemoveAction( USER_ACTION_LEAVE );	
			if ( bInstalled ) 
				actions.RemoveAction( USER_ACTION_INSTALL );
			else
			{
				actions.RemoveAction( USER_ACTION_UNINSTALL );
				actions.RemoveAction( USER_ACTION_ATTACK );
				actions.RemoveAction( USER_ACTION_RANGING );
				actions.RemoveAction( USER_ACTION_SUPPRESS );
				actions.RemoveAction( USER_ACTION_AMBUSH );
			}
			if ( bArtilleryHooked ) 
			{
				actions.RemoveAction( USER_ACTION_HOOK_ARTILLERY );
				actions.RemoveAction( USER_ACTION_LEAVE );
			}
			else
				actions.RemoveAction( USER_ACTION_DEPLOY_ARTILLERY );
		}
		else if ( eActions == IMapObj::ACTIONS_WITH ) 
		{
			if ( !CanSelect() ) 
				actions.RemoveAction( USER_ACTION_HOOK_ARTILLERY );
			if ( bArtilleryHooked ) 
				actions.RemoveAction( USER_ACTION_HOOK_ARTILLERY );
			if ( IsNeutral() ) 
				actions.RemoveAction( USER_ACTION_BOARD );
		}
	}
	else
	{
		NPlatform::BreakIntoDebugger();
	}
	*pActions |= actions;
}
bool CMOUnitMechanical::Load( IMOUnit *pMO, bool bEnter )
{
	if ( bEnter )
	{
		if ( GetGlobalVar("MultiplayerGame" ,0) == 1 )
		{
			if ( CMOUnitInfantry *pMOInf = dynamic_cast<CMOUnitInfantry*>(pMO) )
				pMOInf->SetHPSimpleBar();
		}
		AddPassanger( passangers, this, pMO, GetVisObj(), CVec3(0, 0, 2), CVec3(0, 0, 2), ICON_ALIGNMENT_TOP | ICON_ALIGNMENT_HCENTER | ICON_PLACEMENT_VERTICAL );
	}
	else
	{
		if ( GetGlobalVar("MultiplayerGame" ,0) == 1 )
		{
			if ( CMOUnitInfantry *pMOInf = dynamic_cast<CMOUnitInfantry*>(pMO) )
				pMOInf->SetHPSimpleBar( false );
		}
		RemovePassanger( passangers, pMO, GetVisObj() );
	}
	GetSingleton<IInput>()->AddMessage( SGameMessage(MC_UPDATE_WHO_IN_CONTAINER, static_cast<int>( reinterpret_cast<std::uintptr_t>( static_cast<IMOContainer*>(this) ) )) );
	UpdatePassangers();
	return true;
}
int CMOUnitMechanical::GetPassangers( IMOUnit **pBuffer, const bool bCanSelectOnly ) const
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
void CMOUnitMechanical::ChangeExtPassangers( IScene *pScene, IVisObjBuilder *pVOB )
{
	const std::string szModelName = NStr::Format( "%s\\%dp", pDesc->szPath.c_str(), nNumExtPassangers );
	const std::string szTextureName = NStr::Format( "%s\\1p%s", pDesc->szPath.c_str(), GetSeasonApp2(GetSeason()) );
	if ( pExtPassangers == 0 ) 
	{
		pExtPassangers = static_cast<IMeshVisObj*>( pVOB->BuildObject( szModelName.c_str(), szTextureName.c_str(), SGVOT_MESH ) );
		if ( pExtPassangers ) 
		{
			pExtPassangers->SetPosition( GetVisObj()->GetPosition() );
			pExtPassangers->SetDirection( GetVisObj()->GetDirection() );
			pExtPassangers->SetAnim( GetVisObj()->GetAnimation() );
			pExtPassangers->SetVisible( IsVisibleLocal() );
			pScene->AddObject( pExtPassangers, SGVOGT_UNIT );
		}
	}
	else
	{
		pVOB->ChangeObject( pExtPassangers, szModelName.c_str(), szTextureName.c_str(), SGVOT_MESH );
		pExtPassangers->SetAnim( GetVisObj()->GetAnimation() );
		pExtPassangers->SetVisible( IsVisibleLocal() );
	}
}
void CMOUnitMechanical::UpdatePassangers()
{
	EnablePassangersIcons( passangers, GetVisObj(), IsFriend() || IsPassangersVisible(passangers) );
	{
		IScene *pScene = GetSingleton<IScene>();
		IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
		if ( passangers.empty() && nNumExtPassangers > 0 ) 
		{
			pScene->RemoveObject( pExtPassangers );
			pExtPassangers = 0;
		}
		else
		{
			const int nNumPassangers = passangers.size();
			if ( (nNumPassangers >= MAX_NUM_EXT_PASSANGERS) && (nNumExtPassangers < MAX_NUM_EXT_PASSANGERS) ) 
			{
				nNumExtPassangers = MAX_NUM_EXT_PASSANGERS;
				ChangeExtPassangers( pScene, pVOB );
			}
			else if ( (nNumPassangers < MAX_NUM_EXT_PASSANGERS) && (nNumPassangers != nNumExtPassangers) )
			{
				nNumExtPassangers = nNumPassangers;
				ChangeExtPassangers( pScene, pVOB );
			}
		}
	}
}
void CMOUnitMechanical::RemoveExhaustedEffects( const NTimer::STime &time )
{
	for ( CEffectsList::iterator it = effects.begin(); it != effects.end(); )
	{
		if ( it->pEffect->IsFinished(time) )
			it = effects.erase( it );
		else
			++it;
	}
}
void CMOUnitMechanical::UpdateAttachedEffects( const NTimer::STime &currTime, IScene *pScene )
{
	RemoveExhaustedEffects( currTime );
	if ( !HasEffects() )
		return;
	pVisObj->Update( currTime );
	const int nNumNodes = GetAnim()->GetNumNodes();
	const SHMatrix *matrices = GetVisObj()->GetMatrices();
	for ( CEffectsList::iterator it = effects.begin(); it != effects.end(); ++it )
	{
		if ( it->nPointIndex >= nNumNodes ) 
			continue;
		const SHMatrix &matrix = matrices[it->nPointIndex];
		const CVec3 vNewPos = matrix.GetTrans3();
		CVec3 vPos = it->pEffect->GetPosition();
		if ( fabs2(vNewPos - vPos) >= 25 )
		{
			NTimer::STime diff = currTime - it->timeLastUpdate;
			const CVec3 vPosStep = ( vNewPos - vPos ) / ( float(diff) / INTERIM_STEP );
			while ( diff > INTERIM_STEP ) 
			{
				it->timeLastUpdate += INTERIM_STEP;
				diff -= INTERIM_STEP;
				vPos += vPosStep;
				if ( (pScene == 0) || (pScene->MoveObject(it->pEffect, vPos) == false) ) 
					it->pEffect->SetPlacement( vPos, 0 );
				it->pEffect->Update( it->timeLastUpdate );
			}
		}
		if ( (pScene == 0) || (pScene->MoveObject(it->pEffect, vNewPos) == false) ) 
			it->pEffect->SetPlacement( vNewPos, 0 );
		it->pEffect->SetEffectDirection( matrix );
		it->pEffect->SetSuspendedState( !(GetVisObj()->IsVisible()) );
	}
}
bool CMOUnitMechanical::Update( const NTimer::STime &currTime )
{
	for ( CModelChangesList::iterator it = modelchanges.begin(); it != modelchanges.end(); )
	{
		if ( it->time <= currTime ) 
		{
			ChangeModel( it->szModelName, it->time, it->time );
			it = modelchanges.erase( it );
		}
		else
			++it;
	}
	for ( CAnimChangeList::iterator it = animchanges.begin(); it != animchanges.end(); )
	{
		if ( it->time <= currTime )
		{
			GetAnim()->SetAnimation( it->nAnim );
			GetAnim()->SetStartTime( it->time );
			it = animchanges.erase( it );
		}
		else
			++it;
	}
	UpdateAttachedEffects( currTime );
	return modelchanges.empty() && animchanges.empty() && effects.empty();
}
int CMOUnitMechanical::ChangeModel( const std::string &szModelName, const NTimer::STime &currTime, const NTimer::STime &timeChange )
{
	if ( timeChange <= currTime )
	{
		CPtr<IMatrixEffector> pEffector = GetAnim()->GetEffector( ANIM_EFFECTOR_LEVELING, -2 );
		GetSingleton<IVisObjBuilder>()->ChangeObject( pVisObj, (pDesc->szPath + "\\" + szModelName).c_str(), 0, pDesc->eVisType );
		GetAnim()->AddEffector( pEffector, ANIM_EFFECTOR_LEVELING, -2 );
		return 0;
	}
	else
	{
		modelchanges.push_back( SModelChange(szModelName, timeChange) );
		return 1;
	}
}
void CMOUnitMechanical::ChangeState( EUnitState state, const NTimer::STime &currTime )
{
	if ( GetCurrState() == state ) 
		return;
	SetCurrState( state );
}
void CMOUnitMechanical::AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	CMOUnit::AIUpdatePlacement( placement, currTime, pScene );
	if ( 0 != wMoveSoundID )
		pScene->SetSoundPos( wMoveSoundID, pVisObj->GetPosition() );
	if ( 0 != wNonCycleSoundID )
		pScene->SetSoundPos( wNonCycleSoundID, pVisObj->GetPosition() );
	UpdateAttachedEffects( currTime, pScene );
	IMeshAnimation *pAnim = GetAnim();
	if ( IMatrixEffectorLeveling *pEffector = static_cast<IMatrixEffectorLeveling*>( pAnim->GetEffector( ANIM_EFFECTOR_LEVELING, -2 ) ) )
	{
		pVisObj->Update( currTime );
		SHMatrix matInvPlacement;
		Invert( &matInvPlacement, GetVisObj()->GetPlacement() );
		CVec3 vNormal = DWORDToVec3( placement.dwNormal );
		Normalize( &vNormal );
		matInvPlacement.RotateVector( &vNormal, vNormal );
		pEffector->SetupData( vNormal, currTime );
	}
	if ( pExtPassangers ) 
	{
		CVec3 vPos;
		AI2Vis( &vPos, placement.center.x, placement.center.y, placement.z );
		pExtPassangers->SetDirection( placement.dir );
		pScene->MoveObject( pExtPassangers, vPos );
	}
	const SMechUnitRPGStats *pStats = GetRPGStats();
	if ( pStats->bLeavesTracks )
	{
		const CVec3 &vPos = GetVisObj()->GetPosition();
		const CVec3 speed = vPos - vLastPos;
		const SHMatrix mPlacement = GetVisObj()->GetBasePlacement();
		CVec3 dir = CVec3( 0, 1, 0 );
		mPlacement.RotateVector( &dir, dir );
		bool dirChanged = ((dir * speed >= 0) != bLastTracedDir);
		float fSqRange = fabs2(vPos - vLastPos) * 4;
		if ( fSqRange > fTraceLenSq || ( fSqRange * 4 > fTraceLenSq && ( abs(wLastDir - placement.dir) > 1000 || abs(wLastDir - placement.dir) > 63000 ) ) )
		{
			CVec3 corner = CVec3( pStats->vAABBVisCenter.x, pStats->vAABBVisCenter.y, 0 );
			CVec3 shift = CVec3( pStats->vAABBHalfSize.x, 0, 0 );
			AI2Vis( &corner );
			AI2Vis( &shift );
			mPlacement.RotateVector( &corner, corner );
			mPlacement.RotateVector( &shift, shift );
			corner += vPos;
			SMechTrace trace;
			trace.vCorners[2] = corner - ( 1.0f - 2.0f * pStats->fTrackOffset ) * shift;
			trace.vCorners[3] = corner - ( 1.0f - 2.0f * ( pStats->fTrackOffset + pStats->fTrackWidth ) ) * shift;
			LeaveTrace( &trace, placement, currTime, false, pStats, vPos, bLastTracedDir, dir, pScene );
			trace.vCorners[2] = corner + ( 1.0f - 2.0f * ( pStats->fTrackOffset + pStats->fTrackWidth ) ) * shift;
			trace.vCorners[3] = corner + ( 1.0f - 2.0f * pStats->fTrackOffset ) * shift;
			LeaveTrace( &trace, placement, currTime, true, pStats, vPos, bLastTracedDir, dir, pScene );
			if ( !bSkipTrack )
			{
				if ( dirChanged )
					bLastTracedDir = !bLastTracedDir;
				else
					vLastPos = vPos;
				wLastDir = placement.dir;
			}
			bSkipTrack = !bSkipTrack;
		}
	}
}
void CMOUnitMechanical::LeaveTrace( SMechTrace *pTrace, const SAINotifyPlacement &placement, const NTimer::STime &currTime, bool secondTrack,	const SMechUnitRPGStats *pStats, const CVec3 &vPos, bool isForward, const CVec3 &dir, IScene *pScene )
{
	const int idx = secondTrack ? 2 : 0;
	pTrace->birthTime = currTime;
	pTrace->deathTime = currTime + pStats->nTrackLifetime * fTrackLifeCoeff;
	pTrace->alpha = (1.0f - pStats->fTrackIntensity) * 255.0;
	DWORD col = ((DWORD)(pTrace->alpha));
	pTrace->dwColor = 0xFF000000 || col << 16 || col << 8 || col;
	if ( abs(placement.dir - wLastDir) < 8000 || abs(placement.dir - wLastDir) > 56000 )
	{
		pTrace->vCorners[0] = vLastTracedCorners[idx];
		pTrace->vCorners[1] = vLastTracedCorners[idx + 1];
	}
	else
	{
		if ( !isForward )
		{
			pTrace->vCorners[0] = pTrace->vCorners[2] + dir * (pStats->vAABBVisHalfSize.y * (1.0f - 2.0f * pStats->fTrackEnd ));
			pTrace->vCorners[1] = pTrace->vCorners[3] + dir * (pStats->vAABBVisHalfSize.y * (1.0f - 2.0f * pStats->fTrackEnd ));
		}
		else
		{
			pTrace->vCorners[0] = pTrace->vCorners[2] - dir * (pStats->vAABBVisHalfSize.y * (1.0f - 2.0f * pStats->fTrackStart ));
			pTrace->vCorners[1] = pTrace->vCorners[3] - dir * (pStats->vAABBVisHalfSize.y * (1.0f - 2.0f * pStats->fTrackStart ));
		}
	}
	if ( !isForward )
	{
		pTrace->vCorners[2] -= dir * (pStats->vAABBVisHalfSize.y * (1.0f - 2.0f * pStats->fTrackEnd ));
		pTrace->vCorners[3] -= dir * (pStats->vAABBVisHalfSize.y * (1.0f - 2.0f * pStats->fTrackEnd ));
		CVec3 temp = pTrace->vCorners[2];
		pTrace->vCorners[2] = pTrace->vCorners[3];
		pTrace->vCorners[3] = temp;
		temp = pTrace->vCorners[0];
		pTrace->vCorners[0] = pTrace->vCorners[1];
		pTrace->vCorners[1] = temp;	
		if ( !bSkipTrack )
		{
			vLastTracedCorners[idx] = pTrace->vCorners[3];
			vLastTracedCorners[idx + 1] = pTrace->vCorners[2];
		}
	}
	else
	{
		pTrace->vCorners[2] += dir * (pStats->vAABBVisHalfSize.y * (1.0f - 2.0f * pStats->fTrackStart ));
		pTrace->vCorners[3] += dir * (pStats->vAABBVisHalfSize.y * (1.0f - 2.0f * pStats->fTrackStart ));
		if ( !bSkipTrack )
		{
			vLastTracedCorners[idx] = pTrace->vCorners[2];
			vLastTracedCorners[idx + 1] = pTrace->vCorners[3];
		}
	}
	pTrace->vPos.x = (pTrace->vCorners[0].x + pTrace->vCorners[3].x) / 2;
	pTrace->vPos.y = (pTrace->vCorners[0].y + pTrace->vCorners[3].y) / 2;
	pTrace->vPos.z = vPos.z;
	pTrace->nNumTracks = int( fabs(pTrace->vCorners[0] - pTrace->vCorners[3]) ) / 10 + 1;
	if ( IsVisible() && !bInEditor )
	{
		if ( placement.cSoil & STerrTypeDesc::ESP_TRACE && !bSkipTrack )
		{
			pScene->AddMechTrace( *pTrace );
		}
		if ( pStats->szEffectWheelDust.length() != 0 && (placement.cSoil & STerrTypeDesc::ESP_DUST) && !pScene->IsRaining() )
		{		
			CVec3 vDustPos = pTrace->vCorners[2];
			if ( isForward )
				vDustPos -= dir * (pStats->vAABBVisHalfSize.y * 1.5f * (1 - pStats->fTrackStart - pStats->fTrackEnd ));
			else
				vDustPos += dir * (pStats->vAABBVisHalfSize.y * 1.5f * (1 - pStats->fTrackStart - pStats->fTrackEnd ));
			float fSideOffset = pStats->vAABBVisHalfSize.y * pStats->fTrackOffset;
			if ( secondTrack )
				vDustPos.Set( vDustPos.x + dir.y * fSideOffset, vDustPos.y - dir.x * fSideOffset, vDustPos.z );
			else
				vDustPos.Set( vDustPos.x - dir.y * fSideOffset, vDustPos.y + dir.x * fSideOffset, vDustPos.z );
			PlayEffect( pStats->szEffectWheelDust, vDustPos, currTime, false, pScene, GetSingleton<IVisObjBuilder>() );
		}
	}
}
void CMOUnitMechanical::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	CMOUnit::SetPlacement( vPos, wDir );
	const SMechUnitRPGStats *pStats = GetRPGStats();
	if ( pStats->bLeavesTracks )
	{
		pVisObj->Update( GetSingleton<IGameTimer>()->GetGameTime(), true );
		const SHMatrix &mPlacement = GetVisObj()->GetPlacement();
		CVec3 corner = CVec3( pStats->vAABBVisCenter.x, pStats->vAABBVisCenter.y - pStats->vAABBHalfSize.y * (1 - 2 * pStats->fTrackEnd), 0 );
		CVec3 shift = CVec3( pStats->vAABBHalfSize.x, 0, 0 );
		AI2Vis( &corner );
		AI2Vis( &shift );
		mPlacement.RotateVector( &corner, corner );
		mPlacement.RotateVector( &shift, shift );
		corner += vPos;
		vLastTracedCorners[0] = corner - ( 1.0f - 2.0f * pStats->fTrackOffset ) * shift;
		vLastTracedCorners[1] = corner - ( 1.0f - 2.0f * ( pStats->fTrackOffset + pStats->fTrackWidth ) ) * shift;
		vLastTracedCorners[2] = corner + ( 1.0f - 2.0f * ( pStats->fTrackOffset + pStats->fTrackWidth ) ) * shift;
		vLastTracedCorners[3] = corner + ( 1.0f - 2.0f * pStats->fTrackOffset ) * shift;
		vLastPos = corner;
		bLastTracedDir = true;
		wLastDir = wDir;
		fTraceLenSq = fabs2( pStats->vAABBVisHalfSize.y * (1 - pStats->fTrackEnd - pStats->fTrackStart) * 1.5f );
	}
}
bool CMOUnitMechanical::AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene )
{
	const float fNewHP = stats.fHitPoints / GetRPG()->fMaxHP;
	CommonUpdateRPGStats( fNewHP, stats, pVOB );
	return fNewHP > 0;
}
IMapObj* CMOUnitMechanical::AIUpdateFireWithProjectile( const SAINotifyNewProjectile &projectile,
																											  const NTimer::STime &currTime, IVisObjBuilder *pVOB )
{
	const SMechUnitRPGStats::SGun &gun = GetRPGStats()->guns[projectile.nGun];
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
	WORD wDir = 0;
	CVec3 vDiffPos = VNULL3, vUnitPos;
	GetPlacement( &vUnitPos, &wDir );
	pMO->SetPlacement( vUnitPos, 0 );
	if ( gun.nShootPoint >= 0 ) 
	{
		const SHMatrix *matrices = GetVisObj()->GetMatrices();
		if ( gun.nShootPoint < GetAnim()->GetNumNodes() ) 
		{
			vDiffPos = matrices[gun.nShootPoint].GetTrans3() - vUnitPos;
			if ( fabs(vDiffPos) > 5.0f * fWorldCellSize ) 
				vDiffPos = VNULL3;
		}
		else
		{
			CheckFixedRange( gun.nShootPoint, GetAnim()->GetNumNodes(), pDesc->szPath.c_str() );
			vDiffPos = VNULL3;
		}
	}
	static_cast<CMOProjectile*>(pMO)->Init( projectile.startTime, projectile.flyingTime, vDiffPos );
	return pMO;
}
int CMOUnitMechanical::AIUpdateActions( const SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager )
{
	int nRetVal = 0;
	switch ( action.typeID )
	{
		case ACTION_NOTIFY_MOVE:
			ActionMove( action, currTime, pVOB, pScene );
			ChangeState( STATE_MOVE, currTime );
			break;
		case ACTION_NOTIFY_DIE:
		case ACTION_NOTIFY_DEADPLANE:
			SendDeathAcknowledgement( pAckManager, currTime - Min( action.time, currTime ) );
			pAckManager->UnitDead( this, pScene );
			ActionDie( action, currTime, pVOB, pScene );
			ChangeState( STATE_DIE, currTime );
			break;
		case ACTION_NOTIFY_STOP:
			ActionStop( action, pScene );
			break;
		case ACTION_NOTIFY_INSTALL_ROTATE:
		case ACTION_NOTIFY_UNINSTALL_ROTATE:
		case ACTION_NOTIFY_INSTALL_TRANSPORT:
		case ACTION_NOTIFY_UNINSTALL_TRANSPORT:
		case ACTION_NOTIFY_INSTALL_MOVE:
		case ACTION_NOTIFY_UNINSTALL_MOVE:
			nRetVal = ActionInstall( action, currTime, pVOB );
			ChangeState( STATE_IDLE, currTime );
			break;
		case ACTION_NOTIFY_BREAK_TRACK:
			SetIcon( ICON_TRACK, pVOB );
			ChangeState( STATE_IDLE, currTime );
			break;
		case ACTION_NOTIFY_REPAIR_TRACK:
			RemoveIcon( ICON_TRACK );
			break;
		case ACTION_NOTIFY_CHANGE_VISIBILITY:
			SetVisible( action.nParam );
			GetVisObj()->SetVisible( IsVisibleLocal() );
			if ( pExtPassangers ) 
				pExtPassangers->SetVisible( IsVisibleLocal() );
			break;
		case ACTION_NOTIFY_DELAYED_SHOOT:
			if ( const SUnitBaseRPGStats::SAnimDesc *pAnimDesc = GetAnimationByType(ANIMATION_SHOOT) )
			{
				const NTimer::STime timeEffect = Min( currTime, action.time );
				IMeshAnimation *pAnim = GetAnim();
				pAnim->SetAnimation( pAnimDesc->nIndex );
				pAnim->SetStartTime( timeEffect );
			}
			break;
		case ACTION_NOTIFY_STATE_CHANGED:
			switch ( action.nParam ) 
			{
				case ECS_HOOK_CANNON:
					bArtilleryHooked = true;
					break;
				case ECS_UNHOOK_CANNON:
					bArtilleryHooked = false;
					break;
			}
			break;
		case ACTION_NOTIFY_SHELLTYPE_CHANGED:
			switch ( action.nParam )
			{
				case SWeaponRPGStats::SShell::DAMAGE_HEALTH:
					RemoveIcon( ICON_ALT_SHELL );
					break;
				case SWeaponRPGStats::SShell::DAMAGE_MORALE:
				case SWeaponRPGStats::SShell::DAMAGE_FOG:
					RemoveIcon( ICON_ALT_SHELL );
					SetIcon( ICON_ALT_SHELL, pVOB );
					break;
				default:
					NI_ASSERT_T( false, NStr::Format("Unknown shell type (%d) to change to", action.nParam) );
			}
			break;
		case ACTION_NOTIFY_LEVELUP:
			{
				const std::string szSeason = GetGlobalVar( "World.Season", "Summer" );
				DWORD dwColor = GetGlobalVar( ("Scene.Colors." + szSeason + ".LevelUp.Color").c_str(), 0x00FF0000 );
				SFlashEffect flash;
				flash.nDuration = 2000;
				flash.nPower = 300;
				SetFlashEffect( flash, currTime, GetVisObj()->GetPosition(), dwColor, pScene, pVOB);
				ISceneMaterialEffector *pEffector = CreateObject<ISceneMaterialEffector>( SCENE_EFFECTOR_MATERIAL );
				pEffector->SetupTimes( currTime, 2000 );
				pEffector->SetupData( 0xFF, dwColor );
				GetVisObj()->AddMaterialEffector( pEffector );
				CVec3 vPos;
				WORD wDir;
				GetPlacement( &vPos,&wDir );
				pScene->AddSound( "Int_information2", vPos, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET );
				const DWORD dwTextColor = GetGlobalVar( (std::string("Scene.Colors.") + szSeason + ".Text.Information.Color").c_str(), int(0xffffffff) );
				ITextManager *pTM = GetSingleton<ITextManager>();
				IText *pText = pTM->GetString( "Textes\\FeedBacks\\unit_gain_level" );
				IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
				pBuffer->Write( CONSOLE_STREAM_CHAT, reinterpret_cast<const wchar_t*>( pText->GetString() ), dwTextColor );
			}
			nRetVal = CMOUnit::AIUpdateActions( action, currTime, pVOB, pScene, pAckManager );
			break;
		case ACTION_NOTIFY_PRE_DISAPPEAR:
			{
				ISceneMaterialEffector *pEffector = CreateObject<ISceneMaterialEffector>( SCENE_EFFECTOR_MATERIAL );
				pEffector->SetupTimes( currTime, action.nParam * 2 );
				pEffector->SetupData( 0x00, 0xFF000000 );
				GetVisObj()->AddMaterialEffector( pEffector );
				CVec3 vPos;
				WORD wDir;
				const SMechUnitRPGStats *pStats = GetRPGStats();
				GetPlacement( &vPos,&wDir );
				PlayEffect( pStats->szEffectDisappear, vPos, currTime, false, pScene, pVOB );
			}
			nRetVal = CMOUnit::AIUpdateActions( action, currTime, pVOB, pScene, pAckManager );
			break;
		case ACTION_NOTIFY_ENTRENCHMENT_STARTED:
			if ( action.nParam == -1 )
			{
				CVec3 vPos;
				WORD wDir;
				const SMechUnitRPGStats *pStats = GetRPGStats();
				GetPlacement( &vPos,&wDir );
				PlayEffect( pStats->szEffectEntrenching, vPos, currTime, false, pScene, pVOB );
				SetIcon( ICON_ENTRENCHED, pVOB );
			}
			else
			{
				RemoveIcon( ICON_ENTRENCHED );				
			}
			nRetVal = CMOUnit::AIUpdateActions( action, currTime, pVOB, pScene, pAckManager );
			break;
		default:
			nRetVal = CMOUnit::AIUpdateActions( action, currTime, pVOB, pScene, pAckManager );
	}
	return nRetVal;
}
void CMOUnitMechanical::ActionMove( const SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene )
{
	const SMechUnitRPGStats *pRPG = GetRPGStats();
	IMeshVisObj *pObj = GetVisObj();
	IMeshAnimation *pAnim = GetAnim();
	if ( IsVisibleLocal() && !pRPG->exhaustPoints.empty() )
	{
		RemoveExhaustedEffects( currTime );
		const std::string szEffectName = "effects\\effects\\" + pRPG->szEffectDiesel;
		pVisObj->Update( currTime );
		const SHMatrix *matrices = pObj->GetMatrices();
		const NTimer::STime timeEffect = Min( action.time, currTime );
		const int nNumNodes = GetAnim()->GetNumNodes();
		for ( int i = 0; i < pRPG->exhaustPoints.size(); ++i )
		{
			if ( pRPG->exhaustPoints[i] < nNumNodes ) 
			{
				if ( IEffectVisObj *pEffect = static_cast<IEffectVisObj*>(pVOB->BuildObject(szEffectName.c_str(), 0, SGVOT_EFFECT)) )
				{
					const SHMatrix &matrix = matrices[ pRPG->exhaustPoints[i] ];
					pEffect->SetPlacement( matrix.GetTrans3(), 0 );
					pEffect->SetEffectDirection( matrix );
					pEffect->SetStartTime( timeEffect );
					AddEffect( pEffect, pRPG->exhaustPoints[i], timeEffect );
					
					pScene->AddObject( pEffect, SGVOGT_EFFECT, 0 );
					smokeEffects.push_back( pEffect );
				}
			}
		}
	}
/*	switch ( action.nParam )
	{
	case MOVE_TYPE_DIVE:
		if ( !bDiveMove )
		{
			if ( wMoveSoundID )
				pScene->RemoveSound( wMoveSoundID  );
			wMoveSoundID = pScene->AddSound( "Sounds\\Move\\stallbomber",pVisObj->GetPosition(), SFX_MIX_ALWAYS, SAM_LOOPED_NEED_ID, 25, 40 );
		}
		bDiveMove = true;
		break;
	case MOVE_TYPE_MOVE:*/
		{
			if ( !pRPG->szSoundMoveStart.empty() )
				pScene->AddSound( pRPG->szSoundMoveStart.c_str(), pVisObj->GetPosition(), 
													SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_GENERIC, 1, 100 );
			/*if ( bDiveMove && 0 != wMoveSoundID )
			{
				pScene->RemoveSound( wMoveSoundID );
				wMoveSoundID = 0;
			}*/
			if ( !pRPG->szSoundMoveCycle.empty() && 0 == wMoveSoundID )
				wMoveSoundID = pScene->AddSound( pRPG->szSoundMoveCycle.c_str(), pVisObj->GetPosition(), 
																					SFX_MIX_ALWAYS, SAM_LOOPED_NEED_ID, ESCT_GENERIC, 1, 100 );
		}
		/*break;
	}*/
	
	if ( pRPG->platforms[0].nModelPart >= 0 ) 
	{
		IMatrixEffectorJogging *pEffector = CreateObject<IMatrixEffectorJogging>( ANIM_EFFECTOR_JOGGING );
		pEffector->SetupTimes( action.time, -1 );
		pEffector->SetupData( pRPG->jx.fPeriod1, pRPG->jx.fPeriod2, pRPG->jx.fAmp1, pRPG->jx.fAmp2, pRPG->jx.fPhase1, pRPG->jx.fPhase2,
													pRPG->jy.fPeriod1, pRPG->jy.fPeriod2, pRPG->jy.fAmp1, pRPG->jy.fAmp2, pRPG->jy.fPhase1, pRPG->jy.fPhase2,
													pRPG->jz.fPeriod1, pRPG->jz.fPeriod2, pRPG->jz.fAmp1, pRPG->jz.fAmp2, pRPG->jz.fPhase1, pRPG->jz.fPhase2 );
		pAnim->AddEffector( pEffector, ANIM_EFFECTOR_JOGGING, pRPG->platforms[0].nModelPart );
	}
}
void CMOUnitMechanical::ActionDie( const SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene )
{
	SetVisible( true );
	std::list< CPtr<IEffectVisObj> >::iterator it = smokeEffects.begin();
	while ( it != smokeEffects.end() )
	{
		(*it)->Stop();
		++it;
	}
	pVOB->ChangeObject( pVisObj, 0, (pDesc->szPath + "\\2" + GetSeasonApp2(GetSeason())).c_str(), pDesc->eVisType );
	GetVisObj()->RemoveEffector( -1, -1 );
	const NTimer::STime timeEffect = Min( action.time, currTime );
	const NTimer::STime timePassed = currTime - timeEffect;
	const SMechUnitRPGStats *pRPG = GetRPGStats();
	IMeshVisObj *pObj = GetVisObj();
	pObj->RemoveIcon( -1 );
	if ( !pRPG->platforms.empty() && (pRPG->platforms[0].nModelPart >= 0) )
		GetAnim()->RemoveEffector( ANIM_EFFECTOR_JOGGING, pRPG->platforms[0].nModelPart );
	if ( ((action.nParam & 0xffff) != 0xffff) && !pRPG->IsAviation() ) 
	{
		ChangeModel( "2", timeEffect, timeEffect );
		IMeshAnimation *pAnim = GetAnim();
		pAnim->CutProceduralAnimation( timeEffect );
		if ( const SUnitBaseRPGStats::SAnimDesc *pDesc = GetAnimDesc(action.nParam) )
		{
			pAnim->SetAnimation( pDesc->nIndex );
			pAnim->SetStartTime( timeEffect );
		}
	}
	else
		GetAnim()->CutProceduralAnimation( timeEffect );
	if ( 0 != wMoveSoundID )
	{
		pScene->RemoveSound( wMoveSoundID );
		wMoveSoundID = 0;
		pScene->RemoveSound( wNonCycleSoundID );
		wNonCycleSoundID = 0;
	}
	if ( action.nParam == -1 ) // not ordinary death
	{
		wNonCycleSoundID = pScene->AddSound( "plane_fly_death", pVisObj->GetPosition(),
																				SFX_MIX_ALWAYS, SAM_NEED_ID);
	}
	if ( (int((action.nParam >> 16) & 0x0fff) == ANIMATION_DEATH) || ((action.nParam & 0xffff)== 0xffff) ) 
	{
		if ( !pRPG->damagePoints.empty() && pRPG->HasSmokeEffect() ) 
		{
			const int nIndex = pRPG->damagePoints[ rand() % pRPG->damagePoints.size() ];
			const SHMatrix *matrices = pObj->GetMatrices();
			CheckFixedRange( nIndex, GetAnim()->GetNumNodes(), pDesc->szPath.c_str() );
			const SHMatrix &matrix = matrices[nIndex];
			if ( IEffectVisObj *pEffect = PlayEffect(pRPG->szEffectSmoke, matrix.GetTrans3(), timeEffect, pRPG->IsAviation(), 
				                                       pScene, pVOB, timePassed, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_GENERIC) )
			{
				pEffect->SetEffectDirection( matrix );
				AddEffect( pEffect, nIndex, timeEffect );
			}
		}
	}
	else if ( int((action.nParam >> 16) & 0x0fff) == ANIMATION_DEATH_FATALITY ) 
	{
		if ( !pRPG->szEffectFatality.empty() )
		{
			if ( pRPG->nFatalitySmokePoint != -1 ) 
			{
				const SHMatrix *matrices = pObj->GetMatrices();
				const SHMatrix &matrix = matrices[pRPG->nFatalitySmokePoint];
				CheckFixedRange( pRPG->nFatalitySmokePoint, GetAnim()->GetNumNodes(), pDesc->szPath.c_str() );
				const CVec3 vEffPos = matrix.GetTrans3();
				if ( IEffectVisObj *pEffect = PlayEffect(pRPG->szEffectFatality, vEffPos, timeEffect, pRPG->IsAviation(), 
					                                       pScene, pVOB, timePassed, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_GENERIC) )
				{
					pEffect->SetEffectDirection( matrix );
					AddEffect( pEffect, pRPG->nFatalitySmokePoint, timeEffect );
				}
			}
			else if ( pRPG->IsAviation() )
			{
				SPlane vViewVolumePlanes[6];
				GetSingleton<IGFX>()->GetViewVolume( &(vViewVolumePlanes[0]) );
				if ( GetVisObj()->CheckForViewVolume(vViewVolumePlanes) != GFXCP_OUT )
				{
					IGammaEffect *pGamma = CreateObject<IGammaEffect>( SCENE_GAMMA_EFFECT );
					pGamma->Init( 1, 1, 1, timeEffect, 250 );
					pScene->AddSceneObject( pGamma );
				}
				PlayEffect( pRPG->szEffectFatality, GetVisObj()->GetPosition(), timeEffect, true, 
					          pScene, pVOB, timePassed, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_GENERIC );
			}
		}
	}
	if ( ((action.nParam & 0x80000000) != 0) && !pRPG->IsAviation() && !pRPG->deathCraters.empty() )
	{
		SetCraterEffect( pRPG->deathCraters[rand() % pRPG->deathCraters.size()], 
		                 GetSeason(), pVisObj->GetPosition(), 100, pScene, pVOB );
	}
}
void CMOUnitMechanical::ActionStop( const SAINotifyAction &action, IScene *pScene )
{
	if ( 0 != wMoveSoundID )
	{
		pScene->RemoveSound( wMoveSoundID );
		wMoveSoundID = 0;
	}
	const SMechUnitRPGStats *pRPG = GetRPGStats();
	if ( !pRPG->szSoundMoveStop.empty() )
		pScene->AddSound( pRPG->szSoundMoveStop.c_str(), pVisObj->GetPosition(),
											SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_GENERIC, 1, 100 );
	if ( !pRPG->platforms.empty() && (pRPG->platforms[0].nModelPart >= 0) )
		GetAnim()->RemoveEffector( ANIM_EFFECTOR_JOGGING, pRPG->platforms[0].nModelPart );
	std::list< CPtr<IEffectVisObj> >::iterator it = smokeEffects.begin();
	while ( it != smokeEffects.end() )
	{
		(*it)->Stop();
		++it;
	}
}
const int CMOUnitMechanical::DoInstall( const int nAnimation, const NTimer::STime &timeAction, 
																			  const int nRPGDuration, const char *pszModel2 )
{
	RemoveIcon( ICON_UNINSTALL );
	if ( pszModel2 ) 
		ChangeModel( pszModel2, timeAction, timeAction );
	IMeshAnimation *pAnim = GetAnim();
	int nAddTime = 0;
	if ( const SUnitBaseRPGStats::SAnimDesc *pAnimDesc = GetAnimationByType(nAnimation) )
	{
		pAnim->SetAnimation( pAnimDesc->nIndex );
		pAnim->SetStartTime( timeAction );
		pAnim->SetAnimSpeedCoeff( float(pAnimDesc->nLength) / float(nRPGDuration) );
		nAddTime = nRPGDuration;
	}
	return ChangeModel( "1", timeAction, timeAction + nAddTime );
}
const int CMOUnitMechanical::DoUnInstall( const int nAnimation, const NTimer::STime &timeAction, 
																				  const int nRPGDuration, IVisObjBuilder *pVOB, const char *pszModel3, bool bInstantly )
{
	SetIcon( ICON_UNINSTALL, pVOB );
	int nAddTime = 0;
	if ( !bInstantly ) 
	{
		ChangeModel( "2", timeAction, timeAction );
		IMeshAnimation *pAnim = GetAnim();
		if ( const SUnitBaseRPGStats::SAnimDesc *pAnimDesc = GetAnimationByType(nAnimation) )
		{
			pAnim->SetAnimation( pAnimDesc->nIndex );
			pAnim->SetStartTime( timeAction );
			pAnim->SetAnimSpeedCoeff( float(pAnimDesc->nLength) / float(nRPGDuration) );
			nAddTime = nRPGDuration;
		}
	}
	return pszModel3 == 0 ? 0 : ChangeModel( pszModel3, timeAction, timeAction + nAddTime );
}
int CMOUnitMechanical::ActionInstall( const SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB )
{
	const NTimer::STime timeAction = Min( action.time, currTime );
	int nRetVal = 0;
	switch( action.typeID ) 
	{
		case ACTION_NOTIFY_INSTALL_ROTATE:
			nRetVal = DoInstall( ANIMATION_UNINSTALL_ROT, timeAction, GetRPGStats()->nUninstallRotate );
			bInstalled = true;
			break;
		case ACTION_NOTIFY_UNINSTALL_ROTATE:
			nRetVal = DoUnInstall( ANIMATION_INSTALL_ROT, timeAction, GetRPGStats()->nUninstallRotate, pVOB );
			bInstalled = false;
			break;
		case ACTION_NOTIFY_INSTALL_MOVE:
			nRetVal = DoInstall( ANIMATION_UNINSTALL_PUSH, timeAction, GetRPGStats()->nUninstallRotate );
			bInstalled = true;
			break;
		case ACTION_NOTIFY_UNINSTALL_MOVE:
			nRetVal = DoUnInstall( ANIMATION_INSTALL_PUSH, timeAction, GetRPGStats()->nUninstallRotate, pVOB );
			bInstalled = false;
			break;
		case ACTION_NOTIFY_INSTALL_TRANSPORT:
			nRetVal = DoInstall( ANIMATION_INSTALL, timeAction, GetRPGStats()->nUninstallTransport, "2" );
			bInstalled = true;
			break;
		case ACTION_NOTIFY_UNINSTALL_TRANSPORT:
			nRetVal = DoUnInstall( ANIMATION_UNINSTALL, timeAction, GetRPGStats()->nUninstallTransport, pVOB, "3", action.nParam == 0 );
			bInstalled = false;
			break;
	}
	return nRetVal;
}
void CMOUnitMechanical::AddAnimation( const SUnitBaseRPGStats::SAnimDesc *pDesc )
{
	if ( animchanges.empty() )
	{
		const NTimer::STime currTime = GetSingleton<IGameTimer>()->GetGameTime();
		animchanges.push_back( SAnimChange(pDesc->nIndex, currTime + 1000, pDesc->nLength) );
	}
	else
	{
		const SAnimChange &change = animchanges.back();
		animchanges.push_back( SAnimChange(pDesc->nIndex, change.time + change.length + 2000, pDesc->nLength) );
	}
}
void CMOUnitMechanical::RemoveSounds( interface IScene * pScene )
{
	if ( 0 != wMoveSoundID )
	{
		pScene->RemoveSound( wMoveSoundID );
		wMoveSoundID = 0;
	}
	if ( 0 != wNonCycleSoundID )
	{
		pScene->RemoveSound( wNonCycleSoundID );
		wNonCycleSoundID = 0;
	}
}
void CMOUnitMechanical::AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB )
{
	if ( hit.wShell >= hit.pWeapon->shells.size() )
		return;
	const SWeaponRPGStats::SShell &shell = hit.pWeapon->shells[hit.wShell];
	PlayEffect( *GetHitEffect(hit, shell), pVisObj->GetPosition(), currTime, GetRPGStats()->IsAviation(), pScene, pVOB, 0, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT );
	const CVec3 vPos = pVisObj->GetPosition();
	if ( shell.flashExplosion.HasFlash() ) 
		SetFlashEffect( shell.flashExplosion, currTime, vPos, GetFlashExpColor(), pScene, pVOB );
}
void CMOUnitMechanical::Select( ISelector *pSelector, bool bSelect, bool bSelectSuper )
{
	if ( !bCanSelect && bSelect )
		return;
	if ( bSelectSuper ) 
		pSelector->Select( this, bSelect, false );
	else
		pVisObj->Select( bSelect ? SGVOSS_SELECTED : SGVOSS_UNSELECTED );
}
void CMOUnitMechanical::AIUpdateShot( const struct SAINotifyBaseShot &_shot, const NTimer::STime &currTime, 
																		  IVisObjBuilder *pVOB, IScene *pScene )
{
	const SAINotifyMechShot &shot = *( static_cast<const SAINotifyMechShot*>(&_shot) );
	const SMechUnitRPGStats *pRPG = GetRPGStats();
	const SMechUnitRPGStats::SGun &gun = pRPG->guns[shot.cGun];
	const SWeaponRPGStats::SShell &shell = gun.pWeapon->shells[shot.cShell];
	const NTimer::STime timeEffect = Min( currTime, shot.time );
	IMeshAnimation *pAnim = GetAnim();
	bool bProceduralAnimation = true;
	if ( (GetAnimationByType(ANIMATION_SHOOT) == 0) && gun.bRecoil && (gun.nModelPart != -1) ) // add procedural recoil
	{
		pAnim->CutProceduralAnimation( timeEffect, gun.nModelPart );
		pAnim->AddProceduralNode( gun.nModelPart, timeEffect, timeEffect, timeEffect + gun.recoilTime, -gun.fRecoilLength );
		pAnim->AddProceduralNode( gun.nModelPart, timeEffect, timeEffect + gun.recoilTime, timeEffect + gun.recoilTime*10, 0 );
	}
	if ( (gun.nShootPoint != -1) && (gun.pWeapon != 0) && !shell.szEffectGunFire.empty() )
	{
		pVisObj->Update( currTime );
		IMeshVisObj *pObj = GetVisObj();
		const SHMatrix *matrices = pObj->GetMatrices();
		CheckFixedRange( gun.nShootPoint, GetAnim()->GetNumNodes(), pDesc->szPath.c_str() );
		const SHMatrix &matGlobalShoot = matrices[ gun.nShootPoint ];
		if ( shell.trajectory == SWeaponRPGStats::SShell::TRAJECTORY_LINE && NWin32Random::Random( 100 ) + 1 <= shell.fTraceProbability * fTraceProbabilityCoeff * 100.0f )
		{
			const CVec3 vStart = matGlobalShoot.GetTrans3();
			CVec3 vEnd;
			AI2Vis( &vEnd, shot.vDestPos );
			UpdateGunTraces( vStart, vEnd, AI2VisX(shell.fSpeed) * shell.fTraceSpeedCoeff * fTraceSpeedCoeff, shot.time, pScene );
		}
		if ( gun.bRecoil && bProceduralAnimation && (gun.nRecoilShakeTime > 0) )
		{
			SHMatrix matInverse; // inverse object's matrix
			Invert( &matInverse, pObj->GetPlacement() );
			SHMatrix matLocalShoot;
			Multiply( &matLocalShoot, matInverse, matGlobalShoot );
			CVec3 vAxis;
			matLocalShoot.RotateVector( &vAxis, V3_AXIS_X );
			ISceneEffectorRecoil *pEffector = CreateObject<ISceneEffectorRecoil>( SCENE_EFFECTOR_RECOIL );
			pEffector->SetupTimes( shot.time, gun.nRecoilShakeTime );
			pEffector->SetupData( gun.fRecoilShakeAngle, vAxis );
			pObj->AddEffector( SCENE_EFFECTOR_RECOIL, pEffector );
		}
		if ( IsVisibleLocal() || GetRPGStats()->IsAviation() ) 
		{
			if ( IEffectVisObj *pEffect = PlayEffect(shell.szEffectGunFire, matGlobalShoot.GetTrans3(), timeEffect, GetRPGStats()->IsAviation(), pScene, pVOB, 0, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT) )
			{
				pEffect->SetEffectDirection( matGlobalShoot );
				AddEffect( pEffect, gun.nShootPoint, timeEffect );
			}
			if ( shell.flashFire.HasFlash() ) 
			{
				CVec3 vPos = matGlobalShoot.GetTrans3();
				Vis2AI( &vPos );
				vPos.z = GetSingleton<IAILogic>()->GetZ( CVec2(vPos.x, vPos.y) );
				AI2Vis( &vPos );
				SetFlashEffect( shell.flashFire, currTime, vPos, GetFlashFireColor(), pScene, pVOB );
			}
			if ( (pRPG->pPrimaryGun == &(pRPG->guns[shot.cGun])) && !pRPG->szEffectShootDust.empty() ) 
			{
				if ( IEffectVisObj *pEffect = PlayEffect(pRPG->szEffectShootDust, pVisObj->GetPosition(), timeEffect, false, pScene, pVOB, 0, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT) )
				{
					if ( pRPG->nShootDustPoint != -1 ) 
					{
						pEffect->SetEffectDirection( matrices[pRPG->nShootDustPoint] );
						pEffect->SetPosition( matrices[pRPG->nShootDustPoint].GetTrans3() );
					}
					else
						pEffect->SetPosition( pVisObj->GetPosition() );
				}
			}
		}
	}
}
void CMOUnitMechanical::MakeVisible( const bool bVisible )
{
	GetVisObj()->SetVisible( bVisible );
	UpdateAttachedEffects( GetSingleton<IGameTimer>()->GetGameTime() );
}
int CMOUnitMechanical::SEffect::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pEffect );
	saver.Add( 2, &nPointIndex );
	saver.Add( 3, &timeLastUpdate );
	return 0;
}
int CMOUnitMechanical::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMOUnit*>(this) );
	saver.Add( 2, &passangers );
	saver.Add( 3, &modelchanges );
	saver.Add( 4, &wMoveSoundID );
	saver.Add( 5, &bArtilleryHooked );
	saver.Add( 6, &bInstalled );
	saver.Add( 7, &bDiveMove );
	saver.Add( 8, &pExtPassangers );
	saver.Add( 9, &nNumExtPassangers );
	saver.AddRawData( 10, &(vLastTracedCorners[0]), sizeof(vLastTracedCorners) );
	saver.Add( 12, &fTraceLenSq );
	saver.Add( 15, &bLastTracedDir );
	saver.Add( 16, &vLastPos );
	saver.Add( 17, &wLastDir );
	saver.Add( 18, &smokeEffects );
	saver.Add( 19, &effects );
	saver.Add( 20, &wNonCycleSoundID );
	return 0;
}
