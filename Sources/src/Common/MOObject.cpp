#include "StdAfx.h"

#include "MOObject.h"

#include "../Common/Actions.h"
#include "../Common/Icons.h"
#include "../GameTT/iMission.h"
#include "../Formats/fmtTerrain.h"
#include "PlayEffect.h"
#include "../Main/ScenarioTracker.h"
#include "../UI/UI.h"
int CMOObject::nLastMarkerID = 0;
bool CMOObject::Create( IRefCount *pAIObjLocal, const SGDBObjectDesc *pDescLocal, int nSeason, int nFrameIndex, 
											  float fNewHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	bDead = false;
	pDesc = pDescLocal;
	pRPG = NGDB::GetRPGStats<SHPObjectRPGStats>( pGDB, pDesc );
	NI_ASSERT_TF( pRPG != 0, NStr::Format("Can't find RPG stats for object \"%s\"", pDesc->szKey.c_str()), return 0 );
	if ( pRPG == 0 )
		return false;
	if ( pDesc->eGameType == SGVOGT_FLAG )
	{
		szFlagSide = pDesc->szKey.substr( 5 );
		nLastMarkerID++;
		nMarkerID = nLastMarkerID;
	}
	else
		szFlagSide = "";
	switch ( pDesc->eGameType ) 
	{
		case SGVOGT_OBJECT:
			{
				const char *pszName = nSeason == 1 ? "\\1w" : "\\1";
				pVisObj = pVOB->BuildObject( (pDesc->szPath + pszName).c_str(), 0, pDesc->eVisType );
				if ( pVisObj )									// try to create normal object (take season into account)
					pShadow = pVOB->BuildObject( (pDesc->szPath + pszName + "s").c_str(), 0, SGVOT_SPRITE );
				else														// try to create summer (default) object, if correct one failed
				{
					const char *pszName = "\\1";
					pVisObj = pVOB->BuildObject( (pDesc->szPath + pszName).c_str(), 0, pDesc->eVisType );
					if ( pVisObj )
						pShadow = pVOB->BuildObject( (pDesc->szPath + pszName + "s").c_str(), 0, SGVOT_SPRITE );
				}
			}
			break;
		case SGVOGT_EFFECT:
			pVisObj = pVOB->BuildObject( pDesc->szPath.c_str(), 0, pDesc->eVisType );
			break;
		case SGVOGT_PROJECTILE:
			pVisObj = pVOB->BuildObject( pDesc->szPath.c_str(), 0, pDesc->eVisType );
			break;
		case SGVOGT_ENTRENCHMENT:
			{
				const SEntrenchmentRPGStats::SSegmentRPGStats &segment = static_cast<const SEntrenchmentRPGStats*>( pRPG.GetPtr() )->GetSegmentStats( nFrameIndex );
				const char *pszName = nSeason == 1 ? "\\1w" : "\\1";
				pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\" + segment.szModel).c_str(), (pDesc->szPath + pszName).c_str(), pDesc->eVisType );
			}
			break;
		case SGVOGT_TANK_PIT:
			pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\1").c_str(), (pDesc->szPath + "\\1" + GetSeasonApp2(nSeason)).c_str(), pDesc->eVisType );
			break;
		case SGVOGT_MINE:
			pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\" + static_cast_gdb<const SMineRPGStats*>(pRPG)->szFlagModel).c_str(), 0, pDesc->eVisType );
			break;
		case SGVOGT_BRIDGE:
			{
				const SBridgeRPGStats::SSegmentRPGStats &segment = static_cast_gdb<const SBridgeRPGStats*>(pRPG)->GetSegmentStats( nFrameIndex );
				if ( segment.szModel.empty() || segment.nFrameIndex == -1 ) 
					return false;
				pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\" + segment.szModel).c_str(), 0, SGVOT_SPRITE );
				if ( pVisObj == 0 )				// bridge have no this part... skip it
					return false;
				if ( fNewHP < 0 )					// special case - "blueprint" of the bridge
					pVisObj->SetOpacity( 0x80 );
				pShadow = pVOB->BuildObject( (pDesc->szPath + "\\" + segment.szModel + "s").c_str(), 0, SGVOT_SPRITE );
				nFrameIndex = segment.nFrameIndex;
			}
			break;
		case SGVOGT_FENCE:
			pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\1").c_str(), 0, pDesc->eVisType );
			if ( pVisObj ) 
				pShadow = pVOB->BuildObject( (pDesc->szPath + "\\1s").c_str(), 0, SGVOT_SPRITE );
			break;
		default:
			pVisObj = pVOB->BuildObject( (pDesc->szPath + "\\1").c_str(), 0, pDesc->eVisType );
	}
	NI_ASSERT_T( pVisObj != 0, NStr::Format("Can't create object \"%s\" from path \"%s\"", pDesc->szKey.c_str(), pDesc->szPath.c_str()) );
	if ( pDesc->eVisType == SGVOT_SPRITE )
	{
		ISpriteAnimation *pAnim = static_cast<ISpriteAnimation*>( static_cast_ptr<IObjVisObj*>(pVisObj)->GetAnimation() );
		pAnim->SetFrameIndex( nFrameIndex );
		if ( pShadow )
		{
			pAnim = static_cast<ISpriteAnimation*>( static_cast_ptr<IObjVisObj*>(pShadow)->GetAnimation() );
			pAnim->SetFrameIndex( nFrameIndex );
		}
	}	
	pAIObj = pAIObjLocal;
	UpdateModelWithHP( fNewHP / pRPG->fMaxHP, pVOB );
	fHP = fNewHP / pRPG->fMaxHP;
	FillActions();
	return true;
}
void CMOObject::Visit( IMapObjVisitor *pVisitor )
{
	pVisitor->VisitSprite( pVisObj, pDesc->eGameType, pDesc->eVisType );
	if ( pShadow ) 
		pVisitor->VisitSprite( pShadow, SGVOGT_SHADOW, SGVOT_SPRITE );
}
int CMOObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SMapObject*>(this) );
	saver.Add( 2, &bDead );
	saver.Add( 3, &szFlagSide );
	if ( !szFlagSide.empty() )
		saver.Add( 4, &nMarkerID );
	if ( saver.IsReading() ) 
		FillActions();
	return 0;
}
bool CMOObject::AddAction( int nAction )
{
	if ( nAction > 63 ) 
		return false;
	const int nIndex = nAction >> 5;
	actions[nIndex] |= 1UL << ( nAction - nIndex*32 );
	return true;
}
void CMOObject::FillActions()
{
	actions[0] = actions[1] = 0;
}
void CMOObject::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	pVisObj->SetPlacement( vPos, wDir );
	if ( pShadow )
		pShadow->SetPlacement( vPos, wDir );
	if ( !szFlagSide.empty() )
	{
		IUIScreen *pUIScreen = GetSingleton<IScene>()->GetMissionScreen();
		if ( pUIScreen )
		{
			if ( IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pUIScreen->GetChildByID( 20000 ) ) )
			{
				pUIMiniMap->AddMarker( szFlagSide + "Flag", CVec2(vPos.x, vPos.y), true, nMarkerID, GetSingleton<IGameTimer>()->GetAbsTime(), 0, false );
			}
		}
	}
}
void CMOObject::GetPlacement( CVec3 *pvPos, WORD *pwDir )
{
	*pvPos = pVisObj->GetPosition();
	*pwDir = pVisObj->GetDirection();
}
void CMOObject::GetStatus( struct SMissionStatusObject *pStatus ) const
{
	pStatus->nScenarioIndex = -1;
	pStatus->dwIconsStatus = 0;
	pStatus->dwPlayer = -1;
	pStatus->params[0] = PackParams( MINT(fHP * pRPG->fMaxHP), MINT(pRPG->fMaxHP) );
	pStatus->params[1] = 0;
	pStatus->params[2] = 0;
	pStatus->params[3] = 0;
	if ( IText *pName = GetLocalName() ) 
		memcpy( pStatus->pszName, pName->GetString(), (pName->GetLength() + 1) * 2 );
	else
	{
		static std::wstring szName;
		NStr::ToUnicode( &szName, pDesc->szKey );
		memcpy( pStatus->pszName, szName.c_str(), (szName.size() + 1) * sizeof(szName[0]) );
	}
	Zero( pStatus->weaponstats );
	Zero( pStatus->armors );
}
void CMOObject::GetActions( CUserActions *pActions, EActionsType eActions ) const
{
	if ( const CUserActions *pUserActions = pRPG->GetUserActions(eActions == IMapObj::ACTIONS_BY) ) 
		*pActions = *pUserActions;
	else if ( eActions == IMapObj::ACTIONS_WITH ) 
	{
		pActions->SetAction( USER_ACTION_ATTACK );
		pActions->SetAction( USER_ACTION_MOVE );
		pActions->SetAction( USER_ACTION_UNKNOWN );
		if ( pDesc->eGameType == SGVOGT_BRIDGE ) 
		{
			if ( fHP < 0 ) 
				pActions->SetAction( USER_ACTION_ENGINEER_BUILD_BRIDGE );
			else if ( fHP < 1 ) 
				pActions->SetAction( USER_ACTION_ENGINEER_REPAIR_BUILDING );
		}
	}
}
void CMOObject::AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	CVec3 vPos;
	AI2Vis( &vPos, placement.center.x, placement.center.y, placement.z );
	pVisObj->SetDirection( placement.dir );
	pScene->MoveObject( pVisObj, vPos );
	pVisObj->Update( currTime, true );
	if ( pShadow )
	{
		pScene->MoveObject( pShadow, vPos );
		pShadow->Update( currTime, true );
	}
}
inline int GetObjectDamageState( float fHP )
{
	if ( fHP > 0.5f )
		return 0;
	else if ( fHP > 0.0f )
		return 1;
	else
		return 2;
}
void CMOObject::UpdateModelWithHP( const float fNewHP, IVisObjBuilder *pVOB )
{
	if ( (fHP == -1) && (fNewHP == 1) )		// bridge segment was built
		pVisObj->SetOpacity( 0xff );
	const int nOldState = GetObjectDamageState( fHP );
	const int nNewState = GetObjectDamageState( fNewHP );
	if ( nNewState != nOldState )
	{
		if ( (pDesc->eVisType == SGVOT_SPRITE) && (pDesc->eGameType == SGVOGT_BUILDING) )
		{
			pVOB->ChangeObject( pVisObj, NStr::Format( "%s\\%d", pDesc->szPath.c_str(), nNewState + 1 ), 0, SGVOT_SPRITE );
			if ( pShadow )
				pVOB->ChangeObject( pShadow, NStr::Format( "%s\\%ds", pDesc->szPath.c_str(), nNewState + 1 ), 0, SGVOT_SPRITE );
		}
	}
}
bool CMOObject::AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene )
{
	const float fNewHP = stats.fHitPoints / pRPG->fMaxHP;
	UpdateModelWithHP( fNewHP, pVOB );
	if ( fHP != fNewHP ) 
	{
		ISceneIconBar *pBar = static_cast<ISceneIconBar*>( static_cast_ptr<IObjVisObj*>(pVisObj)->GetIcon( ICON_HP_BAR ) );
		if ( pBar )
		{
			pBar->SetLength( fHP );
			if ( fHP >= 0.5f )
			{
				const DWORD r = DWORD( ( 1.0f - fHP ) * 510.0f );
				pBar->SetColor( 0xff00ff00 | (r << 16) );
			}
			else
			{
				const DWORD g = DWORD( fHP * 510.0f );
				pBar->SetColor( 0xffff0000 | (g << 8) );
			}
		}
	}
	fHP = fNewHP;
	return fNewHP > 0;
}
void CMOObject::AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB )
{
	if ( hit.wShell >= hit.pWeapon->shells.size() )
		return;
	const SWeaponRPGStats::SShell &shell = hit.pWeapon->shells[hit.wShell];
	const CVec3 &vPos = pVisObj->GetPosition();
	PlayEffect( *GetHitEffect(hit, shell), vPos, currTime, false, pScene, pVOB, 0, SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET, ESCT_COMBAT );
}
int CMOObject::AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager )
{
	switch ( action.typeID ) 
	{
		case ACTION_NOTIFY_SILENT_DEATH:
			if ( (pDesc->eGameType == SGVOGT_OBJECT) || (pDesc->eGameType == SGVOGT_FENCE) )
			{
				if ( pRPG && pVisObj ) 
				{
					const SStaticObjectRPGStats *pRPGStats = static_cast_gdb<const SStaticObjectRPGStats*>( pRPG );
					const NTimer::STime timeEffect = Min( action.time, currTime );
					const NTimer::STime timePassed = currTime - timeEffect;
					PlayEffect( pRPGStats->szEffectDeath, pVisObj->GetPosition(), timeEffect, false, pScene, pVOB, timePassed );
				}
			}
			bDead = true;
			break;
		case ACTION_NOTIFY_DIE:
			if ( !bDead && ((pDesc->eGameType == SGVOGT_OBJECT) || (pDesc->eGameType == SGVOGT_FENCE)) )
			{
				if ( pRPG && pVisObj ) 
				{
					const SStaticObjectRPGStats *pRPGStats = static_cast_gdb<const SStaticObjectRPGStats*>( pRPG );
					const NTimer::STime timeEffect = Min( action.time, currTime );
					const NTimer::STime timePassed = currTime - timeEffect;
					PlayEffect( pRPGStats->szEffectExplosion, pVisObj->GetPosition(), timeEffect, false, pScene, pVOB, timePassed );
				}
			}
			break;
		case ACTION_NOTIFY_CHANGE_FRAME_INDEX:
			{
				static_cast<ISpriteAnimation*>(static_cast_ptr<IObjVisObj*>(pVisObj)->GetAnimation())->SetFrameIndex( action.nParam );
				pVisObj->Update( currTime, true );
				if ( pShadow ) 
				{
					static_cast<ISpriteAnimation*>(static_cast_ptr<IObjVisObj*>(pShadow)->GetAnimation())->SetFrameIndex( action.nParam );
					pShadow->Update( currTime, true );
				}
			}
			break;
		case ACTION_NOTIFY_SIDE_CHANGED:
			for ( CPtr<IPlayerScenarioInfoIterator> pIt = GetSingleton<IScenarioTracker>()->CreatePlayerScenarioInfoIterator();
				    !pIt->IsEnd(); pIt->Next() )
			{
				IPlayerScenarioInfo *pPlayer = pIt->Get();
				if ( pPlayer->GetDiplomacySide() == action.nParam ) 
				{
					IPlayerScenarioInfo *pUserPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
					std::string szFlagName = pPlayer->GetGeneralSide();
					NStr::ToLower( szFlagName );
					if ( IUIMiniMap *pUIMiniMap = checked_cast<IUIMiniMap*>( pScene->GetMissionScreen()->GetChildByID( 20000 ) ) )
					{
						WORD wCircleColor;
						std::string szSoundName;
						if ( action.nParam == pUserPlayer->GetDiplomacySide() )
						{
							wCircleColor = 0xF0F0;
							szSoundName = "Int_completed";
						}
						else if ( action.nParam == 2 )
						{
							wCircleColor = 0xF00F;
							szSoundName = IsFriend() ? "Int_failed" : "Int_information";
						}
						else
						{
							wCircleColor = 0xFF00;
							szSoundName = IsNeutral() ? "Int_failed" : "Int_flag_captured";
						}
						CVec3 vPos;
						WORD wDir;
						GetPlacement( &vPos, &wDir );
						pUIMiniMap->RemoveMarker( nMarkerID );
						pUIMiniMap->AddMarker( szFlagName + "Flag", CVec2( vPos.x, vPos.y ), true, nMarkerID, GetSingleton<IGameTimer>()->GetAbsTime(), 0, false );
						pUIMiniMap->AddCircle( CVec2( vPos.x, vPos.y ), fWorldCellSize * 16.0f, MMC_STYLE_MIXED, wCircleColor, GetSingleton<IGameTimer>()->GetAbsTime(), 5000, false, 0 );
						pUIMiniMap->AddCircle( CVec2( vPos.x, vPos.y ), fWorldCellSize * 8.0f, MMC_STYLE_MIXED, wCircleColor, GetSingleton<IGameTimer>()->GetAbsTime(), 5000, false, 0 );
						pScene->AddSound( szSoundName.c_str(), VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
					}
					if ( action.nParam == 2 )
						SetDiplomacy( EDI_NEUTRAL );
					else if ( action.nParam == pUserPlayer->GetDiplomacySide() )
						SetDiplomacy( EDI_FRIEND );
					else
						SetDiplomacy( EDI_ENEMY );
					szFlagName = "Flag_" + szFlagName;
					if ( const SGDBObjectDesc *pNewDesc = GetSingleton<IObjectsDB>()->GetDesc(szFlagName.c_str()) )
					{
						const std::string szFlagFileName = pNewDesc->szPath + "\\1";
						if ( pVOB->ChangeObject(pVisObj, szFlagFileName.c_str(), 0, SGVOT_SPRITE) )
						{
							pDesc = pNewDesc;
							pRPG = NGDB::GetRPGStats<SObjectBaseRPGStats>( pDesc );
							pVisObj->Update( currTime, true );
							if ( pShadow )
							{
								pVOB->ChangeObject( pShadow, (szFlagFileName + "s").c_str(), 0, SGVOT_SPRITE );
								pShadow->Update( currTime, true );
							}
						}
					}
					break;
				}
			}
			break;
	}
	if ( (pDesc->eGameType == SGVOGT_FENCE) && ((action.typeID == ACTION_NOTIFY_SILENT_DEATH) || (action.typeID == ACTION_NOTIFY_DIE)) ) 
	{
		pScene->RemoveObject( pVisObj );
		pScene->AddObject( pVisObj, SGVOGT_TERRAOBJ, pDesc );
	}

	return 0;
}
