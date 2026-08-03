#ifndef __ANIMVISITOR_H__
#define __ANIMVISITOR_H__
#pragma ONCE
#include "../Anim/Animation.h"
class CExtractAnimVisitor : public CTRefCount<IAnimVisitor>
{
	OBJECT_SERVICE_METHODS( CExtractAnimVisitor );
	const SSpriteRect *pSpriteRect;
	const SSpritesPack::SSprite *pComplexSprite;
public:
	CExtractAnimVisitor() { Clear(); }
	void Clear() { pSpriteRect = 0; pComplexSprite = 0; }
	const SSpriteRect* GetSpriteRect() const { return pSpriteRect; }
	const SSpritesPack::SSprite* GetComplexSprite() const { return pComplexSprite; }
	virtual void STDCALL VisitSprite( const SSpriteRect *pSprite ) { pSpriteRect = pSprite; pComplexSprite = 0; }
	virtual void STDCALL VisitSprite( const SSpritesPack::SSprite *pSprite ) { pComplexSprite = pSprite; pSpriteRect = 0; }
	virtual void STDCALL VisitMesh( const SHMatrix *matrices, int nNumMatrices ) {  }
	virtual void STDCALL VisitUIRects( IGFXTexture *pTexture, const int nShadingEffect, SGFXRect2 *rects, const int nNumRects ) {  }
	virtual void STDCALL VisitUIText( IGFXText *pText, const CTRect<float> &rcRect, const int nY, const DWORD dwColor, const DWORD dwFlags ) {  }
	virtual void STDCALL VisitUICustom( interface IUIElement *pElement ) {  }
};
#endif // __ANIMVISITOR_H__
