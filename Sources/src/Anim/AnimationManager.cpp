#include "StdAfx.h"

#include "AnimationManager.h"

#include "SpriteAnimation.h"
#include "MeshAnimation.h"
ISpriteAnimation* CAnimationManager::GetSpriteAnimation( const char *pszName )
{
	const std::string szName = pszName;
	if ( SSpriteAnimationFormat *pAnimations = sprites.Get(szName) )
	{
		CSpriteAnimation *pAnim = new CSpriteAnimation();
		pAnim->Init( pAnimations );
		return pAnim;
	}
	else if ( SSpritesPack *pSprites = complexsprites.Get(szName) )
	{
		CComplexSprite *pAnim = new CComplexSprite();
		pAnim->Init( pSprites );
		return pAnim;
	}
	return 0;
}
IMeshAnimation* CAnimationManager::GetMeshAnimation( const char *pszName )
{
	std::string szName = pszName;
	SMeshSkeletonData *pSkeletonData = skeletons.Get( szName );
	if ( pSkeletonData == 0 )
		return 0;
	SMeshAnimationData *pMeshAnimation = meshanims.Get( szName );
	CMeshSkeleton *pSkeleton = CreateObject<CMeshSkeleton>( ANIM_MESH_SKELETON );
	pSkeleton->Init( pSkeletonData );
	CMeshAnimation *pAnim = CreateObject<CMeshAnimation>( ANIM_MESH_ANIMATION );
	pAnim->Init( pSkeleton, pMeshAnimation );
	return pAnim;
}
void CAnimationManager::Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount )
{ 
	if ( eMode == ISharedManager::CLEAR_ALL ) 
	{
		sprites.Clear(); 
		complexsprites.Clear(); 
		skeletons.Clear(); 
		meshanims.Clear();
	}
	else
	{
		sprites.ClearUnreferencedResources(); 
		complexsprites.ClearUnreferencedResources(); 
		skeletons.ClearUnreferencedResources(); 
		meshanims.ClearUnreferencedResources(); 
	}
}
int CAnimationManager::operator&( IStructureSaver &ss )
{
	sprites.Serialize( &ss );
	skeletons.Serialize( &ss );
	meshanims.Serialize( &ss );
	complexsprites.Serialize( &ss );
	return 0;
}
