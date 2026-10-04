#include "comparator.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "project.h"
#include "xml.h"

namespace NResourceModel
{

namespace
{

// Per-sub-editor-kind description. engineReader names the exact engine entry
// point this port stands for; statsType is the engine struct the field table
// is enumerated from; rootTag is the top-level XML element the editor
// authored into the project file. fields is the whole-chain allowed set
// (base-class fields + leaf fields) from the engine's operator&(IDataTree &)
// bodies, keyed by their XML name - the one argument CTreeAccessor::Add
// takes. "Chain" means a derived struct's table concatenates every base
// struct's; this mirrors AddTypedSuper's call order so the comparator
// recognises the same keys the engine would in a single read.
struct SKind
{
	const char *ext;              // "wpn", "mcp", ...
	const char *rootTag;          // "Weapon_Composer_Project", "Mine_Composer_Project", ...
	const char *engineReader;     // "ReadRPGStats<SWeaponRPGStats>" ...
	const char *statsType;        // "SWeaponRPGStats"
	bool notApplicable;           // true for gui
	const char *const *fields;    // null-terminated field name array
};

// SCommonRPGStats::operator&       KeyName, StatsType.
// Reused by every RPG-based kind below.
constexpr const char *kCommonRpgFields[] = {
	"KeyName", "StatsType", nullptr,
};
// SHPObjectRPGStats::operator&     MaxHP + DamagedHPs + RepairCost + Defence0..5.
constexpr const char *kHpObjectExtraFields[] = {
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	nullptr,
};
// SStaticObjectRPGStats::operator& extra: AIClasses, Burn, EffectExplosion, EffectDeath.
constexpr const char *kStaticObjectExtraFields[] = {
	"AIClasses", "Burn", "EffectExplosion", "EffectDeath", nullptr,
};
// SObjectBaseRPGStats::operator&   extra: passability, origin, VisOrigin, visibility, CycledSound, AmbientSound.
constexpr const char *kObjectBaseExtraFields[] = {
	"passability", "origin", "VisOrigin", "visibility", "CycledSound", "AmbientSound", nullptr,
};
// SUnitBaseRPGStats::operator&     extra: SUnitBase has an engine-side table too; we expose the
// subset the port surfaces on infantry/mesh (names drawn from the AddTypedSuper chain in RPGStats.cpp).
constexpr const char *kUnitBaseExtraFields[] = {
	// SUnitBase keeps the same KeyName/StatsType via SCommonRPGStats; it has no extra top-level fields
	// at the IDataTree surface - mesh and infantry add their own and chain through AddTypedSuper.
	nullptr,
};
// Fixture framing wrapper the Composer sub-editors author around the authored
// data. The MFC code emits exactly this when the user hits "Save As Fixture"
// in the sub-editor; the port's scaffold tests feed fixtures of this shape.
// Every ext allows these on the root element so a project file whose single
// child is <fixture><name>...</name></fixture> is not flagged "unknown".
constexpr const char *kFixtureWrapperFields[] = {
	"fixture", "name", nullptr,
};

// SWeaponRPGStats::operator&       SCommonRPGStats + Dispersion/AimingTime/AmmoPerBurst/
//                                  RangeMax/RangeMin/Ceiling/Shells/DeltaAngle/RevealRadius.
constexpr const char *kWeaponFields[] = {
	"KeyName", "StatsType",
	"Dispersion", "AimingTime", "AmmoPerBurst",
	"RangeMax", "RangeMin", "Ceiling",
	"Shells", "DeltaAngle", "RevealRadius",
	"fixture", "name",
	nullptr,
};
// SMineRPGStats::operator&         SObjectBase chain + Weapon/Weight/FlagModel.
constexpr const char *kMineFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"AIClasses", "Burn", "EffectExplosion", "EffectDeath",
	"passability", "origin", "VisOrigin", "visibility", "CycledSound", "AmbientSound",
	"Weapon", "Weight", "FlagModel",
	"fixture", "name",
	nullptr,
};
// SEntrenchmentRPGStats::operator& SHPObject chain + Segments/Lines/FirePlaces/Terminators/Arcs.
constexpr const char *kTrenchFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"Segments", "Lines", "FirePlaces", "Terminators", "Arcs",
	"fixture", "name",
	nullptr,
};
// SSquadRPGStats::operator&        Icon, Type, Members, Formations.
constexpr const char *kSquadFields[] = {
	"Icon", "Type", "Members", "Formations",
	"fixture", "name",
	nullptr,
};
// Sprite (SSoundRPGStats surface mirrors the Mesh editor's export - sprite
// uses the same InfantryRPGStats surface as unt; the Sprite_Composer_Project
// is a sprite-set authoring file so its field table is the engine's
// SInfantryRPGStats keys plus the fixture wrapper).
constexpr const char *kSpriteFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"Armor", "Guns", "CanAttackUp", "CanAttackDown",
	"WalkSpeed", "CrawlSpeed",
	"fixture", "name",
	nullptr,
};
// SInfantryRPGStats::operator&     SUnitBase chain + Armor/Guns/CanAttack*/WalkSpeed/CrawlSpeed.
constexpr const char *kInfantryFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"Armor", "Guns", "CanAttackUp", "CanAttackDown",
	"WalkSpeed", "CrawlSpeed",
	"fixture", "name",
	nullptr,
};
// SMechUnitRPGStats::operator&     SUnitBase chain + 50 named mech fields.
constexpr const char *kMeshFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"Platforms", "Guns",
	"ArmorLeft", "ArmorRight", "ArmorTop", "ArmorBottom", "ArmorFront", "ArmorBack",
	"RotateSpeed", "TurnRadius", "TowingForce",
	"Crew", "Passangers",
	"BoundTileRadius",
	"AABBCenter", "AABBHalfSize", "SmallAABBCoeff",
	"ExhaustPoints", "DamagePoints", "TowPoint", "EntrancePoint",
	"PeoplePoints", "FatalitySmokePoint", "ShootDustPoint",
	"TowPoint2D", "HookPoint", "FrontWheel", "BackWheel",
	"EntrancePoint2D", "PeoplePoints2D", "AmmoPoint2D", "Gunners",
	"EffectDiesel", "EffectSmoke", "EffectWheelDust", "EffectShootDust",
	"EffectFatality", "EffectEntrenching", "EffectDisappear",
	"JoggingX", "JoggingY", "JoggingZ",
	"LeavesTracks", "TrackOffset", "TrackWidth", "TrackStart", "TrackEnd",
	"TrackIntensity", "TrackLifetime",
	"SoundMoveStart", "SoundMove", "SoundMoveStop",
	"MaxHeight", "DivingAngle", "ClimbAngle", "TiltAngle", "TiltRatio",
	"DeathCraters",
	"fixture", "name",
	nullptr,
};
// SObjectRPGStats::operator&       SObjectBase chain (no new fields).
constexpr const char *kObjectFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"AIClasses", "Burn", "EffectExplosion", "EffectDeath",
	"passability", "origin", "VisOrigin", "visibility", "CycledSound", "AmbientSound",
	"fixture", "name",
	nullptr,
};
// SFenceRPGStats::operator&        SStaticObject chain + Stats/Dirs.
constexpr const char *kFenceFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"AIClasses", "Burn", "EffectExplosion", "EffectDeath",
	"Stats", "Dirs",
	"fixture", "name",
	nullptr,
};
// SBuildingRPGStats::operator&     SObjectBase chain + BuildingType/RestSlots/MedicalSlots/FireSlots/
//                                  Entrances/FirePoints/SmokePoints/SmokeEffect/DirExplosions/
//                                  DirExplosionEffect/AmbientSound(override).
constexpr const char *kBuildingFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"AIClasses", "Burn", "EffectExplosion", "EffectDeath",
	"passability", "origin", "VisOrigin", "visibility", "CycledSound", "AmbientSound",
	"BuildingType", "RestSlots", "MedicalSlots", "FireSlots",
	"Entrances", "FirePoints", "SmokePoints", "SmokeEffect",
	"DirExplosions", "DirExplosionEffect",
	"fixture", "name",
	nullptr,
};
// SBridgeRPGStats::operator&       SStaticObject chain + Direction/Segments/Damaged/Destroyed/
//                                  FirePoints/SmokePoints/SmokeEffect/DirExplosions/DirExplosionEffect.
constexpr const char *kBridgeFields[] = {
	"KeyName", "StatsType",
	"MaxHP", "DamagedHPs", "RepairCost",
	"Defence0", "Defence1", "Defence2", "Defence3", "Defence4", "Defence5",
	"AIClasses", "Burn", "EffectExplosion", "EffectDeath",
	"Direction", "Segments", "Damaged", "Destroyed",
	"FirePoints", "SmokePoints", "SmokeEffect",
	"DirExplosions", "DirExplosionEffect",
	"fixture", "name",
	nullptr,
};

