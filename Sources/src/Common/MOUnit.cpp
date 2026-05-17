#include "StdAfx.h"

#include "MOUnit.h"

#include "..\Common\Actions.h"
#include "..\Common\Icons.h"
#include "..\GameTT\iMission.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Main\ScenarioTracker.h"
#include "..\Main\ScenarioTrackerTypes.h"
#include "Season.h"
#include "..\Misc\Win32Helper.h"

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
#include "..\AILogic\AILogic.h"
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD CMOUnit::dwFlashFireColor = 0xffffffff;
DWORD CMOUnit::dwFlashExpColor = 0xffffffff;
int CMOUnit::nSeason = 0;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMOUnit::CMOUnit()
{
	fMorale = 1;
	eState = STATE_IDLE;
	fAmmoValue = 1;
	bVisible = false;
	SetScenarioIndex( -1 );
	dwIconFlags = 0;
	nPlayerIndex = -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMOUnit::OnCreate()
{
	if ( dwFlashFireColor == 0xffffffff ) 
		dwFlashFireColor = GetGlobalVar( (std::string("Scene.Colors.") + GetSeasonName(nSeason) + ".FlashFire.Color").c_str(), int(0xffffffff) );
	if ( dwFlashExpColor == 0xffffffff ) 
		dwFlashExpColor = GetGlobalVar( (std::string("Scene.Colors.") + GetSeasonName(nSeason) + ".FlashExplode.Color").c_str(), int(0xffffffff) );
	// add icon with level for scenario units
	if ( nScenarioIndex >= 0 ) 
		UpdateLevel( GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetUnit(nScenarioIndex)->GetValue(STUT_LEVEL) );
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::GetActionsLocal( EActionsType eActions, CUserActions *pActions ) const
{
	if ( (eActions == IMapObj::ACTIONS_BY) || (eActions == IMapObj::ACTIONS_ALL) )
	{
		if ( pRPG != 0 ) 
			*pActions = static_cast_gdb<const SUnitBaseRPGStats*>(pRPG)->availUserActions;
		else
			pActions->Clear();
	}
	else if ( eActions == IMapObj::ACTIONS_WITH ) 
	{
		if ( (pRPG != 0) && IsAlive() ) 
		{
			*pActions = static_cast_gdb<const SUnitBaseRPGStats*>(pRPG)->availUserExposures;
			// damaged unit can be repaired
			if ( fHP < 1 ) 
				pActions->SetAction( USER_ACTION_ENGINEER_REPAIR );
			// units with spend ammos can be resupplied
			if ( (ammos[0] < static_cast_gdb<const SUnitBaseRPGStats*>(pRPG)->nAmmos[0]) || (ammos[1] < static_cast_gdb<const SUnitBaseRPGStats*>(pRPG)->nAmmos[1]) ) 
				pActions->SetAction( USER_ACTION_SUPPORT_RESUPPLY );
		}
		else																// special case - dead unit - can be attacked and moved to
		{
			pActions->Clear();
			pActions->SetAction( USER_ACTION_ATTACK );
			pActions->SetAction( USER_ACTION_MOVE );
			pActions->SetAction( USER_ACTION_SWARM );
			pActions->SetAction( USER_ACTION_RANGING );
			pActions->SetAction( USER_ACTION_SUPPRESS );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CMOUnit::CanShowIcons() const
{
	if ( GetContainer() && GetContainer()->GetDesc()->eGameType != SGVOGT_ENTRENCHMENT )
		return false;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CMOUnit::CanAddIcon( int nType ) const
{
	if ( !IsFriend() && ( (nType == ICON_LOW_AMMO) || (nType == ICON_NO_AMMO) || (nType == ICON_LOW_MORAL) || (nType == ICON_UNIT_NO_SUPPLY) || (nType == ICON_ALT_SHELL) || (nType == ICON_AMBUSH) ) ) 
		return false;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::SetIcon( int nType, IVisObjBuilder *pVOB )
{
	if ( !CanAddIcon(nType) ) 
		return;
	//
	dwIconFlags |= 1 << (nType & 0xffff);
	//
	const char *pszIconName = GetIconName( nType );
	const int nTypeID = nType & 0xffff;
	switch ( nTypeID )
	{
		case ICON_LOW_AMMO:
			GetSingleton<IClientAckManager>()->RegisterAsBored( ACK_BORED_LOW_AMMO, this );
			break;
		case ICON_NO_AMMO:
			GetSingleton<IClientAckManager>()->RegisterAsBored( ACK_BORED_NO_AMMO, this );
			break;
	}
	//
	if ( ISceneIcon *pIcon = static_cast<ISceneIcon*>(pVOB->BuildSceneObject(pszIconName, SCENE_OBJECT_TYPE_ICON)) )
	{
		pIcon->Enable( CanShowIcons() );
		static_cast_ptr<IObjVisObj*>(pVisObj)->AddIcon( pIcon, nTypeID, GetIconAddValue(), VNULL3, nTypeID, 
			                                              ICON_ALIGNMENT_LEFT | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_HORIZONTAL,
																										CanShowIcons() );
		if ( pObserver ) 
			pObserver->AddIcon( nType, pIcon );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::RemoveIcon( int nType )
{
	//commented to remove icons when gun crew is dead
	//if ( !CanAddIcon(nType) ) 
	//	return;
	//
	dwIconFlags &= ~( 1 << (nType & 0xffff) );
	//
	const int nTypeID = nType & 0xffff;
	switch ( nTypeID )
	{
	case ICON_LOW_AMMO:
		GetSingleton<IClientAckManager>()->UnRegisterAsBored( ACK_BORED_LOW_AMMO, this );
		break;
	case ICON_NO_AMMO:
		{
			IClientAckManager * pAckManager = GetSingleton<IClientAckManager>();
			pAckManager->UnRegisterAsBored( ACK_BORED_NO_AMMO, this );
		}
		break;
	}
	
	static_cast_ptr<IObjVisObj*>(pVisObj)->RemoveIcon( nType, CanShowIcons() );
	//
	if ( pObserver ) 
		pObserver->RemoveIcon( nType );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	pVisObj->SetPlacement( vPos, wDir );
}
void CMOUnit::GetPlacement( CVec3 *pvPos, WORD *pwDir )
{
	*pvPos = pVisObj->GetPosition();
	*pwDir = pVisObj->GetDirection();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::GetStatus( struct SMissionStatusObject *pStatus ) const
{
	pStatus->nScenarioIndex = nScenarioIndex;
	pStatus->dwIconsStatus = dwIconFlags & 0xfffffff0;
	pStatus->dwPlayer = PackDWORD( diplomacy, nPlayerIndex );
	//
	const SUnitBaseRPGStats *pRPGStats = static_cast_gdb<const SUnitBaseRPGStats *>( pRPG );
	// primary attributes
	pStatus->params[0] = PackParams( MINT(fHP * pRPG->fMaxHP), MINT(pRPG->fMaxHP) );
	pStatus->params[1] = PackParams( ammos[0], pRPGStats->nAmmos[AMMO_TYPE_PRIMARY] );
	pStatus->params[2] = PackParams( ammos[1], pRPGStats->nAmmos[AMMO_TYPE_SECONDARY] );
	pStatus->params[3] = PackParams( MINT(fMorale * 100.0f), 100 );
	// name (unicode)
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	if ( IText *pName = GetLocalNameLocal() )
	{
		IRefCount *pState = GetSingleton<IAILogic>()->GetUnitState( pAIObj );
		bool bFrozen = GetSingleton<IAILogic>()->IsFrozen( pAIObj );
		bool bFrozenByState = GetSingleton<IAILogic>()->IsFrozenByState( pAIObj );
		const std::string szFrozenInfo = NStr::Format( ",(%d,%d)", (int)bFrozen, (int)bFrozenByState );
		const std::string szStateName = pState ? typeid( *pState ).name() + 6 : "";
		const std::wstring wszStateName = pState ? NStr::ToUnicode( szStateName ) : L"";
		const std::wstring wszName = NStr::ToUnicode( NStr::Format( "id %d", GetSingleton<IAILogic>()->GetUniqueIDOfObject( pAIObj ) ) ) +
																 NStr::ToUnicode( szFrozenInfo ) +
																 L"," + wszStateName + L"," + reinterpret_cast<const wchar_t*>(pName->GetString());
		memcpy( pStatus->pszName, wszName.c_str(), (wszName.size() + 1) * 2 );
	}
#else
	if ( IText *pName = GetLocalNameLocal() ) 
		memcpy( pStatus->pszName, pName->GetString(), (pName->GetLength() + 1) * 2 );
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	else
	{
		static std::wstring szName;
		NStr::ToUnicode( &szName, pDesc->szKey );
		memcpy( pStatus->pszName, szName.c_str(), (szName.size() + 1) * sizeof(szName[0]) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene )
{
	CVec3 vPos;
	AI2Vis( &vPos, placement.center.x, placement.center.y, placement.z );
	// move main object
	pVisObj->SetDirection( placement.dir );
	pScene->MoveObject( pVisObj, vPos );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMOUnit::CommonUpdateHP( const float fNewHP, bool bUpdateHPBar )
{
	if ( fNewHP <= 0 )
	{
		fHP = fNewHP;
		/*
		// remove dead unit from container
		if ( pContainer ) 
		{
			pContainer->Load( this, false );
			pContainer = 0;
		}
		// remove all icons from dead unit
		static_cast_ptr<IObjVisObj*>(pVisObj)->RemoveIcon( -1 );
		*/
		// unit is dead, so we needn't modify any stats
		return false;
	}
	//
	if ( (fHP != fNewHP) && bUpdateHPBar )
	{
		// change HP bar
		if ( ISceneIconBar *pBar = static_cast<ISceneIconBar*>(static_cast_ptr<IObjVisObj*>(pVisObj)->GetIcon(ICON_HP_BAR)) )
		{
			pBar->SetLength( fNewHP );
			pBar->SetColor( MakeHPBarColor(fNewHP) );
		}
		//
		if ( pObserver ) 
			pObserver->UpdateHP( fNewHP );
	}
	//
	fHP = fNewHP;
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const float VALUE_MORALE_LOW = 0.5f;
static const float VALUE_AMMO_LOW = 0.3f;
static const float VALUE_AMMO_NO = 0.0f;
inline bool IsValueDecreased( const float fOld, const float fNew, const float fBound )
{
	return (fOld > fBound) && (fNew <= fBound);
}
inline bool IsValueIncreased( const float fOld, const float fNew, const float fBound )
{
	return (fOld <= fBound) && (fNew > fBound);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::CommonUpdateRPGStats( const float fNewHP, const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, bool bUpdateHPBar )
{
	if( CommonUpdateHP(fNewHP, bUpdateHPBar) == false )
		return;
	// modify morale icon
	if ( IsValueDecreased(fMorale, stats.fMorale, VALUE_MORALE_LOW) ) 
		SetIcon( ICON_LOW_MORAL, pVOB );
	else if ( IsValueIncreased(fMorale, stats.fMorale, VALUE_MORALE_LOW) ) 
		RemoveIcon( ICON_LOW_MORAL );
	//
	fMorale = stats.fMorale;
	// modify ammos
	const SUnitBaseRPGStats *pRPGStats = static_cast_gdb<const SUnitBaseRPGStats *>( pRPG );
	float fNewAmmoValue = 1;
	if ( pRPGStats->nAmmos[AMMO_TYPE_PRIMARY] ) 
		fNewAmmoValue = float( stats.nMainAmmo ) / float( pRPGStats->nAmmos[AMMO_TYPE_PRIMARY] );
	if ( pRPGStats->nAmmos[AMMO_TYPE_SECONDARY] ) 
		fNewAmmoValue = Min( fNewAmmoValue, float( stats.nSecondaryAmmo ) / float( pRPGStats->nAmmos[AMMO_TYPE_SECONDARY] ) );
	// check for no/low ammo
	if ( fAmmoValue != fNewAmmoValue ) 
	{
		if ( fNewAmmoValue < fAmmoValue ) 
		{
			if ( IsValueDecreased(fAmmoValue, fNewAmmoValue, VALUE_AMMO_NO) )
			{
				RemoveIcon( ICON_LOW_AMMO );
				SetIcon( ICON_NO_AMMO, pVOB );
			}
			else if ( IsValueDecreased(fAmmoValue, fNewAmmoValue, VALUE_AMMO_LOW) )
				SetIcon( ICON_LOW_AMMO, pVOB );
		}
		else
		{
			if ( IsValueIncreased(fAmmoValue, fNewAmmoValue, VALUE_AMMO_LOW) )
			{
				RemoveIcon( ICON_NO_AMMO );
				RemoveIcon( ICON_LOW_AMMO );
			}
			else if ( IsValueIncreased(fAmmoValue, fNewAmmoValue, VALUE_AMMO_NO) )
			{
				RemoveIcon( ICON_NO_AMMO );
				SetIcon( ICON_LOW_AMMO, pVOB );
			}
		}
	}
	//
	ammos[AMMO_TYPE_PRIMARY] = stats.nMainAmmo;
	ammos[AMMO_TYPE_SECONDARY] = stats.nSecondaryAmmo;
	fAmmoValue = fNewAmmoValue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMOUnit::AIUpdateDiplomacy( const SAINotifyDiplomacy &_diplomacy )
{
	SetDiplomacy( _diplomacy.eDiplomacy );
	nPlayerIndex = _diplomacy.nPlayer;
	if ( ISceneIconBar *pBar = static_cast<ISceneIconBar*>(static_cast_ptr<IObjVisObj*>(pVisObj)->GetIcon(ICON_HP_BAR)) )
		pBar->SetBorderColor( GetGlobalVar(NStr::Format("Scene.PlayerColors.Player%d", GetPlayerIndex()), int(0xff000000)) );
	if ( !IsFriend() )
	{
		RemoveIcon( ICON_NO_AMMO );
		RemoveIcon( ICON_LOW_AMMO );
		RemoveIcon( ICON_LOW_MORAL );
		RemoveIcon( ICON_UNIT_NO_SUPPLY );
		RemoveIcon( ICON_ALT_SHELL );
		RemoveIcon( ICON_AMBUSH );
	}
	else
	{
		// restore state icons
		// ammo
		if ( fAmmoValue <= VALUE_AMMO_NO ) 
			SetIcon( ICON_NO_AMMO, GetSingleton<IVisObjBuilder>() );
		else if ( fAmmoValue <= VALUE_AMMO_LOW ) 
			SetIcon( ICON_LOW_AMMO, GetSingleton<IVisObjBuilder>() );
		// morale
		if ( fMorale <= VALUE_MORALE_LOW ) 
			SetIcon( ICON_LOW_MORAL, GetSingleton<IVisObjBuilder>() );
	}
	return IsFriend();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::AssignSelectionGroup( const int nGroupID )
{
	if ( nSelectionGroupID != nGroupID )
	{
		// remove old group ID icon
		RemoveIcon( ICON_GROUP );
		// set new one
		if ( (nGroupID >= 0) && (nGroupID <= 9) )
			SetIcon( ICON_GROUP | (nGroupID << 16), GetSingleton<IVisObjBuilder>() );
		nSelectionGroupID = nGroupID;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::AIUpdateAcknowledgement( const EUnitAckType eAck, IClientAckManager *pAckManager, const int nSet )
{
	const SUnitBaseRPGStats *pStats = static_cast_gdb<const SUnitBaseRPGStats*>( pRPG );
	const float r = 1.0f * rand() / RAND_MAX;
	std::string szSound;
	// если звука негативного аска нет, то попытаться сыграть Generic
	if ( !pStats->ChooseAcknowledgement( r, eAck, &szSound, nSet ) &&	
			 pAckManager->IsNegative( eAck ) && 
			 !pStats->ChooseAcknowledgement( 0.0f, eAck, 0, nSet ) )
	{
		pStats->ChooseAcknowledgement( r, ACK_NEGATIVE, &szSound, nSet );
	}
	pAckManager->AddAcknowledgement( this, eAck, szSound, nSet );	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::AIUpdateBoredAcknowledgement( const SAIBoredAcknowledgement &ack, IClientAckManager *pAckManager )
{
	if ( ack.bPresent )
		pAckManager->RegisterAsBored( ack.eAck, this );
	else
		pAckManager->UnRegisterAsBored( ack.eAck, this );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::SendAcknowledgement( interface IClientAckManager *pAckManager, const EUnitAckType eAckType, const int nSet )
{
	const float fRand = 1.0f * rand() / RAND_MAX;
	std::string szAck;
	static_cast_gdb<const SUnitBaseRPGStats*>(pRPG)->ChooseAcknowledgement( fRand, eAckType, &szAck, nSet );
	pAckManager->AddAcknowledgement( this, eAckType, szAck, nSet );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::SetObserver( IUnitStateObserver *_pObserver ) 
{ 
	pObserver = _pObserver; 
	if ( pObserver ) 
	{
		IObjVisObj *pVO = static_cast_ptr<IObjVisObj*>( pVisObj );
		for ( int i = 1; i < ICON_NUM_ICONS; ++i )
		{
			if ( ISceneIcon *pIcon = pVO->GetIcon(i) )
				pObserver->AddIcon( i, pIcon );
		}
		pObserver->UpdateHP( fHP );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::UpdateLevel( const int nLevel )
{
	RemoveIcon( ICON_LEVEL );
	SetIcon( (nLevel << 16) | ICON_LEVEL, GetSingleton<IVisObjBuilder>() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::SendDeathAcknowledgement( interface IClientAckManager *pAckManager, const unsigned int nTimeAfterStart )
{
	const SUnitBaseRPGStats *pStats = static_cast_gdb<const SUnitBaseRPGStats*>( pRPG );
	const float r = 1.0f * rand() / RAND_MAX;
	std::string szSound;
	if ( pStats->ChooseAcknowledgement( r, ACK_UNIT_DIED, &szSound, 0 ) )
	{
		CVec3 vPos;
		WORD wDir;
		GetPlacement( &vPos, &wDir );
		pAckManager->AddDeathAcknowledgement( vPos, szSound, nTimeAfterStart );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMOUnit::AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, 
														  IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager )
{
	switch ( action.typeID ) 
	{
		case ACTION_NOTIFY_LEVELUP:
			UpdateLevel( action.nParam );
			break;
		case ACTION_NOTIFY_SELECTABLE_CHANGED:
			bCanSelect = action.nParam;
			break;
		case ACTION_NOTIFY_SET_AMBUSH:
			SetIcon( ICON_AMBUSH, pVOB );
			break;
		case ACTION_NOTIFY_REMOVE_AMBUSH:
			RemoveIcon( ICON_AMBUSH );
			break;
		case ACTION_NOTIFY_STORAGE_CONNECTED:
			if ( action.nParam == 0 ) 
				SetIcon( ICON_UNIT_NO_SUPPLY, pVOB );
			else
				RemoveIcon( ICON_UNIT_NO_SUPPLY );
			break;
		case ACTION_NOTIFY_CHANGE_SCENARIO_INDEX:
			SetScenarioIndex( action.nParam );
			break;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOUnit::UpdateGunTraces( const CVec3 &vStart, const CVec3 &vEnd, float fSpeed, NTimer::STime nCurrTime, IScene *pScene )
{
	SGunTrace trace;
	trace.birthTime = nCurrTime;
	trace.vStart = vStart;
	trace.vDir = vEnd - vStart;
	trace.vPoints[0] = vStart;
	trace.vPoints[1] = vStart;
	trace.vPoints[2] = vStart;
	trace.vPoints[3] = vStart;
	trace.deathTime = int( fabs( trace.vDir ) / fSpeed ) + trace.birthTime;
	pScene->AddGunTrace( trace );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMOUnit::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SMapObject*>(this) );
	saver.Add( 2, &ammos );
	saver.Add( 3, &fMorale );
	saver.Add( 4, &pContainer );
	saver.Add( 5, &pLocalName );
	saver.Add( 6, &fAmmoValue );
	saver.Add( 7, &bVisible );
	saver.Add( 8, &nSeason );
	saver.Add( 9, &nScenarioIndex );
	saver.Add( 10, &pObserver );
	saver.Add( 11, &dwIconFlags );
	saver.Add( 12, &nPlayerIndex );
	if ( saver.IsReading() ) 
	{
		if ( dwFlashFireColor == 0xffffffff ) 
			dwFlashFireColor = GetGlobalVar( (std::string("Scene.Colors.") + GetSeasonName(nSeason) + ".FlashFire.Color").c_str(), int(0xffffffff) );
		if ( dwFlashExpColor == 0xffffffff ) 
			dwFlashExpColor = GetGlobalVar( (std::string("Scene.Colors.") + GetSeasonName(nSeason) + ".FlashExplode.Color").c_str(), int(0xffffffff) );
	}
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

