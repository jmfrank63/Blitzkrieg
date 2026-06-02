#ifndef __ICONTEXT_H__
#define __ICONTEXT_H__
#pragma ONCE
class CIconText : public ISceneIconText
{
	OBJECT_NORMAL_METHODS( CIconText );
	DECLARE_SERIALIZE;
	CPtr<IGFXFont> pFont;									// font to draw text
	std::string szText;										// text to draw
	CVec3 vPos;														// relative position
	CVec3 vAbsPos;												// absolute world position
	DWORD color;													// modulation color
	bool bEnable;													// enable to draw this icon?
public:
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) { return false; }
	virtual bool STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
	virtual void STDCALL SetPosition( const CVec3 &_vPos ) { vPos = _vPos; }
	virtual void STDCALL Reposition( const CVec3 &vWorldPos ) { vAbsPos = vWorldPos + vPos; }
	virtual const CVec2 STDCALL GetSize() { return pFont == 0 ? VNULL2 : CVec2(pFont->GetTextWidth(szText.c_str()), pFont->GetHeight()); }
	virtual void STDCALL SetColor( DWORD _color ) { color = (color & 0xff000000) | (_color & 0x00ffffff); }
	virtual void STDCALL SetAlpha( BYTE alpha ) { color = (color & 0x00ffffff) | (DWORD(alpha) << 24); }
	virtual void STDCALL SetFont( IGFXFont *_pFont ) { pFont = _pFont; }
	virtual void STDCALL SetText( const char *pszText ) { szText = pszText; }

	virtual void STDCALL Enable( bool _bEnable ) { bEnable = _bEnable; }
};
#endif // __ICONTEXT_H__
