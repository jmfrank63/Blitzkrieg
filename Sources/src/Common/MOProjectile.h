#ifndef __MOPROJECTILE_H__
#define __MOPROJECTILE_H__
#pragma ONCE
#include "MapObject.h"
class CMOProjectile : public CTRefCount<IMOEffect>
{
	OBJECT_SERVICE_METHODS( CMOProjectile );
	DECLARE_SERIALIZE;
	float fTimeStart;											// start time of this projectile
	float fTimeDuration;									// duration of this effect
	CVec3 delta;													// difference between object's center and real gun fire point
	WORD wMoveSoundID;										// projectile movement sound
	CVec3 vLastPos;												// last update position
	NTimer::STime timeLastTime;						// last update time
	IEffectVisObj* GetVisObj() { return static_cast_ptr<IEffectVisObj*>( pVisObj ); }
public:
	CMOProjectile();
	virtual ~CMOProjectile();
	void Init( const NTimer::STime &_timeStart, const NTimer::STime &_timeDuration, const CVec3 &_delta );
	virtual bool STDCALL Create( IRefCount *pAIObj, const SGDBObjectDesc *pDesc, int nSeason, int nFrameIndex, float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB );
	virtual bool STDCALL Create( IRefCount *pAIObj, const char *pszName, interface IVisObjBuilder *pVOB );
	virtual void STDCALL SetPlacement( const CVec3 &vPos, const WORD &wDir );
	virtual void STDCALL GetPlacement( CVec3 *pvPos, WORD *pwDir );
	virtual const SGDBObjectDesc* STDCALL GetDesc() const { return pDesc; }
	virtual const SHPObjectRPGStats* STDCALL GetRPG() const { return pRPG;  }
	virtual IRefCount* STDCALL GetAIObj() { return pAIObj; }
	virtual IRefCount* STDCALL GetParentAIObj() { return 0; }
	virtual void STDCALL GetStatus( struct SMissionStatusObject *pStatus ) const {  }
	virtual void STDCALL GetActions( CUserActions *pActions, EActionsType eActions ) const {  }
	virtual void STDCALL AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene );
	virtual bool STDCALL AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene ) { return true; }
	virtual int STDCALL AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager );
	virtual void STDCALL AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB ) {  }
	virtual void STDCALL Visit( IMapObjVisitor *pVisitor );
};
#endif // __MOPROJECTILE_H__
