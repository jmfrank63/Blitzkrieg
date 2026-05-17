#include "stdafx.h"

#include "RMG_Types.h"
#include "..\Formats\FmtTerrain.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char * RMGC_CONTAINER_XML_NAME = "Container";
const char *RMGC_GRAPH_XML_NAME = "Graph";
const char *RMGC_OBJECTSET_XML_NAME = "ObjectSet";
const char *RMGC_TILESET_XML_NAME = "TileSet";
const char *RMGC_FIELDSET_XML_NAME = "FieldSet";
const char *RMGC_TEMPLATE_XML_NAME = "Template";

const char *RMGC_ROAD_LEVEL_FILE_NAME = "RoadLevel";//"Scenarios\\Profiles\\roadProfile";
const char *RMGC_RIVER_LEVEL_FILE_NAME = "RiverLevel";//"Scenarios\\Profiles\\riverProfile";
const char *RMGC_RM_LEVEL_VSO_PARAMETER_NAME = "SRMLevelVSOParameter";
const char *RMGC_CONTEXT_NAME = "ChapterUnitsTable";
const char *RMGC_SETTING_NAME = "ChapterSetting";
const char *RMGC_SETTING_DEFAULT_FOLDER = "Scenarios\\Settings\\";
const char *RMGC_ANY_SETTING_NAME = "<any setting>";
const char *RMGC_NO_MOD_FOLDER = "<no any MOD>";
const char *RMGC_CURRENT_MOD_FOLDER = "<current MOD>";
const char *RMGC_OWN_MOD_FOLDER = "<map MOD>";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int		SRMGraphLink::INVALID_LINK_VALUE =  -1;
const float SRMGraphLink::DEFAULT_RADIUS = fWorldCellSize * 4;
const int		SRMGraphLink::DEFAULT_PARTS = 8;
const float SRMGraphLink::DEFAULT_MIN_LENGTH = fWorldCellSize * 4;
const float SRMGraphLink::DEFAULT_DISTANCE = 0.3f;
const float SRMGraphLink::DEFAULT_DISTURBANCE = 0.1f;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float SRMVSODesc::DEFAULT_WIDTH = fWorldCellSize * 2;
const float SRMVSODesc::DEFAULT_OPACITY = 1.0f;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int SRMTemplateUnitsTable::UNIT_RPG_TYPE_COUNT = 28;
const DWORD SRMTemplateUnitsTable::INVALID_UNIT_RPG_TYPE = 0xFFffFFff;
const DWORD SRMTemplateUnitsTable::SQUAD_UNIT_RPG_TYPE = SSquadRPGStats::SNIPERS;
const DWORD SRMTemplateUnitsTable::DEFAULT_LEVELS_COUNT = 3;
const DWORD SRMTemplateUnitsTable::UNIT_RPG_TYPES[UNIT_RPG_TYPE_COUNT] = 
{
	//squads
	SSquadRPGStats::RIFLEMANS,
	SSquadRPGStats::INFANTRY,
	SSquadRPGStats::SUBMACHINEGUNNERS,
	SSquadRPGStats::MACHINEGUNNERS,
	SSquadRPGStats::AT_TEAM,
	SSquadRPGStats::SNIPERS,
	// transport
	RPG_TYPE_TRN_CARRIER,
	RPG_TYPE_TRN_SUPPORT,
	RPG_TYPE_TRN_MEDICINE,
	RPG_TYPE_TRN_TRACTOR,
	RPG_TYPE_TRN_MILITARY_AUTO,
	RPG_TYPE_TRN_CIVILIAN_AUTO,
	// artillery
	RPG_TYPE_ART_GUN,
	RPG_TYPE_ART_HOWITZER,
	RPG_TYPE_ART_HEAVY_GUN,
	RPG_TYPE_ART_AAGUN,
	RPG_TYPE_ART_ROCKET,
	RPG_TYPE_ART_SUPER,
	RPG_TYPE_ART_MORTAR,
	RPG_TYPE_ART_HEAVY_MG,
	// SPG
	RPG_TYPE_SPG_ASSAULT,
	RPG_TYPE_SPG_ANTITANK,
	RPG_TYPE_SPG_SUPER,
	RPG_TYPE_SPG_AAGUN,
	// armor
	RPG_TYPE_ARM_LIGHT,
	RPG_TYPE_ARM_MEDIUM,
	RPG_TYPE_ARM_HEAVY,
	RPG_TYPE_ARM_SUPER,
};

const std::string SRMTemplateUnitsTable::UNIT_RPG_MNEMONICS[UNIT_RPG_TYPE_COUNT] =
{
	//squads
	"squad riflemans",
	"squad infantry",
	"squad submachinegunners",
	"squad machinegunners",
	"squad at team",
	"squad snipers",
	// transport
	"transport carrier",
	"transport support",
	"transport medicine",
	"transport tractor",
	"transport military auto",
	"transport civilian auto",
	// artillery
	"artillery gun",
	"artillery howitzer",
	"artillery heavy gun",
	"artillery aagun",
	"artillery rocket",
	"artillery super",
	"artillery mortar",
	"artillery heavy mg",
	// SPG
	"spg assault",
	"spg antitank",
	"spg super",
	"spg aagun",
	// armor
	"armor light",
	"armor medium",
	"armor heavy",
	"armor super",
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::unordered_map<DWORD, int> SRMTemplateUnitsTable::unitRPGTypeToIndex;
std::unordered_map<std::string, int> SRMTemplateUnitsTable::unitRPGMnemonicToIndex;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