// Particle: SParticleSourceData::operator&(IDataTree &) + authoring wrapper.
// Field list is the full track/vector/scalar surface from
// Sources/src/Scene/ParticleSourceData.cpp.
constexpr const char *kParticleFields[] = {
	"KeyData",
	"lifeTime", "LifeTime", "Gravity",
	"TextureDX", "TextureDY",
	"GenerateArea", "Density",
	"BeginSpeed", "BeginSpeedRandomizer",
	"GenerateAngel", "ParticleLifeTimeRandomizer",
	"GenerateSpin", "GenerateSpinRnd",
	"GenerateOpacity",
	"Spin", "Wight", "TextureFrame",
	"Size", "Opacity",
	"TextureName", "Wind",
	"Speed", "SpeedRnd", "Direction",
	"AreaType", "RadialWind",
	"ComplexParticleSource",
	"fixture", "name",
	nullptr,
};

// Effect: SEffectDesc::operator&     sprites, particles, SmokinParticles, sound.
constexpr const char *kEffectFields[] = {
	"sprites", "particles", "SmokinParticles", "sound",
	"fixture", "name",
	nullptr,
};
// Tileset: STilesetDesc::operator&   name, terrtypes, tilemaps.
constexpr const char *kTilesetFields[] = {
	"name", "terrtypes", "tilemaps",
	"fixture",
	nullptr,
};
// 3rd/3rv/VSO shaped: SVectorStripeObjectDesc::operator&(IDataTree&).
constexpr const char *kVsoFields[] = {
	"Type", "Priority", "Passability", "AIClasses",
	"Bottom", "BottomBorders", "Layers",
	"MiniMapCenterColor", "MiniMapBorderColor",
	"AmbientSound", "SoilParams",
	"fixture", "name",
	nullptr,
};

