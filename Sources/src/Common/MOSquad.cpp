#include "StdAfx.h"

#include "MOSquad.h"
#include "..\Common\Actions.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMOSquad::CMOSquad()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMOSquad::~CMOSquad()
{
	while ( !passangers.empty() ) 
	{
		CObj<IMOUnit> pUnit = passangers.back().pUnit;
		passangers.pop_back();
		pUnit->SetSquad( 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMOSquad::Create( IRefCount *_pAIObj, const SGDBObjectDesc *_pDesc, int nSeason, int _nFrameIndex, float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB )
{
	pAIObj = _pAIObj;
	pDesc = _pDesc;
	pRPG = NGDB::GetRPGStats<SHPObjectRPGStats>( pGDB, pDesc );
	NI_ASSERT_TF( pRPG != 0, NStr::Format("Can't find RPG stats for object \"%s\"", pDesc->szKey.c_str()), return 0 );
	return pRPG != 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOSquad::SetPlacement( const CVec3 &vPos, const WORD &wDir )
{
	if ( passangers.empty() ) 
		return;
	CVec3 vAvePos = VNULL3;
	WORD wAveDir = 0;
	GetPlacement( &vAvePos, &wAveDir );
	//
	for ( CUnitsList::iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		CVec3 vLocalPos;
		WORD wLocalDir;
		it->pUnit->GetPlacement( &vLocalPos, &wLocalDir );
		vLocalPos -= vAvePos;
		wLocalDir -= wAveDir;
		it->pUnit->SetPlacement( vLocalPos + vPos, wLocalDir + wDir );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOSquad::GetPlacement( CVec3 *pvPos, WORD *pwDir )
{
	if ( passangers.empty() ) 
	{
		*pvPos = VNULL3;
		*pwDir = 0;
		return;
	}
	//
	CVec3 vAvePos = VNULL3;
	DWORD wAveDir = 0;
	int nNumUnits = 0;
	for ( CUnitsList::iterator it = passangers.begin(); it != passangers.end(); ++it, ++nNumUnits )
	{
		CVec3 vLocalPos;
		WORD wLocalDir;
		it->pUnit->GetPlacement( &vLocalPos, &wLocalDir );
		vAvePos += vLocalPos;
		wAveDir += wLocalDir;
	}
	//
	*pvPos = vAvePos / float( nNumUnits );
	*pwDir = WORD( wAveDir / nNumUnits );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// change selection state for this object
void CMOSquad::Select( ISelector *pSelector, bool bSelect, bool bSelectSuper )
{
	for ( CUnitsList::iterator it = passangers.begin(); it != passangers.end(); ++it )
		pSelector->Select( it->pUnit, bSelect, false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// load unit onboard or unload it
bool CMOSquad::Load( interface IMOUnit *pUnit, bool bEnter )
{
	if ( bEnter )
	{
		if ( GetUnit(pUnit) == 0 ) 
		{
			passangers.push_back( SUnitDesc(pUnit, pUnit->fHP) );
//			pUnit->Select( GetSelectionState() );
			pUnit->SetSquad( this );
			pUnit->AIUpdateDiplomacy( suspendedDiplomacy );
		}
	}
	else
	{
		for ( CUnitsList::iterator it = passangers.begin(); it != passangers.end(); ++it )
		{
			if ( it->pUnit == pUnit ) 
			{
				// first, remove it from internal container
				passangers.erase( it );
				// and, then, forget about squad :)
				pUnit->SetSquad( 0 );
				break;
			}
		}
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// get all passangers from container. return number of passangers. if pBuffer == 0, only returns number of passangers
int CMOSquad::GetPassangers( IMOUnit **pBuffer, const bool bCanSelectOnly ) const
{
	if ( pBuffer != 0 ) 
	{
		for ( CUnitsList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
			*pBuffer++ = it->pUnit;
	}
	return passangers.size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// notify about RPG stats changing fot the single squad member
void CMOSquad::NotifyStatsChanged( IMOUnit *pUnit, float fHP, float fAmmo1, float fAmmo2 )
{
	for ( CUnitsList::iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		if ( it->pUnit.GetPtr() == pUnit ) 
		{
			it->fHP = fHP;
			it->fAmmo1 = fAmmo1;
			it->fAmmo2 = fAmmo2;
			//
			UpdateVisObj();
			//
			break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOSquad::UpdateVisObj()
{
	if ( passangers.empty() ) 
		return;

	std::vector<ISquadVisObj::SData> units;
	units.reserve( passangers.size() );
	for ( CUnitsList::iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMOSquad::GetActions( CUserActions *pActions, EActionsType eActions ) const
{
	const SSquadRPGStats *pRPGStats = static_cast_gdb<const SSquadRPGStats*>( pRPG );
	*pActions = eActions == IMapObj::ACTIONS_WITH ? pRPGStats->availExposures : pRPGStats->availActions;
	//
	if ( (pRPGStats->members.size() > 1) || ((pRPGStats->members.size() == 1) && (pRPGStats->type != 0)) ) 
		pActions->RemoveAction( USER_ACTION_FORM_SQUAD );
	//
	if ( passangers.size() == 1 ) 
		pActions->RemoveAction( USER_ACTION_DISBAND_SQUAD );
	else if ( !passangers.empty() )
		pActions->RemoveAction( USER_ACTION_FORM_SQUAD );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CMOSquad::GetSelectionGroupID() const
{
	return passangers.empty() ? -1 : passangers.front().pUnit->nSelectionGroupID;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMOSquad::IsSelected() const
{
	for ( CUnitsList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		if ( it->pUnit->IsSelected() ) 
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMOSquad::AIUpdateDiplomacy( const struct SAINotifyDiplomacy &_diplomacy )
{
	suspendedDiplomacy.eDiplomacy = _diplomacy.eDiplomacy;
	suspendedDiplomacy.nPlayer = _diplomacy.nPlayer;
	suspendedDiplomacy.pObj = 0;//_diplomacy.pObj;
	for ( CUnitsList::iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		it->pUnit->AIUpdateDiplomacy( _diplomacy );
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMOSquad::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<SMapObject*>(this) );
	saver.Add( 2, &passangers );
	saver.Add( 3, &suspendedDiplomacy.eDiplomacy );
	saver.Add( 4, &suspendedDiplomacy.nPlayer );
	if ( saver.IsReading() ) 
		suspendedDiplomacy.pObj = 0;
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
