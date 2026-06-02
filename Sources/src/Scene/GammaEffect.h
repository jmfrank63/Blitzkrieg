#ifndef __GAMMAEFFECT_H__
#define __GAMMAEFFECT_H__
#pragma ONCE
class CGammaEffect : public CTRefCount<IGammaEffect>
{
	OBJECT_SERVICE_METHODS( CGammaEffect );
	DECLARE_SERIALIZE;
	CVec3 vGamma[3];
	CVec3 vCurrGamma;
	CVec3 vOriginal;
	NTimer::STime timeStart;
	NTimer::STime timeDuration;
	CGammaEffect() : vOriginal( VNULL3 ), timeStart( 0 ), timeDuration( 0 ) {  }
	virtual ~CGammaEffect();
public:	
	bool STDCALL Update( const NTimer::STime &time, bool bForced = false );
	bool STDCALL Draw( interface IGFX *pGFX );
	void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
	void STDCALL Init( const float fGammaR, const float fGammaG, const float fGammaB,
 										 const NTimer::STime &timeStart, const NTimer::STime &timeDuration );
};
class CGammaFader : public CTRefCount<ISceneObject>
{
	OBJECT_SERVICE_METHODS( CGammaFader );
	DECLARE_SERIALIZE;
	CVec3 vOriginal;											// original brightness/contrast/gamma
	float fCurrValue;											// current value for brightness
	NTimer::STime timeStart;							// start time
	NTimer::STime timeDuration;						// duration (from global var)
	virtual ~CGammaFader();
public:
	CGammaFader() 
		: vOriginal( VNULL3 ), timeStart( 0 ), timeDuration( 0 ) {  }
	bool STDCALL Update( const NTimer::STime &time, bool bForced = false );
	bool STDCALL Draw( interface IGFX *pGFX );
	void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
};
#endif // __GAMMAEFFECT_H__
