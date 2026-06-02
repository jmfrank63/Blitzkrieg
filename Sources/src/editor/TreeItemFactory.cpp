#include "StdAfx.h"
#include "localization.h"
#include "TreeItemFactory.h"
#include "TreeItem.h"
#include "AnimTreeItem.h"
#include "SpriteTreeItem.h"
#include "EffTreeItem.h"
#include "ObjTreeItem.h"
#include "MeshTreeItem.h"
#include "WeaponTreeItem.h"
#include "BuildTreeItem.h"
#include "TileTreeItem.h"
#include "FenceTreeItem.h"
#include "ParticleTreeItem.h"
#include "TrenchTreeItem.h"
#include "SquadTreeItem.h"
#include "GUITreeItem.h"
#include "MineTreeItem.h"
#include "BridgeTreeItem.h"
#include "MissionTreeItem.h"
#include "ChapterTreeItem.h"
#include "CampaignTreeItem.h"
#include "3dRoadTreeItem.h"
#include "3dRiverTreeItem.h"
#include "MedalTreeItem.h"


CTreeItemObjectFactory theTreeItemObjectFactory;
IObjectFactory* STDCALL GetTreeItemObjectFactory()
{
	return &theTreeItemObjectFactory;
}

CTreeItemObjectFactory::CTreeItemObjectFactory()
{
	REGISTER_CLASS( this, E_ANIMATION_ROOT_ITEM,				CAnimationTreeRootItem );
	REGISTER_CLASS( this, E_UNIT_COMMON_PROPS_ITEM,			CUnitCommonPropsItem );
	REGISTER_CLASS( this, E_LOCALIZATION_ITEM,					CLocalizationItem );
	REGISTER_CLASS( this, E_UNIT_AI_PROPS_ITEM,					CUnitAIPropsItem );
	REGISTER_CLASS( this, E_UNIT_WEAPON_PROPS_ITEM,			CUnitWeaponPropsItem );
	REGISTER_CLASS( this, E_UNIT_GRENADE_PROPS_ITEM,		CUnitGrenadePropsItem );
	REGISTER_CLASS( this, E_UNIT_DIRECTORY_PROPS_ITEM,	CDirectoryPropsItem );
	REGISTER_CLASS( this, E_UNIT_SEASON_PROPS_ITEM,			CUnitSeasonPropsItem );
	REGISTER_CLASS( this, E_UNIT_DIRECTORIES_ITEM,			CDirectoriesItem );
	REGISTER_CLASS( this, E_UNIT_ANIMATIONS_ITEM,				CUnitAnimationsItem );
	REGISTER_CLASS( this, E_UNIT_ANIMATION_PROPS_ITEM,	CUnitAnimationPropsItem );
	REGISTER_CLASS( this, E_UNIT_FRAME_PROPS_ITEM,			CUnitFramePropsItem );
	REGISTER_CLASS( this, E_UNIT_ACTIONS_ITEM,					CUnitActionsItem );
	REGISTER_CLASS( this, E_UNIT_ACTION_PROPS_ITEM,			CUnitActionPropsItem );
	REGISTER_CLASS( this, E_UNIT_EXPOSURES_ITEM,				CUnitExposuresItem );
	REGISTER_CLASS( this, E_UNIT_ACKS_ITEM,							CUnitAcksItem );
	REGISTER_CLASS( this, E_UNIT_ACK_TYPES_ITEM,				CUnitAckTypesItem );
	REGISTER_CLASS( this, E_UNIT_ACK_TYPE_PROPS_ITEM,		CUnitAckTypePropsItem );
	
	REGISTER_CLASS( this, E_SPRITE_ROOT_ITEM,						CSpriteTreeRootItem );
	REGISTER_CLASS( this, E_SPRITE_PROPS_ITEM,					CSpritePropsItem );
	REGISTER_CLASS( this, E_SPRITES_ITEM,								CSpritesItem );
	
	REGISTER_CLASS( this, E_EFFECT_ROOT_ITEM,						CEffectTreeRootItem );
	REGISTER_CLASS( this, E_EFFECT_COMMON_PROPS_ITEM,		CEffectCommonPropsItem );
	REGISTER_CLASS( this, E_EFFECT_ANIMATIONS_ITEM,			CEffectAnimationsItem );
	REGISTER_CLASS( this, E_EFFECT_MESHES_ITEM,					CEffectMeshesItem );
	REGISTER_CLASS( this, E_EFFECT_FUNC_PARTICLES_ITEM,	CEffectFuncParticlesItem );
	REGISTER_CLASS( this, E_EFFECT_MAYA_PARTICLES_ITEM,	CEffectMayaParticlesItem );
	REGISTER_CLASS( this, E_EFFECT_LIGHTS_ITEM,					CEffectLightsItem );
	REGISTER_CLASS( this, E_EFFECT_ANIMATION_PROPS_ITEM,CEffectAnimationPropsItem );
	REGISTER_CLASS( this, E_EFFECT_MESH_PROPS_ITEM,			CEffectMeshPropsItem );
	REGISTER_CLASS( this, E_EFFECT_FUNC_PROPS_ITEM,			CEffectFuncPropsItem );
	REGISTER_CLASS( this, E_EFFECT_MAYA_PROPS_ITEM,			CEffectMayaPropsItem );
	
	REGISTER_CLASS( this, E_OBJECT_ROOT_ITEM,						CObjectTreeRootItem );
	REGISTER_CLASS( this, E_OBJECT_COMMON_PROPS_ITEM,		CObjectCommonPropsItem );
	REGISTER_CLASS( this, E_OBJECT_GRAPHICS_ITEM,				CObjectGraphicsItem );

	REGISTER_CLASS( this, E_OBJECT_SPRITE_PROPS_ITEM,		CObjectSpritePropsItem );
	REGISTER_CLASS( this, E_OBJECT_SHADOW_PROPS_ITEM,		CObjectShadowPropsItem );
	REGISTER_CLASS( this, E_OBJECT_PARTICLES_ITEM,			CObjectParticlesItem );

	REGISTER_CLASS( this, E_OBJECT_PASSES_ITEM,					CObjectPassesItem );
	REGISTER_CLASS( this, E_OBJECT_PASS_PROPS_ITEM,			CObjectPassPropsItem );
	REGISTER_CLASS( this, E_OBJECT_GRAPHIC1_PROPS_ITEM,	CObjectGraphic1PropsItem );
	REGISTER_CLASS( this, E_OBJECT_GRAPHICW1_PROPS_ITEM,CObjectGraphicW1PropsItem );
	REGISTER_CLASS( this, E_OBJECT_EFFECTS_ITEM,				CObjectEffectsItem );
	REGISTER_CLASS( this, E_OBJECT_GRAPHICA1_PROPS_ITEM,CObjectGraphicA1PropsItem );
	
	REGISTER_CLASS( this, E_MESH_ROOT_ITEM,							CMeshTreeRootItem );
	REGISTER_CLASS( this, E_MESH_COMMON_PROPS_ITEM,			CMeshCommonPropsItem );
	REGISTER_CLASS( this, E_MESH_DEFENCES_ITEM,					CMeshDefencesItem );
	REGISTER_CLASS( this, E_MESH_DEFENCE_PROPS_ITEM,		CMeshDefencePropsItem );
	REGISTER_CLASS( this, E_MESH_GRAPHICS_ITEM,					CMeshGraphicsItem );
	REGISTER_CLASS( this, E_MESH_PLATFORMS_ITEM,				CMeshPlatformsItem );
	REGISTER_CLASS( this, E_MESH_PLATFORM_PROPS_ITEM,		CMeshPlatformPropsItem );
	REGISTER_CLASS( this, E_MESH_GUNS_ITEM,							CMeshGunsItem );
	REGISTER_CLASS( this, E_MESH_GUN_PROPS_ITEM,				CMeshGunPropsItem );
	REGISTER_CLASS( this, E_MESH_JOGGINGS_ITEM,					CMeshJoggingsItem );
	REGISTER_CLASS( this, E_MESH_JOGGING_PROPS_ITEM,		CMeshJoggingPropsItem );
	REGISTER_CLASS( this, E_MESH_LOCATORS_ITEM,					CMeshLocatorsItem );
	REGISTER_CLASS( this, E_MESH_LOCATOR_PROPS_ITEM,		CMeshLocatorPropsItem );
	REGISTER_CLASS( this, E_MESH_AVIA_ITEM,							CMeshAviaItem );
	REGISTER_CLASS( this, E_MESH_EFFECTS_ITEM,					CMeshEffectsItem );
	REGISTER_CLASS( this, E_MESH_SOUND_PROPS_ITEM,			CMeshSoundPropsItem );
	REGISTER_CLASS( this, E_MESH_DEATH_CRATERS_ITEM,		CMeshDeathCratersItem );
	REGISTER_CLASS( this, E_MESH_DEATH_CRATER_PROPS_ITEM, CMeshDeathCraterPropsItem );
	REGISTER_CLASS( this, E_MESH_TRACK_ITEM,						CMeshTrackItem );
	
	REGISTER_CLASS( this, E_WEAPON_ROOT_ITEM,						CWeaponTreeRootItem );
	REGISTER_CLASS( this, E_WEAPON_COMMON_PROPS_ITEM,		CWeaponCommonPropsItem );
	REGISTER_CLASS( this, E_WEAPON_SHOOT_TYPES_ITEM,		CWeaponShootTypesItem );
	REGISTER_CLASS( this, E_WEAPON_DAMAGE_PROPS_ITEM,		CWeaponDamagePropsItem );
	REGISTER_CLASS( this, E_WEAPON_SOUND_PROPS_ITEM,		CWeaponSoundPropsItem );
	REGISTER_CLASS( this, E_WEAPON_EFFECT_PROPS_ITEM,		CWeaponEffectPropsItem );
	REGISTER_CLASS( this, E_WEAPON_FLASH_PROPS_ITEM,		CWeaponFlashPropsItem );
	REGISTER_CLASS( this, E_WEAPON_CRATERS_ITEM,				CWeaponCratersItem );
	REGISTER_CLASS( this, E_WEAPON_CRATER_PROPS_ITEM,		CWeaponCraterPropsItem );
	REGISTER_CLASS( this, E_WEAPON_EFFECTS_ITEM,				CWeaponEffectsItem );

	REGISTER_CLASS( this, E_BUILDING_ROOT_ITEM,					CBuildingTreeRootItem );
	REGISTER_CLASS( this, E_BUILDING_COMMON_PROPS_ITEM,	CBuildingCommonPropsItem );
	REGISTER_CLASS( this, E_BUILDING_ENTRANCES_ITEM,		CBuildingEntrancesItem );
	REGISTER_CLASS( this, E_BUILDING_ENTRANCE_PROPS_ITEM,	CBuildingEntrancePropsItem );
	REGISTER_CLASS( this, E_BUILDING_SLOTS_ITEM,				CBuildingSlotsItem );
	REGISTER_CLASS( this, E_BUILDING_SLOT_PROPS_ITEM,		CBuildingSlotPropsItem );
	REGISTER_CLASS( this, E_BUILDING_GRAPHICS_ITEM,			CBuildingGraphicsItem );
	REGISTER_CLASS( this, E_BUILDING_GRAPHIC1_PROPS_ITEM,CBuildingGraphic1PropsItem );
	REGISTER_CLASS( this, E_BUILDING_GRAPHIC2_PROPS_ITEM,CBuildingGraphic2PropsItem );
	REGISTER_CLASS( this, E_BUILDING_GRAPHIC3_PROPS_ITEM,CBuildingGraphic3PropsItem );
	REGISTER_CLASS( this, E_BUILDING_DEFENCES_ITEM,			CBuildingDefencesItem );
	REGISTER_CLASS( this, E_BUILDING_DEFENCE_PROPS_ITEM,CBuildingDefencePropsItem );
	REGISTER_CLASS( this, E_BUILDING_SUMMER_PROPS_ITEM,	CBuildingSummerPropsItem );
	REGISTER_CLASS( this, E_BUILDING_WINTER_PROPS_ITEM,	CBuildingWinterPropsItem );
	REGISTER_CLASS( this, E_BUILDING_GRAPHICW1_PROPS_ITEM,CBuildingGraphicW1PropsItem );
	REGISTER_CLASS( this, E_BUILDING_GRAPHICW2_PROPS_ITEM,CBuildingGraphicW2PropsItem );
	REGISTER_CLASS( this, E_BUILDING_GRAPHICW3_PROPS_ITEM,CBuildingGraphicW3PropsItem );
	REGISTER_CLASS( this, E_BUILDING_PASSES_ITEM,				CBuildingPassesItem );
	REGISTER_CLASS( this, E_BUILDING_PASS_PROPS_ITEM,		CBuildingPassPropsItem );
	REGISTER_CLASS( this, E_BUILDING_FIRE_POINTS_ITEM,	CBuildingFirePointsItem );
	REGISTER_CLASS( this, E_BUILDING_FIRE_POINT_PROPS_ITEM, CBuildingFirePointPropsItem );
	REGISTER_CLASS( this, E_BUILDING_DIR_EXPLOSIONS_ITEM, CBuildingDirExplosionsItem );
	REGISTER_CLASS( this, E_BUILDING_DIR_EXPLOSION_PROPS_ITEM, CBuildingDirExplosionPropsItem );
	REGISTER_CLASS( this, E_BUILDING_SMOKES_ITEM,				CBuildingSmokesItem );
	REGISTER_CLASS( this, E_BUILDING_SMOKE_PROPS_ITEM,	CBuildingSmokePropsItem );
	
	REGISTER_CLASS( this, E_TILESET_ROOT_ITEM,					CTileSetTreeRootItem );
	REGISTER_CLASS( this, E_TILESET_COMMON_PROPS_ITEM,	CTileSetCommonPropsItem );
	REGISTER_CLASS( this, E_TILESET_TERRAINS_ITEM,			CTileSetTerrainsItem );
	REGISTER_CLASS( this, E_TILESET_TERRAIN_PROPS_ITEM,	CTileSetTerrainPropsItem );
	REGISTER_CLASS( this, E_TILESET_TILE_PROPS_ITEM,		CTileSetTilePropsItem );
	REGISTER_CLASS( this, E_CROSSETS_ITEM,							CCrossetsItem );
	REGISTER_CLASS( this, E_CROSSET_PROPS_ITEM,					CCrossetPropsItem );
	REGISTER_CLASS( this, E_CROSSET_TILES_ITEM,					CCrossetTilesItem );
	REGISTER_CLASS( this, E_CROSSET_TILE_PROPS_ITEM,		CCrossetTilePropsItem );
	REGISTER_CLASS( this, E_TILESET_TILES_ITEM,					CTileSetTilesItem );
	REGISTER_CLASS( this, E_TILESET_ASOUNDS_ITEM,				CTileSetASoundsItem );
	REGISTER_CLASS( this, E_TILESET_ASOUND_PROPS_ITEM,	CTileSetASoundPropsItem );
	REGISTER_CLASS( this, E_TILESET_LSOUNDS_ITEM,				CTileSetLSoundsItem );
	REGISTER_CLASS( this, E_TILESET_LSOUND_PROPS_ITEM,	CTileSetLSoundPropsItem );

	REGISTER_CLASS( this, E_FENCE_ROOT_ITEM,						CFenceTreeRootItem );
	REGISTER_CLASS( this, E_FENCE_COMMON_PROPS_ITEM,		CFenceCommonPropsItem );
	REGISTER_CLASS( this, E_FENCE_DIRECTION_ITEM,				CFenceDirectionItem );
	REGISTER_CLASS( this, E_FENCE_INSERT_ITEM,					CFenceInsertItem );
	REGISTER_CLASS( this, E_FENCE_PROPS_ITEM,						CFencePropsItem );
	
	REGISTER_CLASS( this, E_KEYFRAME_TREE_ITEM,					CKeyFrameTreeItem );
	REGISTER_CLASS( this, E_PARTICLE_ROOT_ITEM,					CParticleTreeRootItem );
	REGISTER_CLASS( this, E_PARTICLE_COMMON_PROPS_ITEM,	CParticleCommonPropsItem );
	REGISTER_CLASS( this, E_PARTICLE_SOURCE_PROP_ITEMS,	CParticleSourcePropItems );
	REGISTER_CLASS( this, E_PARTICLE_GENERATE_SPIN_ITEM,		CParticleGenerateSpinItem );
	REGISTER_CLASS( this, E_PARTICLE_GENERATE_AREA_ITEM,		CParticleGenerateAreaItem );
	REGISTER_CLASS( this, E_PARTICLE_GENERATE_ANGLE_ITEM,		CParticleGenerateAngleItem );
	REGISTER_CLASS( this, E_PARTICLE_GENERATE_OPACITY_ITEM,	CParticleGenerateOpacityItem );
	REGISTER_CLASS( this, E_PARTICLE_GENERATE_SPEED_ITEM,		CParticleGenerateSpeedItem );
	REGISTER_CLASS( this, E_PARTICLE_GENERATE_LIFE_ITEM,		CParticleGenerateLifeItem );
	REGISTER_CLASS( this, E_PARTICLE_GENERATE_DENSITY_ITEM,	CParticleGenerateDensityItem );
	REGISTER_CLASS( this, E_PARTICLE_GENERATE_RANDOM_SPIN_ITEM,	CParticleGenerateRandomSpinItem );
	
	REGISTER_CLASS( this, E_PARTICLE_PROP_ITEMS,				CParticlePropItems );
	REGISTER_CLASS( this, E_PARTICLE_SPIN_ITEM,					CParticleSpinItem );
	REGISTER_CLASS( this, E_PARTICLE_WEIGHT_ITEM,				CParticleWeightItem );
	REGISTER_CLASS( this, E_PARTICLE_SPEED_ITEM,				CParticleSpeedItem );
	REGISTER_CLASS( this, E_PARTICLE_SIZE_ITEM,					CParticleSizeItem );
	REGISTER_CLASS( this, E_PARTICLE_OPACITY_ITEM,			CParticleOpacityItem );
	REGISTER_CLASS( this, E_PARTICLE_TEXTURE_FRAME_ITEM,CParticleTextureFrameItem );
	REGISTER_CLASS( this, E_PARTICLE_COMPLEX_SOURCE_ITEM,	CParticleComplexSourceItem );
	REGISTER_CLASS( this, E_PARTICLE_RAND_LIFE_ITEM,		CParticleRandLifeItem );
	REGISTER_CLASS( this, E_PARTICLE_RAND_SPEED_ITEM,		CParticleRandSpeedItem );
	REGISTER_CLASS( this, E_PARTICLE_COMPLEX_ITEM,			CParticleComplexItem );
	REGISTER_CLASS( this, E_PARTICLE_C_RANDOM_SPEED_ITEM,	CParticleCRandomSpeedItem );
	
	REGISTER_CLASS( this, E_TRENCH_ROOT_ITEM,						CTrenchTreeRootItem );
	REGISTER_CLASS( this, E_TRENCH_COMMON_PROPS_ITEM,		CTrenchCommonPropsItem );
	REGISTER_CLASS( this, E_TRENCH_SOURCES_ITEM,				CTrenchSourcesItem );
	REGISTER_CLASS( this, E_TRENCH_SOURCE_PROPS_ITEM,		CTrenchSourcePropsItem );
	REGISTER_CLASS( this, E_TRENCH_DEFENCES_ITEM,				CTrenchDefencesItem );
	REGISTER_CLASS( this, E_TRENCH_DEFENCE_PROPS_ITEM,	CTrenchDefencePropsItem );
	
	REGISTER_CLASS( this, E_SQUAD_ROOT_ITEM,						CSquadTreeRootItem );
	REGISTER_CLASS( this, E_SQUAD_COMMON_PROPS_ITEM,		CSquadCommonPropsItem );
	REGISTER_CLASS( this, E_SQUAD_MEMBERS_ITEM,					CSquadMembersItem );
	REGISTER_CLASS( this, E_SQUAD_MEMBER_PROPS_ITEM,		CSquadMemberPropsItem );
	REGISTER_CLASS( this, E_SQUAD_FORMATIONS_ITEM,			CSquadFormationsItem );
	REGISTER_CLASS( this, E_SQUAD_FORMATION_PROPS_ITEM,	CSquadFormationPropsItem );

	REGISTER_CLASS( this, E_GUI_ROOT_ITEM,							CGUITreeRootItem );
	REGISTER_CLASS( this, E_GUI_MOUSE_SELECT_ITEM,			CGUIMouseSelectItem );
	REGISTER_CLASS( this, E_STATICS_TREE_ITEM,					CStaticsTreeItem );
	REGISTER_CLASS( this, E_BUTTONS_TREE_ITEM,					CButtonsTreeItem );
	REGISTER_CLASS( this, E_SLIDERS_TREE_ITEM,					CSlidersTreeItem );
	REGISTER_CLASS( this, E_SCROLLBARS_TREE_ITEM,				CScrollBarsTreeItem );
	REGISTER_CLASS( this, E_STATUSBARS_TREE_ITEM,				CStatusBarsTreeItem );
	REGISTER_CLASS( this, E_LISTS_TREE_ITEM,						CListsTreeItem );
	REGISTER_CLASS( this, E_DIALOGS_TREE_ITEM,					CDialogsTreeItem );
	REGISTER_CLASS( this, E_UNKNOWNS_UI_TREE_ITEM,			CUnknownsTreeItem );
	
	REGISTER_CLASS( this, E_STATIC_PROPS_TREE_ITEM,			CStaticPropsTreeItem );
	REGISTER_CLASS( this, E_BUTTON_PROPS_TREE_ITEM,			CButtonPropsTreeItem );
	REGISTER_CLASS( this, E_SLIDER_PROPS_TREE_ITEM,			CSliderPropsTreeItem );
	REGISTER_CLASS( this, E_SCROLLBAR_PROPS_TREE_ITEM,	CScrollBarPropsTreeItem );
	REGISTER_CLASS( this, E_STATUSBAR_PROPS_TREE_ITEM,	CStatusBarPropsTreeItem );
	REGISTER_CLASS( this, E_LIST_PROPS_TREE_ITEM,				CListPropsTreeItem );
	REGISTER_CLASS( this, E_DIALOG_PROPS_TREE_ITEM,			CDialogPropsTreeItem );
	
	REGISTER_CLASS( this, E_MINE_ROOT_ITEM,							CMineTreeRootItem );
	REGISTER_CLASS( this, E_MINE_COMMON_PROPS_ITEM,			CMineCommonPropsItem );
	
	REGISTER_CLASS( this, E_BRIDGE_ROOT_ITEM,						CBridgeTreeRootItem );
	REGISTER_CLASS( this, E_BRIDGE_DEFENCES_ITEM,			  CBridgeDefencesItem );
	REGISTER_CLASS( this, E_BRIDGE_DEFENCE_PROPS_ITEM,  CBridgeDefencePropsItem );
	REGISTER_CLASS( this, E_BRIDGE_COMMON_PROPS_ITEM,		CBridgeCommonPropsItem );
	REGISTER_CLASS( this, E_BRIDGE_BEGIN_SPANS_ITEM,		CBridgeBeginSpansItem );
	REGISTER_CLASS( this, E_BRIDGE_CENTER_SPANS_ITEM,		CBridgeCenterSpansItem );
	REGISTER_CLASS( this, E_BRIDGE_END_SPANS_ITEM,			CBridgeEndSpansItem );
	REGISTER_CLASS( this, E_BRIDGE_PARTS_ITEM,					CBridgePartsItem );
	REGISTER_CLASS( this, E_BRIDGE_PART_PROPS_ITEM,			CBridgePartPropsItem );
	REGISTER_CLASS( this, E_BRIDGE_STAGE_PROPS_ITEM,		CBridgeStagePropsItem );

	REGISTER_CLASS( this, E_BRIDGE_FIRE_POINTS_ITEM,			CBridgeFirePointsItem );
	REGISTER_CLASS( this, E_BRIDGE_FIRE_POINT_PROPS_ITEM,	CBridgeFirePointPropsItem );
	REGISTER_CLASS( this, E_BRIDGE_DIR_EXPLOSIONS_ITEM,		CBridgeDirExplosionsItem );
	REGISTER_CLASS( this, E_BRIDGE_DIR_EXPLOSION_PROPS_ITEM,CBridgeDirExplosionPropsItem );
	REGISTER_CLASS( this, E_BRIDGE_SMOKES_ITEM,						CBridgeSmokesItem );
	REGISTER_CLASS( this, E_BRIDGE_SMOKE_PROPS_ITEM,			CBridgeSmokePropsItem );
	
	REGISTER_CLASS( this, E_MISSION_ROOT_ITEM,					CMissionTreeRootItem );
	REGISTER_CLASS( this, E_MISSION_COMMON_PROPS_ITEM,	CMissionCommonPropsItem );
	REGISTER_CLASS( this, E_MISSION_OBJECTIVES_ITEM,		CMissionObjectivesItem );
	REGISTER_CLASS( this, E_MISSION_OBJECTIVE_PROPS_ITEM,	CMissionObjectivePropsItem );
	REGISTER_CLASS( this, E_MISSION_MUSICS_ITEM,				CMissionMusicsItem );
	REGISTER_CLASS( this, E_MISSION_MUSIC_PROPS_ITEM,		CMissionMusicPropsItem );

	REGISTER_CLASS( this, E_CHAPTER_ROOT_ITEM,					CChapterTreeRootItem );
	REGISTER_CLASS( this, E_CHAPTER_COMMON_PROPS_ITEM,	CChapterCommonPropsItem );
	REGISTER_CLASS( this, E_CHAPTER_MISSIONS_ITEM,			CChapterMissionsItem );
	REGISTER_CLASS( this, E_CHAPTER_MISSION_PROPS_ITEM,	CChapterMissionPropsItem );
	REGISTER_CLASS( this, E_CHAPTER_PLACES_ITEM,				CChapterPlacesItem );
	REGISTER_CLASS( this, E_CHAPTER_PLACE_PROPS_ITEM,		CChapterPlacePropsItem );
	
	REGISTER_CLASS( this, E_CAMPAIGN_ROOT_ITEM,					CCampaignTreeRootItem );
	REGISTER_CLASS( this, E_CAMPAIGN_COMMON_PROPS_ITEM,	CCampaignCommonPropsItem );
	REGISTER_CLASS( this, E_CAMPAIGN_CHAPTERS_ITEM,			CCampaignChaptersItem );
	REGISTER_CLASS( this, E_CAMPAIGN_CHAPTER_PROPS_ITEM,CCampaignChapterPropsItem );
	REGISTER_CLASS( this, E_CAMPAIGN_TEMPLATES_ITEM,		CCampaignTemplatesItem );
	REGISTER_CLASS( this, E_CAMPAIGN_TEMPLATE_PROPS_ITEM,CCampaignTemplatePropsItem );
	
	REGISTER_CLASS( this, E_3DROAD_ROOT_ITEM,						C3DRoadTreeRootItem );
	REGISTER_CLASS( this, E_3DROAD_COMMON_PROPS_ITEM,		C3DRoadCommonPropsItem );
	REGISTER_CLASS( this, E_3DROAD_LAYER_PROPS_ITEM,		C3DRoadLayerPropsItem );

	REGISTER_CLASS( this, E_3DRIVER_ROOT_ITEM,					C3DRiverTreeRootItem );
	REGISTER_CLASS( this, E_3DRIVER_BOTTOM_LAYER_PROPS_ITEM, C3DRiverBottomLayerPropsItem );
	REGISTER_CLASS( this, E_3DRIVER_LAYER_PROPS_ITEM,		C3DRiverLayerPropsItem );
	REGISTER_CLASS( this, E_3DRIVER_LAYERS_ITEM,				C3DRiverLayersItem );

	REGISTER_CLASS( this, E_MEDAL_ROOT_ITEM,						CMedalTreeRootItem );
	REGISTER_CLASS( this, E_MEDAL_COMMON_PROPS_ITEM,		CMedalCommonPropsItem );
	REGISTER_CLASS( this, E_MEDAL_PICTURE_PROPS_ITEM,		CMedalPicturePropsItem );
	REGISTER_CLASS( this, E_MEDAL_TEXT_PROPS_ITEM,			CMedalTextPropsItem );
}
