#pragma once
// The MFC editor stores a property's edit widget as a DomenID: a plain int whose
// values live in Sources/src/editor/COI/CtrlObjectInspector.h (DT_*). The port
// carries the same values so a future round-trip through PropertyInspector reads
// the right widget back out of the XML's "domen" attribute. The names mirror the
// original enum verbatim; the DT_SOLDIER_REF .. DT_WATER_TEXTURE_REF reference
// kinds are what T07 (references) will consume. This is a header-only enum so
// comparator printing can quote the symbolic name without pulling the whole
// Variant translation unit in.

namespace NResourceModel
{

using DomenID = int;

enum : DomenID
{
	DT_ERROR = 0,
	DT_DEC,
	DT_HEX,
	DT_STR,
	DT_BOOL,
	DT_BROWSE,
	DT_BROWSEDIR,
	DT_COMBO,
	DT_COLOR,
	DT_FLOAT,

	DT_ANIMATION_REF,
	DT_FUNC_PARTICLE_REF,
	DT_EFFECT_REF,
	DT_WEAPON_REF,
	DT_SOLDIER_REF,
	DT_ACTION_REF,
	DT_SCENARIO_MISSION_REF,
	DT_TEMPLATE_MISSION_REF,
	DT_CHAPTER_REF,
	DT_SOUND_REF,
	DT_SETTING_REF,
	DT_ASK_REF,
	DT_DEATH_REF,
	DT_CRATER_REF,
	DT_MAP_REF,
	DT_MUSIC_REF,
	DT_MOVIE_REF,
	DT_PARTICLE_TEXTURE_REF,
	DT_WATER_TEXTURE_REF,
	DT_ROAD_TEXTURE_REF,

	DT_CUSTOM
};

}
