#ifndef __MAPOBJECT_H__
#define __MAPOBJECT_H__
#pragma ONCE
#include "../Main/RPGStats.h"
#include "../Scene/PFX.h"
#include "../SFX/SFX.h"
#include "../Scene/Scene.h"
#include "../AILogic/AITypes.h"
#include "../Image/Image.h"
interface IMapObjVisitor
{
	virtual void STDCALL VisitSprite( IVisObj *pVO, EObjGameType eGameType, EObjVisType eVisType, bool bOutbound = false ) = 0;
	virtual void STDCALL VisitMesh( IVisObj *pVO, EObjGameType eGameType, EObjVisType eVisType, bool bOutbound = false ) = 0;
	virtual void STDCALL VisitEffect( IVisObj *pVO, EObjGameType eGameType, EObjVisType eVisType, bool bOutbound = false ) = 0;
};
interface ISelectorVisitor
{
	virtual void STDCALL VisitMapObject( struct SMapObject *pMO ) const = 0;
};
interface ISelector
{
	virtual bool STDCALL Select( struct SMapObject *pMO, bool bSelect, bool bSelectSuper ) = 0;
	virtual bool STDCALL IsSelected( const struct SMapObject *pMO ) const = 0;
	virtual void STDCALL DoneSelection() = 0;
	virtual int STDCALL GetAIGroup() = 0;
	virtual bool STDCALL IsEmpty() const = 0;
	virtual void STDCALL Visit( ISelectorVisitor *pVisitor ) const = 0;
};
interface IMapObj : public IRefCount
{
	enum EActionsType
	{
		ACTIONS_WITH	= 0,
		ACTIONS_BY		= 1,
		ACTIONS_ALL		= 2
	};
	virtual bool STDCALL Create( IRefCount *pAIObj, const SGDBObjectDesc *pDesc, int nSeason, int nFrameIndex, float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB ) = 0;
	virtual void STDCALL SetPlacement( const CVec3 &vPos, const WORD &wDir ) = 0;
	virtual void STDCALL GetPlacement( CVec3 *pvPos, WORD *pwDir ) = 0;
	virtual const SGDBObjectDesc* STDCALL GetDesc() const = 0;
	virtual const SHPObjectRPGStats* STDCALL GetRPG() const = 0;
	virtual IRefCount* STDCALL GetAIObj() = 0;
	virtual IRefCount* STDCALL GetParentAIObj() = 0;
	virtual bool STDCALL CanSelect() const = 0;
	virtual void STDCALL GetStatus( struct SMissionStatusObject *pStatus ) const = 0;
	virtual void STDCALL GetActions( CUserActions *pActions, EActionsType eActions ) const = 0;
	virtual void STDCALL AIUpdatePlacement( const struct SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene ) = 0;
	virtual bool STDCALL AIUpdateRPGStats( const struct SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene ) = 0;
	virtual void STDCALL AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB ) = 0;
	virtual int STDCALL AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager ) = 0;
	virtual void STDCALL Visit( IMapObjVisitor *pVisitor ) = 0;
};
struct SMapObject : public IMapObj
{
	DECLARE_SERIALIZE;
public:
	CPtr<IRefCount> pAIObj;								// AI unit ��� ����� � AI
	CPtr<IVisObj> pVisObj;								// visual object - ��, ��� �������
	CPtr<IVisObj> pShadow;								// ���� �� ����� �������. ����� ���� ����� ����!!!
	CGDBPtr<SGDBObjectDesc> pDesc;				// game DB entry - ���������� �� ������� ���� ������, ����������� ������
	CGDBPtr<SHPObjectRPGStats> pRPG;			// RPG stats for all objects and units
	BYTE diplomacy;												// diplomacy settings for this object
	bool bCanSelect;											// can object be selected?
	int nSelectionGroupID;								// visual selection group for a fast reseting
	float fHP;														// current health (%)
	SMapObject() : diplomacy( EDI_NEUTRAL ), bCanSelect( false ), nSelectionGroupID( -1 ), fHP( 1 ) {  }
	void SetHP( float _fHP ) { fHP = _fHP; }
	float GetHP() const { return pRPG != 0 ? pRPG->fMaxHP * fHP : 0; }
	bool IsAlive() const { return ( GetHP() > 0 ) && ( pAIObj != 0 ); }
	float GetMaxHP() const { return pRPG != 0 ? pRPG->fMaxHP : 0; }
	bool IsHuman() const { return pDesc && ( pDesc->eGameType == SGVOGT_UNIT ) && ( pDesc->eVisType == SGVOT_SPRITE ); }
	bool IsTechnics() const { return pDesc && ( pDesc->eGameType == SGVOGT_UNIT ) && ( pDesc->eVisType == SGVOT_MESH ); }
	void SetDiplomacy( const EDiplomacyInfo eDiplomacy );
	bool IsEnemy() const { return diplomacy == EDI_ENEMY; }
	bool IsFriend() const { return diplomacy == EDI_FRIEND; }
	bool IsNeutral() const { return diplomacy == EDI_NEUTRAL; }
	virtual bool STDCALL CanSelect() const { return bCanSelect; }
	// The scene sound ID of the looped sound this object owns right now, 0
	// for none. The world uses it to reconcile the sound scene: a looped
	// sound whose owner is gone plays forever otherwise (and rides along in
	// every save made afterwards). See CWorldBase::ReconcileLoopedSounds.
	virtual WORD STDCALL GetOwnedLoopedSoundID() const { return 0; }
};
struct SBridgeSpanObject : public IMapObj
{
	DECLARE_SERIALIZE;
public:
	CPtr<IRefCount> pAIObj;								// span AI object
	CPtr<SMapObject> pSlab;
	CPtr<SMapObject> pBackGirder;
	CPtr<SMapObject> pFrontGirder;
	int nIndex;														// span index
	void SetHP( float _fHP ) { pSlab->SetHP( _fHP ); }
	float GetHP() const { return pSlab->GetHP(); }
	float GetMaxHP() const { return pSlab->GetMaxHP(); }
	void SetOpacity( BYTE op )
	{
		pSlab->pVisObj->SetOpacity( op );
		if ( pBackGirder )
			pBackGirder->pVisObj->SetOpacity( op );
		if ( pFrontGirder )
			pFrontGirder->pVisObj->SetOpacity( op );
	}

