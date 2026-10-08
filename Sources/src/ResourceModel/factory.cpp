#include "factory.h"

// All per-kind headers that provide the typed CTreeItem subclasses. The
// eleven stats-only sub-editors T02 ported land first; T03 adds the ten
// keyframe/graph/image/UI sub-editors (Particle, Effect, TileSet, 3dRoad,
// 3dRiver, Mission, Chapter, Campaign, Medal, GUI). The order of the
// registrations below matches Sources/src/editor/TreeItemFactory.cpp
// line-for-line within each block so an audit against the MFC file is one
// `diff`.

#include "items/stats_item.h"
#include "localization.h"
#include "items/bridge/bridge.h"
#include "items/building/building.h"
#include "items/campaign/campaign.h"
#include "items/chapter/chapter.h"
#include "items/effect/effect.h"
#include "items/fence/fence.h"
#include "items/gui/gui.h"
#include "items/infantry/infantry.h"
#include "items/medal/medal.h"
#include "items/mesh/mesh.h"
#include "items/mine/mine.h"
#include "items/mission/mission.h"
#include "items/object/object.h"
#include "items/particle/particle.h"
#include "items/river3d/river3d.h"
#include "items/road3d/road3d.h"
#include "items/sprite/sprite.h"
#include "items/squad/squad.h"
#include "items/tileset/tileset.h"
#include "items/trench/trench.h"
#include "items/weapon/weapon.h"

