#include "StdAfx.h"

#include "SceneInternal.h"

#include <float.h>

#include "MeshVisObj.h"
#include "FrameSelection.h"
#include "..\GFX\GFXHelper.h"
#include "DrawVisitor.h"
#include "FastSinCos.h"
#include "..\Formats\fmtMap.h"
#include "..\Misc\Win32Random.h"
#include "SoundScene.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MESH_SHADOW_DENSITY 0.5f
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
class CRawBuffer
{
	TYPE *pData;													// data pointer
	int nSize;														// data size <= capacity
	int nCapacity;												// total buffer capacity
public:
	CRawBuffer() : pData( 0 ), nSize( 0 ), nCapacity( 0 ) {  }
	~CRawBuffer() { if ( pData ) ::operator delete(pData); }
	//
	int size() const { return nSize; }
	int capacity() const { return nCapacity; }
	bool empty() const { return nSize == 0; }
	//
	void reserve( const int _nCapacity )
	{
		if ( nCapacity < _nCapacity ) 
		{
			TYPE *pNewData = reinterpret_cast<TYPE*>(::operator new(_nCapacity * sizeof(TYPE)));
			if ( nSize > 0 ) 
				memcpy( pNewData, pData, nSize * sizeof(TYPE) );
			if ( pData ) 
				::operator delete(pData);
			pData = pNewData;
			nCapacity = _nCapacity;
		}
	}
	void resize( const int _nSize )
	{
		reserve( _nSize );
		nSize = _nSize;
	}
	//
	TYPE& operator[]( const int nIndex )
	{
		NI_ASSERT_SLOW_T( nIndex >= 0 && nIndex < nSize, NStr::Format("Can't access %d element in RAW buffer of range [0..%d]", nIndex, nSize) );
		return pData[nIndex];
	}
	const TYPE& operator[]( const int nIndex ) const
	{
		NI_ASSERT_SLOW_T( nIndex >= 0 && nIndex < nSize, NStr::Format("Can't access %d element in RAW buffer of range [0..%d]", nIndex, nSize) );
		return pData[nIndex];
	}
	//
	const TYPE* begin() const { return pData; }
	const TYPE* end() const { return pData + nSize; }
	TYPE* begin() { return pData; }
	TYPE* end() { return pData + nSize; }
	//
	typedef TYPE value_type;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
__forceinline bool DrawTemp( IGFX *pGFX, const CRawBuffer<TYPE> &vertices, const CRawBuffer<WORD> &indices, 
										         EGFXPrimitiveType eGFXPT = GFXPT_TRIANGLELIST )
{
	if ( vertices.empty() || indices.empty() ) 
		return false;
	NI_ASSERT_SLOW_TF( vertices.size() < 65536, NStr::Format("Can't draw more then 65536 vertices, but %d sent to render", vertices.size()), return false );
	CTempBufferLock<TYPE> verts = pGFX->GetTempVertices( vertices.size(), TYPE::format, eGFXPT );
	memcpy( verts.GetBuffer(), &(vertices[0]), vertices.size() * sizeof(TYPE) );
	CTempBufferLock<WORD> inds = pGFX->GetTempIndices( indices.size(), GFXIF_INDEX16, eGFXPT );
	memcpy( inds.GetBuffer(), &(indices[0]), indices.size() * sizeof(WORD) );
	//
	return pGFX->DrawTemp();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TContainer>
__forceinline typename TContainer::value_type* Resize2Fit( TContainer &data, const int nAdd )
{
	const int nOldSize = data.size();
	data.resize( nOldSize + nAdd );
	return &( data[nOldSize] );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float fCircleWidth = 10.0f;
const int nNumSegments = 64;
const float fAngleStep = FP_2PI / nNumSegments;

void DrawCircle( IGFX *pGFX, const CVec3 &vCenter3D, const float fR, const DWORD dwColor )
{
	CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( (nNumSegments + 1) * 2, SGFXLineVertex::format, GFXPT_TRIANGLESTRIP );
	
	float fCurrAngle = 0.0f;
	for ( int i = 0; i < nNumSegments; ++i )
	{
		vertices[2*i].Setup( vCenter3D.x + (fR - fCircleWidth) * Cos(fCurrAngle), 
												 vCenter3D.y + (fR - fCircleWidth) * Sin(fCurrAngle), 
												 vCenter3D.z, dwColor );
		vertices[2*i + 1].Setup( vCenter3D.x + fR * Cos(fCurrAngle), vCenter3D.y + fR * Sin(fCurrAngle), 
														 vCenter3D.z, dwColor );
		fCurrAngle += fAngleStep;
	}
	vertices[2*nNumSegments].Setup( vCenter3D.x + (fR - fCircleWidth), vCenter3D.y, vCenter3D.z, dwColor );
	vertices[2*nNumSegments + 1].Setup( vCenter3D.x + fR, vCenter3D.y, vCenter3D.z, dwColor );

	pGFX->DrawTemp();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DrawArea( IGFX *pGFX, const SShootArea &area, bool bFill )
{
	const float fCircleWidth = 10.0f;
	if ( area.fMaxR < 3.0f * fCircleWidth )
		return;

	const float fMinR = area.fMinR < fCircleWidth ? 0.0f : area.fMinR;
	const DWORD dwColor = area.GetColor();

	DrawCircle( pGFX, area.vCenter3D, area.fMaxR, dwColor );
	if ( fMinR > 0.0f )
		DrawCircle( pGFX, area.vCenter3D, area.fMinR, dwColor );

	if ( area.wStartAngle != area.wFinishAngle && bFill )
	{
		float fStartAngle = float( area.wStartAngle ) / 65535.0f * FP_2PI + PI * 0.5;
		float fEndAngle = float( area.wFinishAngle ) / 65535.0f * FP_2PI + PI * 0.5;
		if ( fEndAngle < fStartAngle )
			fEndAngle += FP_2PI;

		float fCurrAngle = fStartAngle;
		float fNextAngle = fCurrAngle - fmod( fCurrAngle, fAngleStep ) + fAngleStep;
		std::list<float> rays;
		rays.push_back( fmod( fCurrAngle, FP_2PI - 0.001 ) );
		while ( fNextAngle < fEndAngle )
		{
			fCurrAngle = fNextAngle;
			rays.push_back( fmod( fCurrAngle, FP_2PI - 0.001 ) );
			fNextAngle += fAngleStep;
		}
		rays.push_back( fmod( fEndAngle, FP_2PI - 0.001 ) );
		CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( rays.size() * 2, SGFXLineVertex::format, GFXPT_TRIANGLESTRIP );
		int idx = 0;
		for ( std::list<float>::const_iterator it = rays.begin(); it != rays.end(); ++it, ++idx )
		{
			vertices[2*idx].Setup( area.vCenter3D.x + fMinR * Cos(*it), 
													   area.vCenter3D.y + area.fMinR * Sin(*it), 
													   area.vCenter3D.z, dwColor);
			vertices[2*idx + 1].Setup( area.vCenter3D.x + (area.fMaxR - fCircleWidth) * Cos(*it), 
															   area.vCenter3D.y + (area.fMaxR - fCircleWidth) * Sin(*it), 
															   area.vCenter3D.z, dwColor );			
		}
		pGFX->DrawTemp();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DrawMeshes( CMeshVisList &meshes, IGFX *pGFX, bool bEnableBBs, const SGFXLightDirectional &sunlight )
{
	if ( meshes.empty() ) 
		return;
	//
	pGFX->EnableLighting( true );
	pGFX->SetLight( 0, sunlight );
	pGFX->EnableLight( 0, true );
	pGFX->SetShadingEffect( 8 );
	for ( CMeshVisList::iterator it = meshes.begin(); it != meshes.end(); ++it )
		(*it)->Draw( pGFX );
	pGFX->EnableLighting( false );
	// draw bounding boxes
	if ( bEnableBBs )
	{
		pGFX->SetTexture( 0, 0 );
		for ( CMeshVisList::iterator it = meshes.begin(); it != meshes.end(); ++it )
			(*it)->DrawBB( pGFX );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DrawMeshesChecked( CMeshVisList &meshes, IGFX *pGFX, bool bEnableBBs, const SGFXLightDirectional &sunlight, const SPlane *pvViewVolumePlanes )
{
	if ( meshes.empty() ) 
		return;
	//
	pGFX->EnableLighting( true );
	pGFX->SetLight( 0, sunlight );
	pGFX->EnableLight( 0, true );
	pGFX->SetShadingEffect( 8 );
	for ( CMeshVisList::iterator it = meshes.begin(); it != meshes.end(); ++it )
	{
		const DWORD dwClipFlags = (*it)->CheckForViewVolume( pvViewVolumePlanes );
		if ( dwClipFlags != GFXCP_OUT )
			(*it)->Draw( pGFX );
	}
	pGFX->EnableLighting( false );
	// draw bounding boxes
	if ( bEnableBBs )
	{
		pGFX->SetTexture( 0, 0 );
		for ( CMeshVisList::iterator it = meshes.begin(); it != meshes.end(); ++it )
		{
			const DWORD dwClipFlags = (*it)->CheckForViewVolume( pvViewVolumePlanes );
			if ( dwClipFlags != GFXCP_OUT )
				(*it)->DrawBB( pGFX );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CObj<CDrawVisitor> pDrawVisitor;
void CScene::Draw( ICamera *pCamera )
{
	_control87( _RC_NEAR, _MCW_RC );

	CTRect<short> rcScreenRect = pGFX->GetScreenRect();
	//DWORD time = GetTickCount();
	pCamera->Update();
	pGFX->SetViewTransform( pCamera->GetPlacement() );
	// form matrix to manually transform sprites to the screen
	UpdateTransformMatrix();
	// determine z-bias to keep z-buffer happy (treat sprites as vertical)
	CVec3 vecZ1, vecZ2;
	matTransform.RotateHVector( &vecZ1, CVec3(0, 0, 0) );
	matTransform.RotateHVector( &vecZ2, CVec3(0, 0, 2.0f/FP_SQRT_3) );
	fZBias = vecZ2.z - vecZ1.z;
	matTransform.RotateHVector( &vecZ2, CVec3(-FP_SQRT_2, FP_SQRT_2, 0) );
	fZBias2 = vecZ2.z - vecZ1.z;
	// setup depth complexity rendering
	if ( bEnableDepthComplexity )
		pGFX->SetShadingEffect( 300 );
	// draw terrain
	if ( bEnableTerrain && pTerrain )
	{
		pGFX->SetupDirectTransform();
		pTerrain->Draw( pCamera );
		pGFX->RestoreTransform();
		//
		pTerrain->DrawVectorObjects();
		pTerrain->DrawMarkers();
	}
	// form visibility list
	if ( pDrawVisitor == 0 )
		pDrawVisitor = new CDrawVisitor( GetGlobalVar("particlesdepth", 20.0f) );
	pDrawVisitor->Clear();
	// retrieve clip planes
	SPlane vViewVolumePlanes[6];
	{
		pGFX->GetViewVolume( vViewVolumePlanes );
		pDrawVisitor->Init( pCamera, matTransform, rcScreenRect, &(vViewVolumePlanes[0]) );
	}
	FormVisibilityLists( pCamera, pDrawVisitor );
	pDrawVisitor->Sort();
	// base sprite objects (terra + shadows)
	if ( bEnableObjects )
	{
		pGFX->SetupDirectTransform();
		// draw terrain objects (w/o depth buffer)
		if ( !pDrawVisitor->terraObjects.empty() )
		{
			pGFX->SetDepthBufferMode( GFXDB_NONE );
			pGFX->SetShadingEffect( 3 );
			DrawTerraObjects( pDrawVisitor->terraObjects );
		}
		pGFX->RestoreTransform();
		/*
		if ( !pDrawVisitor->meshTerraObjects.empty() ) 
		{
			pGFX->SetDepthBufferMode( GFXDB_NONE );
			DrawMeshes( pDrawVisitor->meshTerraObjects, pGFX, false, sunlight );
			pGFX->SetDepthBufferMode( GFXDB_USE_Z );
		}
		*/
		//
		// new draw areas
		{
			// NTimer::STime time = pTimer->GetGameTime();
			// pGFX->SetWorldTransforms( 0, &MONE, 1 );
		  pGFX->SetShadingEffect( 9 );
			pGFX->SetTexture( 0, 0 );
			pGFX->SetDepthBufferMode( GFXDB_NONE );
			pGFX->SetCullMode( GFXC_NONE );
			// draw areas
			for ( CShootAreasList::iterator it = areas.begin(); it != areas.end(); ++it )
			{
				bool bIsBallistic = false;
				for ( std::list<SShootArea>::iterator innerIter = it->areas.begin(); innerIter != it->areas.end(); ++innerIter )
				{
					if ( innerIter->eType == SShootArea::ESAT_BALLISTIC )
						bIsBallistic = true;
				}

				for ( std::list<SShootArea>::iterator innerIter = it->areas.begin(); innerIter != it->areas.end(); ++innerIter )
					DrawArea( pGFX, *innerIter, innerIter->eType != SShootArea::ESAT_LINE || !bIsBallistic );
			}
			//
			pGFX->SetDepthBufferMode( GFXDB_USE_Z );
			pGFX->SetCullMode( GFXC_CW );
		}
		///
		//drawing directional arrow and markers
		if ( bDrawArrow || !clickMarkers.empty() || !posMarkers.empty() )
		{
			pGFX->SetDepthBufferMode( GFXDB_NONE );
			pGFX->SetShadingEffect( 3 );
			pGFX->SetWorldTransforms( 0, &MONE, 1 );
			pGFX->SetTexture( 0, 0 );
			DrawMarkers();
			pGFX->SetDepthBufferMode( GFXDB_USE_Z );			
		}
		//drawing black stripes on border
		if ( pTerrain != 0 && pCamera != 0 && bEnableShowBorder )
		{
			pGFX->SetTexture( 0, 0 );
			pGFX->SetShadingEffect( 2 );
			pTerrain->DrawBorder( 0x00000000, 32, false );
		}
		// draw mech traces
		if ( !pDrawVisitor->traces.empty() )
		{
			pGFX->SetDepthBufferMode( GFXDB_NONE );
			pGFX->SetShadingEffect( 20 );
			pGFX->SetWorldTransforms( 0, &MONE, 1 );
			pGFX->SetTexture( 0, 0 );
			DrawMechTraces( pDrawVisitor->traces );
			pGFX->SetDepthBufferMode( GFXDB_USE_Z );
		}
		// draw shadow objects (/w special depth buffer)
		if ( bEnableShadows )
		{
			pGFX->SetShadingEffect( 110 );
			// для рендеринга теней для отсечения пересечений ставим режим z-buffer'а в '!=', т.е. чтобы рисовать с незаполненой области только
			//pGFX->SetDepthBufferMode( GFXDB_USE_Z, GFXCMP_NOTEQUAL );
			if ( !pDrawVisitor->shadowObjects.empty() )
			{
				pGFX->SetupDirectTransform();
				pGFX->SetShadingEffect( 111 );
				DrawTerraObjects( pDrawVisitor->shadowObjects );
				pGFX->RestoreTransform();
			}
			//pGFX->SetDepthBufferMode( GFXDB_NONE );
			// draw shadows from 3D mesh objects
			if ( !pDrawVisitor->meshes.empty() )
			{
				pGFX->SetShadingEffect( 112 );
				// compose shadow matrix
				SHMatrix matShadow;
				Identity( &matShadow );
				matShadow._13 = -sunlight.vDir.x / sunlight.vDir.z;
				matShadow._23 = -sunlight.vDir.y / sunlight.vDir.z;
				matShadow._33 = 0;
				// enable lighting
				pGFX->EnableSpecular( false );
				pGFX->EnableLighting( true );
				pGFX->EnableLight( 0, false );
				//pGFX->SetShadingEffect( 8 );
				// set material for shadow rendering
				SGFXMaterial material;
				Zero( material );
				material.vDiffuse.a = MESH_SHADOW_DENSITY;
				pGFX->SetMaterial( material );
				pGFX->SetTexture( 0, 0 );
				// main meshes
				for ( CMeshVisList::iterator it = pDrawVisitor->meshes.begin(); it != pDrawVisitor->meshes.end(); ++it )
				{
					CVisObjDescMap::const_iterator pos = objdescs.find( *it );
					if ( (pos->second.gametype != SGVOGT_ENTRENCHMENT) && (pos->second.gametype != SGVOGT_TANK_PIT) )
						(*it)->DrawShadow( pGFX, 0, sunlight.vDir );
				}
				pGFX->EnableLighting( false );
			}
			pGFX->SetShadingEffect( 113 );
		}
		pGFX->SetDepthBufferMode( GFXDB_USE_Z );
	}
	// units
	if ( bEnableUnits )
	{
		// sprite
		if ( !pDrawVisitor->spriteUnits.empty() )
		{
			pGFX->SetupDirectTransform();
			pGFX->SetShadingEffect( 3 );
			DrawSprites( pDrawVisitor->spriteUnits );
			pGFX->RestoreTransform();
		}
		// mesh
		DrawMeshes( pDrawVisitor->meshes, pGFX, bEnableBBs, sunlight );
	}
	// other sprite objects
	if ( bEnableObjects )
	{
		pGFX->SetupDirectTransform();
		// draw sprite buildings
		if ( !pDrawVisitor->spriteBuildings.empty() )
		{
			pGFX->SetShadingEffect( 3 );
			DrawSprites( pDrawVisitor->spriteBuildings );
		}
		// draw main sprites
		if ( !pDrawVisitor->sprites.empty() )
		{
			pGFX->SetShadingEffect( 3 );
			DrawSprites( pDrawVisitor->sprites );
		}
		// draw icons
		// bar/picture
		if ( !pDrawVisitor->icons.empty() )
		{
			pGFX->SetDepthBufferMode( GFXDB_NONE );
			// disable z-write and set 'icon drawing' state
			pGFX->SetShadingEffect( 14 );
			// draw icons
			DrawSprites( pDrawVisitor->icons );
			// enable z-write
			pGFX->SetShadingEffect( 11 );
			pGFX->SetDepthBufferMode( GFXDB_USE_Z );
		}
		// text
		if ( !pDrawVisitor->textes.empty() )
		{
			pDrawVisitor->textes.sort( SSortByFont() );
			for ( CTextVisList::iterator it = pDrawVisitor->textes.begin(); it != pDrawVisitor->textes.end(); ++it )
			{
				pGFX->SetFont( it->pFont );
				pGFX->DrawStringA( it->pszText, it->vScreenPos.x, it->vScreenPos.y, it->color );
			}
		}
		pGFX->RestoreTransform();
	}
	// draw shadow from aviation
	if ( bEnableShadows && !pDrawVisitor->aviation.empty() )
	{
		pGFX->SetDepthBufferMode( GFXDB_NONE );
		pGFX->SetShadingEffect( 110 );
		// draw shadows from 3D mesh objects
		{
			pGFX->SetShadingEffect( 112 );
			// compose shadow matrix
			SHMatrix matShadow;
			Identity( &matShadow );
			matShadow._13 = -sunlight.vDir.x / sunlight.vDir.z;
			matShadow._23 = -sunlight.vDir.y / sunlight.vDir.z;
			matShadow._33 = 0;
			// enable lighting
			pGFX->EnableSpecular( false );
			pGFX->EnableLighting( true );
			pGFX->EnableLight( 0, false );
			//pGFX->SetShadingEffect( 8 );
			// set material for shadow rendering
			SGFXMaterial material;
			Zero( material );
			material.vDiffuse.a = MESH_SHADOW_DENSITY;
			pGFX->SetMaterial( material );
			pGFX->SetTexture( 0, 0 );
			// avaiation
			for ( CMeshVisList::iterator it = pDrawVisitor->aviation.begin(); it != pDrawVisitor->aviation.end(); ++it )
				(*it)->DrawShadow( pGFX, &matShadow, sunlight.vDir );
			//
			pGFX->EnableLighting( false );
		}
		pGFX->SetShadingEffect( 113 );
		pGFX->SetDepthBufferMode( GFXDB_USE_Z );
	}
	// draw gun traces
	if ( !pDrawVisitor->gunTraces.empty() )
	{
		pGFX->SetDepthBufferMode( GFXDB_NONE );
		pGFX->SetShadingEffect( 3 );
		pGFX->SetWorldTransforms( 0, &MONE, 1 );
		pGFX->SetTexture( 0, 0 );
		DrawGunTraces( pDrawVisitor->gunTraces );
		pGFX->SetDepthBufferMode( GFXDB_USE_Z );
	}
	if ( bEnableEffects )
	{
		DrawRain();
		DrawSnow();
		DrawSand();
	}
	// draw particles
	if ( bEnableEffects )
	{
		if ( !pDrawVisitor->particles.empty() )
		{
			pGFX->SetupDirectTransform();
			pGFX->SetShadingEffect( 12 );
			for ( CParticlesVisMap::const_iterator it = pDrawVisitor->particles.begin(); it != pDrawVisitor->particles.end(); ++it )
			{
				if ( it->second.empty() )
					continue;
				pGFX->SetTexture( 0, it->first );
				DrawParticles( it->second );
			}
			pGFX->SetShadingEffect( 11 );
			pGFX->RestoreTransform();
		}
		// draw booms
		if ( !pDrawVisitor->spriteEffects.empty() )
		{
			pGFX->SetupDirectTransform();
			pGFX->SetShadingEffect( 3 );
			DrawSprites( pDrawVisitor->spriteEffects );
			pGFX->RestoreTransform();
		}
	}
	// draw lines
	if ( !pDrawVisitor->boldLines.empty() )
	{
		const int nNumLines = pDrawVisitor->boldLines.size();
		CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( nNumLines * 4, SGFXLineVertex::format, GFXPT_TRIANGLELIST );
		CTempBufferLock<WORD> indices = pGFX->GetTempIndices( nNumLines * 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
		int nLine = 0;
		for ( CBoldLineVisList::iterator it = pDrawVisitor->boldLines.begin(); it != pDrawVisitor->boldLines.end(); ++it, ++nLine )
		{
			vertices[nLine*4 + 0].Setup( it->corners[0], it->color );
			vertices[nLine*4 + 1].Setup( it->corners[1], it->color );
			vertices[nLine*4 + 2].Setup( it->corners[2], it->color );
			vertices[nLine*4 + 3].Setup( it->corners[3], it->color );
			//
			indices[nLine*6 + 0] = 0;
			indices[nLine*6 + 1] = 1;
			indices[nLine*6 + 2] = 2;
			indices[nLine*6 + 3] = 0;
			indices[nLine*6 + 4] = 2;
			indices[nLine*6 + 5] = 3;
		}
		//
		pGFX->SetWorldTransforms( 0, &MONE, 1 );
		pGFX->SetShadingEffect( 3 );
		pGFX->SetTexture( 0, 0 );
		pGFX->DrawTemp();
	}
	// draw additional meshes
	pGFX->SetWorldTransforms( 0, &MONE, 1 );
	for ( std::list<STemporalMesh>::iterator it = tempmeshes.begin(); it != tempmeshes.end(); )
	{
		pGFX->SetTexture( 0, it->pTexture );
		pGFX->SetShadingEffect( it->nShadingEffect );
		pGFX->Draw( it->pVertices, it->pIndices );
		if ( it->bTemporal )
			it = tempmeshes.erase( it );
		else
			++it;
	}
	// draw additional meshes 2
	for ( std::list<SMeshPair2>::iterator it = meshpairs2.begin(); it != meshpairs2.end(); )
	{
		pGFX->SetTexture( 0, it->pTexture );
		pGFX->SetShadingEffect( it->nShadingEffect );
		CTempBufferLock<BYTE> vertices = pGFX->GetTempVertices( it->nNumVertices, it->dwVertexFormat, it->ePrimitiveType );
		vertices = it->vertices;
		if ( !it->indices.empty() ) 
		{
			CTempBufferLock<WORD> indices = pGFX->GetTempIndices( it->indices.size(), GFXIF_INDEX16, it->ePrimitiveType );
			indices = it->indices;
		}
		pGFX->DrawTemp();
		if ( it->bTemporary ) 
			it = meshpairs2.erase( it );
		else
			++it;
	}
	//pGFX->SetWorldTransforms( 0, &MONE, 1 );
	//pGFX->SetShadingEffect( 3 );
	//pGFX->SetTexture( 0, 0 );
	// draw areas
	//pGFX->SetCullMode( GFXC_CW );
	// draw flashes
	if ( !pDrawVisitor->spriteFlashes.empty() )
	{
		//pGFX->SetDepthBufferMode( GFXDB_NONE );
		pGFX->SetupDirectTransform();
		pGFX->SetShadingEffect( 16 );
		pGFX->SetShadingEffect( 22 );
		DrawSprites( pDrawVisitor->spriteFlashes );
		pGFX->SetShadingEffect( 23 );
		pGFX->SetDepthBufferMode( GFXDB_USE_Z );
		pGFX->RestoreTransform();
	}
	// draw haze
	if ( bEnableHaze )
	{
		pGFX->SetupDirectTransform();
		// vertices
		CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( 4, SGFXLVertex::format, GFXPT_TRIANGLELIST );
		vertices[0].Setup( rcScreenRect.x1, rcScreenRect.y1, 0, 1, dwHazeColorTop, 0xff000000, 0, 0 );
		vertices[1].Setup( rcScreenRect.x2, rcScreenRect.y1, 0, 1, dwHazeColorTop, 0xff000000, 0, 0 );
		vertices[2].Setup( rcScreenRect.x1, rcScreenRect.y1 + rcScreenRect.Height()*fHazeHeight, 0, 1, dwHazeColorBottom, 0xff000000, 0, 0 );
		vertices[3].Setup( rcScreenRect.x2, rcScreenRect.y1 + rcScreenRect.Height()*fHazeHeight, 0, 1, dwHazeColorBottom, 0xff000000, 0, 0 );
		// indices
		CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
		indices[0] = 0;
		indices[1] = 2;
		indices[2] = 1;
		indices[3] = 1;
		indices[4] = 2;
		indices[5] = 3;
		//
		pGFX->SetTexture( 0, 0 );
		pGFX->SetShadingEffect( 15 );
		pGFX->SetDepthBufferMode( GFXDB_NONE );
		pGFX->DrawTemp();
		pGFX->SetDepthBufferMode( GFXDB_USE_Z );
		pGFX->RestoreTransform();
	}
 	// draw fog of war
	if ( bEnableWarFog && pTerrain )
		pTerrain->DrawWarFog();
	// units - aviation
	if ( bEnableUnits )
		DrawMeshesChecked( pDrawVisitor->aviation, pGFX, bEnableBBs, sunlight, &(vViewVolumePlanes[0]) );
	//
	// CODE{ 2D scene part
	//
	pGFX->SetupDirectTransform();
	pGFX->SetDepthBufferMode( GFXDB_NONE );
	// draw frame selection
	if ( pFrameSelection->IsActive() )
		pFrameSelection->Draw( pGFX );
	// squad icons
	if ( !squads.empty() ) 
	{
		CVec2 vPos( 5, 5 );
		pGFX->SetShadingEffect( 3 );
		for ( CSquadVisObjList::iterator it = squads.begin(); it != squads.end(); ++it )
		{
			(*it)->SetPosition( vPos );
			(*it)->Draw( pGFX );
			vPos.y += 35;
		}
	}
	// draw UI screens
	if ( !pDrawVisitor->uiObjects.empty() && bShowUI ) 
	{
		pGFX->SetShadingEffect( 3 );
		for ( CDrawVisitor::CUIObjectsList::const_iterator it = pDrawVisitor->uiObjects.begin(); it != pDrawVisitor->uiObjects.end(); ++it )
		{
			switch ( it->eType ) 
			{
				case SUIObject::TYPE_RECTS:
					pGFX->SetTexture( 0, it->pTexture );
					if ( it->nShadingEffect != -1 ) 
						pGFX->SetShadingEffect( it->nShadingEffect );
					pGFX->DrawRects( &(it->rects[0]), it->rects.size() );
					break;
				case SUIObject::TYPE_TEXT:
					if ( it->dwColor != 0 )
					  it->pText->SetColor( it->dwColor );
					pGFX->SetShadingEffect( 3 );
					pGFX->DrawText( it->pText, it->rcRect, it->nY, it->dwFlags );
					break;
				case SUIObject::TYPE_CUSTOM:
					it->pElement->Draw( pGFX );
					break;
			}
		}
	}
	/*
	if ( !uiScreens.empty() && bShowUI )
	{
		pGFX->SetShadingEffect( 3 );
		for ( std::list< CPtr<IUIScreen> >::iterator it = uiScreens.begin(); it != uiScreens.end(); ++it )
			(*it)->Draw( pGFX );
	}
	*/
	// draw tooltip
	if ( tooltip.bHasText )
	{
		pGFX->SetTexture( 0, 0 );
		SGFXRect2 gfxRect;
		gfxRect.rect.Set( tooltip.rcRect.x1, tooltip.rcRect.y1, tooltip.rcRect.x2, tooltip.rcRect.y2 );
		gfxRect.rect.Inflate( 3, 3 );
		gfxRect.maps.SetEmpty();
		gfxRect.fZ = 0;
		gfxRect.color = 0x86000000;
		pGFX->DrawRects( &gfxRect, 1, true );
		gfxRect.color = tooltip.dwBorderColor;
		pGFX->DrawRects( &gfxRect, 1, false );
		pGFX->DrawText( tooltip.pText, tooltip.rcRect, 0, FNT_FORMAT_CENTER );
	}
	// draw 'always' objects
	if ( !alwaysObjects.empty() )
	{
		const NTimer::STime currTime = pTimer->GetGameTime();
		for ( CSceneObjectsList::iterator it = alwaysObjects.begin(); it != alwaysObjects.end(); )
		{
			if ( (*it)->Update(currTime) == false )
				it = alwaysObjects.erase( it );
			else
			{
				(*it)->Draw( pGFX );
				++it;
			}
		}
	}
	// draw cursor
	if ( pCursor )
	{
		pCursor->GetPos();
		pCursor->Draw( pGFX );
	}
	//
	if ( bEnableDepthComplexity )
	{
		pGFX->SetShadingEffect( 301 );
		// rendering
		SGFXRect2 rect;
		rect.rect = pGFX->GetScreenRect();
		rect.fZ = 0;
		for ( int i=0; i<20; ++i )
		{
			pGFX->SetShadingEffect( 310 + i );
			pGFX->DrawRects( &rect, 1 );
		}
		//
		pGFX->SetShadingEffect( 302 );
	}
	pGFX->SetDepthBufferMode( GFXDB_USE_Z );
	pGFX->RestoreTransform();
	//
	// CODE} end of 2D scene
	//
	//
	pDrawVisitor->Clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** sprites drawing
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSpritesSortFunctional
{
public:
	const bool operator()( const SBasicSpriteInfo *pSp1, const SBasicSpriteInfo *pSp2 ) const
	{
		if ( (pSp1->dwCheckFlags & 0xffff0000) == (pSp2->dwCheckFlags & 0xffff0000) ) 
			return pSp1->pTexture == pSp2->pTexture ? pSp1->relpos.z > pSp2->relpos.z : pSp1->pTexture < pSp2->pTexture;
		else
			return (pSp1->dwCheckFlags & 0xffff0000) < (pSp2->dwCheckFlags & 0xffff0000);
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static struct SSpritesPackToDraw
{
	std::vector<const SSpriteInfo*> sprites;
	IGFXTexture *pTexture;
} sprites2Draw;
static struct SComplexSpritesPackToDraw
{
	std::vector<const SComplexSpriteInfo*> sprites;
	IGFXTexture *pTexture;
} complexsprites2Draw;

template <class TYPE>
inline void Reserve2Draw( TYPE &sprites, const int nNumElements )
{
	sprites.sprites.resize( 0 );
	sprites.sprites.reserve( nNumElements );
	sprites.pTexture = 0;
}
inline void ReserveSprites2Draw( const int nNumElements )
{
	Reserve2Draw( sprites2Draw, nNumElements );
	Reserve2Draw( complexsprites2Draw, nNumElements );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TDepthCalculator>
void DrawSprites( CSpriteVisList &sprites, const TDepthCalculator &calculator,
								  const CTRect<float> &rcScreen, IGFX *pGFX )
{
	const int nNumSprites = sprites.size();
	sprites.sort( CSpritesSortFunctional() );
	// sort sprites and render
	ReserveSprites2Draw( nNumSprites );
	for ( CSpriteVisList::const_iterator it = sprites.begin(); it != sprites.end(); ++it )
	{
		switch ( (*it)->type ) 
		{
			case SBasicSpriteInfo::TYPE_NORMAL_SPRITE:
				if ( (*it)->pTexture != sprites2Draw.pTexture ) 
				{
					if ( !sprites2Draw.sprites.empty() ) 
					{
						DrawSingleSpritesPack( sprites2Draw.sprites, calculator, rcScreen, pGFX );
						Reserve2Draw( sprites2Draw, nNumSprites );
					}
					sprites2Draw.pTexture = (*it)->pTexture;
				}
				sprites2Draw.sprites.push_back( static_cast<const SSpriteInfo*>(*it) );
				break;
			case SBasicSpriteInfo::TYPE_COMPLEX_SPRITE:
				if ( (*it)->pTexture != complexsprites2Draw.pTexture ) 
				{
					if ( !complexsprites2Draw.sprites.empty() ) 
					{
						DrawComplexSpritesPack( complexsprites2Draw.sprites, calculator, rcScreen, pGFX );
						Reserve2Draw( complexsprites2Draw, nNumSprites );
					}
					complexsprites2Draw.pTexture = (*it)->pTexture;
				}
				complexsprites2Draw.sprites.push_back( static_cast<const SComplexSpriteInfo*>(*it) );
				break;
		}
	}
	if ( !sprites2Draw.sprites.empty() ) 
		DrawSingleSpritesPack( sprites2Draw.sprites, calculator, rcScreen, pGFX );
	if ( !complexsprites2Draw.sprites.empty() ) 
		DrawComplexSpritesPack( complexsprites2Draw.sprites, calculator, rcScreen, pGFX );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef _USE_HWTL
typedef SGFXLVertex SSpriteVertex;
#else
typedef SGFXTLVertex SSpriteVertex;
#endif // _USE_HWTL
static CRawBuffer<SSpriteVertex> drawvertices;
static CRawBuffer<WORD> drawindices;
//
//		Z1				 2		 4
//	+---+					+---+
//	|		|					|		|
//	|		|					|		|
//	+---+					+---+
//		Z2				 1		 3
//
class CNormalDepthCalculator
{
	const float fZBias;
	const float fZBias2;
public:
	CNormalDepthCalculator( const float _fZBias, const float _fZBias2 ) : fZBias( _fZBias ), fZBias2( _fZBias2 ) {  }
	//
	const float GetZ1( const SSpriteInfo *pInfo ) const 
	{ 
		//return pInfo->relpos.z - fZBias*pInfo->rect.y2;
		return pInfo->relpos.z - fZBias*(pInfo->rect.y2 - pInfo->fDepthLeft) + fZBias2*pInfo->fDepthLeft;
	}
	const float GetZ2( const SSpriteInfo *pInfo ) const 
	{ 
		//return pInfo->relpos.z - fZBias*pInfo->rect.y1;
		return pInfo->relpos.z - fZBias*(pInfo->rect.y1 - pInfo->fDepthLeft) + fZBias2*pInfo->fDepthLeft;
	}
	const float GetZ3( const SSpriteInfo *pInfo ) const 
	{ 
		//return pInfo->relpos.z - fZBias*pInfo->rect.y2;
		return pInfo->relpos.z - fZBias*(pInfo->rect.y2 - pInfo->fDepthRight) + fZBias2*pInfo->fDepthRight;
	}
	const float GetZ4( const SSpriteInfo *pInfo ) const 
	{ 
		//return pInfo->relpos.z - fZBias*pInfo->rect.y1;
		return pInfo->relpos.z - fZBias*(pInfo->rect.y1 - pInfo->fDepthRight) + fZBias2*pInfo->fDepthRight;
	}
	//
	const float GetZ1( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square ) const 
	{ 
		return pInfo->relpos.z - fZBias*(square.vLeftTop.y + square.fDepthLeft + square.fSize) + fZBias2*square.fDepthLeft;
	}
	const float GetZ2( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square ) const 
	{ 
		return pInfo->relpos.z - fZBias*(square.vLeftTop.y + square.fDepthLeft) + fZBias2*square.fDepthLeft;
	}
	const float GetZ3( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square ) const 
	{ 
		return pInfo->relpos.z - fZBias*(square.vLeftTop.y + square.fDepthRight + square.fSize) + fZBias2*square.fDepthRight;
	}
	const float GetZ4( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square ) const 
	{ 
		return pInfo->relpos.z - fZBias*(square.vLeftTop.y + square.fDepthRight) + fZBias2*square.fDepthRight;
	}
};
class CTerrainDepthCalculator
{
public:
	const float GetZ1( const SSpriteInfo *pInfo ) const { return 0.999999f; }
	const float GetZ2( const SSpriteInfo *pInfo ) const { return 0.999999f; }
	const float GetZ3( const SSpriteInfo *pInfo ) const { return 0.999999f; }
	const float GetZ4( const SSpriteInfo *pInfo ) const { return 0.999999f; }
	//
	const float GetZ1( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square ) const { return 0.999999f; }
	const float GetZ2( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square ) const { return 0.999999f; }
	const float GetZ3( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square ) const { return 0.999999f; }
	const float GetZ4( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square ) const { return 0.999999f; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// x1 <= x < x2
// y1 <= y < y2
//			( (x <  x1)      ) |
//			( (x >= x2) << 1 ) |
//			( (y <  y1) << 2 ) |
//			( (y >= y2) << 3 ) |
//
// 4 bits per point
//
inline const DWORD GetFlags( const float x1, const float y1, const float x2, const float y2, const float x, const float y )
{
	const float t1 = x - x1, t2 = y - y1, t3 = x2 - x - 1.0f, t4 = y2 - y - 1.0f;
	return (  FP_BITS_CONST(t1) >> 31 ) |
				 (( FP_BITS_CONST(t2) >> 30 ) & 2 ) |
				 (( FP_BITS_CONST(t3) >> 29 ) & 4 ) |
				 (( FP_BITS_CONST(t4) >> 28 ) & 8 );
}

inline const DWORD CheckForRect( const CTRect<float> &rect, const float x, const float y )
{
	return GetFlags( rect.x1, rect.y1, rect.x2, rect.y2, x, y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TDepthCalculator>
int AddSquare( const SComplexSpriteInfo *pInfo, const SSpritesPack::SSprite::SSquare &square, const WORD wCurrVertex,
							 const TDepthCalculator &calculator, const CTRect<float> &rcScreen,
							 const float fTexDiffX, const float fTexDiffY, const float fScrDiff )
{
	const float fX1 = MINT( pInfo->relpos.x + square.vLeftTop.x ) + fScrDiff;
	const float fX2 = MINT( pInfo->relpos.x + square.vLeftTop.x + square.fSize ) + fScrDiff;
	const float fY1 = MINT( pInfo->relpos.y + square.vLeftTop.y ) + fScrDiff;
	const float fY2 = MINT( pInfo->relpos.y + square.vLeftTop.y + square.fSize ) + fScrDiff;
	// perform checking
	if ( pInfo->dwCheckFlags & 1 ) 
	{
		const DWORD dwPoint1 = CheckForRect( rcScreen, fX1, fY2 );
		const DWORD dwPoint2 = CheckForRect( rcScreen, fX1, fY1 );
		const DWORD dwPoint3 = CheckForRect( rcScreen, fX2, fY2 );
		const DWORD dwPoint4 = CheckForRect( rcScreen, fX2, fY1 );
		if ( (dwPoint1 & dwPoint2 & dwPoint3 & dwPoint4) != 0 ) 
			return 0;
	}
	//
	const float fZ1 = calculator.GetZ1( pInfo, square );
	const float fZ2 = calculator.GetZ2( pInfo, square );
	const float fZ3 = calculator.GetZ3( pInfo, square );
	const float fZ4 = calculator.GetZ4( pInfo, square );
	//
	const float fMapX1 = square.rcMaps.x1 + fTexDiffX;
	const float fMapY1 = square.rcMaps.y1 + fTexDiffY;
	const float fMapX2 = square.rcMaps.x2 + fTexDiffX;
	const float fMapY2 = square.rcMaps.y2 + fTexDiffY;
	//
	SSpriteVertex *pVertices = Resize2Fit( drawvertices, 4 );
	//
	pVertices->Setup( fX1, fY2, fZ1, 1, pInfo->color, pInfo->specular, fMapX1, fMapY2 );
	++pVertices;
	pVertices->Setup( fX1, fY1, fZ2, 1, pInfo->color, pInfo->specular, fMapX1, fMapY1 );
	++pVertices;
	pVertices->Setup( fX2, fY2, fZ3, 1, pInfo->color, pInfo->specular, fMapX2, fMapY2 );
	++pVertices;
	pVertices->Setup( fX2, fY1, fZ4, 1, pInfo->color, pInfo->specular, fMapX2, fMapY1 );
	++pVertices;
	//
	WORD *pIndices = Resize2Fit( drawindices, 6 );
	//
	*pIndices++ = wCurrVertex + 2;
	*pIndices++ = wCurrVertex + 1;
	*pIndices++ = wCurrVertex + 0;
	*pIndices++ = wCurrVertex + 1;
	*pIndices++ = wCurrVertex + 2;
	*pIndices++ = wCurrVertex + 3;
	//
	return 4;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TDepthCalculator>
int AddSquare( const SSpriteInfo *pInfo, const WORD wCurrVertex,
							 const TDepthCalculator &calculator, const CTRect<float> &rcScreen,
							 const float fTexDiffX, const float fTexDiffY, const float fScrDiff )
{
	const float fX1 = MINT( pInfo->relpos.x + pInfo->rect.x1 ) + fScrDiff;
	const float fX2 = MINT( pInfo->relpos.x + pInfo->rect.x2 ) + fScrDiff;
	const float fY1 = MINT( pInfo->relpos.y + pInfo->rect.y1 ) + fScrDiff;
	const float fY2 = MINT( pInfo->relpos.y + pInfo->rect.y2 ) + fScrDiff;
	// perform checking
	if ( pInfo->dwCheckFlags & 1 ) 
	{
		const DWORD dwPoint1 = CheckForRect( rcScreen, fX1, fY2 );
		const DWORD dwPoint2 = CheckForRect( rcScreen, fX1, fY1 );
		const DWORD dwPoint3 = CheckForRect( rcScreen, fX2, fY2 );
		const DWORD dwPoint4 = CheckForRect( rcScreen, fX2, fY1 );
		if ( (dwPoint1 & dwPoint2 & dwPoint3 & dwPoint4) != 0 ) 
			return 0;
	}
	//
	const float fZ1 = calculator.GetZ1( pInfo );
	const float fZ2 = calculator.GetZ2( pInfo );
	const float fZ3 = calculator.GetZ3( pInfo );
	const float fZ4 = calculator.GetZ4( pInfo );
	//
	const float fMapX1 = pInfo->maps.x1 + fTexDiffX;
	const float fMapY1 = pInfo->maps.y1 + fTexDiffY;
	const float fMapX2 = pInfo->maps.x2 + fTexDiffX;
	const float fMapY2 = pInfo->maps.y2 + fTexDiffY;
	//
	SSpriteVertex *pVertices = Resize2Fit( drawvertices, 4 );
	pVertices->Setup( fX1, fY2, fZ1, 1, pInfo->color, pInfo->specular, fMapX1, fMapY2 );
	++pVertices;
	pVertices->Setup( fX1, fY1, fZ2, 1, pInfo->color, pInfo->specular, fMapX1, fMapY1 );
	++pVertices;
	pVertices->Setup( fX2, fY2, fZ3, 1, pInfo->color, pInfo->specular, fMapX2, fMapY2 );
	++pVertices;
	pVertices->Setup( fX2, fY1, fZ4, 1, pInfo->color, pInfo->specular, fMapX2, fMapY1 );
	++pVertices;
	//
	WORD *pIndices = Resize2Fit( drawindices, 6 );
	*pIndices++ = wCurrVertex + 2;
	*pIndices++ = wCurrVertex + 1;
	*pIndices++ = wCurrVertex + 0;
	*pIndices++ = wCurrVertex + 1;
	*pIndices++ = wCurrVertex + 2;
	*pIndices++ = wCurrVertex + 3;
	//
	return 4;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TDepthCalculator>
bool DrawSingleSpritesPack( const std::vector<const SSpriteInfo*> &sprites, const TDepthCalculator &calculator, 
													  const CTRect<float> &rcScreen, IGFX *pGFX )
{
	if ( sprites.empty() )
		return false;
	const int nNumSprites = sprites.size();
	float fTexDiffX = 0, fTexDiffY = 0, fScrDiff = -0.5f;
	if ( sprites[0]->pTexture != 0 ) 
	{
		fTexDiffX = -0.5f / float( sprites[0]->pTexture->GetSizeX(0) );
		fTexDiffY = -0.5f / float( sprites[0]->pTexture->GetSizeY(0) );
		fScrDiff = -0.5f;
	}
	//
	WORD wCurrVertex = 0;
	DWORD dwSpecularCheck = 0;;
	//
	drawvertices.resize( 0 );
	drawvertices.reserve( nNumSprites * 4 );
	drawindices.resize( 0 );
	drawindices.reserve( nNumSprites * 6 );
	//
	_control87( _RC_CHOP, _MCW_RC );
	for ( std::vector<const SSpriteInfo*>::const_iterator it = sprites.begin(); it != sprites.end(); ++it ) 
	{
		wCurrVertex += AddSquare( *it, wCurrVertex, calculator, rcScreen, fTexDiffX, fTexDiffY, fScrDiff );
		dwSpecularCheck |= (*it)->specular;
	}
	_control87( _RC_NEAR, _MCW_RC );
	//
	pGFX->EnableSpecular( dwSpecularCheck != 0xff000000 );
	pGFX->SetTexture( 0, sprites[0]->pTexture );
	return DrawTemp( pGFX, drawvertices, drawindices );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TDepthCalculator>
bool DrawComplexSpritesPack( const std::vector<const SComplexSpriteInfo*> &sprites, const TDepthCalculator &calculator, 
														 const CTRect<float> &rcScreen, IGFX *pGFX )
{
	if ( sprites.empty() )
		return false;
	const int nNumSprites = sprites.size();
	float fTexDiffX = 0, fTexDiffY = 0, fScrDiff = -0.5f;
	if ( sprites[0]->pTexture != 0 ) 
	{
		fTexDiffX = -0.5f / float( sprites[0]->pTexture->GetSizeX(0) );
		fTexDiffY = -0.5f / float( sprites[0]->pTexture->GetSizeY(0) );
		fScrDiff = -0.5f;
	}
	// estimate num squares to draw
	int nNumSquares = 0;
	for ( std::vector<const SComplexSpriteInfo*>::const_iterator it = sprites.begin(); it != sprites.end(); ++it )
		nNumSquares += (*it)->pSprite->squares.size();
	drawvertices.resize( 0 );
	drawvertices.reserve( nNumSquares * 4 );
	drawindices.resize( 0 );
	drawindices.reserve( nNumSquares * 6 );
	//
	DWORD dwSpecularCheck = 0;
	WORD wCurrVertex = 0;
	_control87( _RC_CHOP, _MCW_RC );
	for ( std::vector<const SComplexSpriteInfo*>::const_iterator sprite = sprites.begin(); sprite != sprites.end(); ++sprite )
	{
		const SComplexSpriteInfo *pInfo = *sprite;
		dwSpecularCheck |= pInfo->specular;
		for ( SSpritesPack::SSprite::CSquaresList::const_iterator it = pInfo->pSprite->squares.begin(); it != pInfo->pSprite->squares.end(); ++it )
		{
			if ( drawvertices.size() > 65000 ) 
			{
				// draw
				_control87( _RC_NEAR, _MCW_RC );
				pGFX->EnableSpecular( dwSpecularCheck != 0xff000000 );
				pGFX->SetTexture( 0, sprites[0]->pTexture );
				DrawTemp( pGFX, drawvertices, drawindices );
				_control87( _RC_CHOP, _MCW_RC );
				// resize
				drawvertices.resize( 0 );
				drawvertices.reserve( nNumSquares * 4 );
				drawindices.resize( 0 );
				drawindices.reserve( nNumSquares * 6 );
			}
			wCurrVertex += AddSquare( *sprite, *it, wCurrVertex, calculator, rcScreen, fTexDiffX, fTexDiffY, fScrDiff );
		}
	}
	_control87( _RC_NEAR, _MCW_RC );
	//
	pGFX->EnableSpecular( dwSpecularCheck != 0xff000000 );
	pGFX->SetTexture( 0, sprites[0]->pTexture );
	return DrawTemp( pGFX, drawvertices, drawindices );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawSprites( CSpriteVisList &sprites )
{
	if ( sprites.empty() )
		return;
	::DrawSprites( sprites, CNormalDepthCalculator(fZBias, fZBias2), pGFX->GetScreenRect(), pGFX );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawTerraObjects( CSpriteVisList &sprites )
{
	if ( sprites.empty() )
		return;
	::DrawSprites( sprites, CTerrainDepthCalculator(), pGFX->GetScreenRect(), pGFX );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** particles
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawParticles( const CParticlesVisList &particles )
{
	if ( particles.empty() )
		return;
	int nNumParticles = particles.size();
	DrawSingleParticlesPack( particles, nNumParticles );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void FastRotate( float *pfX, float *pfY, const float fX, const float fY, const float fCos, const float fSin )
{
	*pfX = fX*fCos - fY*fSin;
	*pfY = fX*fSin + fY*fCos;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawSingleParticlesPack( const CParticlesVisList &particles, int nNumParticles )
{
	SSpriteVertex *pVertices = (SSpriteVertex*)pGFX->GetTempVertices( nNumParticles * 4, SSpriteVertex::format, GFXPT_TRIANGLELIST );
	WORD *pIndices = (WORD*)pGFX->GetTempIndices( nNumParticles * 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
	WORD wCurrVertex = 0;
	bool bSpecularEnable = false;
	float fX, fY;
	for ( CParticlesVisList::const_iterator it = particles.begin(); it != particles.end(); ++it, wCurrVertex += 4 )
	{
		const SParticleInfo &info = *it;
		const int nAngleCalibrated = FSinCosMakeAngleChecked( info.fAngle );
		const float fCos = FCosCalibrated( nAngleCalibrated );
		const float fSin = FSinCalibrated( nAngleCalibrated );
		//
		FastRotate( &fX, &fY, -info.fSize, +info.fSize, fCos, fSin );
		pVertices->Setup( info.vPos.x + fX, info.vPos.y + fY, info.vPos.z - fZBias*fY, 1, 
			                info.color, info.specular, info.maps.x1, info.maps.y2 );
		++pVertices;

		FastRotate( &fX, &fY, -info.fSize, -info.fSize, fCos, fSin );
		pVertices->Setup( info.vPos.x + fX, info.vPos.y + fY, info.vPos.z - fZBias*fY, 1, 
			                info.color, info.specular, info.maps.x1, info.maps.y1 );
		++pVertices;

		FastRotate( &fX, &fY, +info.fSize, +info.fSize, fCos, fSin );
		pVertices->Setup( info.vPos.x + fX, info.vPos.y + fY, info.vPos.z - fZBias*fY, 1, 
			                info.color, info.specular, info.maps.x2, info.maps.y2 );
		++pVertices;

		FastRotate( &fX, &fY, +info.fSize, -info.fSize, fCos, fSin );
		pVertices->Setup( info.vPos.x + fX, info.vPos.y + fY, info.vPos.z - fZBias*fY, 1, 
			                info.color, info.specular, info.maps.x2, info.maps.y1 );
		++pVertices;

		*pIndices++ = wCurrVertex + 2;
		*pIndices++ = wCurrVertex + 1;
		*pIndices++ = wCurrVertex + 0;
		*pIndices++ = wCurrVertex + 1;
		*pIndices++ = wCurrVertex + 2;
		*pIndices++ = wCurrVertex + 3;
		//
		bSpecularEnable |= ( info.specular != 0xff000000 );
	}
	//
	pGFX->EnableSpecular( bSpecularEnable );
	//
	pGFX->DrawTemp();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawMechTraces( const std::list<SMechTrace> &traces )
{
	const int nNumTraces = traces.size();
 
	if ( pTrackTexture == 0 )
		pTrackTexture = GetSingleton<ITextureManager>()->GetTexture("units\\technics\\tanktrack");
	pGFX->SetTexture( 0, pTrackTexture );
	CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( nNumTraces * 4, SGFXLVertex::format, GFXPT_TRIANGLELIST );
	CTempBufferLock<WORD> indices = pGFX->GetTempIndices( nNumTraces * 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
	SGFXLVertex *pVertex = vertices.GetBuffer();
	WORD *pIndex = indices.GetBuffer();
	WORD wCurrVertex = 0;
	for ( std::list<SMechTrace>::const_iterator it = traces.begin(); it != traces.end(); ++it, wCurrVertex += 4 )
	{
		const SMechTrace &trace = *it;
		pVertex->Setup( it->vCorners[0], it->dwColor, 0xff000000, CVec2(0, 0) );
		++pVertex;
		pVertex->Setup( it->vCorners[1], it->dwColor, 0xff000000, CVec2(1, 0) );
		++pVertex;
		pVertex->Setup( it->vCorners[2], it->dwColor, 0xff000000, CVec2(0, it->nNumTracks) );
		++pVertex;
		pVertex->Setup( it->vCorners[3], it->dwColor, 0xff000000, CVec2(1, it->nNumTracks) );
		++pVertex;
		//
		*pIndex++ = wCurrVertex + 0;
		*pIndex++ = wCurrVertex + 1;
		*pIndex++ = wCurrVertex + 2;
		*pIndex++ = wCurrVertex + 1;
		*pIndex++ = wCurrVertex + 3;
		*pIndex++ = wCurrVertex + 2;
	}
	//
	pGFX->DrawTemp();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawGunTraces( const std::list<SGunTrace> &traces )
{
	const int nNumTraces = traces.size();

	CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( nNumTraces * 4, SGFXLVertex::format, GFXPT_TRIANGLELIST );
	CTempBufferLock<WORD> indices = pGFX->GetTempIndices( nNumTraces * 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
	SGFXLVertex *pVertex = vertices.GetBuffer();
	WORD *pIndex = indices.GetBuffer();
	WORD wCurrVertex = 0;
	for ( std::list<SGunTrace>::const_iterator it = traces.begin(); it != traces.end(); ++it, wCurrVertex += 4 )
	{
		const SGunTrace &trace = *it;
		pVertex->Setup( it->vPoints[0], dwGunTraceColor, 0xff000000, VNULL2 );
		++pVertex;
		pVertex->Setup( it->vPoints[1], dwGunTraceColor, 0xff000000, VNULL2 );
		++pVertex;
		pVertex->Setup( it->vPoints[2], dwGunTraceColor, 0xff000000, VNULL2 );
		++pVertex;
		pVertex->Setup( it->vPoints[3], dwGunTraceColor, 0xff000000, VNULL2 );
		++pVertex;
		//
		*pIndex++ = wCurrVertex + 0;
		*pIndex++ = wCurrVertex + 1;
		*pIndex++ = wCurrVertex + 2;
		*pIndex++ = wCurrVertex + 0;
		*pIndex++ = wCurrVertex + 2;
		*pIndex++ = wCurrVertex + 3;
	}
	//
	pGFX->DrawTemp();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawRain()
{
	if ( rainDrops.size() != 0 )
	{
		CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( 3 * rainDrops.size(), SGFXLVertex::format, GFXPT_TRIANGLELIST );
		CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 3 * rainDrops.size(), GFXIF_INDEX16, GFXPT_TRIANGLELIST );
		int nCounter = 0;
		for ( std::vector<SRainDrop>::const_iterator it = rainDrops.begin(); it != rainDrops.end(); ++it )
		{
			vertices[nCounter].Setup( it->vPos, dwRainTopColor, 0xffffffff, VNULL2 );
			CVec3 vEnd = it->vPos + it->fLength * vRainDir;
			vEnd.x -= 1.5f;
			vertices[nCounter + 1].Setup( vEnd, dwRainBottomColor, 0xffffffff, VNULL2 );
			vEnd.x += 3.0f;
			vertices[nCounter + 2].Setup( vEnd, dwRainBottomColor, 0xffffffff, VNULL2 );
			indices[nCounter] = nCounter;
			indices[nCounter + 1] = nCounter + 1;
			indices[nCounter + 2] = nCounter + 2;
			nCounter += 3;
		}
		pGFX->SetTexture( 0, 0 );
		pGFX->SetShadingEffect( 3 );
		pGFX->SetWorldTransforms( 0, &MONE, 1 );
		pGFX->SetDepthBufferMode( GFXDB_NONE );
		pGFX->DrawTemp();
		pGFX->SetDepthBufferMode( GFXDB_USE_Z );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawMarkers()
{
	if ( bDrawArrow )
	{
		CVec3 vNDir;
		vNDir.x = - vArrowDir.y;
		vNDir.y = vArrowDir.x;
		vNDir.z = vArrowDir.z;
		const float fArrowLen = 20.0f;
		const float fArrowHeadLen = 5.0f;
		CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( 7, SGFXLVertex::format, GFXPT_TRIANGLELIST );
		CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 9, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
		vertices[0].Setup( vArrowStart + vNDir, dwArrowColor, 0xff000000, VNULL2 );
		vertices[1].Setup( vArrowStart - vNDir, dwArrowColor, 0xff000000, VNULL2 );
		vertices[2].Setup( vArrowStart + vArrowDir * ( fArrowLen - fArrowHeadLen ) + vNDir, dwArrowColor, 0xff000000, VNULL2 );
		vertices[3].Setup( vArrowStart + vArrowDir * ( fArrowLen - fArrowHeadLen ) - vNDir, dwArrowColor, 0xff000000, VNULL2 );
		vertices[4].Setup( vArrowStart + vArrowDir * ( fArrowLen - fArrowHeadLen ) + vNDir * 2.0f, dwArrowColor, 0xff000000, VNULL2 );
		vertices[5].Setup( vArrowStart + vArrowDir * ( fArrowLen - fArrowHeadLen ) - vNDir * 2.0f, dwArrowColor, 0xff000000, VNULL2 );
		vertices[6].Setup( vArrowStart + vArrowDir * fArrowLen, dwArrowColor, 0xff000000, VNULL2 );
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 1;
		indices[4] = 3;
		indices[5] = 2;
		indices[6] = 4;
		indices[7] = 5;
		indices[8] = 6;
		pGFX->DrawTemp();
	}
	if ( !posMarkers.empty() )
	{
		float fShiftAngle = 0;
		if ( bRotateMarkers )
		{
			CVec2 vDir;
			vDir.Set( vArrowDir.x, vArrowDir.y );
			Normalize( &vDir );
			fShiftAngle = vDir.x < 0 ? -1.0f * acos( vDir.y ) : acos( vDir.y );
			fShiftAngle -= fArrowBegin;
		}
		for ( std::list<CVec3>::iterator it = posMarkers.begin(); it != posMarkers.end(); ++it )
		{
			CVec3 vPos = (*it);
			if ( bRotateMarkers )
			{
				vPos.Set( vPos.x * FCos( fShiftAngle ) + vPos.y * FSin( fShiftAngle ), - vPos.x * FSin( fShiftAngle ) + vPos.y * FCos( fShiftAngle ), vPos.z );
				vPos += vArrowStart;
			}
			const float fInnerRadius = 10;
			const float fOuterRadius = 15;
			const float fCosCoeff = 0.707f; //sqrt(2)/2
			const DWORD dwColor = 0x80000000 | dwMarkerColor;
			CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( 18, SGFXLineVertex::format, GFXPT_TRIANGLESTRIP );
			vertices[0].Setup( vPos.x, vPos.y + fOuterRadius, vPos.z, dwColor );
			vertices[1].Setup( vPos.x, vPos.y + fInnerRadius, vPos.z, dwColor );
			vertices[2].Setup( vPos.x + fOuterRadius * fCosCoeff, vPos.y + fOuterRadius * fCosCoeff, vPos.z, dwColor );
			vertices[3].Setup( vPos.x + fInnerRadius * fCosCoeff, vPos.y + fInnerRadius * fCosCoeff, vPos.z, dwColor );
			vertices[4].Setup( vPos.x + fOuterRadius, vPos.y, vPos.z, dwColor );
			vertices[5].Setup( vPos.x + fInnerRadius, vPos.y, vPos.z, dwColor );
			vertices[6].Setup( vPos.x + fOuterRadius * fCosCoeff, vPos.y - fOuterRadius * fCosCoeff, vPos.z, dwColor );
			vertices[7].Setup( vPos.x + fInnerRadius * fCosCoeff, vPos.y - fInnerRadius * fCosCoeff, vPos.z, dwColor );
			vertices[8].Setup( vPos.x, vPos.y - fOuterRadius, vPos.z, dwColor );
			vertices[9].Setup( vPos.x, vPos.y - fInnerRadius, vPos.z, dwColor );
			vertices[10].Setup( vPos.x - fOuterRadius * fCosCoeff, vPos.y - fOuterRadius * fCosCoeff, vPos.z, dwColor );
			vertices[11].Setup( vPos.x - fInnerRadius * fCosCoeff, vPos.y - fInnerRadius * fCosCoeff, vPos.z, dwColor );
			vertices[12].Setup( vPos.x - fOuterRadius, vPos.y, vPos.z, dwColor );
			vertices[13].Setup( vPos.x - fInnerRadius, vPos.y, vPos.z, dwColor );
			vertices[14].Setup( vPos.x - fOuterRadius * fCosCoeff, vPos.y + fOuterRadius * fCosCoeff, vPos.z, dwColor );
			vertices[15].Setup( vPos.x - fInnerRadius * fCosCoeff, vPos.y + fInnerRadius * fCosCoeff, vPos.z, dwColor );
			vertices[16].Setup( vPos.x, vPos.y + fOuterRadius, vPos.z, dwColor );
			vertices[17].Setup( vPos.x, vPos.y + fInnerRadius, vPos.z, dwColor );
			pGFX->DrawTemp();
		}
	}
	for ( std::list<SClickMarker>::iterator it = clickMarkers.begin(); it != clickMarkers.end(); ++it ) 
	{
		const float fStage = float( GetSingleton<IGameTimer>()->GetAbsTime() - it->nStartTime ) / nMarkerLifetime;
		if ( fStage > 1 || fStage < 0 )
		{
			it = clickMarkers.erase( it );
		}	
		else
		{
			const float fInnerRadius = 15 * fStage;
			const float fOuterRadius = 20 * fStage;
			const float fCosCoeff = 0.707f; //sqrt(2)/2
			const CVec3 vPos = it->vPos;
			const DWORD dwColor = ( DWORD( 0xff * (1.0 - fStage) ) << 24 ) | dwMarkerColor;
			CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( 18, SGFXLineVertex::format, GFXPT_TRIANGLESTRIP );
			vertices[0].Setup( vPos.x, vPos.y + fOuterRadius, vPos.z, dwColor );
			vertices[1].Setup( vPos.x, vPos.y + fInnerRadius, vPos.z, dwColor );
			vertices[2].Setup( vPos.x + fOuterRadius * fCosCoeff, vPos.y + fOuterRadius * fCosCoeff, vPos.z, dwColor );
			vertices[3].Setup( vPos.x + fInnerRadius * fCosCoeff, vPos.y + fInnerRadius * fCosCoeff, vPos.z, dwColor );
			vertices[4].Setup( vPos.x + fOuterRadius, vPos.y, vPos.z, dwColor );
			vertices[5].Setup( vPos.x + fInnerRadius, vPos.y, vPos.z, dwColor );
			vertices[6].Setup( vPos.x + fOuterRadius * fCosCoeff, vPos.y - fOuterRadius * fCosCoeff, vPos.z, dwColor );
			vertices[7].Setup( vPos.x + fInnerRadius * fCosCoeff, vPos.y - fInnerRadius * fCosCoeff, vPos.z, dwColor );
			vertices[8].Setup( vPos.x, vPos.y - fOuterRadius, vPos.z, dwColor );
			vertices[9].Setup( vPos.x, vPos.y - fInnerRadius, vPos.z, dwColor );
			vertices[10].Setup( vPos.x - fOuterRadius * fCosCoeff, vPos.y - fOuterRadius * fCosCoeff, vPos.z, dwColor );
			vertices[11].Setup( vPos.x - fInnerRadius * fCosCoeff, vPos.y - fInnerRadius * fCosCoeff, vPos.z, dwColor );
			vertices[12].Setup( vPos.x - fOuterRadius, vPos.y, vPos.z, dwColor );
			vertices[13].Setup( vPos.x - fInnerRadius, vPos.y, vPos.z, dwColor );
			vertices[14].Setup( vPos.x - fOuterRadius * fCosCoeff, vPos.y + fOuterRadius * fCosCoeff, vPos.z, dwColor );
			vertices[15].Setup( vPos.x - fInnerRadius * fCosCoeff, vPos.y + fInnerRadius * fCosCoeff, vPos.z, dwColor );
			vertices[16].Setup( vPos.x, vPos.y + fOuterRadius, vPos.z, dwColor );
			vertices[17].Setup( vPos.x, vPos.y + fInnerRadius, vPos.z, dwColor );
			pGFX->DrawTemp();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawSnow()
{
	if ( snowFlakes.size() != 0 )
	{
		CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( 3 * snowFlakes.size(), SGFXLVertex::format, GFXPT_TRIANGLELIST );
		CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 3 * snowFlakes.size(), GFXIF_INDEX16, GFXPT_TRIANGLELIST );
		int nCounter = 0;
		for ( std::vector<SSnowFlake>::const_iterator it = snowFlakes.begin(); it != snowFlakes.end(); ++it )
		{
			CVec3 vPos = it->vPos;
			vertices[nCounter].Setup( vPos, dwSnowColor, 0xffffffff, VNULL2 );
			vPos.z -= 3.0f;
			vertices[nCounter + 1].Setup( vPos, dwSnowColor, 0xffffffff, VNULL2 );
			vPos.x += 3.0f;
			vertices[nCounter + 2].Setup( vPos, dwSnowColor, 0xffffffff, VNULL2 );
			indices[nCounter] = nCounter;
			indices[nCounter + 1] = nCounter + 1;
			indices[nCounter + 2] = nCounter + 2;
			nCounter += 3;
		}
		pGFX->SetTexture( 0, 0 );
		pGFX->SetShadingEffect( 3 );
		pGFX->SetWorldTransforms( 0, &MONE, 1 );
		pGFX->SetDepthBufferMode( GFXDB_NONE );
		pGFX->DrawTemp();
		pGFX->SetDepthBufferMode( GFXDB_USE_Z );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::DrawSand()
{
	if ( pSandTexture == 0 )
		pSandTexture = GetSingleton<ITextureManager>()->GetTexture( "effects\\particles\\sanddust" ); 
	int nNumCone = 0;
	for ( std::vector<SSandParticle>::const_iterator it = sandParticles.begin(); it != sandParticles.end(); ++it )
	{
		if ( it->bConeDraw )
			nNumCone++;
	}
	pGFX->SetTexture( 0, pSandTexture );
	pGFX->SetShadingEffect( 3 );
	pGFX->SetWorldTransforms( 0, &MONE, 1 );
	pGFX->SetDepthBufferMode( GFXDB_NONE );
	if ( nNumCone > 0 )
	{
		CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( 4 * nNumCone, SGFXLVertex::format, GFXPT_TRIANGLELIST );
		CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 6 * nNumCone, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
		int nCounter = 0;
		int nIndCounter = 0;
		for ( std::vector<SSandParticle>::const_iterator it = sandParticles.begin(); it != sandParticles.end(); ++it )
		{
			if ( it->bConeDraw )
			{
				CVec3 vPos = it->vPos;
				vertices[nCounter].Setup( vPos, 0xffffffff, 0xff000000, VNULL2 );
				vPos.z += 40.0f;
				vertices[nCounter + 1].Setup( vPos, 0xffffffff, 0xff000000, CVec2(0.0f, 0.9f) );
				vPos.x += 40.0f;
				vertices[nCounter + 2].Setup( vPos, 0xffffffff, 0xff000000, CVec2(1.0f, 0.9f) );
				vPos.z -= 40.0f;
				vertices[nCounter + 3].Setup( vPos, 0xffffffff, 0xff000000, CVec2(1.0f, 0.0f) );
				indices[nIndCounter] = nCounter;
				indices[nIndCounter + 1] = nCounter + 2;
				indices[nIndCounter + 2] = nCounter + 1;
				indices[nIndCounter + 3] = nCounter;
				indices[nIndCounter + 4] = nCounter + 3;
				indices[nIndCounter + 5] = nCounter + 2;
				nCounter += 4;
				nIndCounter += 6;
			}
		}
		pGFX->DrawTemp();
	}
	if ( sandParticles.size() - nNumCone > 0 )
	{
		CTempBufferLock<SGFXLVertex> vertices = pGFX->GetTempVertices( 3 * (sandParticles.size() - nNumCone), SGFXLVertex::format, GFXPT_TRIANGLELIST );
		CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 3 * (sandParticles.size() - nNumCone), GFXIF_INDEX16, GFXPT_TRIANGLELIST );
		int nCounter = 0;
		int nIndCounter = 0;
		for ( std::vector<SSandParticle>::const_iterator it = sandParticles.begin(); it != sandParticles.end(); ++it )
		{
			if ( !(it->bConeDraw) )
			{
				CVec3 vPos = it->vPos;
				vertices[nCounter].Setup( vPos, 0xffffffff, 0xffffffff, CVec2(0.0f, 0.9f) );
				vPos.z += 4.0f;
				vertices[nCounter + 1].Setup( vPos, 0xffffffff, 0xffffffff, CVec2(0.0f, 1.0f) );
				vPos.x += 4.0f;
				vertices[nCounter + 2].Setup( vPos, 0xffffffff, 0xffffffff, CVec2(1.0f, 1.0f) );
				indices[nIndCounter] = nCounter;
				indices[nIndCounter + 1] = nCounter + 2;
				indices[nIndCounter + 2] = nCounter + 1;
				nCounter += 3;
				nIndCounter += 3;
			}
		}
		pGFX->DrawTemp();
	}
	pGFX->SetDepthBufferMode( GFXDB_USE_Z );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** visibility list forming
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateMechTrace( SMechTrace *pTrace, const NTimer::STime &time )
{
	if ( time > pTrace->deathTime ) 
		return false;
	DWORD col = (DWORD)(pTrace->alpha + (1.0 - (pTrace->deathTime - time) / (float)(pTrace->deathTime - pTrace->birthTime)) * (255.0 - pTrace->alpha));
	pTrace->dwColor &= 0xFF000000;
	pTrace->dwColor |= ( col << 16 ) | ( col << 8 ) | col;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateGunTrace( SGunTrace *pTrace, const NTimer::STime &time, float fTraceLen )
{
	if ( time > pTrace->deathTime ) 
		return false;
	if ( time > pTrace->birthTime )
	{
		const float fTime = (time - pTrace->birthTime) / float(pTrace->deathTime - pTrace->birthTime);
		const float fTail = Clamp( fTime - fTraceLen, 0.0f, 1.0f );
		pTrace->vPoints[0] = pTrace->vStart + pTrace->vDir * fTail;
		pTrace->vPoints[1] = pTrace->vStart + pTrace->vDir * fTime;
		pTrace->vPoints[2] = pTrace->vPoints[1];
		pTrace->vPoints[3] = pTrace->vPoints[0];
		pTrace->vPoints[2].x += 1;
		pTrace->vPoints[3].x += 1;
		pTrace->vPoints[2].y += 1;
		pTrace->vPoints[3].y += 1;
		pTrace->vPoints[2].z += 1;
		pTrace->vPoints[3].z += 1;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::UpdateWeather()
{
	NTimer::STime time = GetSingleton<IGameTimer>()->GetAbsTime();
	if ( !bWeatherInitialized )
	{
			//init weather
		snowFlakes.resize( 0 );
		const std::string szSeason = GetGlobalVar( "World.Season", "Summer" );
		if ( strcmp( szSeason.c_str(), "Summer" ) == 0 )
			eCurrSetting = ST_RAIN;
		else if ( strcmp( szSeason.c_str(), "Winter" ) == 0 )
			eCurrSetting = ST_SNOW;
		else if ( strcmp( szSeason.c_str(), "Africa" ) == 0 )
			eCurrSetting = ST_SAND;
		else
			eCurrSetting = ST_SNOW;
		if ( eCurrSetting == ST_SNOW )
			snowFlakes.resize( nMinSnowDensity );
		//eWeatherCondition = SC_NONE;
		bWeatherInitialized = true;
	}
	if ( nLastWeatherUpdate != 0 )
	{
		//rain
		if ( eCurrSetting == ST_RAIN && rainDrops.size() != nRainDensity && (eWeatherCondition == SC_STARTING || eWeatherCondition == SC_ON) )
		{
			rainDrops.resize( Clamp(int(rainDrops.size() + float(time - nLastWeatherUpdate) / fChangeSpeed * nRainDensity), 0, nRainDensity) );
			if ( IsSoundFinished( wAmbientID ) )
			{
				RemoveSound( wAmbientID );
				wAmbientID = AddSound( "Amb_Rain_circle", VNULL3, SFX_MIX_IF_TIME_EQUALS, SAM_LOOPED_NEED_ID );
				eWeatherCondition = SC_ON;
			}
		}
		else if ( eCurrSetting == ST_RAIN && rainDrops.size() != 0 && (eWeatherCondition == SC_FINISHING || eWeatherCondition == SC_NONE) )
		{
			if ( IsSoundFinished( wAmbientID ) && wAmbientID != 0 )
			{
				RemoveSound( wAmbientID );
				wAmbientID = 0;
				eWeatherCondition = SC_NONE;
			}
			rainDrops.resize( Clamp(int(rainDrops.size() - float(time - nLastWeatherUpdate) / fChangeSpeed * nRainDensity), 0, nRainDensity) );
		}
		for ( std::vector<SRainDrop>::iterator it = rainDrops.begin(); it != rainDrops.end(); ++it )
		{
			if ( it->fLength * vRainDir.z < -it->vPos.z || it->vPos.x < viewableTerrainRect.x1 || it->vPos.x > viewableTerrainRect.x2 || it->vPos.y < viewableTerrainRect.y1 || it->vPos.y > viewableTerrainRect.y2 )
				RandomizeRainDrop( *it );
			else
				it->fLength += time - nLastWeatherUpdate;
		}
		//snow
		if ( eCurrSetting == ST_SNOW && snowFlakes.size() != nMaxSnowDensity && (eWeatherCondition == SC_STARTING || eWeatherCondition == SC_ON) )
		{
			snowFlakes.resize( Clamp(int(snowFlakes.size() + float(time - nLastWeatherUpdate) / fChangeSpeed * (nMaxSnowDensity - nMinSnowDensity)), nMinSnowDensity, nMaxSnowDensity) );
		}
		else if ( eCurrSetting == ST_SNOW && snowFlakes.size() != nMinSnowDensity && (eWeatherCondition == SC_FINISHING || eWeatherCondition == SC_NONE) )
		{
			snowFlakes.resize( Clamp(int(snowFlakes.size() - float(time - nLastWeatherUpdate) / fChangeSpeed * (nMaxSnowDensity - nMinSnowDensity)), nMinSnowDensity, nMaxSnowDensity) );
		}
		CVec3 vSpeed = VNULL3;
		for ( std::vector<SSnowFlake>::iterator it = snowFlakes.begin(); it != snowFlakes.end(); ++it )
		{
			if ( it->vPos.z < 0 || it->vPos.x < viewableTerrainRect.x1 || it->vPos.x > viewableTerrainRect.x2 || it->vPos.y < viewableTerrainRect.y1 || it->vPos.y > viewableTerrainRect.y2 )
				RandomizeSnowFlake( *it );
			vSpeed.z = - fabs( sin( it->fPhase ) ) * fSnowFallingSpeed;
			vSpeed.x = cos( it->fPhase ) * fSnowAmplitude;
			it->fPhase += float(time - nLastWeatherUpdate) * fSnowFrequency;
			it->vPos += float(time - nLastWeatherUpdate) * vSpeed ;
		}
		//sand
		if ( eCurrSetting == ST_SAND && sandParticles.size() != nSandDensity && (eWeatherCondition == SC_STARTING || eWeatherCondition == SC_ON) )
		{
			sandParticles.resize( Clamp(int(sandParticles.size() + float(time - nLastWeatherUpdate) / fChangeSpeed * nSandDensity), 0, nSandDensity) );
		}
		else if ( eCurrSetting == ST_SAND && sandParticles.size() != 0 && (eWeatherCondition == SC_FINISHING || eWeatherCondition == SC_NONE) )
		{
			sandParticles.resize( Clamp(int(sandParticles.size() - float(time - nLastWeatherUpdate) / fChangeSpeed * nSandDensity), 0, nSandDensity) );
		}
		CVec2 vConeDir;
		float fRangeCoeff;
		if ( vSandCone.x < viewableTerrainRect.x1 || vSandCone.x > viewableTerrainRect.x2 || vSandCone.y < viewableTerrainRect.y1 || vSandCone.y > viewableTerrainRect.y2 || time - nLastConeGenerated > 20000 )
		{
			vSandCone.x = NWin32Random::Random( viewableTerrainRect.x1, viewableTerrainRect.x2 );
			vSandCone.y = NWin32Random::Random( viewableTerrainRect.y1, viewableTerrainRect.y2 );
			const float fAngle = NWin32Random::Random( 0.0f, 2.0f * float(PI) );
			vConeSpeed.x = cos( fAngle );
			vConeSpeed.y = sin( fAngle );
			nLastConeGenerated = time;
			for ( std::vector<SSandParticle>::iterator it = sandParticles.begin(); it != sandParticles.end(); ++it )
				it->bCone = false;
		}
		for ( std::vector<SSandParticle>::iterator it = sandParticles.begin(); it != sandParticles.end(); ++it )
		{
			if ( it->vPos.z > fSandHeight || it->vPos.z < 0 || it->vPos.x < viewableTerrainRect.x1 || it->vPos.x > viewableTerrainRect.x2 || it->vPos.y < viewableTerrainRect.y1 || it->vPos.y > viewableTerrainRect.y2 )
				RandomizeSand( *it );
			if ( it->bCone )
			{
				vConeDir.x = vSandCone.x - it->vPos.x;
				vConeDir.y = vSandCone.y - it->vPos.y;
				fRangeCoeff = 1.0f / (fabs( vConeDir ) - fConeRadius * (it->vPos.z / fSandHeight + 0.1f));
				fRangeCoeff = Clamp( fRangeCoeff, -0.01f, 0.01f ) * 5.0f;
				Normalize( &vConeDir );
				vSpeed.x = fRangeCoeff * vConeDir.x - fabs(fRangeCoeff) * vConeDir.y;
				vSpeed.y = fRangeCoeff * vConeDir.y + fabs(fRangeCoeff) * vConeDir.x;
				vSpeed.z = fRangeCoeff * 0.5f;
				it->vPos += ( float(time - nLastWeatherUpdate) * fSandSpeed ) * vSpeed;
			}
			else
			{
				vConeDir.x = vSandCone.x - it->vPos.x;
				vConeDir.y = vSandCone.y - it->vPos.y;
				fRangeCoeff = 1.0f / (fabs( vConeDir ) - fConeRadius * (it->vPos.z / fSandHeight + 0.1f));
				fRangeCoeff = Clamp( fRangeCoeff, -0.01f, 0.01f );
				Normalize( &vConeDir );
				vSpeed.x = fSandAmplitude * sin(it->vPhase.x) + fRangeCoeff * vConeDir.x - 5.0f * fabs(fRangeCoeff) * vConeDir.y;;
				vSpeed.y = fSandAmplitude * sin(it->vPhase.y) + fRangeCoeff * vConeDir.y + 5.0f * fabs(fRangeCoeff) * vConeDir.x;;
				vSpeed.z = fSandAmplitude * sin(it->vPhase.z) + fRangeCoeff * 0.5f;;
				vSpeed += vSandWind * ( Clamp(0.00001f / fabs2(fRangeCoeff), 0.0f, 1.0f) * 3.0f );
				if ( it->bConeDraw )
					vSpeed.z -= 0.02f;
				it->vPos += ( float(time - nLastWeatherUpdate) * fSandSpeed ) * vSpeed;
				it->vPhase.x += float(time - nLastWeatherUpdate) * fSandFrequency;
				it->vPhase.y += float(time - nLastWeatherUpdate) * fSandFrequency;
				it->vPhase.z += float(time - nLastWeatherUpdate) * fSandFrequency;
			}
		}
		vSandCone += ( float(time - nLastWeatherUpdate) * fSandConeSpeed ) * vConeSpeed;
		if ( wAmbientID != 0 && pSoundScene->GetMode() == ESSM_INGAME )
		{
			switch ( eCurrSetting )
			{
				case ST_RAIN:
					SetSoundPos( wAmbientID, CVec3((viewableTerrainRect.x1 + viewableTerrainRect.x2) * 0.5f, (viewableTerrainRect.y1 + viewableTerrainRect.y2) * 0.5f, 0.0f) );
					if ( nNextRandomSound < time && eWeatherCondition == SC_ON )
					{
						if ( nNextRandomSound != 0 )
							AddSound( "Amb_Rain", CVec3((viewableTerrainRect.x1 + viewableTerrainRect.x2) * 0.5f, (viewableTerrainRect.y1 + viewableTerrainRect.y2) * 0.5f, 0.0f), SFX_MIX_IF_TIME_EQUALS, SAM_ADD_N_FORGET );
						nNextRandomSound = time + NWin32Random::Random( 10 ) * 1000;
					}
					break;
				case ST_SAND:
					SetSoundPos( wAmbientID, CVec3( vSandCone, 0.0f ) );
					break;
				default:
					break;
			}
		}
	}
	nLastWeatherUpdate = time;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::FormVisibilityLists( ICamera *pCamera, ISceneVisitor *pVisitor )
{
	NTimer::STime time = pTimer->GetGameTime();
	// select visible patches
	CPatchesList patches;
	SelectPatches( pCamera, areaUnits.GetSizeX(), areaUnits.GetSizeY(), AREA_MAP_CELL_SIZE, &patches );
	// retrieve data from this patches
	const CTRect<short> rcScreen = pGFX->GetScreenRect();
	//weather
	if ( bWeatherOn && bEnableEffects )
	{
		viewableTerrainRect.x1 = patches.begin()->first;
		viewableTerrainRect.y1 = patches.begin()->second;
		viewableTerrainRect.x2 = patches.begin()->first;
		viewableTerrainRect.y2 = patches.begin()->second;
		for ( CPatchesList::const_iterator it = patches.begin(); it != patches.end(); ++it )
		{
			viewableTerrainRect.x1 = Min( viewableTerrainRect.x1, float(it->first) );
			viewableTerrainRect.y1 = Min( viewableTerrainRect.y1, float(it->second) );
			viewableTerrainRect.x2 = Max( viewableTerrainRect.x2, float(it->first) );
			viewableTerrainRect.y2 = Max( viewableTerrainRect.y2, float(it->second) );
		}
		viewableTerrainRect.x2 += 1.0f;
		viewableTerrainRect.y2 += 1.0f;
		viewableTerrainRect.x1 *= fCellSizeY * FP_SQRT_2 * STerrainPatchInfo::nSizeX;
		viewableTerrainRect.x2 *= fCellSizeY * FP_SQRT_2 * STerrainPatchInfo::nSizeX;
		viewableTerrainRect.y1 *= fCellSizeY * FP_SQRT_2 * STerrainPatchInfo::nSizeY;
		viewableTerrainRect.y2 *= fCellSizeY * FP_SQRT_2 * STerrainPatchInfo::nSizeY;
		UpdateWeather();
	}
	// units (dynamic)
	for ( CPatchesList::const_iterator pos = patches.begin(); pos != patches.end(); ++pos )
	{
		// alive units
		{
			CObjVisObjArea::CDataList &data = areaUnits[pos->second][pos->first];
			for ( CObjVisObjArea::CDataList::iterator it = data.begin(); it != data.end(); ++it )
			{
				if ( IsVisible(*it) )
				{
					(*it)->Update( time );
					(*it)->Visit( pVisitor, SGVOGT_UNIT );
				}
			}
		}
		// dead mesh units
		{
			CMeshesArea::CDataList &data = meshGraveyardArea[pos->second][pos->first];
			for ( CMeshesArea::CDataList::iterator it = data.begin(); it != data.end(); ++it )
			{
				(*it)->Update( time );
				(*it)->Visit( pVisitor, SGVOGT_UNIT );
			}
		}
		// sprite objects (static - don't need update!)
		{
			CSpritesArea::CDataList &data = spriteObjectsArea[pos->second][pos->first];
			for ( CSpritesArea::CDataList::iterator it = data.begin(); it != data.end(); ++it )
				(*it)->Visit( pVisitor, SGVOGT_OBJECT );
		}
		// terrain objects
		{
			CObjVisObjArea::CDataList &data = terraObjectsArea[pos->second][pos->first];
			for ( CObjVisObjArea::CDataList::iterator it = data.begin(); it != data.end(); ++it )
				(*it)->Visit( pVisitor, SGVOGT_TERRAOBJ );
		}
		// craters and shell-holes
		for ( CObjFixedArea::iterator it( areaCraters.Iterate(pos->first, pos->second) ); !it.IsEnd(); it.Next() )
		{
#ifdef _DO_ASSERT_SLOW
			IObjVisObj *pObj = it;
			NI_ASSERT_SLOW_T( pObj != 0, "NULL object in area craters" );
#endif // _DO_ASSERT_SLOW
			it->Visit( pVisitor, SGVOGT_TERRAOBJ );
		}
		// shadow objects
		{
			CSpritesArea::CDataList &data = shadowObjectsArea[pos->second][pos->first];
			for ( CSpritesArea::CDataList::iterator it = data.begin(); it != data.end(); ++it )
				(*it)->Visit( pVisitor, SGVOGT_SHADOW );
		}
		// mech trace objects
		// CRAP{ к сожалению нет времени на хорошее продумывание...
		{
			CMechTraceArea::CDataList &data = mechTracesArea[pos->second][pos->first];
			for ( CMechTraceArea::CDataList::iterator it = data.begin(); it != data.end(); )
			{
				if ( UpdateMechTrace(&(*it), time) == false )
					it = data.erase( it );
				else
				{
					pVisitor->VisitMechTrace( *it );
					++it;
				}
			}
		}
		// CRAP}
		// gun trace objects
		// CRAP{ к сожалению нет времени на хорошее продумывание...
		{
			CGunTraceArea::CDataList &data = gunTracesArea[pos->second][pos->first];
			for ( CGunTraceArea::CDataList::iterator it = data.begin(); it != data.end(); )
			{
				if ( UpdateGunTrace(&(*it), time, fTraceLen) == false )
					it = data.erase( it );
				else
				{
					pVisitor->VisitGunTrace( *it );
					++it;
				}
			}
		}
		// CRAP}
	}
	// effects (dynamic)
	if ( bEnableEffects )
	{
		for ( CPatchesList::const_iterator pos = patches.begin(); pos != patches.end(); ++pos )
		{
			CVisObjArea::CDataList &data = effectsArea[pos->second][pos->first];
			for ( CVisObjArea::CDataList::iterator it = data.begin(); it != data.end();  )
			{
				// update. remove exhausted effects
				if ( (*it)->Update( time ) == false )
				{
					objdescs.erase( *it );
					it = data.erase( it );
					continue;
				}
				else
				{
					(*it)->Visit( pVisitor );
					++it;
				}
			}
		}
	}
	// outbound sprites
	for ( CSpritesObjList::iterator it = outboundSprites.begin(); it != outboundSprites.end(); ++it )
	{
		if ( IsVisible(*it) )
		{
			(*it)->Update( time );
			(*it)->Visit( pVisitor, SGVOGT_UNIT );
		}
	}
	// outbound mesh objects
	for ( CMeshObjList::iterator it = outboundObjects.begin(); it != outboundObjects.end(); ++it )
	{
		(*it)->Update( time );
		(*it)->Visit( pVisitor, -1 );
	}
	// outbound objects 2
	for ( CMeshObjList::iterator it = outboundObjects2.begin(); it != outboundObjects2.end(); ++it )
	{
		if ( IsVisible(*it) )
		{
			(*it)->Update( time );
			(*it)->Visit( pVisitor, SGVOGT_UNIT );
		}
	}
	// outbound effects
	for ( CEffectObjList::iterator it = outboundEffects.begin(); it != outboundEffects.end(); )
	{
		// update. remove exhausted effects
		if ( (*it)->Update( time ) == false )
		{
			objdescs.erase( *it );
			it = outboundEffects.erase( it );
		}
		else
		{
			(*it)->Visit( pVisitor );
			++it;
		}
	}
	// line objects
	for ( std::list< CPtr<IBoldLineVisObj> >::iterator it = boldLines.begin(); it != boldLines.end(); ++it )
		(*it)->Visit( pVisitor );
	// UI screens
	if ( bShowUI ) 
	{
		for ( std::list< CPtr<IUIScreen> >::iterator it = uiScreens.begin(); it != uiScreens.end(); ++it )
			(*it)->Visit( pVisitor );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScene::SelectPatches( ICamera *pCamera, float fPatchesX, float fPatchesY, float fPatchSize, CPatchesList *pPatches )
{
	const CVec3 vCamera = pCamera->GetAnchor();
	//
	const float fPatchHalfAxis = fPatchSize * FP_SQRT_2 / 2.0f; //fCellSizeX * STerrainPatchInfo::nSizeX;
	// выделить патчи, которые попадают в обзор
	// это базисные линии (X, Y) системы координат ландшафта
	CVec3 vAxisX, vAxisY;
	GetLineEq( 0, 0, 1, 0, &vAxisX.x, &vAxisX.y, &vAxisX.z );
	GetLineEq( 0, 1, 0, 0, &vAxisY.x, &vAxisY.y, &vAxisY.z );
	//
	const RECT rcScreen = pGFX->GetScreenRect();
	// half-width and half-height
	const float fWidth = ( rcScreen.right - rcScreen.left ) / 2;
	const float fHeight = rcScreen.bottom - rcScreen.top;					// height * 2 due to camera yaw = 30 degrees
	// оси камеры в мировой системе координат:
	CVec2 vCameraX( fWidth / FP_SQRT_2, fWidth / FP_SQRT_2 ), vCameraY( -fHeight / FP_SQRT_2, fHeight / FP_SQRT_2 );
	//
	//
	// определим грубый прямоугольник (в мировой системе координат, в целых патчах), в который вписывается экран
	// определение производим на основании расстояния от углов экрана до координатных осей мировой системы 
	// NOTE: границы по принципу [min, max)
	const CVec2 vCameraO( vCamera.x, vCamera.y );
	CTRect<int> rcL0Rect;								// level 0 of roughness rect
	{
		// LT
		const CVec2 point = vCameraO + vCameraY - vCameraX;
		const float fDist = vAxisY.x*point.x + vAxisY.y*point.y + vAxisY.z;
		rcL0Rect.minx = int( Clamp( fDist / fPatchSize, 0.0f, fPatchesX ) );
	}
	{
		// RT
		const CVec2 point = vCameraO + vCameraY + vCameraX;
		const float fDist = vAxisX.x*point.x + vAxisX.y*point.y + vAxisX.z;
		rcL0Rect.miny = int( Clamp( fDist / fPatchSize, 0.0f, fPatchesY ) );
	}
	{
		// RB
		const CVec2 point = vCameraO + vCameraX - vCameraY;
		const float fDist = vAxisY.x*point.x + vAxisY.y*point.y + vAxisY.z;
		rcL0Rect.maxx = int( Clamp( fDist / fPatchSize, 0.0f, fPatchesX ) );
	}
	{
		// LB
		const CVec2 point = vCameraO - vCameraY - vCameraX;
		const float fDist = vAxisX.x*point.x + vAxisX.y*point.y + vAxisX.z;
		rcL0Rect.maxy = int( Clamp( fDist / fPatchSize, 0.0f, fPatchesY ) );
	}
	rcL0Rect.Normalize();
	//
	//
	// найдём расстояние (в патчах) от нуля мира до горизонтальных границ экрана (по Y) и до вертикальных (по X)
	CTRect<int> rcL1Rect;								// level 1 of roughness rect
	const CVec3 vTerraOX( 0, 0, 0 );
	const CVec3 vTerraOY( 0, fPatchesY * fPatchSize, 0 );
	//
	{
		// miny
		const CVec2 vO = vCameraO + vCameraY;
		CVec3 vLine;
		GetLineEq( vO.x, vO.y, vO.x + 1000, vO.y + 1000, &vLine.x, &vLine.y, &vLine.z );
		const int nDist = int( ( vLine.x*vTerraOY.x + vLine.y*vTerraOY.y + vLine.z ) / fPatchHalfAxis );
		rcL1Rect.miny = nDist;
	}
	{
		// maxy
		const CVec2 vO = vCameraO - vCameraY;
		CVec3 vLine;
		GetLineEq( vO.x, vO.y, vO.x + 1000, vO.y + 1000, &vLine.x, &vLine.y, &vLine.z );
		const int nDist = int( ( vLine.x*vTerraOY.x + vLine.y*vTerraOY.y + vLine.z ) / fPatchHalfAxis );
		rcL1Rect.maxy = nDist;
	}
	{
		// minx
		const CVec2 vO = vCameraO - vCameraX;
		CVec3 vLine;
		GetLineEq( vO.x, vO.y, vO.x - 1000, vO.y + 1000, &vLine.x, &vLine.y, &vLine.z );
		const int nDist = int( ( vLine.x*vTerraOX.x + vLine.y*vTerraOX.y + vLine.z ) / fPatchHalfAxis );
		rcL1Rect.minx = nDist;
	}
	{
		// maxx
		const CVec2 vO = vCameraO + vCameraX;
		CVec3 vLine;
		GetLineEq( vO.x, vO.y, vO.x - 1000, vO.y + 1000, &vLine.x, &vLine.y, &vLine.z );
		const int nDist = int( ( vLine.x*vTerraOX.x + vLine.y*vTerraOX.y + vLine.z ) / fPatchHalfAxis );
		rcL1Rect.maxx = nDist;
	}
	rcL1Rect.Normalize();
	//
	// теперь из полученного прямоугольника (rcL0Rect) проверим все патчи на предмет следующих условий:
	// сумма индексов патча (x, y) должна быть >= rcL1Rect.miny, <= rcL1Rect.maxy
	// сумма индексов патча (x, y - num patches Y) должна быть >= rcL1Rect.minx, <= rcL1Rect.maxx
	rcL0Rect.x1 = Clamp( rcL0Rect.x1, 0, int(fPatchesX) );
	rcL0Rect.y1 = Clamp( rcL0Rect.y1, 0, int(fPatchesY) );
	rcL0Rect.x2 = Clamp( rcL0Rect.x2 + 3, 0, int(fPatchesX) );
	rcL0Rect.y2 = Clamp( rcL0Rect.y2 + 3, 0, int(fPatchesY) );
	int nVertDist = fPatchesY;
	for ( int j = rcL0Rect.miny; j < rcL0Rect.maxy; ++j )
	{
		for ( int i = rcL0Rect.minx; i < rcL0Rect.maxx; ++i )
		{
			const int nSumY = i + ( nVertDist - j );
			if ( (nSumY < rcL1Rect.miny) || (nSumY > rcL1Rect.maxy + 2) )
				continue;
			const int nSumX = i + j;
			if ( (nSumX < rcL1Rect.minx - 2) || (nSumX > rcL1Rect.maxx + 2) )
				continue;
			pPatches->push_back( std::pair<int, int>( i, j ) );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
