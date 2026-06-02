#ifndef __ICONPIC_H__
#define __ICONPIC_H__
#pragma ONCE
class CIconPic : public ISceneIconPic
{
	OBJECT_NORMAL_METHODS( CIconPic );
	DECLARE_SERIALIZE;
	CVec3 vPos;														// relative position
	CPtr<IGFXTexture> pTexture;						// texture
	bool bEnable;													// enable icon drawing
	SSpriteInfo info;											// temporal structure
public:
	CIconPic();
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) { return false; }
	virtual bool STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
	virtual void STDCALL SetPosition( const CVec3 &_vPos ) { vPos = _vPos; }
	virtual void STDCALL Reposition( const CVec3 &_vPos ) { info.pos = _vPos + vPos; }
	virtual const CVec2 STDCALL GetSize() { return pTexture == 0 ? VNULL2 : CVec2(pTexture->GetSizeX(0), pTexture->GetSizeY(0)); }
	virtual void STDCALL SetColor( DWORD _color ) { info.color = (info.color & 0xff000000) | (_color & 0x00ffffff); }
	virtual void STDCALL SetAlpha( BYTE alpha ) { info.color = (info.color & 0x00ffffff) | (DWORD(alpha) << 24); }
	virtual void STDCALL Enable( bool _bEnable ) { bEnable = _bEnable; }
	virtual void STDCALL SetTexture( IGFXTexture *_pTexture ) { pTexture = _pTexture; info.pTexture = _pTexture; }
	virtual void STDCALL SetRect( const CTRect<short> &rect, const CTRect<float> &maps ) { info.rect = rect; info.maps = maps; }
};
#endif // __ICONPIC_H__