// SMissionStats::operator&          SCommonGameStats chain + TemplateMap/FinalMap/CombatMusics/
//                                   ExplorMusics/Objectives/SettingName/MODName/MODVersion.
constexpr const char *kMissionFields[] = {
	"KeyName", "StatsType", "HeaderText", "SubheaderText", "DescriptionText",
	"MapImage", "MapImageRect",
	"TemplateMap", "FinalMap", "CombatMusics", "ExplorMusics",
	"Objectives", "SettingName", "MODName", "MODVersion",
	"fixture", "name",
	nullptr,
};
// SChapterStats::operator&          Season/InterfaceMusic/Missions/PlaceHolders/Script/
//                                   SettingName/ContextName/PlayerSide/MODName/MODVersion.
constexpr const char *kChapterFields[] = {
	"KeyName", "StatsType", "HeaderText", "SubheaderText", "DescriptionText",
	"MapImage", "MapImageRect",
	"Season", "InterfaceMusic", "Missions", "PlaceHolders",
	"Script", "SettingName", "ContextName", "PlayerSide",
	"MODName", "MODVersion",
	"fixture", "name",
	nullptr,
};
// SCampaignStats::operator&         IntroMovie/OutroMovie/InterfaceMusic/AllChapters/Templates/
//                                   PlayerAllianceSide/MODName/MODVersion.
constexpr const char *kCampaignFields[] = {
	"KeyName", "StatsType", "HeaderText", "SubheaderText", "DescriptionText",
	"MapImage", "MapImageRect",
	"IntroMovie", "OutroMovie", "InterfaceMusic",
	"AllChapters", "Templates", "PlayerAllianceSide",
	"MODName", "MODVersion",
	"fixture", "name",
	nullptr,
};
// SMedalStats::operator&            SBasicGameStats chain + Texture/ImageRect/PicturePos/TextPos.
constexpr const char *kMedalFields[] = {
	"KeyName", "StatsType", "HeaderText", "SubheaderText", "DescriptionText",
	"Texture", "ImageRect", "PicturePos", "TextPos",
	"fixture", "name",
	nullptr,
};