namespace NResourceModel
{

namespace
{

// Mirror of the MFC `REGISTER_CLASS( factory, type, Class )` macro from
// Sources/src/editor/TreeItemFactory.cpp - one invocation per sub-editor item
// class. The grep audit `grep -c REGISTER_CLASS` over this file counts the
// invocations and must equal the sum of the 11 stats-only kinds (121).
// Root items also get a RegisterRootTag call on the following line so the
// Project loader can match an XML root tag like "Weapon_Composer_Project" to
// its ETreeItemType.
#define REGISTER_CLASS( type, Class )                                                \
	factory.Register( ( type ), [] { return std::unique_ptr<CTreeItem>( new Class() ); } )

void PopulateFactory( CTreeItemFactory &factory )
{
	// --- Infantry (Unit/Animation) sub-editor -------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:36-53. CLocalizationItem is
	// shared by several sub-editors and lives in localization.h, as in MFC.
	REGISTER_CLASS( ETIT_ANIMATION_ROOT_ITEM,       CAnimationTreeRootItem );
	RegisterRootTag( "Unit_Composer_Project", ETIT_ANIMATION_ROOT_ITEM );
	REGISTER_CLASS( ETIT_UNIT_COMMON_PROPS_ITEM,    CUnitCommonPropsItem );
	REGISTER_CLASS( ETIT_LOCALIZATION_ITEM,         CLocalizationItem );
	REGISTER_CLASS( ETIT_UNIT_AI_PROPS_ITEM,        CUnitAIPropsItem );
	REGISTER_CLASS( ETIT_UNIT_WEAPON_PROPS_ITEM,    CUnitWeaponPropsItem );
	REGISTER_CLASS( ETIT_UNIT_GRENADE_PROPS_ITEM,   CUnitGrenadePropsItem );
	REGISTER_CLASS( ETIT_UNIT_DIRECTORY_PROPS_ITEM, CDirectoryPropsItem );
	REGISTER_CLASS( ETIT_UNIT_SEASON_PROPS_ITEM,    CUnitSeasonPropsItem );
	REGISTER_CLASS( ETIT_UNIT_DIRECTORIES_ITEM,     CDirectoriesItem );
	REGISTER_CLASS( ETIT_UNIT_ANIMATIONS_ITEM,      CUnitAnimationsItem );
	REGISTER_CLASS( ETIT_UNIT_ANIMATION_PROPS_ITEM, CUnitAnimationPropsItem );
	REGISTER_CLASS( ETIT_UNIT_FRAME_PROPS_ITEM,     CUnitFramePropsItem );
	REGISTER_CLASS( ETIT_UNIT_ACTIONS_ITEM,         CUnitActionsItem );
	REGISTER_CLASS( ETIT_UNIT_ACTION_PROPS_ITEM,    CUnitActionPropsItem );
	REGISTER_CLASS( ETIT_UNIT_EXPOSURES_ITEM,       CUnitExposuresItem );
	REGISTER_CLASS( ETIT_UNIT_ACKS_ITEM,            CUnitAcksItem );
	REGISTER_CLASS( ETIT_UNIT_ACK_TYPES_ITEM,       CUnitAckTypesItem );
	REGISTER_CLASS( ETIT_UNIT_ACK_TYPE_PROPS_ITEM,  CUnitAckTypePropsItem );

	// --- Sprite sub-editor --------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:55-57.
	REGISTER_CLASS( ETIT_SPRITE_ROOT_ITEM,  CSpriteTreeRootItem );
	RegisterRootTag( "Sprite_Composer_Project", ETIT_SPRITE_ROOT_ITEM );
	REGISTER_CLASS( ETIT_SPRITE_PROPS_ITEM, CSpritePropsItem );
	REGISTER_CLASS( ETIT_SPRITES_ITEM,      CSpritesItem );

	// --- Object sub-editor --------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:71-84.
	REGISTER_CLASS( ETIT_OBJECT_ROOT_ITEM,            CObjectTreeRootItem );
	RegisterRootTag( "Object_Composer_Project", ETIT_OBJECT_ROOT_ITEM );
	REGISTER_CLASS( ETIT_OBJECT_COMMON_PROPS_ITEM,    CObjectCommonPropsItem );
	REGISTER_CLASS( ETIT_OBJECT_GRAPHICS_ITEM,        CObjectGraphicsItem );
	REGISTER_CLASS( ETIT_OBJECT_SPRITE_PROPS_ITEM,    CObjectSpritePropsItem );
	REGISTER_CLASS( ETIT_OBJECT_SHADOW_PROPS_ITEM,    CObjectShadowPropsItem );
	REGISTER_CLASS( ETIT_OBJECT_PARTICLES_ITEM,       CObjectParticlesItem );
	REGISTER_CLASS( ETIT_OBJECT_PASSES_ITEM,          CObjectPassesItem );
	REGISTER_CLASS( ETIT_OBJECT_PASS_PROPS_ITEM,      CObjectPassPropsItem );
	REGISTER_CLASS( ETIT_OBJECT_GRAPHIC1_PROPS_ITEM,  CObjectGraphic1PropsItem );
	REGISTER_CLASS( ETIT_OBJECT_GRAPHICW1_PROPS_ITEM, CObjectGraphicW1PropsItem );
	REGISTER_CLASS( ETIT_OBJECT_EFFECTS_ITEM,         CObjectEffectsItem );
	REGISTER_CLASS( ETIT_OBJECT_GRAPHICA1_PROPS_ITEM, CObjectGraphicA1PropsItem );

	// --- Mesh (Unit) sub-editor ---------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:86-104.
	REGISTER_CLASS( ETIT_MESH_ROOT_ITEM,              CMeshTreeRootItem );
	RegisterRootTag( "Mesh_Composer_Project", ETIT_MESH_ROOT_ITEM );
	REGISTER_CLASS( ETIT_MESH_COMMON_PROPS_ITEM,      CMeshCommonPropsItem );
	REGISTER_CLASS( ETIT_MESH_DEFENCES_ITEM,          CMeshDefencesItem );
	REGISTER_CLASS( ETIT_MESH_DEFENCE_PROPS_ITEM,     CMeshDefencePropsItem );
	REGISTER_CLASS( ETIT_MESH_GRAPHICS_ITEM,          CMeshGraphicsItem );
	REGISTER_CLASS( ETIT_MESH_PLATFORMS_ITEM,         CMeshPlatformsItem );
	REGISTER_CLASS( ETIT_MESH_PLATFORM_PROPS_ITEM,    CMeshPlatformPropsItem );
	REGISTER_CLASS( ETIT_MESH_GUNS_ITEM,              CMeshGunsItem );
	REGISTER_CLASS( ETIT_MESH_GUN_PROPS_ITEM,         CMeshGunPropsItem );
	REGISTER_CLASS( ETIT_MESH_JOGGINGS_ITEM,          CMeshJoggingsItem );
	REGISTER_CLASS( ETIT_MESH_JOGGING_PROPS_ITEM,     CMeshJoggingPropsItem );
	REGISTER_CLASS( ETIT_MESH_LOCATORS_ITEM,          CMeshLocatorsItem );
	REGISTER_CLASS( ETIT_MESH_LOCATOR_PROPS_ITEM,     CMeshLocatorPropsItem );
	REGISTER_CLASS( ETIT_MESH_AVIA_ITEM,              CMeshAviaItem );
	REGISTER_CLASS( ETIT_MESH_EFFECTS_ITEM,           CMeshEffectsItem );
	REGISTER_CLASS( ETIT_MESH_SOUND_PROPS_ITEM,       CMeshSoundPropsItem );
	REGISTER_CLASS( ETIT_MESH_DEATH_CRATERS_ITEM,     CMeshDeathCratersItem );
	REGISTER_CLASS( ETIT_MESH_DEATH_CRATER_PROPS_ITEM, CMeshDeathCraterPropsItem );
	REGISTER_CLASS( ETIT_MESH_TRACK_ITEM,             CMeshTrackItem );

	// --- Weapon sub-editor --------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:106-115.
	REGISTER_CLASS( ETIT_WEAPON_ROOT_ITEM,         CWeaponTreeRootItem );
	RegisterRootTag( "Weapon_Composer_Project", ETIT_WEAPON_ROOT_ITEM );
	REGISTER_CLASS( ETIT_WEAPON_COMMON_PROPS_ITEM, CWeaponCommonPropsItem );
	REGISTER_CLASS( ETIT_WEAPON_SHOOT_TYPES_ITEM,  CWeaponShootTypesItem );
	REGISTER_CLASS( ETIT_WEAPON_DAMAGE_PROPS_ITEM, CWeaponDamagePropsItem );
	REGISTER_CLASS( ETIT_WEAPON_SOUND_PROPS_ITEM,  CWeaponSoundPropsItem );
	REGISTER_CLASS( ETIT_WEAPON_EFFECT_PROPS_ITEM, CWeaponEffectPropsItem );
	REGISTER_CLASS( ETIT_WEAPON_FLASH_PROPS_ITEM,  CWeaponFlashPropsItem );
	REGISTER_CLASS( ETIT_WEAPON_CRATERS_ITEM,      CWeaponCratersItem );
	REGISTER_CLASS( ETIT_WEAPON_CRATER_PROPS_ITEM, CWeaponCraterPropsItem );
	REGISTER_CLASS( ETIT_WEAPON_EFFECTS_ITEM,      CWeaponEffectsItem );

	// --- Building sub-editor ------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:117-141.
	REGISTER_CLASS( ETIT_BUILDING_ROOT_ITEM,              CBuildingTreeRootItem );
	RegisterRootTag( "Building_Composer_Project", ETIT_BUILDING_ROOT_ITEM );
	REGISTER_CLASS( ETIT_BUILDING_COMMON_PROPS_ITEM,      CBuildingCommonPropsItem );
	REGISTER_CLASS( ETIT_BUILDING_ENTRANCES_ITEM,         CBuildingEntrancesItem );
	REGISTER_CLASS( ETIT_BUILDING_ENTRANCE_PROPS_ITEM,    CBuildingEntrancePropsItem );
	REGISTER_CLASS( ETIT_BUILDING_SLOTS_ITEM,             CBuildingSlotsItem );
	REGISTER_CLASS( ETIT_BUILDING_SLOT_PROPS_ITEM,        CBuildingSlotPropsItem );
	REGISTER_CLASS( ETIT_BUILDING_GRAPHICS_ITEM,          CBuildingGraphicsItem );
	REGISTER_CLASS( ETIT_BUILDING_GRAPHIC1_PROPS_ITEM,    CBuildingGraphic1PropsItem );
	REGISTER_CLASS( ETIT_BUILDING_GRAPHIC2_PROPS_ITEM,    CBuildingGraphic2PropsItem );
	REGISTER_CLASS( ETIT_BUILDING_GRAPHIC3_PROPS_ITEM,    CBuildingGraphic3PropsItem );
	REGISTER_CLASS( ETIT_BUILDING_DEFENCES_ITEM,          CBuildingDefencesItem );
	REGISTER_CLASS( ETIT_BUILDING_DEFENCE_PROPS_ITEM,     CBuildingDefencePropsItem );
	REGISTER_CLASS( ETIT_BUILDING_SUMMER_PROPS_ITEM,      CBuildingSummerPropsItem );
	REGISTER_CLASS( ETIT_BUILDING_WINTER_PROPS_ITEM,      CBuildingWinterPropsItem );
	REGISTER_CLASS( ETIT_BUILDING_GRAPHICW1_PROPS_ITEM,   CBuildingGraphicW1PropsItem );
	REGISTER_CLASS( ETIT_BUILDING_GRAPHICW2_PROPS_ITEM,   CBuildingGraphicW2PropsItem );
	REGISTER_CLASS( ETIT_BUILDING_GRAPHICW3_PROPS_ITEM,   CBuildingGraphicW3PropsItem );
	REGISTER_CLASS( ETIT_BUILDING_PASSES_ITEM,            CBuildingPassesItem );
	REGISTER_CLASS( ETIT_BUILDING_PASS_PROPS_ITEM,        CBuildingPassPropsItem );
	REGISTER_CLASS( ETIT_BUILDING_FIRE_POINTS_ITEM,       CBuildingFirePointsItem );
	REGISTER_CLASS( ETIT_BUILDING_FIRE_POINT_PROPS_ITEM,  CBuildingFirePointPropsItem );
	REGISTER_CLASS( ETIT_BUILDING_DIR_EXPLOSIONS_ITEM,    CBuildingDirExplosionsItem );
	REGISTER_CLASS( ETIT_BUILDING_DIR_EXPLOSION_PROPS_ITEM, CBuildingDirExplosionPropsItem );
	REGISTER_CLASS( ETIT_BUILDING_SMOKES_ITEM,            CBuildingSmokesItem );
	REGISTER_CLASS( ETIT_BUILDING_SMOKE_PROPS_ITEM,       CBuildingSmokePropsItem );

	// --- Fence sub-editor ---------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:158-162.
	REGISTER_CLASS( ETIT_FENCE_ROOT_ITEM,         CFenceTreeRootItem );
	RegisterRootTag( "Fence_Composer_Project", ETIT_FENCE_ROOT_ITEM );
	REGISTER_CLASS( ETIT_FENCE_COMMON_PROPS_ITEM, CFenceCommonPropsItem );
	REGISTER_CLASS( ETIT_FENCE_DIRECTION_ITEM,    CFenceDirectionItem );
	REGISTER_CLASS( ETIT_FENCE_INSERT_ITEM,       CFenceInsertItem );
	REGISTER_CLASS( ETIT_FENCE_PROPS_ITEM,        CFencePropsItem );

	// --- Trench sub-editor --------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:190-195.
	REGISTER_CLASS( ETIT_TRENCH_ROOT_ITEM,         CTrenchTreeRootItem );
	RegisterRootTag( "Trench_Composer_Project", ETIT_TRENCH_ROOT_ITEM );
	REGISTER_CLASS( ETIT_TRENCH_COMMON_PROPS_ITEM, CTrenchCommonPropsItem );
	REGISTER_CLASS( ETIT_TRENCH_SOURCES_ITEM,      CTrenchSourcesItem );
	REGISTER_CLASS( ETIT_TRENCH_SOURCE_PROPS_ITEM, CTrenchSourcePropsItem );
	REGISTER_CLASS( ETIT_TRENCH_DEFENCES_ITEM,     CTrenchDefencesItem );
	REGISTER_CLASS( ETIT_TRENCH_DEFENCE_PROPS_ITEM, CTrenchDefencePropsItem );

	// --- Squad sub-editor ---------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:197-202.
	REGISTER_CLASS( ETIT_SQUAD_ROOT_ITEM,          CSquadTreeRootItem );
	RegisterRootTag( "Squad_Composer_Project", ETIT_SQUAD_ROOT_ITEM );
	REGISTER_CLASS( ETIT_SQUAD_COMMON_PROPS_ITEM,   CSquadCommonPropsItem );
	REGISTER_CLASS( ETIT_SQUAD_MEMBERS_ITEM,        CSquadMembersItem );
	REGISTER_CLASS( ETIT_SQUAD_MEMBER_PROPS_ITEM,   CSquadMemberPropsItem );
	REGISTER_CLASS( ETIT_SQUAD_FORMATIONS_ITEM,     CSquadFormationsItem );
	REGISTER_CLASS( ETIT_SQUAD_FORMATION_PROPS_ITEM, CSquadFormationPropsItem );

	// --- Mine sub-editor ----------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:223-224.
	REGISTER_CLASS( ETIT_MINE_ROOT_ITEM,         CMineTreeRootItem );
	RegisterRootTag( "Mine_Composer_Project", ETIT_MINE_ROOT_ITEM );
	REGISTER_CLASS( ETIT_MINE_COMMON_PROPS_ITEM, CMineCommonPropsItem );

	// --- Bridge sub-editor --------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:226-242.
	REGISTER_CLASS( ETIT_BRIDGE_ROOT_ITEM,                 CBridgeTreeRootItem );
	RegisterRootTag( "Bridge_Composer_Project", ETIT_BRIDGE_ROOT_ITEM );
	REGISTER_CLASS( ETIT_BRIDGE_DEFENCES_ITEM,             CBridgeDefencesItem );
	REGISTER_CLASS( ETIT_BRIDGE_DEFENCE_PROPS_ITEM,        CBridgeDefencePropsItem );
	REGISTER_CLASS( ETIT_BRIDGE_COMMON_PROPS_ITEM,         CBridgeCommonPropsItem );
	REGISTER_CLASS( ETIT_BRIDGE_BEGIN_SPANS_ITEM,          CBridgeBeginSpansItem );
	REGISTER_CLASS( ETIT_BRIDGE_CENTER_SPANS_ITEM,         CBridgeCenterSpansItem );
	REGISTER_CLASS( ETIT_BRIDGE_END_SPANS_ITEM,            CBridgeEndSpansItem );
	REGISTER_CLASS( ETIT_BRIDGE_PARTS_ITEM,                CBridgePartsItem );
	REGISTER_CLASS( ETIT_BRIDGE_PART_PROPS_ITEM,           CBridgePartPropsItem );
	REGISTER_CLASS( ETIT_BRIDGE_STAGE_PROPS_ITEM,          CBridgeStagePropsItem );
	REGISTER_CLASS( ETIT_BRIDGE_FIRE_POINTS_ITEM,          CBridgeFirePointsItem );
	REGISTER_CLASS( ETIT_BRIDGE_FIRE_POINT_PROPS_ITEM,     CBridgeFirePointPropsItem );
	REGISTER_CLASS( ETIT_BRIDGE_DIR_EXPLOSIONS_ITEM,       CBridgeDirExplosionsItem );
	REGISTER_CLASS( ETIT_BRIDGE_DIR_EXPLOSION_PROPS_ITEM,  CBridgeDirExplosionPropsItem );
	REGISTER_CLASS( ETIT_BRIDGE_SMOKES_ITEM,               CBridgeSmokesItem );
	REGISTER_CLASS( ETIT_BRIDGE_SMOKE_PROPS_ITEM,          CBridgeSmokePropsItem );

	// --- Effect sub-editor --------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:59-69.
	REGISTER_CLASS( ETIT_EFFECT_ROOT_ITEM,            CEffectTreeRootItem );
	RegisterRootTag( "Effect_Composer_Project", ETIT_EFFECT_ROOT_ITEM );
	REGISTER_CLASS( ETIT_EFFECT_COMMON_PROPS_ITEM,    CEffectCommonPropsItem );
	REGISTER_CLASS( ETIT_EFFECT_ANIMATIONS_ITEM,      CEffectAnimationsItem );
	REGISTER_CLASS( ETIT_EFFECT_MESHES_ITEM,          CEffectMeshesItem );
	REGISTER_CLASS( ETIT_EFFECT_FUNC_PARTICLES_ITEM,  CEffectFuncParticlesItem );
	REGISTER_CLASS( ETIT_EFFECT_MAYA_PARTICLES_ITEM,  CEffectMayaParticlesItem );
	REGISTER_CLASS( ETIT_EFFECT_LIGHTS_ITEM,          CEffectLightsItem );
	REGISTER_CLASS( ETIT_EFFECT_ANIMATION_PROPS_ITEM, CEffectAnimationPropsItem );
	REGISTER_CLASS( ETIT_EFFECT_MESH_PROPS_ITEM,      CEffectMeshPropsItem );
	REGISTER_CLASS( ETIT_EFFECT_FUNC_PROPS_ITEM,      CEffectFuncPropsItem );
	REGISTER_CLASS( ETIT_EFFECT_MAYA_PROPS_ITEM,      CEffectMayaPropsItem );

	// --- TileSet sub-editor -------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:143-156.
	REGISTER_CLASS( ETIT_TILESET_ROOT_ITEM,           CTileSetTreeRootItem );
	RegisterRootTag( "TileSet_Composer_Project", ETIT_TILESET_ROOT_ITEM );
	REGISTER_CLASS( ETIT_TILESET_COMMON_PROPS_ITEM,   CTileSetCommonPropsItem );
	REGISTER_CLASS( ETIT_TILESET_TERRAINS_ITEM,       CTileSetTerrainsItem );
	REGISTER_CLASS( ETIT_TILESET_TERRAIN_PROPS_ITEM,  CTileSetTerrainPropsItem );
	REGISTER_CLASS( ETIT_TILESET_TILE_PROPS_ITEM,     CTileSetTilePropsItem );
	REGISTER_CLASS( ETIT_CROSSETS_ITEM,               CCrossetsItem );
	REGISTER_CLASS( ETIT_CROSSET_PROPS_ITEM,          CCrossetPropsItem );
	REGISTER_CLASS( ETIT_CROSSET_TILES_ITEM,          CCrossetTilesItem );
	REGISTER_CLASS( ETIT_CROSSET_TILE_PROPS_ITEM,     CCrossetTilePropsItem );
	REGISTER_CLASS( ETIT_TILESET_TILES_ITEM,          CTileSetTilesItem );
	REGISTER_CLASS( ETIT_TILESET_ASOUNDS_ITEM,        CTileSetASoundsItem );
	REGISTER_CLASS( ETIT_TILESET_ASOUND_PROPS_ITEM,   CTileSetASoundPropsItem );
	REGISTER_CLASS( ETIT_TILESET_LSOUNDS_ITEM,        CTileSetLSoundsItem );
	REGISTER_CLASS( ETIT_TILESET_LSOUND_PROPS_ITEM,   CTileSetLSoundPropsItem );

	// --- Particle sub-editor (incl. CKeyFrameTreeItem) ----------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:164-188.
	REGISTER_CLASS( ETIT_KEYFRAME_TREE_ITEM,                 CKeyFrameTreeItem );
	REGISTER_CLASS( ETIT_PARTICLE_ROOT_ITEM,                 CParticleTreeRootItem );
	RegisterRootTag( "Particle_Composer_Project", ETIT_PARTICLE_ROOT_ITEM );
	REGISTER_CLASS( ETIT_PARTICLE_COMMON_PROPS_ITEM,         CParticleCommonPropsItem );
	REGISTER_CLASS( ETIT_PARTICLE_SOURCE_PROP_ITEMS,         CParticleSourcePropItems );
	REGISTER_CLASS( ETIT_PARTICLE_GENERATE_SPIN_ITEM,        CParticleGenerateSpinItem );
	REGISTER_CLASS( ETIT_PARTICLE_GENERATE_AREA_ITEM,        CParticleGenerateAreaItem );
	REGISTER_CLASS( ETIT_PARTICLE_GENERATE_ANGLE_ITEM,       CParticleGenerateAngleItem );
	REGISTER_CLASS( ETIT_PARTICLE_GENERATE_OPACITY_ITEM,     CParticleGenerateOpacityItem );
	REGISTER_CLASS( ETIT_PARTICLE_GENERATE_SPEED_ITEM,       CParticleGenerateSpeedItem );
	REGISTER_CLASS( ETIT_PARTICLE_GENERATE_LIFE_ITEM,        CParticleGenerateLifeItem );
	REGISTER_CLASS( ETIT_PARTICLE_GENERATE_DENSITY_ITEM,     CParticleGenerateDensityItem );
	REGISTER_CLASS( ETIT_PARTICLE_GENERATE_RANDOM_SPIN_ITEM, CParticleGenerateRandomSpinItem );
	REGISTER_CLASS( ETIT_PARTICLE_PROP_ITEMS,                CParticlePropItems );
	REGISTER_CLASS( ETIT_PARTICLE_SPIN_ITEM,                 CParticleSpinItem );
	REGISTER_CLASS( ETIT_PARTICLE_WEIGHT_ITEM,               CParticleWeightItem );
	REGISTER_CLASS( ETIT_PARTICLE_SPEED_ITEM,                CParticleSpeedItem );
	REGISTER_CLASS( ETIT_PARTICLE_SIZE_ITEM,                 CParticleSizeItem );
	REGISTER_CLASS( ETIT_PARTICLE_OPACITY_ITEM,              CParticleOpacityItem );
	REGISTER_CLASS( ETIT_PARTICLE_TEXTURE_FRAME_ITEM,        CParticleTextureFrameItem );
	REGISTER_CLASS( ETIT_PARTICLE_COMPLEX_SOURCE_ITEM,       CParticleComplexSourceItem );
	REGISTER_CLASS( ETIT_PARTICLE_RAND_LIFE_ITEM,            CParticleRandLifeItem );
	REGISTER_CLASS( ETIT_PARTICLE_RAND_SPEED_ITEM,           CParticleRandSpeedItem );
	REGISTER_CLASS( ETIT_PARTICLE_COMPLEX_ITEM,              CParticleComplexItem );
	REGISTER_CLASS( ETIT_PARTICLE_C_RANDOM_SPEED_ITEM,       CParticleCRandomSpeedItem );

	// --- GUI sub-editor -----------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:204-221. E_TEMPLATE_TREE_ITEM
	// and E_TEMPLATE_PROPS_TREE_ITEM are present in the MFC enum but not
	// REGISTER_CLASSed in the MFC factory, so the port omits them too.
	REGISTER_CLASS( ETIT_GUI_ROOT_ITEM,             CGUITreeRootItem );
	RegisterRootTag( "GUI_Composer_Project", ETIT_GUI_ROOT_ITEM );
	REGISTER_CLASS( ETIT_GUI_MOUSE_SELECT_ITEM,     CGUIMouseSelectItem );
	REGISTER_CLASS( ETIT_STATICS_TREE_ITEM,         CStaticsTreeItem );
	REGISTER_CLASS( ETIT_BUTTONS_TREE_ITEM,         CButtonsTreeItem );
	REGISTER_CLASS( ETIT_SLIDERS_TREE_ITEM,         CSlidersTreeItem );
	REGISTER_CLASS( ETIT_SCROLLBARS_TREE_ITEM,      CScrollBarsTreeItem );
	REGISTER_CLASS( ETIT_STATUSBARS_TREE_ITEM,      CStatusBarsTreeItem );
	REGISTER_CLASS( ETIT_LISTS_TREE_ITEM,           CListsTreeItem );
	REGISTER_CLASS( ETIT_DIALOGS_TREE_ITEM,         CDialogsTreeItem );
	REGISTER_CLASS( ETIT_UNKNOWNS_UI_TREE_ITEM,     CUnknownsTreeItem );
	REGISTER_CLASS( ETIT_STATIC_PROPS_TREE_ITEM,    CStaticPropsTreeItem );
	REGISTER_CLASS( ETIT_BUTTON_PROPS_TREE_ITEM,    CButtonPropsTreeItem );
	REGISTER_CLASS( ETIT_SLIDER_PROPS_TREE_ITEM,    CSliderPropsTreeItem );
	REGISTER_CLASS( ETIT_SCROLLBAR_PROPS_TREE_ITEM, CScrollBarPropsTreeItem );
	REGISTER_CLASS( ETIT_STATUSBAR_PROPS_TREE_ITEM, CStatusBarPropsTreeItem );
	REGISTER_CLASS( ETIT_LIST_PROPS_TREE_ITEM,      CListPropsTreeItem );
	REGISTER_CLASS( ETIT_DIALOG_PROPS_TREE_ITEM,    CDialogPropsTreeItem );

	// --- Mission sub-editor -------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:244-249.
	REGISTER_CLASS( ETIT_MISSION_ROOT_ITEM,           CMissionTreeRootItem );
	RegisterRootTag( "Mission_Composer_Project", ETIT_MISSION_ROOT_ITEM );
	REGISTER_CLASS( ETIT_MISSION_COMMON_PROPS_ITEM,   CMissionCommonPropsItem );
	REGISTER_CLASS( ETIT_MISSION_OBJECTIVES_ITEM,     CMissionObjectivesItem );
	REGISTER_CLASS( ETIT_MISSION_OBJECTIVE_PROPS_ITEM, CMissionObjectivePropsItem );
	REGISTER_CLASS( ETIT_MISSION_MUSICS_ITEM,         CMissionMusicsItem );
	REGISTER_CLASS( ETIT_MISSION_MUSIC_PROPS_ITEM,    CMissionMusicPropsItem );

	// --- Chapter sub-editor -------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:251-256.
	REGISTER_CLASS( ETIT_CHAPTER_ROOT_ITEM,          CChapterTreeRootItem );
	RegisterRootTag( "Chapter_Composer_Project", ETIT_CHAPTER_ROOT_ITEM );
	REGISTER_CLASS( ETIT_CHAPTER_COMMON_PROPS_ITEM,  CChapterCommonPropsItem );
	REGISTER_CLASS( ETIT_CHAPTER_MISSIONS_ITEM,      CChapterMissionsItem );
	REGISTER_CLASS( ETIT_CHAPTER_MISSION_PROPS_ITEM, CChapterMissionPropsItem );
	REGISTER_CLASS( ETIT_CHAPTER_PLACES_ITEM,        CChapterPlacesItem );
	REGISTER_CLASS( ETIT_CHAPTER_PLACE_PROPS_ITEM,   CChapterPlacePropsItem );

	// --- Campaign sub-editor ------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:258-263.
	REGISTER_CLASS( ETIT_CAMPAIGN_ROOT_ITEM,          CCampaignTreeRootItem );
	RegisterRootTag( "Campaign_Composer_Project", ETIT_CAMPAIGN_ROOT_ITEM );
	REGISTER_CLASS( ETIT_CAMPAIGN_COMMON_PROPS_ITEM,  CCampaignCommonPropsItem );
	REGISTER_CLASS( ETIT_CAMPAIGN_CHAPTERS_ITEM,      CCampaignChaptersItem );
	REGISTER_CLASS( ETIT_CAMPAIGN_CHAPTER_PROPS_ITEM, CCampaignChapterPropsItem );
	REGISTER_CLASS( ETIT_CAMPAIGN_TEMPLATES_ITEM,     CCampaignTemplatesItem );
	REGISTER_CLASS( ETIT_CAMPAIGN_TEMPLATE_PROPS_ITEM, CCampaignTemplatePropsItem );

	// --- 3dRoad sub-editor --------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:265-267.
	REGISTER_CLASS( ETIT_3DROAD_ROOT_ITEM,          C3DRoadTreeRootItem );
	RegisterRootTag( "Road3D_Composer_Project", ETIT_3DROAD_ROOT_ITEM );
	REGISTER_CLASS( ETIT_3DROAD_COMMON_PROPS_ITEM,  C3DRoadCommonPropsItem );
	REGISTER_CLASS( ETIT_3DROAD_LAYER_PROPS_ITEM,   C3DRoadLayerPropsItem );

	// --- 3dRiver sub-editor -------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:269-272.
	REGISTER_CLASS( ETIT_3DRIVER_ROOT_ITEM,             C3DRiverTreeRootItem );
	RegisterRootTag( "River3D_Composer_Project", ETIT_3DRIVER_ROOT_ITEM );
	REGISTER_CLASS( ETIT_3DRIVER_BOTTOM_LAYER_PROPS_ITEM, C3DRiverBottomLayerPropsItem );
	REGISTER_CLASS( ETIT_3DRIVER_LAYER_PROPS_ITEM,      C3DRiverLayerPropsItem );
	REGISTER_CLASS( ETIT_3DRIVER_LAYERS_ITEM,           C3DRiverLayersItem );

	// --- Medal sub-editor ---------------------------------------------------
	// MFC: Sources/src/editor/TreeItemFactory.cpp:274-277.
	REGISTER_CLASS( ETIT_MEDAL_ROOT_ITEM,          CMedalTreeRootItem );
	RegisterRootTag( "Medal_Composer_Project", ETIT_MEDAL_ROOT_ITEM );
	REGISTER_CLASS( ETIT_MEDAL_COMMON_PROPS_ITEM,  CMedalCommonPropsItem );
	REGISTER_CLASS( ETIT_MEDAL_PICTURE_PROPS_ITEM, CMedalPicturePropsItem );
	REGISTER_CLASS( ETIT_MEDAL_TEXT_PROPS_ITEM,    CMedalTextPropsItem );
}

#undef REGISTER_CLASS

}

CTreeItemFactory &CTreeItemFactory::Instance()
{
	// Meyers singleton. PopulateFactory runs once on first access; its order
	// mirrors Sources/src/editor/TreeItemFactory.cpp line-for-line within each
	// per-kind block so a byte-level audit is a single `diff` of this file
	// against the MFC one.
	static CTreeItemFactory instance = [] {
		CTreeItemFactory f;
		PopulateFactory( f );
		return f;
	}();
	return instance;
}

}
