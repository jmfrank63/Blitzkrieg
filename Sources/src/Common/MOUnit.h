#ifndef __MOUNIT_H__
#define __MOUNIT_H__
#pragma ONCE
#include "MapObject.h"
#include "../Scene/Scene.h"
#include "../Anim/Animation.h"
#include "../Main/TextSystem.h"
class CMOUnit : public CTRefCount<IMOUnit>
{
	DECLARE_SERIALIZE;
protected:
	enum { AMMO_TYPE_PRIMARY = 0, AMMO_TYPE_SECONDARY = 1 };
	enum EUnitState
	{
		STATE_IDLE			= 0,
		STATE_MOVE			= 1,
		STATE_AMBUSH		= 2,
		STATE_DIE				= 3
	};
private:
	int ammos[2];													// primary and secondary ammo counts
	float fAmmoValue;											// ammo value (minimum respection curr_ammo/max_ammo)
	float fMorale;												// morale
	EUnitState eState;										// current unit's state
	CPtr<IMOContainer> pContainer;				// AI object, this units contained in
	mutable CPtr<IText> pLocalName;				// localized name of this unit (ZB "Medium tank T-34")
	bool bVisible;												// is this unit visible by others?
	static int nSeason;										// season (one for all)
	static DWORD dwFlashFireColor;				// flash color during firing(one for all units)
	static DWORD dwFlashExpColor;					// flash color in explosion(one for all units)
	int nScenarioIndex;										// scenario unit index
	int nPlayerIndex;											// player index
	CPtr<IUnitStateObserver> pObserver;		// icon updater for 'who-in-container' interface
	DWORD dwIconFlags;										// icons by bits
	virtual const CVec3 GetIconAddValue() const = 0;
protected:
	bool OnCreate();
	void CommonUpdateRPGStats( const float fNewHP, const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, bool bUpdateHPBar = true );
	bool CommonUpdateHP( const float fNewHP, bool bUpdateHPBar = true );
	const int GetAmmo( const int nIndex ) const { return ammos[nIndex]; }
	void SetSeason( const int _nSeason ) { nSeason = _nSeason; }
	const int GetSeason() const { return nSeason; }
	void SetScenarioIndex( const int nIndex ) { nScenarioIndex = nIndex == -1 ? -2 : nIndex; }
	const int GetScenarioIndex() const { return nScenarioIndex; }
	void SetVisible( const bool _bVisible ) { bVisible = _bVisible; MakeVisible( bVisible ); }
	virtual void MakeVisible( const bool bVisible ) = 0;
	const bool IsVisibleLocal() const { return bVisible; }
	void GetActionsLocal( EActionsType eActions, CUserActions *pActions ) const;
	const bool IsDead() const { return fHP <= 0.0f; }
	DWORD GetFlashFireColor() const { return dwFlashFireColor; }
	DWORD GetFlashExpColor() const { return dwFlashExpColor; }
	EUnitState GetCurrState() const { return eState; }
	void SetCurrState( EUnitState state ) { eState = state; }
	const bool CanAddIcon( int nType ) const;
	const bool CanShowIcons() const;
	void SetIcon( int nType, IVisObjBuilder *pVOB );
	void RemoveIcon( int nType );
	const SUnitBaseRPGStats::SAnimDesc* GetAnimDesc( const DWORD dwAnim ) const
	{
		const int dwAnimType = ( dwAnim >> 16 ) & 0x0fff;
		const std::vector<SUnitBaseRPGStats::SAnimDesc> &animdescs = static_cast_gdb<const SUnitBaseRPGStats*>(pRPG)->animdescs[dwAnimType];
		return animdescs.empty() ? 0 : &(animdescs[dwAnim & 0xffff]);
	}
	IText* GetLocalNameLocal() const
	{
		if ( pLocalName == 0 )
			pLocalName = ::GetLocalName( pDesc );
		return pLocalName;
	}
	void ClearLocalName() { pLocalName = 0; }
	IUnitStateObserver* GetObserver() { return pObserver; }
	void UpdateLevel( const int nLevel );
	void UpdateGunTraces( const CVec3 &vStart, const CVec3 &vEnd, float fSpeed, NTimer::STime nCurrTime, IScene *pScene );
	void SendDeathAcknowledgement( interface IClientAckManager *pAckManager, const unsigned int nTimeAfterStart );
public:
	CMOUnit();
	virtual void STDCALL PrepareToRemove() {  }
	virtual bool STDCALL IsSelected() const { return pVisObj->GetSelectionState() == SGVOSS_SELECTED; }
	virtual void STDCALL SetPlacement( const CVec3 &vPos, const WORD &wDir );
	virtual void STDCALL GetPlacement( CVec3 *pvPos, WORD *pwDir );
	virtual const SGDBObjectDesc* STDCALL GetDesc() const { return pDesc; }
	virtual const SHPObjectRPGStats* STDCALL GetRPG() const { return pRPG;  }
	virtual IRefCount* STDCALL GetAIObj() { return pAIObj; }
	virtual IRefCount* STDCALL GetParentAIObj() { return pContainer; }
	virtual void STDCALL GetStatus( struct SMissionStatusObject *pStatus ) const;
	virtual const bool STDCALL IsVisible() const { return IsVisibleLocal(); }
	virtual void STDCALL AssignSelectionGroup( const int nGroupID );
	virtual void STDCALL SetContainer( IMOContainer *_pContainer ) 
	{ 
		pContainer = _pContainer; 
		if ( pContainer == 0 )
			static_cast_ptr<IObjVisObj*>(pVisObj)->SetVisible( IsVisibleLocal() );
		else
			static_cast_ptr<IObjVisObj*>(pVisObj)->SetVisible( false );
	}
	IMOContainer* STDCALL GetContainer() const { return pContainer; }
	virtual void STDCALL SetSquad( interface IMOSquad *pSquad ) {  }
	virtual interface IMOSquad* STDCALL GetSquad() { return 0; }
	virtual void STDCALL AIUpdatePlacement( const struct SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene );
	virtual bool STDCALL AIUpdateDiplomacy( const struct SAINotifyDiplomacy &diplomacy );
	virtual int STDCALL AIUpdateActions( const struct SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager );
	virtual void STDCALL AIUpdateAcknowledgement( const EUnitAckType eAck, IClientAckManager *pAckManager, const int nSet );
	virtual void STDCALL AIUpdateBoredAcknowledgement( const SAIBoredAcknowledgement &ack, IClientAckManager *pAckManager );
	virtual void STDCALL SendAcknowledgement( interface IClientAckManager *pAckManager, const EUnitAckType eAckType, const int nSet  );
	virtual bool STDCALL Update( const NTimer::STime &currTime ) { return true; }
	virtual void STDCALL AIUpdateAiming( const struct AIUpdateAiming &aiming ) {  }
	virtual void STDCALL AddAnimation( const SUnitBaseRPGStats::SAnimDesc *pDesc ) {  }
	virtual void STDCALL RemoveSounds( interface IScene *pScene ) {  }
	virtual interface IText* STDCALL GetLocalName() const { return GetLocalNameLocal(); }
	virtual void STDCALL SetObserver( IUnitStateObserver *pObserver );
	virtual int STDCALL GetPlayerIndex() const { return nPlayerIndex; };
};
#endif // __MOUNIT_H__
