#ifndef __MOENTRENCHMENT_H__
#define __MOENTRENCHMENT_H__
#pragma ONCE
#include "MapObject.h"
#include "..\Main\TextSystem.h"
#include "..\Anim\Animation.h"
#include "..\Scene\Scene.h"
class CMOEntrenchmentSegment : public CTRefCount<SMapObject>
{
	OBJECT_SERVICE_METHODS( CMOEntrenchmentSegment );
	DECLARE_SERIALIZE;
	mutable CPtr<IText> pLocalName;				// localized name of this object (ZB "Гавно на дороге")
	const SEntrenchmentRPGStats* GetRPGStats() const { return static_cast_gdb<const SEntrenchmentRPGStats*>( pRPG ); }
	IMeshVisObj* GetVisObj() { return static_cast_ptr<IMeshVisObj*>( pVisObj ); }
	IMeshAnimation* GetAnim() { return static_cast<IMeshAnimation*>( GetVisObj()->GetAnimation() ); }
	IText* GetLocalName() const
	{
		if ( pLocalName == 0 )
			pLocalName = ::GetLocalName( pDesc );
		return pLocalName;
	}
	void UpdateModelWithHP( const float fNewHP, IVisObjBuilder *pVOB );
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
	virtual int STDCALL AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager ) { return 0; }
	virtual void STDCALL AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB );
	virtual void STDCALL Visit( IMapObjVisitor *pVisitor );
};
#endif // __MOENTRENCHMENT_H__
