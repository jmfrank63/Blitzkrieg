#ifndef __TRANSITION_H__
#define __TRANSITION_H__
#pragma ONCE
class CTransition : public CTRefCount<ITransition>
{
	OBJECT_SERVICE_METHODS( CTransition );
	DECLARE_SERIALIZE;
	NTimer::STime timeStart;							// start time
	float fAlphaStart;										// start alpha value
	float fAlphaEnd;											// end alpha value
	float fAlpha;													// current alpha value
	bool bInfinite;												// infinite updates
public:
	bool STDCALL Update( const NTimer::STime &time, bool bForced = false );
	bool STDCALL Draw( interface IGFX *pGFX );
	void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
	int STDCALL Start( const char *pszVideoName, const DWORD dwAddFlags, const NTimer::STime &currTime, const bool bFadeIn );
};
#endif // __TRANSITION_H__
