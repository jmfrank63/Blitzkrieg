#ifndef __EFFECTVISOBJ_H__
#define __EFFECTVISOBJ_H__
#pragma ONCE
#include "../Formats/fmtEffect.h"
template <class TYPE>
struct SEffect
{
	DWORD dwStart;												// start time (with respect to parent effect start time)
	DWORD dwEnd;													// end time (with respect to parent effect start time)
	bool bActive;													// is effect active&
	CVec3 vPos;														// position (with respect to parent effect position)
	CVec3 vRelPos;
	CPtr<TYPE> pObj;											// effect object
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &dwStart );
		saver.Add( 2, &dwEnd );
		saver.Add( 3, &vPos );
		saver.Add( 4, &vRelPos );
		saver.Add( 5, &pObj );
		return 0;
	}
};
typedef SEffect<ISpriteVisObj> SSpriteEffect;
typedef SEffect<IParticleSource> SParticleEffect;
class CEffectVisObj : public IEffectVisObj
{
	OBJECT_NORMAL_METHODS( CEffectVisObj );
	DECLARE_SERIALIZE;
	std::vector<SSpriteEffect> sprites;		// sprite subeffects
	std::vector<SParticleEffect> particles;	// particle subeffects
	std::string szSoundName;							// sound, attacked to this effect
	DWORD dwStartTime;										// effect start time
	DWORD dwDuration;											// total effect duration
	CVec3 vPos;														// world position
	int nDirection;												// direction...
	EVisObjSelectionState selectionState;	// selection state
	bool bStopped;												// is effect stopped?
	bool bSuspended;
public:
	CEffectVisObj() : dwStartTime( 0 ), dwDuration( 0 ), bStopped( false ), bSuspended( false ) {  }
	CEffectVisObj( const std::string &_szSoundName ) : szSoundName( _szSoundName ), dwStartTime( 0 ), dwDuration( 0 ), bStopped( false ), bSuspended( false ) {  }
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false );
	virtual void STDCALL SetDirection( const int nDir );
	virtual void STDCALL SetPosition( const CVec3 &pos ) { vPos = pos; }
	virtual void STDCALL SetPlacement( const CVec3 &pos, const int nDir ) { SetPosition(pos); SetDirection(nDir); }
	virtual const CVec3& STDCALL GetPosition() const { return vPos; }
	virtual int STDCALL GetDirection() const { return 0; }
	virtual void STDCALL Select( EVisObjSelectionState state ) { selectionState = state; }
	virtual EVisObjSelectionState STDCALL GetSelectionState() const { return selectionState; }
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const CVec2 &point, CVec2 *pShift ) { return false; }
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const RECT &rect ) { return false; }
	virtual void STDCALL SetOpacity( BYTE opacity )
	{
		for ( std::vector<SSpriteEffect>::iterator it = sprites.begin(); it != sprites.end(); ++it )
			it->pObj->SetOpacity( opacity );
		/*
		for ( std::vector<SParticleEffect>::iterator it = particles.begin(); it != particles.end(); ++it )
			it->pObj->SetOpacity( opacity );
			*/
	}
	virtual void STDCALL SetColor( DWORD color ) {  }
	virtual void STDCALL SetSpecular( DWORD color ) {  }
	virtual void STDCALL SetScale( const float fScale );
	virtual bool STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
	virtual const std::string& STDCALL GetSoundEffect() const { return szSoundName; }
	virtual void STDCALL GetSpriteEffects( const SSpriteInfo ***ppEffects, int *pnNumEffects, bool bAll );
	virtual void STDCALL GetParticleEffects( IParticleSource ***ppEffects, int *pnNumEffects, bool bAll );
	virtual void STDCALL SetStartTime( DWORD time ) { dwStartTime = time; }
	virtual void STDCALL SetEffectDirection( const SHMatrix &matrix );
	virtual bool STDCALL IsFinished( const NTimer::STime &time );
	virtual void STDCALL CalibrateDuration( const NTimer::STime &timeDuration );
	virtual void STDCALL Stop();
	virtual void STDCALL SetSuspendedState( bool bState );
	void AddSpriteEffect( ISpriteVisObj *pObj, DWORD dwStart, int nRepeat, const CVec3 &vPos );
	void AddParticleEffect( IParticleSource *pObj, DWORD dwStart, int nDuration, const CVec3 &vPos );
};
#endif // __EFFECTVISOBJ_H__
