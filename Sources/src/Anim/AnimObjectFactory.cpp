#include "StdAfx.h"

#include "AnimObjectFactory.h"

#include "AnimationManager.h"
#include "SpriteAnimation.h"
#include "MeshAnimation.h"

#include "MatrixEffectorJogging.h"
#include "MatrixEffectorLeveling.h"
static CAnimObjectFactory theAnimObjectFactory;
CAnimObjectFactory::CAnimObjectFactory()
{
	REGISTER_CLASS( this, ANIM_ANIMATION_MANAGER, CAnimationManager );
	REGISTER_CLASS( this, ANIM_SPRITE_ANIMATION, CSpriteAnimation );
	REGISTER_CLASS( this, ANIM_SPRITE_ANIMATION_FORMAT, SSpriteAnimationFormat );
	REGISTER_CLASS( this, ANIM_COMPLEX_SPRITE, CComplexSprite );
	REGISTER_CLASS( this, ANIM_COMPLEX_SPRITE_FORMAT, SSpritesPack );
	REGISTER_CLASS( this, ANIM_MESH_ANIMATION, CMeshAnimation );
	REGISTER_CLASS( this, ANIM_MESH_SKELETON, CMeshSkeleton );
	REGISTER_CLASS( this, ANIM_MESH_SKELETON_DATA, SMeshSkeletonData );
	REGISTER_CLASS( this, ANIM_MESH_ANIMATION_DATA, SMeshAnimationData );
	REGISTER_CLASS( this, ANIM_EFFECTOR_JOGGING, CMatrixEffectorJogging );
	REGISTER_CLASS( this, ANIM_EFFECTOR_LEVELING, CMatrixEffectorLeveling );
}
static SModuleDescriptor theModuleDescriptor( "Animation", ANIM_ANIM, 0x0100, &theAnimObjectFactory, 0 );
const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
