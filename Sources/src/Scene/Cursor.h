#ifndef __CURSOR_H__
#define __CURSOR_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Input\Input.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCursorMode
{
	CPtr<IGFXTexture> pTexture;
	std::string szTextureName;
	CTRect<float> rect;
	CVec2 vHotSpot;
	CPtr<ISpriteVisObj> pVisObj;
	int wResourceID;
	//
	int operator&( IStructureSaver &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCursor : public ICursor
{
	OBJECT_COMPLETE_METHODS( CCursor );
	DECLARE_SERIALIZE;
	//
	CPtr<ITextureManager> pTM;
	//
	EUpdateMode eUpdateMode;							// update mode - from Windows and from IInput
	typedef std::unordered_map<int, SCursorMode> CCursorsModeMap;
	CCursorsModeMap modes;								// all loaded modes
	SCursorMode *pMode;										// current mode
	int nCurrMode;												// current mode index
	SCursorMode *pModifier;								// current mode modifier
	int nCurrModifier;										// current mode modifier index
	CVec2 vPos;														// current cursor position
	bool bShow;														// show or not
	bool bPosLocked;											// cursor position locked
	CTRect<float> rcBounds;								// screen bounds
	float fSensitivity;										// sensitivity
	// input sliders
	CPtr<IInputSlider> pScrollX, pScrollY;
	// last unchanged position
	CVec2 vLastPos;												// last position
	NTimer::STime timeLast;								// last time of the posistion above
	bool bAcquired;												// cursor area control acquired
	//
	bool LoadCursor( int nMode );
	SCursorMode* GetCursor( int nMode );
	void Update();
	void AcquireLocal();
public:
	CCursor();
	//
	virtual void STDCALL Init( ISingleton *pSingleton );
	virtual void STDCALL Done();
	virtual void STDCALL Clear();
	virtual void STDCALL SetUpdateMode( const EUpdateMode _eUpdateMode );
	virtual void STDCALL OnSetCursor();
	//
	virtual void STDCALL RegisterMode( int nMode, const char *pszPictureName, int nSizeX, int nSizeY, int hotX, int hotY, WORD wResourceID );
	virtual bool STDCALL SetMode( int nMode );
	virtual bool STDCALL SetModifier( int nMode );
	virtual void STDCALL Show( bool _bShow );
	virtual bool STDCALL IsShown() const { return bShow; }

	virtual void STDCALL SetBounds( int x1, int y1, int x2, int y2 );
	virtual void STDCALL Acquire( bool bAcqire );
	virtual void STDCALL LockPos( bool bLock );
	virtual void STDCALL SetPos( int nX, int nY );
	virtual const CVec2 STDCALL GetPos() { Update(); return vPos; }
	virtual void STDCALL ResetSliders() { pScrollX->Reset(); pScrollY->Reset(); }
	// last unchanged position
	virtual void STDCALL GetLastPos( CVec2 *pvPos, NTimer::STime *pTime ) const { *pvPos = vLastPos; *pTime = timeLast; }
	// update object
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) { return false; }
	//
	virtual bool STDCALL Draw( IGFX *pGFX );
	// visiting
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
	//
	virtual void STDCALL SetSensitivity( float _fSensitivity ) { fSensitivity = _fSensitivity; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __CURSOR_H__
