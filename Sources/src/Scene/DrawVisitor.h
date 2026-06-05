#ifndef __DRAWVISITOR_H__
#define __DRAWVISITOR_H__
#pragma ONCE
#include "SceneInternal.h"
#include "DepthOptimizer.h"
struct STextObject
{
	CPtr<IGFXFont> pFont;
	const char *pszText;
	CVec3 vPos;
	CVec3 vScreenPos;
	DWORD color;
};
struct SBoldLineObject
{
	CVec3 *corners;
	DWORD color;
	SBoldLineObject( CVec3 *_corners, DWORD _color ) : corners( _corners ), color( _color ) {  }
};
typedef std::list<STextObject> CTextVisList;
typedef std::list<ISceneObject*> CUnknownVisList;
typedef std::list<SBoldLineObject> CBoldLineVisList;
struct SSortByFont
{
	bool operator()( const STextObject &s1, const STextObject &s2 ) const
	{
		return s1.pFont.GetPtr() < s2.pFont.GetPtr();
	}
};
struct SUIObject
{
	enum EType
	{
		TYPE_UNKNOWN	= 0,
		TYPE_RECTS	= 1,
		TYPE_TEXT		= 2,
		TYPE_CUSTOM	= 3
	};
	const EType eType;
	union
	{
		struct 
		{
			IGFXTexture *pTexture;
			int nShadingEffect;
		};
		struct
		{
			IGFXText *pText;
			RECT rcRect;
			int nY;
			DWORD dwColor;
			DWORD dwFlags;
		};
		IUIElement *pElement;
	};
	std::vector<SGFXRect2> rects;
	SUIObject( const EType _eType ) : eType( _eType ) {  }
	bool IsRects() const { return eType == TYPE_RECTS; }
	bool IsText() const { return eType == TYPE_TEXT; }
	bool IsCustom() const { return eType == TYPE_CUSTOM; }
	EType GetType() const { return eType; }
};
class CDrawVisitor : public ISceneVisitor
{
	OBJECT_MINIMAL_METHODS( CDrawVisitor );
	ICamera *pCamera;
	SHMatrix matrix;
	CTRect<float> rcScreen;
	float fSpriteScale;
	SPlane vViewVolumePlanes[6];
	CDepthOptimizer depthoptimizer;
	void AddSingleSprite( const SBasicSpriteInfo *pObj, CSpriteVisList *pSprites, WORD wPriority );
	void AddSingleParticleEffect( IParticleSource *pPS, CParticlesVisMap *pParticles );
	void AddSingleMesh( IMeshVisObj *pMesh, CMeshVisList *pMeshes );
	void AddSingleMeshUnchecked( IMeshVisObj *pMesh, CMeshVisList *pMeshes );
public:
	CSpriteVisList sprites;
	CSpriteVisList spriteBuildings;
	CSpriteVisList spriteUnits;
	CSpriteVisList spriteEffects;
	CSpriteVisList spriteFlashes;
	CMeshVisList meshes;
	CMeshVisList aviation;
	CSpriteVisList terraObjects;
	CSpriteVisList shadowObjects;
	CParticlesVisMap particles;
	CUnknownVisList unknowns;
	CBoldLineVisList boldLines;
	std::list<SMechTrace> traces;
	std::list<SGunTrace> gunTraces;
	CSpriteVisList icons;									// bar and picture icons
	CTextVisList textes;									// text icons
	typedef std::deque<SUIObject> CUIObjectsList;
	CUIObjectsList uiObjects;							// ui objects, divided by levels
public:
	CDrawVisitor( const float fAllowedDepth ) 
		: pCamera( 0 ), fSpriteScale( 1.0f ), depthoptimizer( 100, 75, fAllowedDepth ) {  }
	void Init( ICamera *_pCamera, const SHMatrix &_matrix, const CTRect<short> &_rcScreen, const SPlane *pViewVolumePlanes );
	void Clear();
	void Sort();
	void STDCALL VisitSprite( const SBasicSpriteInfo *pObj, int nType, int nPriority );
	void STDCALL VisitMeshObject( IMeshVisObj *pObj, int nType, int nPriority );
	void STDCALL VisitParticles( IParticleSource *pObj );
	void STDCALL VisitSceneObject( ISceneObject *pObj );
	void STDCALL VisitText( const CVec3 &vPos, const char *pszText, IGFXFont *pFont, DWORD color );
	void STDCALL VisitBoldLine( CVec3 *corners, float fWidth, DWORD color );
	void STDCALL VisitMechTrace( const SMechTrace &trace );
	void STDCALL VisitGunTrace( const SGunTrace &trace );
	void STDCALL VisitUIRects( IGFXTexture *pTexture, const int nShadingEffect, SGFXRect2 *rects, const int nNumRects );
	void STDCALL VisitUIText( IGFXText *pText, const CTRect<float> &rcRect, const int nY, const DWORD dwColor, const DWORD dwFlags );
	void STDCALL VisitUICustom( interface IUIElement *pElement );
};
#endif // __DRAWVISITOR_H__
