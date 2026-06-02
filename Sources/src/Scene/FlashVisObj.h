#ifndef __FLASHVISOBJ_H__
#define __FLASHVISOBJ_H__
#pragma ONCE
class CFlashVisObj : public CTRefCount<IFlashVisObj>
{
	OBJECT_SERVICE_METHODS( CFlashVisObj );
	DECLARE_SERIALIZE;
	CPtr<IGFXTexture> pTexture;						// texture reference
	NTimer::STime timeStart;							// time, flash begins
	NTimer::STime timeDuration;						// flash duration
	SSpriteInfo spriteInfo;								// complete sprite info
	DWORD dwAlpha;												// base alpha
	void SetAlpha( DWORD alpha ) { spriteInfo.color = ( spriteInfo.color & 0x00ffffff ) | ( alpha << 24 ); }
	CFlashVisObj() { spriteInfo.color = 0xffffffff; spriteInfo.specular = 0xff000000; timeStart = timeDuration = 0; }
public:
	void SetTexture( IGFXTexture *_pTexture ) { pTexture = _pTexture; spriteInfo.pTexture = _pTexture; }
	virtual bool STDCALL Draw( interface IGFX *pGFX ) { return false; }
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor, int nType = -1 );
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false );
	virtual void STDCALL SetDirection( const int nDirection ) {  }
	virtual void STDCALL SetPosition( const CVec3 &pos ) { spriteInfo.pos = pos; }
	virtual void STDCALL SetPlacement( const CVec3 &pos, const int nDir ) { SetPosition( pos ); }
	virtual const CVec3& STDCALL GetPosition() const { return spriteInfo.pos; }
	virtual int STDCALL GetDirection() const { return 0; }
	virtual void STDCALL SetOpacity( BYTE opacity ) { SetAlpha( opacity ); }
	virtual void STDCALL SetColor( DWORD color ) { spriteInfo.color = ( spriteInfo.color & 0xff000000 ) | ( color & 0x00ffffff ); }
	virtual void STDCALL SetSpecular( DWORD color ) {  }
	virtual void STDCALL Select( EVisObjSelectionState state ) {  }
	virtual EVisObjSelectionState STDCALL GetSelectionState() const { return SGVOSS_UNSELECTED; }
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const CVec2 &point, CVec2 *pShift ) { return false; }
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const RECT &rect ) { return false; }
	virtual void STDCALL Setup( const NTimer::STime &_timeStart, const NTimer::STime &_timeDuration, const int nPower, const DWORD _dwColor )
	{
		timeStart = _timeStart;
		timeDuration = _timeDuration;
		spriteInfo.rect.Set( -nPower/2, -nPower/4, +nPower/2, +nPower/4 );
		spriteInfo.maps.Set( 0, 0, 1, 1 );
		spriteInfo.color = _dwColor;
		dwAlpha = ( _dwColor >> 24 ) & 0x000000ff;
	}
};
#endif // __FLASHVISOBJ_H__
