#ifndef __MOSQUAD_H__
#define __MOSQUAD_H__
#pragma ONCE
#include "MapObject.h"
#include "Actions.h"
class CMOSquad : public CTRefCount<IMOSquad>
{
	OBJECT_SERVICE_METHODS( CMOSquad );
	DECLARE_SERIALIZE;
	struct SUnitDesc
	{
		CObj<IMOUnit> pUnit;								// unit itself
		float fHP;													// HP [0..1]
		float fAmmo1;												// primary ammo [0..1]
		float fAmmo2;												// secondary ammo [0..1]
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
	CPtr<ISquadVisObj> pSquadVisObj;
	typedef std::list<SUnitDesc> CUnitsList;
	CUnitsList passangers;								// all infantry units in the squad
	SAINotifyDiplomacy suspendedDiplomacy;
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
	void UpdateVisObj();
	CMOSquad();
	virtual ~CMOSquad();
public:
	virtual bool STDCALL Create( IRefCount *pAIObj, const SGDBObjectDesc *pDesc, int nSeason, int nFrameIndex, float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB );
	virtual void STDCALL SetPlacement( const CVec3 &vPos, const WORD &wDir );
	virtual void STDCALL GetPlacement( CVec3 *pvPos, WORD *pwDir );
	virtual const SGDBObjectDesc* STDCALL GetDesc() const { return pDesc; }
	virtual const SHPObjectRPGStats* STDCALL GetRPG() const { return pRPG; }
	virtual IRefCount* STDCALL GetAIObj() { return pAIObj; }
	virtual IRefCount* STDCALL GetParentAIObj() { return 0; }
	virtual bool STDCALL CanSelect() const { return passangers.empty() ? false : passangers.back().pUnit->CanSelect(); }
	virtual void STDCALL GetStatus( struct SMissionStatusObject *pStatus ) const {  }
	virtual void STDCALL GetActions( CUserActions *pActions, EActionsType eActions ) const;
	virtual int STDCALL AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager ) { return 0; }
	virtual void STDCALL AIUpdatePlacement( const struct SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene ) {  }
	virtual bool STDCALL AIUpdateRPGStats( const struct SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene ) { return true; }
	virtual bool STDCALL AIUpdateDiplomacy( const struct SAINotifyDiplomacy &diplomacy );
	virtual void STDCALL AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB ) {  }
	virtual void STDCALL AIUpdateShot( const struct SAINotifyBaseShot &shot, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene ) {  }
	virtual void STDCALL Visit( IMapObjVisitor *pVisitor ) {  }
	virtual bool STDCALL IsSelected() const;
	virtual void STDCALL Select( ISelector *pSelector, bool bSelect, bool bSelectSuper );
	virtual bool STDCALL Load( interface IMOUnit *pMO, bool bEnter );
	virtual void STDCALL UpdatePassangers() {  }
	virtual int STDCALL GetPassangers( IMOUnit **pBuffer, const bool bCanSelectOnly ) const;
	virtual int STDCALL GetFreePlaces() const { return 0; }
	virtual void STDCALL NotifyStatsChanged( IMOUnit *pUnit, float fHP, float fAmmo1, float fAmmo2 );
	virtual const int STDCALL GetSelectionGroupID() const;
};
#endif // __MOSQUAD_H__