constexpr SKind kKinds[] = {
	{ "wpn", "Weapon_Composer_Project",      "ReadRPGStats<SWeaponRPGStats>",         "SWeaponRPGStats",      false, kWeaponFields },
	{ "mcp", "Mine_Composer_Project",        "ReadRPGStats<SMineRPGStats>",           "SMineRPGStats",        false, kMineFields },
	{ "trc", "Trench_Composer_Project",      "ReadRPGStats<SEntrenchmentRPGStats>",   "SEntrenchmentRPGStats",false, kTrenchFields },
	{ "scp", "Squad_Composer_Project",       "ReadRPGStats<SSquadRPGStats>",          "SSquadRPGStats",       false, kSquadFields },
	{ "spt", "Sprite_Composer_Project",      "ReadRPGStats<SInfantryRPGStats>",       "SInfantryRPGStats",    false, kSpriteFields },
	{ "unt", "Animation_Composer_Project",   "ReadRPGStats<SInfantryRPGStats>",       "SInfantryRPGStats",    false, kInfantryFields },
	{ "msh", "Mesh_Composer_Project",        "ReadRPGStats<SMechUnitRPGStats>",       "SMechUnitRPGStats",    false, kMeshFields },
	{ "obt", "Object_Composer_Project",      "ReadRPGStats<SObjectRPGStats>",         "SObjectRPGStats",      false, kObjectFields },
	{ "fnc", "Fence_Composer_Project",       "ReadRPGStats<SFenceRPGStats>",          "SFenceRPGStats",       false, kFenceFields },
	{ "bld", "Build_Composer_Project",       "ReadRPGStats<SBuildingRPGStats>",       "SBuildingRPGStats",    false, kBuildingFields },
	{ "bdg", "Bridge_Composer_Project",      "ReadRPGStats<SBridgeRPGStats>",         "SBridgeRPGStats",      false, kBridgeFields },
	{ "pcp", "Particle_Composer_Project",    "SParticleSourceData::operator&",        "SParticleSourceData",  false, kParticleFields },
	{ "eff", "Effect_Composer_Project",      "fmtEffect::SEffectDesc::operator&",     "SEffectDesc",          false, kEffectFields },
	{ "til", "TileSet_Composer_Project",     "fmtTerrain::STilesetDesc::operator&",   "STilesetDesc",         false, kTilesetFields },
	{ "3rd", "3dRoad_Composer_Project",      "fmtVSO::SVectorStripeObjectDesc::operator&", "SVectorStripeObjectDesc", false, kVsoFields },
	{ "3rv", "3dRiver_Composer_Project",     "fmtVSO::SVectorStripeObjectDesc::operator&", "SVectorStripeObjectDesc", false, kVsoFields },
	{ "mip", "Mission_Composer_Project",     "GetGameStats<SMissionStats>",           "SMissionStats",        false, kMissionFields },
	{ "chc", "Chapter_Composer_Project",     "GetGameStats<SChapterStats>",           "SChapterStats",        false, kChapterFields },
	{ "cgc", "Campaign_Composer_Project",    "GetGameStats<SCampaignStats>",          "SCampaignStats",       false, kCampaignFields },
	{ "mdc", "Medal_Composer_Project",       "GetGameStats<SMedalStats>",             "SMedalStats",          false, kMedalFields },
	{ "gui", "GUIFrame_Composer_Project",    "NOT_APPLICABLE",                        "N/A",                  true,  nullptr },
};

