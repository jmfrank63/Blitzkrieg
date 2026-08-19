#include "StdAfx.h"

#include "DrawVisitor.h"

#include "../Main/TextSystem.h"
#include "SceneScreenScale.h"
static void ScaleRect( CTRect<float> *pRect, const float fScale )
{
	if ( fScale <= 1.001f )
		return;

	pRect->Set( pRect->x1 * fScale, pRect->y1 * fScale, pRect->x2 * fScale, pRect->y2 * fScale );
}
void CDrawVisitor::Init( ICamera *_pCamera, const SHMatrix &_matrix, const CTRect<short> &_rcScreen, const SPlane *pViewVolumePlanes )
{
	pCamera = _pCamera;
	matrix = _matrix;
	rcScreen.Set( _rcScreen.x1, _rcScreen.y1, _rcScreen.x2, _rcScreen.y2 );
	fSpriteScale = NSceneScreenScale::GetGameplayScale( rcScreen );
	memcpy( &vViewVolumePlanes, pViewVolumePlanes, sizeof(vViewVolumePlanes) );
	depthoptimizer.SetScreenSize( _rcScreen.Width(), _rcScreen.Height() );
}
void CDrawVisitor::Clear()
{
	sprites.clear();
	spriteBuildings.clear();
	spriteUnits.clear();
	spriteEffects.clear();
	spriteFlashes.clear();
	meshes.clear();
	aviation.clear();
	terraObjects.clear();
	shadowObjects.clear();
	// Empty every per-texture bucket but keep the map itself: the buckets are
	// vectors and clearing them keeps the storage they grew to, so a texture
	// that stays visible never reallocates. HasParticles() is what tells an
	// all-empty frame from a populated one now.
	for ( CParticlesVisMap::iterator it = particles.begin(); it != particles.end(); ++it )
		it->second.clear();
	unknowns.clear();
	icons.clear();
	textes.clear();
	boldLines.clear();
	traces.clear();
	gunTraces.clear();
	depthoptimizer.Clear();
	uiObjects.clear();
}
void CDrawVisitor::VisitSprite( const SBasicSpriteInfo *pObj, int nType, int nPriority )
{
	switch ( nType ) 
	{
		case SGVOGT_UNIT:
			AddSingleSprite( pObj, &spriteUnits, nPriority );
			break;
		case SGVOGT_BUILDING:
		case SGVOGT_FENCE:
			AddSingleSprite( pObj, &spriteBuildings, nPriority );
			break;
		case SGVOGT_SHADOW:
			AddSingleSprite( pObj, &shadowObjects, nPriority );
			break;
		case SGVOGT_EFFECT:
			AddSingleSprite( pObj, &spriteEffects, nPriority );
			break;
		case SGVOGT_TERRAOBJ:
		case SGVOGT_BRIDGE:
		case SGVOGT_FORTIFICATION:
			AddSingleSprite( pObj, &terraObjects, nPriority );
			break;
		case SGVOGT_ICON:
			AddSingleSprite( pObj, &icons, nPriority );
			break;
		case SGVOGT_FLASH:
			AddSingleSprite( pObj, &spriteFlashes, nPriority );
			break;
		default:
			AddSingleSprite( pObj, &sprites, nPriority );
	}
}
void CDrawVisitor::VisitMeshObject( IMeshVisObj *pObj, int nType, int nPriority )
{
	switch ( nType )
	{
		case SGVOGT_UNIT:
		case SGVOGT_ENTRENCHMENT:
		case SGVOGT_TANK_PIT:
			AddSingleMesh( pObj, &meshes );
			break;
		case SGVOGT_TERRAOBJ:
			break;
		case -1:
			AddSingleMeshUnchecked( pObj, &aviation );
			break;
	}
}
void CDrawVisitor::VisitParticles( IParticleSource *pObj )
{
	AddSingleParticleEffect( pObj, &particles );
}
void CDrawVisitor::VisitSceneObject( ISceneObject *pObj )
{
	unknowns.push_back( pObj );
}
void CDrawVisitor::AddSingleSprite( const SBasicSpriteInfo *pObj, CSpriteVisList *pSprites, WORD wPriority )
{
	SBasicSpriteInfo *pInfo = const_cast<SBasicSpriteInfo*>( pObj );
	pInfo->dwCheckFlags = ( pInfo->dwCheckFlags & 0x0000ffff ) | (DWORD(wPriority) << 16);
	matrix.RotateHVector( &pInfo->relpos, pInfo->pos );
	pInfo->relpos.x = MINT( pInfo->relpos.x );
	pInfo->relpos.y = MINT( pInfo->relpos.y );
	CTRect<float> rcRect;
	switch ( pInfo->type ) 
	{
		case SBasicSpriteInfo::TYPE_NORMAL_SPRITE:
			rcRect.Set( static_cast<SSpriteInfo*>(pInfo)->rect.x1, static_cast<SSpriteInfo*>(pInfo)->rect.y1,
				          static_cast<SSpriteInfo*>(pInfo)->rect.x2, static_cast<SSpriteInfo*>(pInfo)->rect.y2 );
			break;
		case SBasicSpriteInfo::TYPE_COMPLEX_SPRITE:
			rcRect = static_cast<SComplexSpriteInfo*>(pInfo)->pSprite->GetBoundBox();
			break;
	}
	ScaleRect( &rcRect, fSpriteScale );
	rcRect.Move( pInfo->relpos.x, pInfo->relpos.y );
	if ( rcScreen.IsInside(rcRect) ) 
	{
		pInfo->dwCheckFlags = pInfo->dwCheckFlags & 0xffff0000;
		pSprites->push_back( pInfo );
	}
	else if ( rcRect.IsIntersect(rcScreen) )
	{
		pInfo->dwCheckFlags |= 1;
		pSprites->push_back( pInfo );
	}
}
void CDrawVisitor::AddSingleParticleEffect( IParticleSource *pPS, CParticlesVisMap *pParticles )
{
	const int nNumParticles = pPS->GetNumParticles();
	if ( nNumParticles != 0 )
	{
		CParticlesVisList &particles = (*pParticles)[ pPS->GetTexture() ];
		static std::vector<SSimpleParticle> buffer;
		buffer.resize( nNumParticles );
		pPS->FillParticleBuffer( &(buffer[0]) );
		for ( int i=0; i<nNumParticles; ++i )
		{
			SParticleInfo info;
			info.color = buffer[i].color;
			info.vPos = buffer[i].vPosition;
			info.fSize = buffer[i].fSize / 2.0f;
			info.maps = buffer[i].rcMaps;
			info.specular =  0xFF000000;
			info.fAngle = buffer[i].fAngle;
			matrix.RotateHVector( &info.vPos, info.vPos );
			info.vPos.x = MINT( info.vPos.x );
			info.vPos.y = MINT( info.vPos.y );
			const float fSize = info.fSize * fSpriteScale * FP_SQRT_2;
			CTRect<float> rcRect( info.vPos.x - fSize, info.vPos.y - fSize, 
				                    info.vPos.x + fSize, info.vPos.y + fSize );
			if ( rcRect.IsIntersect(rcScreen) )
			{
				if ( depthoptimizer.CheckAndAdd(rcRect.x1, rcRect.y1, rcRect.x2, rcRect.y2) )
					particles.push_back( info );
			}
		}
	}
}
void CDrawVisitor::AddSingleMeshUnchecked( IMeshVisObj *pMesh, CMeshVisList *pMeshes )
{
	pMeshes->push_back( pMesh );
}
void CDrawVisitor::AddSingleMesh( IMeshVisObj *pMesh, CMeshVisList *pMeshes )
{
	const DWORD dwClipFlags = pMesh->CheckForViewVolume( &(vViewVolumePlanes[0]) );
	if ( dwClipFlags != GFXCP_OUT )
		pMeshes->push_back( pMesh );
}
void CDrawVisitor::VisitText( const CVec3 &vPos, const char *pszText, IGFXFont *pFont, DWORD color )
{
	textes.push_back( STextObject() );
	STextObject &text = textes.back();
	text.pszText = pszText;
	text.pFont = pFont;
	text.vPos = vPos;
	text.color = color;
	matrix.RotateHVector( &text.vScreenPos, vPos );
}
void CDrawVisitor::VisitBoldLine( CVec3 *corners, float fWidth, DWORD color )
{
	boldLines.push_back( SBoldLineObject(corners, color) );
}
void CDrawVisitor::VisitMechTrace( const SMechTrace &trace )
{
	traces.push_back( trace );
}
void CDrawVisitor::VisitGunTrace( const SGunTrace &trace )
{
	gunTraces.push_back( trace );
}
void CDrawVisitor::VisitUIRects( IGFXTexture *pTexture, const int nShadingEffect, SGFXRect2 *pRects, const int nNumRects )
{
	if ( uiObjects.empty() || !uiObjects.back().IsRects() || 
		   (uiObjects.back().pTexture != pTexture) || 
			 (uiObjects.back().nShadingEffect != nShadingEffect) )
	{
		uiObjects.push_back( SUIObject(SUIObject::TYPE_RECTS) );
		uiObjects.back().pTexture = pTexture;
		uiObjects.back().nShadingEffect = nShadingEffect;
	}
	{
		float fTexDiffX = 0, fTexDiffY = 0, fScrDiff = -0.5f;
		if ( pTexture ) 
		{
			fTexDiffX = -0.5f / float ( pTexture->GetSizeX(0) );
			fTexDiffY = -0.5f / float ( pTexture->GetSizeY(0) );
		}
		SUIObject &obj = uiObjects.back();
		obj.rects.reserve( obj.rects.size() + nNumRects );
		for ( int i = 0; i < nNumRects; ++i )
		{
			if ( rcScreen.IsIntersect(pRects[i].rect) && !pRects[i].rect.IsEmpty() )
			{
				obj.rects.push_back( pRects[i] );
				obj.rects.back().rect.Move( fScrDiff, fScrDiff );
				obj.rects.back().maps.Move( fTexDiffX, fTexDiffY );
			}
		}
	}
	if ( uiObjects.back().rects.empty() ) 
		uiObjects.pop_back();
}
void CDrawVisitor::VisitUIText( IGFXText *pText, const CTRect<float> &rcRect, const int nY, const DWORD dwColor, const DWORD dwFlags )
{
	if ( (pText == 0) || (pText->GetText()->GetLength() == 0) || !rcScreen.IsIntersect(rcRect) ) 
		return;
	uiObjects.push_back( SUIObject(SUIObject::TYPE_TEXT) );
	SUIObject &obj = uiObjects.back();
	obj.pText = pText;
	obj.rcRect = rcRect;
	obj.nY = nY;
	obj.dwColor = dwColor;
	obj.dwFlags = dwFlags;
}
void CDrawVisitor::VisitUICustom( interface IUIElement *pElement )
{
	if ( pElement == 0 ) 
		return;
	uiObjects.push_back( SUIObject(SUIObject::TYPE_CUSTOM) );
	uiObjects.back().pElement = pElement;
}
struct STextureCmpFunctional
{
	bool operator()( const IMeshVisObj *pMesh1, const IMeshVisObj *pMesh2 ) const
	{
		return pMesh1->GetTexture() < pMesh2->GetTexture();
	}
};
void CDrawVisitor::Sort()
{
	// stable_sort, not sort: these used to be std::lists, whose sort is stable,
	// and the draw order of same-texture meshes has to stay the submission order
	// or the overdraw between them changes.
	std::stable_sort( meshes.begin(), meshes.end(), STextureCmpFunctional() );
	std::stable_sort( aviation.begin(), aviation.end(), STextureCmpFunctional() );
}
bool CDrawVisitor::HasParticles() const
{
	for ( CParticlesVisMap::const_iterator it = particles.begin(); it != particles.end(); ++it )
	{
		if ( !it->second.empty() )
			return true;
	}
	return false;
}
