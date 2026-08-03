#ifndef __ANIMATIONMANAGER_H__
#define __ANIMATIONMANAGER_H__
#include "../Misc/BasicShare.h"
#include "MeshAnimation.h"
BASIC_SHARE_DECLARE( CSpriteAnimationShare, std::string, SSpriteAnimationFormat, ANIM_SPRITE_ANIMATION_FORMAT, 101, "" );
BASIC_SHARE_DECLARE( CComplexSpriteShare  , std::string, SSpritesPack          , ANIM_COMPLEX_SPRITE_FORMAT  , 114, "" );
BASIC_SHARE_DECLARE( CMeshSkeletonShare   , std::string, SMeshSkeletonData     , ANIM_MESH_SKELETON_DATA     , 102, "" );
BASIC_SHARE_DECLARE( CMeshAnimationShare  , std::string, SMeshAnimationData    , ANIM_MESH_ANIMATION_DATA    , 103, "" );
class CAnimationManager : public IAnimationManager
{
	OBJECT_COMPLETE_METHODS( CAnimationManager );
	DECLARE_SERIALIZE;
	CSpriteAnimationShare sprites;
	CComplexSpriteShare complexsprites;
	CMeshSkeletonShare skeletons;
	CMeshAnimationShare meshanims;
public:
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) 
	{ 
		sprites.SetSerialMode( eSerialMode );
		complexsprites.SetSerialMode( eSerialMode );
		skeletons.SetSerialMode( eSerialMode );
		meshanims.SetSerialMode( eSerialMode );
	}
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode )
	{
		sprites.SetShareMode( eShareMode );
		complexsprites.SetShareMode( eShareMode );
		skeletons.SetShareMode( eShareMode );
		meshanims.SetShareMode( eShareMode );
	}
	virtual void STDCALL Clear( const ISharedManager::EClearMode eMode, const int nUsage, const int nAmount );
	virtual bool STDCALL Init() 
	{ 
		sprites.Init(); 
		complexsprites.Init(); 
		skeletons.Init(); 
		meshanims.Init();
		return true; 
	}
	virtual ISpriteAnimation* STDCALL GetSpriteAnimation( const char *pszName );
	virtual IMeshAnimation* STDCALL GetMeshAnimation( const char *pszName );
};
#endif // __ANIMATIONMANAGER_H__
