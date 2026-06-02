#ifndef __ICONHPBAR_H__
#define __ICONHPBAR_H__
#pragma ONCE
class CIconHPBar : public CTRefCount<ISceneIconBar>
{
	OBJECT_SERVICE_METHODS( CIconHPBar );
	DECLARE_SERIALIZE;
	CPtr<IGFXTexture> pTexture;						// picture
	CVec3 vPos;														// position, relative to parent
	CVec2 vSize;													// absolute size
	float fPercentage;										// percentage of length
	bool bEnable;													// enable icon drawing
	SSpriteInfo infoMain, infoBar;      	// temporal structure - main & bar
	bool bMultiplayer;
	bool bBarLocked;
	DWORD dwBarColor;
	void CalcSpriteInfo();
public:	
	CIconHPBar();
	void Init( IGFXTexture *_pTexture );
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) { return false; }
	virtual bool STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
	virtual void STDCALL SetPosition( const CVec3 &_vPos ) { vPos = _vPos; }
	virtual void STDCALL LockBarColor();
	virtual void STDCALL UnlockBarColor();
	virtual void STDCALL SetBorderColor( DWORD dwColor );
	virtual void STDCALL ForceThinIcon();
	virtual void STDCALL SetColor( DWORD _color );
	virtual void STDCALL SetAlpha( BYTE alpha );
	virtual void STDCALL SetSize( const CVec2 &_vSize, bool bHorizontal = true );
	virtual void STDCALL SetLength( float _fPercentage );
	virtual const CVec2 STDCALL GetSize() { return vSize; }
	virtual void STDCALL Reposition( const CVec3 &vParentPos );
	virtual void STDCALL Enable( bool _bEnable ) { bEnable = _bEnable; }
};
#endif // __ICONHPBAR_H__
