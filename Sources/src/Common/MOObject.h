#ifndef __MOOBJECT_H__
#define __MOOBJECT_H__
#pragma ONCE
#include "MapObject.h"
#include "..\Main\TextSystem.h"
class CMOObject : public CTRefCount<SMapObject>
{
	OBJECT_SERVICE_METHODS( CMOObject );
	DECLARE_SERIALIZE;
	DWORD actions[2];											// actions, this object can do
	mutable CPtr<IText> pLocalName;				// localized name of this object (ZB "Гавно на дороге")
	bool bDead;														// is object already dead?
	std::string szFlagSide;
	static int nLastMarkerID;
	int nMarkerID;
	void FillActions();
	bool AddAction( int nAction );
	void UpdateModelWithHP( const float fNewHP, IVisObjBuilder *pVOB );
	IText* GetLocalName() const
	{
		if ( pLocalName == 0 )
			pLocalName = ::GetLocalName( pDesc );
		return pLocalName;
	}
public:
	virtual bool STDCALL Create( IRefCount *pAIObj, const SGDBObjectDesc *pDesc, int nSeason, int nFrameIndex, float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB );
	virtual void STDCALL SetPlacement( const CVec3 &vPos, const WORD &wDir );
	virtual void STDCALL GetPlacement( CVec3 *pvPos, WORD *pwDir );
	virtual const SGDBObjectDesc* STDCALL GetDesc() const { return pDesc; }
	virtual const SHPObjectRPGStats* STDCALL GetRPG() const { return pRPG;  }
	virtual IRefCount* STDCALL GetAIObj() { return pAIObj; }
	virtual IRefCount* STDCALL GetParentAIObj() { return 0; }
	virtual void STDCALL GetStatus( struct SMissionStatusObject *pStatus ) const;
	virtual void STDCALL GetActions( CUserActions *pActions, EActionsType eActions ) const;
	virtual void STDCALL AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene );
	virtual bool STDCALL AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene );
	virtual int STDCALL AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager );
	virtual void STDCALL AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB );
	virtual void STDCALL Visit( IMapObjVisitor *pVisitor );
};
#endif // __MOOBJECT_H__