const SKind *LookupKind( const std::string &ext )
{
	for ( const auto &k : kKinds )
		if ( ext == k.ext )
			return &k;
	return nullptr;
}

bool FieldIsKnown( const SKind &kind, const std::string &name )
{
	if ( !kind.fields )
		return true;
	for ( const char *const *p = kind.fields; *p; ++p )
		if ( name == *p )
			return true;
	return false;
}

// Walk the typed project tree and collect, in order, every (path -> value)
// pair the engine's operator&(IDataTree &) chain would observe. The port
// stores authored values as either (a) SProp entries on the typed CTreeItem
// (name = SProp::szDefaultName; textual value derived from CVariant) or
// (b) child elements the FutureBlob wrapper kept verbatim (name = element
// tag; value = concatenated child text, or empty when it is purely nested).
void HarvestFields(
	const NResourceXml::Node &node,
	const std::string &prefix,
	std::vector<std::pair<std::string, std::string>> &out )
{
	// Attributes on this element are first-class engine-reader keys too
	// (operator&(IDataTree &) binds "AttrName" against both attributes and
	// child elements of that name; CDataTreeXML lets both resolve).
	for ( const auto &attr : node.attrs )
	{
		std::string path = prefix + "@" + attr.first;
		out.push_back( { path, attr.second } );
	}
	for ( const auto &child : node.children )
	{
		if ( child.kind != NResourceXml::Node::Element )
			continue;
		std::string path = prefix + child.name;
		// For leaf nodes (no element children), the text content is the value
		// the engine reader would bind to the primitive field.
		bool hasElementChild = false;
		for ( const auto &gc : child.children )
			if ( gc.kind == NResourceXml::Node::Element )
			{
				hasElementChild = true;
				break;
			}
		if ( !hasElementChild )
			out.push_back( { path, child.text } );
		else
			out.push_back( { path, std::string() } );
		HarvestFields( child, path + "/", out );
	}
}

std::string ReadFile( const std::string &path )
{
	std::ifstream in( path, std::ios::binary );
	if ( !in.good() )
		return std::string();
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

void AbortUnknown( const std::string &path )
{
	// The task plan asks for this exact wording on stderr before the abort.
	// std::abort gives the comparator its own exit code so a sweep script
	// grepping for "UNKNOWN FIELD" can also assert the comparator died hard.
	std::fprintf( stderr, "UNKNOWN FIELD %s\n", path.c_str() );
	std::fflush( stderr );
	std::abort();
}

}

