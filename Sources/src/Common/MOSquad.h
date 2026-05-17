#ifndef __MOSQUAD_H__
#define __MOSQUAD_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "MapObject.h"
#include "Actions.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMOSquad : public CTRefCount<IMOSquad>
{
	OBJECT_SERVICE_METHODS( CMOSquad );
	DECLARE_SERIALIZE;
	//
	struct SUnitDesc
	{
		CObj<IMOUnit> pUnit;								// unit itself
		float fHP;													// HP [0..1]
		float fAmmo1;												// primary ammo [0..1]
		float fAmmo2;												// secondary ammo [0..1]
		//
		SUnitDesc() {  }
		SUnitDesc( IMOUnit *_pUnit, const float _fHP )
			: pUnit( _pUnit ), fHP( _fHP ), fAmmo1( 1 ), fAmmo2( 1 ) {  }
		int operator&( IStructureSaver &ss )
		{
			CSaverAccessor saver = &ss;
			saver.Add( 1, &pUnit );
			saver.Add( 2, &fHP );
			saver.Add( 3, &fAmmo1 );
			saver.Add( 4, &fAmmo2 );
			return 0;
		}
	};
	// visual data
	CPtr<ISquadVisObj> pSquadVisObj;
	//
	typedef std::list<SUnitDesc> CUnitsList;
	CUnitsList passangers;								// all infantry units in the squad
	SAINotifyDiplomacy suspendedDiplomacy;
	//
	const int GetSelectionState() const { return IsSelected() ? SGVOSS_SELECTED : SGVOSS_UNSELECTED; }
	SUnitDesc* GetUnit( IMOUnit *pUnit )
	{
		for ( CUnitsList::iterator it = passangers.begin(); it != passangers.end(); ++it )
		{
			if ( it->pUnit.GetPtr() == pUnit )
				return &( *it );
		}
		return 0;
	}
	//
	void UpdateVisObj();
	//
	CMOSquad();
	virtual ~CMOSquad();
public:
	virtual bool STDCALL Create( IRefCount *pAIObj, const SGDBObjectDesc *pDesc, int nSeason, int nFrameIndex, float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB );
	// placement
	virtual void STDCALL SetPlacement( const CVec3 &vPos, const WORD &wDir );
	virtual void STDCALL GetPlacement( CVec3 *pvPos, WORD *pwDir );
	// stats functions
	virtual const SGDBObjectDesc* STDCALL GetDesc() const { return pDesc; }
	virtual const SHPObjectRPGStats* STDCALL GetRPG() const { return pRPG; }
	// AI object retrieving
	virtual IRefCount* STDCALL GetAIObj() { return pAIObj; }
	virtual IRefCount* STDCALL GetParentAIObj() { return 0; }
	//
	virtual bool STDCALL CanSelect() const { return passangers.empty() ? false : passangers.back().pUnit->CanSelect(); }
	// get status for mission status bar
	virtual void STDCALL GetStatus( struct SMissionStatusObject *pStatus ) const {  }
	// get actions, which this object can perform or actions, thi object can be acted with
	virtual void STDCALL GetActions( CUserActions *pActions, EActionsType eActions ) const;
	// common updates
	virtual int STDCALL AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager ) { return 0; }
	virtual void STDCALL AIUpdatePlacement( const struct SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene ) {  }
	virtual bool STDCALL AIUpdateRPGStats( const struct SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene ) { return true; }
	virtual bool STDCALL AIUpdateDiplomacy( const struct SAINotifyDiplomacy &diplomacy );
	virtual void STDCALL AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB ) {  }
	// firing... (from container of by himself)
	virtual void STDCALL AIUpdateShot( const struct SAINotifyBaseShot &shot, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene ) {  }
	// visiting
	virtual void STDCALL Visit( IMapObjVisitor *pVisitor ) {  }
	// check, is this object selected?
	virtual bool STDCALL IsSelected() const;
	// change selection state for this object
	virtual void STDCALL Select( ISelector *pSelector, bool bSelect, bool bSelectSuper );
	// load unit onboard or unload it
	virtual bool STDCALL Load( interface IMOUnit *pMO, bool bEnter );
	//
	virtual void STDCALL UpdatePassangers() {  }
	// get all passangers from container. return number of passangers. if pBuffer == 0, only returns number of passangers
	virtual int STDCALL GetPassangers( IMOUnit **pBuffer, const bool bCanSelectOnly ) const;
	// get free places
	virtual int STDCALL GetFreePlaces() const { return 0; }
	// notify about RPG stats changing fot the single squad member
	virtual void STDCALL NotifyStatsChanged( IMOUnit *pUnit, float fHP, float fAmmo1, float fAmmo2 );
	// get selection ID
	virtual const int STDCALL GetSelectionGroupID() const;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MOSQUAD_H__
