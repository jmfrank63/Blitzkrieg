#include "StdAfx.h"

#include "VisObjBuilder.h"

#include "SpriteVisObj.h"
#include "MeshVisObj.h"
#include "EffectVisObj.h"
#include "SquadVisObj.h"
#include "FlashVisObj.h"

#include "IconPic.h"
#include "IconHPBar.h"

#include "../Common/Icons.h"
bool CVisObjBuilder::Init( ISingleton *pSingleton )
{
	pGFX = GetSingleton<IGFX>( pSingleton );
	pTM = GetSingleton<ITextureManager>( pSingleton );
	pMM = GetSingleton<IMeshManager>( pSingleton );
	pAM = GetSingleton<IAnimationManager>( pSingleton );
	pPM = GetSingleton<IParticleManager>( pSingleton );
	pStorage = GetSingleton<IDataStorage>( pSingleton );

	return true;
}
IVisObj* CVisObjBuilder::BuildObject( const char *pszModelName, const char *pszTextureName, EObjVisType type )
{
	switch ( type )
	{
		case SGVOT_MESH:
			return CreateMeshVisObj( pszModelName, pszTextureName != 0 ? pszTextureName : pszModelName );
		case SGVOT_SPRITE:
			return CreateSpriteVisObj( pszModelName );
		case SGVOT_EFFECT:
			return CreateEffectVisObj( pszModelName );
		case SGVOT_FLASH:
			return CreateFlashVisObj( pszModelName );
		case SGVOT_UNKNOWN:
			return CreateSpriteVisObj( pszModelName );
	}
	return 0;
}
CFlashVisObj* CVisObjBuilder::CreateFlashVisObj( const std::string &szName )
{
	CFlashVisObj *pObj = CreateObject<CFlashVisObj>( SCENE_VISOBJ_FLASH );
	IGFXTexture *pTexture = pTM->GetTexture( szName.c_str() );
	pObj->SetTexture( pTexture );
	return pObj;
}
CSpriteVisObj* CVisObjBuilder::CreateSpriteVisObj( const std::string &szName )
{
	CSpriteVisObj *pObj = new CSpriteVisObj();
	if ( ChangeObject(pObj, szName.c_str(), 0, SGVOT_SPRITE) )
		return pObj;
	pObj->AddRef();
	pObj->Release();
	return 0;
}
CMeshVisObj* CVisObjBuilder::CreateMeshVisObj( const char *pszModelName, const char *pszTextureName )
{
	CMeshVisObj *pObj = new CMeshVisObj();
	if ( ChangeObject(pObj, pszModelName, pszTextureName, SGVOT_MESH) )
		return pObj;
	pObj->AddRef();
	pObj->Release();
	return 0;
}
const char* CVisObjBuilder::GetEffectSound( const std::string &szName )
{
	CEffectDescMap::iterator pos = effectDescs.find( szName );
	if ( pos == effectDescs.end() )
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( (szName + ".xml").c_str(), STREAM_ACCESS_READ );
		if ( pStream )
		{
			CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ, "effect" );
			tree.Add( "effect", &effectDescs[szName] );
			pos = effectDescs.find( szName );
		}
		else
			return 0;
	}
	return pos->second.szSound.c_str();
}
CEffectVisObj* CVisObjBuilder::CreateEffectVisObj( const std::string &szName )
{
	CEffectDescMap::iterator pos = effectDescs.find( szName );
	if ( pos == effectDescs.end() )
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( (szName + ".xml").c_str(), STREAM_ACCESS_READ );
		if ( pStream )
		{
			CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ, "effect" );
			tree.Add( "effect", &effectDescs[szName] );
			pos = effectDescs.find( szName );
		}
		else
			return 0;
	}
	SEffectDesc &effect = pos->second;
	CEffectVisObj *pEffect = new CEffectVisObj( effect.szSound );
	for ( SEffectDesc::CSpritesList::const_iterator it = effect.sprites.begin(); it != effect.sprites.end(); ++it )
	{
		CSpriteVisObj *pSprite = CreateSpriteVisObj( (it->szPath + "\\1").c_str() );
		pEffect->AddSpriteEffect( pSprite, it->nStart, it->nRepeat, it->vPos );
	}
	
	for ( SEffectDesc::CParticlesList::iterator it = effect.particles.begin(); it != effect.particles.end(); ++it )
	{
		IParticleSource *pPS = pPM->GetKeyBasedSource( (it->szPath + ".xml").c_str() );
		if ( pPS )
		{
			pPS->SetPos( VNULL3 );
			pPS->SetScale( it->fScale );
			pEffect->AddParticleEffect( pPS, it->nStart, it->nDuration, it->vPos );
		}
	}
	for ( SEffectDesc::CSmokinParticlesList::iterator it = effect.smokinParticles.begin(); it != effect.smokinParticles.end(); ++it )
	{
		IParticleSource *pPS = pPM->GetSmokinParticleSource( (it->szPath + ".xml").c_str() );
		if ( pPS )
		{
			pPS->SetPos( VNULL3 );
			pPS->SetScale( it->fScale );
			pEffect->AddParticleEffect( pPS, it->nStart, it->nDuration, it->vPos );
		}
	}
	pEffect->SetEffectDirection( MONE );
	return pEffect;
}
bool CVisObjBuilder::ChangeObject( IVisObj *pObj, const char *pszModelName, const char *pszTextureName, EObjVisType type )
{
	if ( type == SGVOT_MESH )
	{
		CMeshVisObj *pVO = static_cast<CMeshVisObj*>( pObj );
		if ( (pszModelName != 0) && (pszTextureName != 0) )
		{
			const std::string szModelName = pszModelName;
			IGFXMesh *pMesh = pMM->GetMesh( (szModelName + ".mod").c_str() );
			IMeshAnimation *pAnimation = pAM->GetMeshAnimation( (szModelName + ".mod").c_str() );
			if ( (pMesh != 0) && (pAnimation != 0) )
			{
				IGFXTexture *pTexture = pTM->GetTexture( pszTextureName );
				pVO->Init( pMesh, pAnimation, pTexture );
				return true;
			}
		}
		else if ( pszModelName == 0 )
		{
			IGFXTexture *pTexture = pTM->GetTexture( pszTextureName );
			pVO->SetTexture( pTexture );
			return true;
		}
		else if ( pszTextureName == 0 )
		{
			const std::string szModelName = pszModelName;
			IGFXMesh *pMesh = pMM->GetMesh( (szModelName + ".mod").c_str() );
			IMeshAnimation *pAnimation = pAM->GetMeshAnimation( (szModelName + ".mod").c_str() );
			if ( (pMesh != 0) && (pAnimation != 0) )
			{
				pVO->SetMeshAnim( pMesh, pAnimation );
				return true;
			}
		}
	}
	else if ( type == SGVOT_SPRITE ) 
	{
		CSpriteVisObj *pVO = static_cast<CSpriteVisObj*>( pObj );

		const std::string szName = pszTextureName == 0 ? pszModelName : pszTextureName;
		ISpriteAnimation *pAnimation = pAM->GetSpriteAnimation( (szName + ".san").c_str() );
		if ( pAnimation != 0 )
		{
			IGFXTexture *pTexture = pTM->GetTexture( szName.c_str() );
			int nDirection = 0;
			ISpriteAnimation *pOldAnim = static_cast<ISpriteAnimation*>( pVO->GetAnimation() );
			if ( pOldAnim ) 
			{
				nDirection = pVO->GetDirection();
				if ( pAnimation->SetAnimation( pOldAnim->GetAnimation() ) == false ) 
					pAnimation->SetAnimation( 0 );
				pAnimation->SetScaleTimer( pOldAnim->GetScaleTimer() );
				pAnimation->SetFrameIndex( pOldAnim->GetFrameIndex() );
			}
			pAnimation->SetDirection( nDirection & 0x0000ffff );
			pVO->Init( pTexture, pAnimation );
			pVO->SetDirection( nDirection );
			if ( pOldAnim == 0 ) 
			{
				pVO->SetAnimation( 0 );
				pVO->Update( 0, true );
			}
			return true;
		}
	}
	return false;
}
ISceneObject* CVisObjBuilder::BuildSceneObject( const char *pszName, ESceneObjectType eType, int nSubtype )
{
	switch( eType ) 
	{
		case SCENE_OBJECT_TYPE_ICON:
			if ( nSubtype == ICON_HP_BAR ) 
			{
				CIconHPBar *pIcon = CreateObject<CIconHPBar>( SCENE_ICON_HP_BAR );
				IGFXTexture *pTexture = pTM->GetTexture( pszName );
				pIcon->Init( pTexture );
				return pIcon;
			}
			else
			{
				ISceneIconPic *pIcon = CreateObject<ISceneIconPic>( SCENE_ICON_PIC );
				IGFXTexture *pTexture = pTM->GetTexture( pszName );
				pIcon->SetTexture( pTexture );
				const float fSizeX = pTexture->GetSizeX( 0 );
				const float fSizeY = pTexture->GetSizeY( 0 );
				const CTRect<short> rcSprite( -fSizeX/2, -fSizeY, fSizeX/2, 0 );
				const CTRect<float> rcMaps( 0, 0, 1.0f + 0.5f/fSizeX, 1.0f + 0.5f/fSizeY );
				pIcon->SetRect( rcSprite, rcMaps );
				return pIcon;
			}
			break;
		case SCENE_OBJECT_TYPE_SQUAD:
			{
				CSquadVisObj *pSquad = CreateObject<CSquadVisObj>( SCENE_VISOBJ_SQUAD );
				IGFXTexture *pTexture = pTM->GetTexture( pszName );
				pSquad->SetIcon( pTexture );
				return pSquad;
			}
			break;
		default:
			NI_ASSERT_T( false, NStr::Format("Unknown object of type %d to build", eType) );
	}
	return 0;
}
bool CVisObjBuilder::ChangeSceneObject( ISceneObject *pObj, const char *pszName, ESceneObjectType eType, int nSubtype )
{
	if ( pObj == 0 ) 
		return false;
	ISceneIconPic *pIcon = dynamic_cast<ISceneIconPic*>( pObj );
	if ( pIcon == 0 ) 
		return false;
	if ( IGFXTexture *pTexture = pTM->GetTexture(pszName) )
	{
		pIcon->SetTexture( pTexture );
		const float fSizeX = pTexture->GetSizeX( 0 );
		const float fSizeY = pTexture->GetSizeY( 0 );
		const CTRect<short> rcSprite( -fSizeX/2, -fSizeY, fSizeX/2, 0 );
		const CTRect<float> rcMaps( 0, 0, 1.0f + 0.5f/fSizeX, 1.0f + 0.5f/fSizeY );
		pIcon->SetRect( rcSprite, rcMaps );
		return true;
	}
	return false;
}