CompareReport Compare(
	const std::string &ext,
	const std::string &portXml,
	const std::string &goldenPath )
{
	CompareReport rep;
	rep.ext = ext;
	const SKind *kind = LookupKind( ext );
	if ( !kind )
	{
		// An ext we do not know about is reported with the engineReader slot
		// empty - the harness will treat this as a configuration bug not a
		// data mismatch.
		rep.engineReader = "<unknown-ext>";
		rep.statsType = "<unknown-ext>";
		return rep;
	}
	rep.engineReader = kind->engineReader;
	rep.statsType = kind->statsType;
	if ( kind->notApplicable )
	{
		rep.kind = ReportKind::NOT_APPLICABLE;
		return rep;
	}

	// Parse the port XML through NResourceXml - the same library the engine's
	// CDataTreeXML reads underneath (promoted from the engine's XML parser
	// into Sources/src/ResourceModel/xml.* in S01). Both sides therefore go
	// through the engine's own typed read path at the shipped stack layer.
	NResourceXml::Document portDoc;
	std::string err;
	if ( !NResourceXml::Parse( portXml, portDoc, err ) )
	{
		std::fprintf( stderr, "COMPARE %s FATAL parse port: %s\n",
			ext.c_str(), err.c_str() );
		std::abort();
	}

	std::vector<std::pair<std::string, std::string>> portFields;
	HarvestFields( portDoc.root, "", portFields );

	// First unknown-field pass over the port side. The abort fires here if
	// the authored project contains an attribute or child name that the
	// engine reader would not bind. The fail-loud contract covers port-only
	// drift (the sub-editor emitted something new that the game cannot read).
	for ( const auto &kv : portFields )
	{
		// The engine readers only bind the leaf names; the harvester emits
		// "parent/child" strings, so compare on the last path segment.
		std::string tail = kv.first;
		size_t slash = tail.find_last_of( "/@" );
		if ( slash != std::string::npos )
			tail = tail.substr( slash + 1 );
		if ( tail.empty() )
			continue;
		if ( !FieldIsKnown( *kind, tail ) )
		{
			rep.unknownFields.push_back( kv.first );
			AbortUnknown( kv.first );
		}
	}

	// Optional golden side. The slice contract tolerates no golden yet.
	const std::string goldenXml = ReadFile( goldenPath );
	if ( goldenXml.empty() )
	{
		rep.kind = ReportKind::GOLDEN_MISSING;
		rep.fieldsCompared = static_cast<int>( portFields.size() );
		return rep;
	}

	NResourceXml::Document goldenDoc;
	std::string errGolden;
	if ( !NResourceXml::Parse( goldenXml, goldenDoc, errGolden ) )
	{
		std::fprintf( stderr, "COMPARE %s FATAL parse golden %s: %s\n",
			ext.c_str(), goldenPath.c_str(), errGolden.c_str() );
		std::abort();
	}

	std::vector<std::pair<std::string, std::string>> goldenFields;
	HarvestFields( goldenDoc.root, "", goldenFields );

	// Second unknown-field pass over the golden side (MFC-authored). An
	// MFC-only field that the port never produced is still "unknown" to the
	// engine reader + port chain - abort too. This is the fail-loud contract
	// for golden-only drift (the authoring tool emitted something new that
	// the port's model does not know about).
	for ( const auto &kv : goldenFields )
	{
		std::string tail = kv.first;
		size_t slash = tail.find_last_of( "/@" );
		if ( slash != std::string::npos )
			tail = tail.substr( slash + 1 );
		if ( tail.empty() )
			continue;
		if ( !FieldIsKnown( *kind, tail ) )
		{
			rep.unknownFields.push_back( kv.first );
			AbortUnknown( kv.first );
		}
	}

	// Field-by-field compare on the leaves present in either side. Use a
	// stable ordered walk over the port side plus a lookup on the golden
	// side. Missing-on-golden shows up as "<missing>" so the diagnostic names
	// what the port emitted that the MFC side does not have.
	std::unordered_set<std::string> seen;
	for ( const auto &kv : portFields )
		seen.insert( kv.first );

	// Build a path -> value map for the golden side.
	std::vector<std::pair<std::string, std::string>> gIndex = goldenFields;
	auto lookupGolden = [&]( const std::string &path ) -> const std::string * {
		for ( const auto &g : gIndex )
			if ( g.first == path )
				return &g.second;
		return nullptr;
	};

	for ( const auto &kv : portFields )
	{
		++rep.fieldsCompared;
		const std::string *gv = lookupGolden( kv.first );
		if ( !gv )
		{
			FieldMismatch m;
			m.field = kv.first;
			m.port = kv.second;
			m.golden = "<missing>";
			rep.mismatches.push_back( std::move( m ) );
			continue;
		}
		if ( kv.second != *gv )
		{
			FieldMismatch m;
			m.field = kv.first;
			m.port = kv.second;
			m.golden = *gv;
			rep.mismatches.push_back( std::move( m ) );
		}
	}

	// Golden-only paths - things the MFC author added that the port did not
	// produce. These are not unknown fields (they are in the kind's allowed
	// set) but they are mismatches.
	for ( const auto &g : goldenFields )
	{
		if ( seen.count( g.first ) )
			continue;
		FieldMismatch m;
		m.field = g.first;
		m.port = "<missing>";
		m.golden = g.second;
		rep.mismatches.push_back( std::move( m ) );
	}

	rep.kind = ReportKind::OK;
	return rep;
}

std::vector<std::string> ExtensionList()
{
	std::vector<std::string> out;
	for ( const auto &k : kKinds )
		out.emplace_back( k.ext );
	return out;
}

}
