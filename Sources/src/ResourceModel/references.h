#pragma once
// MFC-free port of Sources/src/editor/RefDlg.{h,cpp}'s reference lists. The MFC
// CReferenceDialog served two concerns at once: a Win32 list-box dialog, and a
// per-type registry of file-system-sourced strings each resource picker fills
// its combo from. The port keeps only the registry - the picker UI lives in
// S05 - and exposes it as a plain class with a const enumerate() by type.
//
// EReferenceType matches the MFC enum in RefDlg.h:9 byte-for-byte (same names,
// same order). The ordering is load-bearing: a project XML stores a property's
// reference kind as an int (DT_* in domen_id.h), and the picker UI looks up by
// EReferenceType. T07's comparator also key-prints these in enum order so the
// serialised shape is stable.

#include <filesystem>
#include <string>
#include <vector>

namespace NResourceModel
{

enum class EReferenceType : int
{
	E_ANIMATIONS_REF = 0,       // sprites
	E_FUNC_PARTICLES_REF,       // particles
	E_EFFECTS_REF,              // effects
	E_WEAPONS_REF,              // weapons
	E_SOLDIER_REF,              // infantry
	E_ACTIONS_REF,              // actions / exposures (MultiSelDialog)
	E_SCENARIO_MISSIONS_REF,    // missions
	E_TEMPLATE_MISSIONS_REF,    // templates
	E_CHAPTERS_REF,             // chapters
	E_SOUNDS_REF,               // sounds
	E_SETTING_REF,              // settings
	E_ASKS_REF,                 // asks
	E_CRATER_REF,               // craters
	E_DEATHHOLE_REF,            // deathholes
	E_MAP_REF,                  // maps
	E_MUSIC_REF,                // music
	E_MOVIE_REF,                // movies
	E_PARTICLE_TEXTURE_REF,     // particle textures
	E_ROAD_TEXTURE_REF,         // road textures
	E_WATER_TEXTURE_REF,        // water textures
};

// Total number of EReferenceType enumerators; the test iterates [0, Count).
constexpr int kReferenceTypeCount = 20;

// Human-readable symbolic name of a type, for log lines ("REF <name> count=N").
const char *ReferenceTypeName( EReferenceType eType );

// A readonly registry of per-type string lists, built once by rebuild() from a
// Data-root directory. The MFC version pulled this from an IDataStorage; the
// portable port walks the file system under <dataRoot> with std::filesystem.
// Case is handled per path component so a Linux file system (which does not
// fold) still finds "Data/Weapons/..." when the shipped layout uses lower-case
// "data/weapons/..." or vice versa; the matching rule mirrors the DataFile
// helper in tools/zig/editor_bridge_test.cpp (OsPath + per-component resolve).
class References
{
public:
	References() = default;

	// Walks the per-type sub-directories under <dataRoot> and fills the list
	// for every EReferenceType. Any entry that does not resolve (missing
	// directory, zero matching files) ends up as an empty vector; the test
	// then sees `count=0` without the agent having to open a GUI picker.
	// Returns the total number of entries added across every type.
	std::size_t rebuild( const std::filesystem::path &dataRoot );

	const std::vector<std::string> &enumerate( EReferenceType eType ) const;
	std::size_t count( EReferenceType eType ) const { return enumerate( eType ).size(); }

private:
	// 20 slots, EReferenceType -> list of relative-path strings as the picker
	// would store them (forward or backslashes preserved from the enumerator).
	std::vector<std::string> m_lists[kReferenceTypeCount];
};

}
