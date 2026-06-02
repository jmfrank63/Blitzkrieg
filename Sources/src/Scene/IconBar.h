#ifndef __ICONBAR_H__
#define __ICONBAR_H__
#pragma ONCE
class CIconBar : public ISceneIconBar
{
	OBJECT_NORMAL_METHODS( CIconBar );
	DECLARE_SERIALIZE;
	CVec2 vSize;													// absolute size
	bool bHorizontal;											// horizontal or vertical bar (for percentage treating)
	CVec3 vPos;														// position, relative to parent
	float fPercentage;										// percentage of length
	bool bEnable;													// enable icon drawing
	SSpriteInfo info;											// temporal structure - main bar
	void CalcSpriteInfo();
public:
	CIconBar();
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) { return false; }
	virtual bool STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
	virtual void STDCALL SetPosition( const CVec3 &_vPos ) { vPos = _vPos; }
	virtual void STDCALL LockBarColor() {}
	virtual void STDCALL UnlockBarColor() {}
	virtual void STDCALL SetBorderColor( DWORD dwColor ) {  }
	virtual void STDCALL ForceThinIcon() {}
	virtual void STDCALL SetColor( DWORD _color ) { info.color = (info.color & 0xff000000) | (_color & 0x00ffffff); }
	virtual void STDCALL SetAlpha( BYTE alpha ) { info.color = (info.color & 0x00ffffff) | (DWORD(alpha) << 24); }
	virtual void STDCALL SetSize( const CVec2 &_vSize, bool bHorizontal = true );
	virtual void STDCALL SetLength( float _fPercentage );
	virtual const CVec2 STDCALL GetSize() { return vSize; }
	virtual void STDCALL Reposition( const CVec3 &vParentPos ) { info.pos = vParentPos + vPos; }
	virtual void STDCALL Enable( bool _bEnable ) { bEnable = _bEnable; }
};
#endif // __ICONBAR_H__
