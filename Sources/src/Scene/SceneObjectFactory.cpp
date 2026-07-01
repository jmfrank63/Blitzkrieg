#include "StdAfx.h"

#include "SceneObjectFactory.h"

#include "SpriteVisObj.h"
#include "MeshVisObj.h"
#include "EffectVisObj.h"
#include "FlashVisObj.h"
#include "SceneInternal.h"
#include "VisObjBuilder.h"
#include "Camera.h"
#include "Cursor.h"
#include "IconBar.h"
#include "IconText.h"
#include "IconPic.h"
#include "IconHPBar.h"
#include "BoldLineVisObj.h"
#include "MatrixEffector.h"
#include "MaterialEffector.h"
#include "SquadVisObj.h"
#include "SoundScene.h"
#include "VideoPlayer.h"
#include "Transition.h"
#include "ParticleManager.h"
#include "KeyBasedParticleSource.h"
#include "SmokinParticleSource.h"
#include "SmokinParticleSourceData.h"
#include "TerrainInternal.h"
#include "GammaEffect.h"
static CSceneObjectFactory theSceneObjectFactory;
CSceneObjectFactory::CSceneObjectFactory()
{
	REGISTER_CLASS( this, SCENE_SCENE, CScene );
	REGISTER_CLASS( this, SCENE_VISOBJ_BUILDER, CVisObjBuilder );
	REGISTER_CLASS( this, SCENE_CAMERA, CCamera );
	REGISTER_CLASS( this, SCENE_CURSOR, CCursor );
	
	REGISTER_CLASS( this, SCENE_VISOBJ_SPRITE, CSpriteVisObj );
	REGISTER_CLASS( this, SCENE_VISOBJ_MESH, CMeshVisObj );
	REGISTER_CLASS( this, SCENE_VISOBJ_EFFECT, CEffectVisObj );
	REGISTER_CLASS( this, SCENE_VISOBJ_SQUAD, CSquadVisObj );
	REGISTER_CLASS( this, SCENE_VISOBJ_FLASH, CFlashVisObj );

	REGISTER_CLASS( this, SCENE_ICON_BAR, CIconBar );
	REGISTER_CLASS( this, SCENE_ICON_TEXT, CIconText );
	REGISTER_CLASS( this, SCENE_ICON_PIC, CIconPic );
	REGISTER_CLASS( this, SCENE_ICON_HP_BAR, CIconHPBar );
	
	REGISTER_CLASS( this, SCENE_BOLD_LINE, CBoldLineVisObj );

	REGISTER_CLASS( this, SCENE_EFFECTOR_RECOIL, CMatrixEffectorRecoil );
	REGISTER_CLASS( this, SCENE_EFFECTOR_JOGGING, CMatrixEffectorJogging );
	REGISTER_CLASS( this, SCENE_EFFECTOR_MATERIAL, CMaterialEffector );
	
	REGISTER_CLASS( this, SCENE_SOUNDSCENE, CSoundScene );
	REGISTER_CLASS( this, SCENE_SOUNDSCENE_SOUND, CSoundScene::CSound );
	REGISTER_CLASS( this, SCENE_SOUNDSCENE_SUBSTSOUND, CSoundScene::CSubstSound );
	REGISTER_CLASS( this, SCENE_SOUNDSCENE_PLAYLIST, CSoundScene::CPlayList );
	REGISTER_CLASS( this, SCENE_SOUNDSCENE_SOUNDCELL, CSoundScene::CSoundCell );
	REGISTER_CLASS( this, SCENE_VIDEO_PLAYER, CVideoPlayer );
	REGISTER_CLASS( this, SCENE_TRANSITION, CTransition );
	REGISTER_CLASS( this, SCENE_GAMMA_EFFECT, CGammaEffect );
	REGISTER_CLASS( this, SCENE_GAMMA_FADER, CGammaFader );
	REGISTER_CLASS( this, PFX_MANAGER, CParticleDataManager );
	REGISTER_CLASS( this, PFX_KEYBASED, CKeyBasedParticleSource );
	REGISTER_CLASS( this, PFX_KEYDATA, SParticleSourceData );
	REGISTER_CLASS( this, PFX_COMPLEX_SOURCE, CSmokinParticleSource );
	REGISTER_CLASS( this, PFX_COMPLEX_KEYDATA, SSmokinParticleSourceData );

	REGISTER_CLASS( this, TERRAIN_TERRAIN, CTerrain );
}
static SModuleDescriptor theModuleDescriptor( "Scene", SCENE_SCENE, 0x0100, &theSceneObjectFactory, 0 );
const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
