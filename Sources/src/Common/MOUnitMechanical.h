#ifndef __MOUNITMECHANICAL_H__
#define __MOUNITMECHANICAL_H__
#pragma ONCE
#include "MOUnit.h"
#include "Passangers.h"
class CMOUnitMechanical : public CMOUnit
{
	OBJECT_SERVICE_METHODS( CMOUnitMechanical );
	DECLARE_SERIALIZE;
	struct SModelChange
	{
		std::string szModelName;						// model to change to
		NTimer::STime time;									// time to change in
		SModelChange() {  }
		SModelChange( const std::string &_szModelName, const NTimer::STime &_time )
			: szModelName( _szModelName ), time( _time ) {  }
		int operator&( IStructureSaver &ss )
		{
			CSaverAccessor saver = &ss;
			saver.Add( 1, &szModelName );
			saver.Add( 2, &time );
			return 0;
		}
	};
	struct SAnimChange
	{
		int nAnim;													// animation index
		NTimer::STime time;									// time to set this animation
		NTimer::STime length;								// animation length
		SAnimChange() {  }
		SAnimChange( const int _nAnim, const NTimer::STime &_time, const NTimer::STime &_length )
			: nAnim( _nAnim ), time( _time ), length( _length ) {  }
		int operator&( IStructureSaver &ss )
		{
			CSaverAccessor saver = &ss;
			saver.Add( 1, &nAnim );
			saver.Add( 2, &time );
			saver.Add( 2, &length );
			return 0;
		}
	};
	struct SEffect
	{
		CPtr<IEffectVisObj> pEffect;				// attached effect
		int nPointIndex;										// point index, this effect attached to
		NTimer::STime timeLastUpdate;				// last time, this effect was updated (or added/started)
		SEffect() : nPointIndex( -1 ), timeLastUpdate( 0 ) {  }
		SEffect( IEffectVisObj *_pEffect, int _nPointIndex, const NTimer::STime &_timeLastUpdate ) 
			: pEffect( _pEffect ), nPointIndex( _nPointIndex ), timeLastUpdate( _timeLastUpdate ) {  }
		int operator&( IStructureSaver &ss );
	};
	WORD wMoveSoundID;										// sound, this unit producas during movement (0 = invalid)
	WORD wNonCycleSoundID;								// some noncycled sound, that must stop with unit destruction
	bool bDiveMove;												// for dive bombers
	CPassangersList passangers;						// all passangers inside
	CPtr<IMeshVisObj> pExtPassangers;			// 'external passangers' - солдаты на броне
	int nNumExtPassangers;								// на сколько внешних пассажиров сейчас рассчитана моделька
	bool bInstalled;											// is this units installed (for artillery)
	bool bArtilleryHooked;								// artillery already hooked (for transport)
	CVec3 vLastTracedCorners[4];          // last corners for traces
	CVec3 vLastPos;                       // last traced position;
	float fTraceLenSq;										// length of trace;
	bool bLastTracedDir;                  // last traced direction, true if forward;
	WORD wLastDir;                        // last traced direction 
	bool bInEditor;
	float fTrackLifeCoeff;
	float fTraceProbabilityCoeff;
	float fTraceSpeedCoeff;
	bool bSkipTrack;
	std::list< CPtr<IEffectVisObj> > smokeEffects; // train smoke effects
	typedef std::list<SModelChange> CModelChangesList;
	CModelChangesList modelchanges;				// delayed model changes
	typedef std::list<SAnimChange> CAnimChangeList;
	CAnimChangeList animchanges;					// delayed animation changes
	typedef std::list<SEffect> CEffectsList;
	CEffectsList effects;									// attached effects
	IMeshVisObj* GetVisObj() { return static_cast_ptr<IMeshVisObj*>( pVisObj ); }
	IMeshAnimation* GetAnim() { return static_cast<IMeshAnimation*>( GetVisObj()->GetAnimation() ); }
	const SMechUnitRPGStats* GetRPGStats() const { return static_cast_gdb<const SMechUnitRPGStats*>( pRPG ); }
	int GetNumTotalSlots() const { return GetRPGStats()->nPassangers; }
	int GetNumFreeSlots() const { return GetNumTotalSlots() - passangers.size(); }
	void UpdateModelWithHP( const float fHP, IVisObjBuilder *pVOB );
	void ChangeExtPassangers( IScene *pScene, IVisObjBuilder *pVOB );
	bool HasEffects() const { return !effects.empty(); }
	void AddEffect( IEffectVisObj *pEffect, int nPointIndex, const NTimer::STime &timeStart ) { effects.push_back( SEffect(pEffect, nPointIndex, timeStart) ); }
	void RemoveExhaustedEffects( const NTimer::STime &time );
	void UpdateAttachedEffects( const NTimer::STime &currTime, IScene *pScene = 0 );
	virtual void MakeVisible( const bool bVisible );
	void ActionMove( const SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene );
	void ActionDie( const SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene );
	void ActionStop( const SAINotifyAction &action, IScene *pScene );
	int ActionInstall( const SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB );
	const int DoInstall( const int nAnimation, const NTimer::STime &timeAction, const int nRPGDuration, const char *pszModel2 = 0 );
	const int DoUnInstall( const int nAnimation, const NTimer::STime &timeAction, const int nRPGDuration, IVisObjBuilder *pVOB, const char *pszModel3 = 0, bool bInstantly = false );
	int ChangeModel( const std::string &szModelName, const NTimer::STime &currTime, const NTimer::STime &timeChange );
	const SUnitBaseRPGStats::SAnimDesc* GetAnimationByType( int nType )
	{
		if ( (nType >= GetRPGStats()->animdescs.size()) || GetRPGStats()->animdescs[nType].empty() )
			return 0;
		return &( GetRPGStats()->animdescs[nType][ rand() % GetRPGStats()->animdescs[nType].size() ] );
	}
	void ChangeState( EUnitState state, const NTimer::STime &currTime );
	virtual const CVec3 GetIconAddValue() const { return CVec3(0, 0, 10); }
	CMOUnitMechanical();
	virtual ~CMOUnitMechanical();
public:
	virtual bool STDCALL Create( IRefCount *pAIObj, const SGDBObjectDesc *pDesc, int nSeason, int nFrameIndex, float fHP, interface IVisObjBuilder *pVOB, IObjectsDB *pGDB );
	virtual void STDCALL PrepareToRemove();
	virtual void STDCALL GetStatus( struct SMissionStatusObject *pStatus ) const;
	virtual void STDCALL Select( ISelector *pSelector, bool bSelect, bool bSelectSuper );
	virtual void STDCALL SetPlacement( const CVec3 &vPos, const WORD &wDir );
	virtual bool STDCALL Load( interface IMOUnit *pMO, bool bEnter );
	virtual void STDCALL UpdatePassangers();
	virtual int STDCALL GetPassangers( IMOUnit **pBuffer, const bool bCanSelectOnly ) const;
	virtual int STDCALL GetFreePlaces() const { return 0; }
	virtual void STDCALL GetActions( CUserActions *pActions, EActionsType eActions ) const;
	virtual bool STDCALL Update( const NTimer::STime &currTime );
	virtual void STDCALL AIUpdatePlacement( const SAINotifyPlacement &placement, const NTimer::STime &currTime, IScene *pScene );
	virtual void STDCALL LeaveTrace( SMechTrace *pTrace, const SAINotifyPlacement &placement, const NTimer::STime &currTime, bool secondTrack,	const SMechUnitRPGStats *pStats, const CVec3 &vPos, bool isForward, const CVec3 &dir, IScene *pScene );
	virtual bool STDCALL AIUpdateRPGStats( const SAINotifyRPGStats &stats, IVisObjBuilder *pVOB, IScene * pScene );
	virtual void STDCALL AIUpdateHit( const struct SAINotifyHitInfo &hit, const NTimer::STime &currTime, IScene *pScene, IVisObjBuilder *pVOB );
	virtual IMapObj* STDCALL AIUpdateFireWithProjectile( const SAINotifyNewProjectile &projectile, const NTimer::STime &currTime, interface IVisObjBuilder *pVOB );
	virtual int STDCALL AIUpdateActions( const SAINotifyAction &action, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene, interface IClientAckManager *pAckManager );
	virtual void STDCALL AIUpdateShot( const struct SAINotifyBaseShot &shot, const NTimer::STime &currTime, IVisObjBuilder *pVOB, IScene *pScene );
	virtual void STDCALL Visit( IMapObjVisitor *pVisitor );
	virtual void STDCALL AddAnimation( const SUnitBaseRPGStats::SAnimDesc *pDesc );
	virtual void STDCALL RemoveSounds(interface IScene * pScene );
	virtual bool STDCALL ChangeWithBlood( IVisObjBuilder *pVOB ) { return true; }
};
#endif // __MOUNITMECHANICAL_H__
