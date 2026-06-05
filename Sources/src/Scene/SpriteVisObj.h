#ifndef __SPRITEVISOBJ_H__
#define __SPRITEVISOBJ_H__
#pragma ONCE
#include "ObjVisObj.h"
inline DWORD SetOpacity( DWORD original, BYTE opacity ) { return ( DWORD( opacity ) << 24 )  | (original & 0x00ffffff); }
inline DWORD SetColor( DWORD original, DWORD color ) { return ( color & 0x00ffffff ) | ( original & 0xff000000 ); }
class CSpriteVisObj : public CTObjVisObj< CTRefCount<ISpriteVisObj> >
{
	OBJECT_SERVICE_METHODS( CSpriteVisObj );
	DECLARE_SERIALIZE;
	CPtr<ISpriteAnimation> pAnim;					// animation system
	NTimer::STime nNextIdle;              // next idle time
	static DWORD dwIdleData;              // idle animation data
	CPtr<IGFXTexture> pTexture;						// texture
	SSpriteInfo info;											// temp sprite info
	SComplexSpriteInfo info2;							// temp complex sprite info
	bool bComplexSprite;
	virtual void RepositionIcons();
	void RepositionIconsLocal( DWORD placement );
	void RetrieveSpriteInfo();
	virtual ~CSpriteVisObj() {  }
public:
	CSpriteVisObj();
	bool Init( IGFXTexture *_pTexture, ISpriteAnimation *_pAnimation );
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false );
	virtual void STDCALL SetScale( float sx, float sy, float sz );
	virtual void STDCALL SetDirection( const int _nDirection )
	{
		SetDir( _nDirection );
		pAnim->SetDirection( GetDir() );
	}
	virtual void STDCALL SetPosition( const CVec3 &_pos ) { SetPos( _pos ); info.pos = _pos; }
	virtual void STDCALL SetPlacement( const CVec3 &pos, const int nDir ) { SetPosition( pos ); SetDirection( nDir ); }
	virtual void STDCALL SetOpacity( BYTE opacity ) 
	{ 
		info.color = ::SetOpacity( info.color, opacity );
		info2.color = ::SetOpacity( info2.color, opacity );
	}
	virtual void STDCALL SetColor( DWORD color ) 
	{ 
		info.color = ::SetColor( info.color, color );
		info2.color = ::SetColor( info2.color, color );
	}
	virtual void STDCALL SetSpecular( DWORD color ) 
	{ 
		info.specular = ::SetColor( info.specular, color ); 
		info2.specular = ::SetColor( info2.specular, color ); 
	}
	virtual void STDCALL SetAnimation( const int nAnim ) 
	{ 
		pAnim->SetAnimation( nAnim ); 
	}
	virtual IAnimation* STDCALL GetAnimation() { return pAnim; }
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const CVec2 &point, CVec2 *pShift );
	virtual bool STDCALL IsHit( const SHMatrix &matTransform, const RECT &rect );
	virtual bool STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
	virtual const SSpriteInfo* STDCALL GetSpriteInfo() const { return &info; }
	virtual IGFXTexture* STDCALL GetTexture() const { return pTexture; }
};
#endif // __SPRITEVISOBJ_H__