	void SetSpecular( SColor color )
	{
		if ( pSlab )
		{
			pSlab->pVisObj->SetSpecular( color );
		}
		if ( pBackGirder )
		{
			pBackGirder->pVisObj->SetSpecular( color );
		}
		if ( pFrontGirder )
		{
			pFrontGirder->pVisObj->SetSpecular( color );
		}
	}
	
	std::string GetObjectName() { return pSlab->pDesc->szKey; };
};
typedef std::list< CPtr<SBridgeSpanObject> > CBridgeSpanObjectsList;
interface IMOEffect : public SMapObject
{
	virtual bool STDCALL Create( IRefCount *pAIObj, const char *pszName, interface IVisObjBuilder *pVOB ) = 0;
};
interface IMOSelectable : public SMapObject
{
	virtual bool STDCALL IsSelected() const = 0;
	virtual void STDCALL Select( ISelector *pSelector, bool bSelect, bool bSelectSuper ) = 0;
	virtual bool STDCALL AIUpdateDiplomacy( const struct SAINotifyDiplomacy &diplomacy ) = 0;
	virtual void STDCALL SendAcknowledgement( interface IClientAckManager *pAckManager, const EUnitAckType eAckType, const int nSet ) {  }
};
interface IMOContainer : public IMOSelectable
{
	virtual bool STDCALL Load( interface IMOUnit *pMO, bool bEnter ) = 0;
	virtual void STDCALL UpdatePassangers() = 0;
	virtual int STDCALL GetPassangers( IMOUnit **pBuffer, const bool bCanSelectOnly = false ) const = 0;
	virtual int STDCALL GetFreePlaces() const = 0;
	virtual void STDCALL AIUpdateShot( const struct SAINotifyBaseShot &shot, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene ) = 0;
};
interface IUnitStateObserver : public IRefCount
{
	virtual void STDCALL AddIcon( const int nType, interface ISceneIcon *pIcon ) = 0;
	virtual void STDCALL RemoveIcon( const int nType ) = 0;
	virtual void STDCALL UpdateHP( const float fValue ) = 0;
	virtual void STDCALL RemoveUnit() = 0;
	virtual IMOUnit* STDCALL GetMOUnit() = 0;
};
interface IMOUnit : public IMOContainer
{
	virtual void STDCALL PrepareToRemove() = 0;
	virtual const bool STDCALL IsVisible() const = 0;
	virtual void STDCALL AssignSelectionGroup( const int nGroupID ) = 0;
	virtual void STDCALL SetContainer( IMOContainer *pContainer ) = 0;
	virtual IMOContainer* STDCALL GetContainer() const = 0;
	virtual void STDCALL SetSquad( interface IMOSquad *pSquad ) = 0;
	virtual interface IMOSquad* STDCALL GetSquad() = 0;
	virtual bool STDCALL Update( const NTimer::STime &currTime ) = 0;
	virtual void STDCALL AIUpdateAiming( const struct AIUpdateAiming &aiming ) = 0;
	virtual IMapObj* STDCALL AIUpdateFireWithProjectile( const struct SAINotifyNewProjectile &projectile, const NTimer::STime &currTime, interface IVisObjBuilder *pVOB ) = 0;
	virtual void STDCALL AddAnimation( const SUnitBaseRPGStats::SAnimDesc *pDesc ) = 0;
	virtual void STDCALL AIUpdateAcknowledgement( const EUnitAckType eAck, interface IClientAckManager *pAckManager, const int nSet ) = 0;
	virtual void STDCALL AIUpdateBoredAcknowledgement( const struct SAIBoredAcknowledgement &ack, interface IClientAckManager *pAckManager ) = 0;
	virtual void STDCALL RemoveSounds(interface IScene * pScene ) = 0;
	virtual interface IText* STDCALL GetLocalName() const = 0;
	virtual void STDCALL SetObserver( IUnitStateObserver *pObserver ) = 0;
	virtual int STDCALL GetPlayerIndex() const = 0;
	virtual bool STDCALL ChangeWithBlood( IVisObjBuilder *pVOB ) = 0;
};
interface IMOSquad : public IMOContainer
{
	virtual void STDCALL NotifyStatsChanged( IMOUnit *pUnit, float fHP, float fAmmo1, float fAmmo2 ) = 0;
	virtual const int STDCALL GetSelectionGroupID() const = 0;
};
typedef std::list< CPtr<SMapObject> > CMapObjectsList;
typedef std::list<SMapObject*> CMapObjectsPtrList;
typedef std::unordered_map<IRefCount*, CObj<SMapObject>, SDefaultPtrHash> CMapObjectsMap;
typedef std::unordered_set<SMapObject*, SDefaultPtrHash> CMapObjectsSet;
typedef std::unordered_map<IRefCount*, CPtr<SBridgeSpanObject>, SDefaultPtrHash> CBridgeSpanObjectsMap;
interface IText* GetLocalName( const SGDBObjectDesc *pDesc );
#endif // __MAPOBJECT_H__
