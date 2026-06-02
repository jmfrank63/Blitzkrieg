
#if !defined(AFX_BACKGROUND_H__E331DA1D_E450_4271_9D2D_E39295F8A0AF__INCLUDED_)
#define AFX_BACKGROUND_H__E331DA1D_E450_4271_9D2D_E39295F8A0AF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "IUIInternal.h"
#include "..\GFX\GFX.h"

#include "DeepCPtrCopy.h"
#include "..\Misc\Geometry.h"
class CBackground : public IBackground
{
	DECLARE_SERIALIZE
protected:
	DWORD color;
	DWORD specular;
	CTRect<float> pos;
	CPtr<IGFXTexture> pTexture;
public:
	virtual void STDCALL SetPos( const CVec2 &vPos, const CVec2 &vSize );
	virtual int STDCALL operator&( interface IDataTree &ss );
};
class CBackgroundPlainTexture : public CBackground
{
	DECLARE_SERIALIZE
	OBJECT_COMPLETE_METHODS(CBackgroundPlainTexture);
	DECLARE_CLONABLE_CLASS;
	
public:
	CBackgroundPlainTexture();
	
	virtual void STDCALL Visit( interface ISceneVisitor * pVisitor );
	virtual int STDCALL operator&( interface IDataTree &ss );
};
class CBackgroundTiledTexture : public CBackground
{
	DECLARE_SERIALIZE
	OBJECT_COMPLETE_METHODS(CBackgroundTiledTexture)
	DECLARE_CLONABLE_CLASS

	struct SSubRect
	{
		CTPoint<float> vSize;									// size on screen
		CTRect<float> maps;										// texture coords mapping
		CTRect<float> rect;										// rect in screen coordinates
		int nRotate;													// rotation in 90s degrees. for example to transform T->B it is 2.
		void SetRotate( const int _nRotate ) { nRotate = _nRotate; }
		int operator&( interface IDataTree &ss );
	};

	std::vector<SGFXRect2> drawRects;				// contains actually drawed data.
	SSubRect rLT/*base*/, rRT, rLB, rRB;						// corner element
	SSubRect rT/*base*/, rL, rB, rR;								// borded element
	SSubRect rF;														// inner element

	std::vector<SGFXRect2> rects;						// drawind rects
	void InitTiles( SSubRect *pSub );
	void InitBorderAndFill();

	void DivideSubrects( const SSubRect &in, std::vector<SGFXRect2> *pArr );

public:
	CBackgroundTiledTexture();
	virtual void STDCALL Visit( interface ISceneVisitor * pVisitor );
	virtual int STDCALL operator&( interface IDataTree &ss );
	virtual void STDCALL SetPos( const CVec2 &vPos, const CVec2 &vSize );
};
#endif // !defined(AFX_BACKGROUND_H__E331DA1D_E450_4271_9D2D_E39295F8A0AF__INCLUDED_)
