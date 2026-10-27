#ifndef __MOOBJECT_H__
#define __MOOBJECT_H__
#pragma ONCE
#include "MapObject.h"
#include "../Main/TextSystem.h"
class CMOObject : public CTRefCount<SMapObject>
{
	OBJECT_SERVICE_METHODS( CMOObject );
	DECLARE_SERIALIZE;
	DWORD actions[2];											// actions, this object can do
	mutable CPtr<IText> pLocalName;				// localized name of this object (ZB "����� �� ������")
	bool bDead;														// is object already dead?
	std::string szFlagSide;
	static int nLastMarkerID;
	int nMarkerID;
	void FillActions();
	bool AddAction( int nAction );
	void UpdateModelWithHP( const float fNewHP, IVisObjBuilder *pVOB );
	// The Remastered camera turn (Main/CameraTurn.h): the step it is drawn for,
	// and a fence segment's frame as the 2003 view draws it (-1: its sprite's).
	int nCameraTurn;
	int nOwnFrame;
	int nSeason;												// its season (a prop of four sides draws the season's picture of a side)
	int nShownTurn;											// the turn of a prop of four sides it shows (0: its own)
	int nFacing;												// its own facing in quarter turns (a prop of four sides draws by it)
	CVec3 vTurnShift;											// how far a turned fence segment's picture stands from its place (world units)
	void ApplyCameraTurn();
	void ShiftPicture( const CVec3 &vShift );
	IText* GetLocalName() const
	{
		if ( pLocalName == 0 )
			pLocalName = ::GetLocalName( pDesc );
		return pLocalName;
	}
public:
	CMOObject() : nCameraTurn( 0 ), nOwnFrame( -1 ), nSeason( 0 ), nShownTurn( 0 ), nFacing( 0 ), vTurnShift( VNULL3 ) {  }
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
	virtual void STDCALL SetCameraTurn( const int nTurn, IVisObjBuilder *pVOB );
};
#endif // __MOOBJECT_H__
