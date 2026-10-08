#pragma once
// MFC-free port of the two combo lists the property grid fills from code:
// the AI class combo (Sources/src/editor/Reference.cpp LoadAIClassCombo) and
// the player-sides combo (Sources/src/editor/UnitSide.cpp FillVectorOfSides).

#include <filesystem>
#include <string>
#include <vector>

namespace NResourceModel
{

struct SProp;
class CTreeItem;

// "wheel", "halftrack", "track", "human", in the MFC order (the order is the
// AI_CLASS_* value order the project XML stores).
const std::vector<std::string> &aiClasses();
// Reference.cpp LoadAIClassCombo: appends aiClasses() to the prop's combo strings.
void LoadAIClassCombo( SProp *pProp );

// Party names of the shipped Data/partys.xml (USSR, German, GB, African_GB).
// The MFC code read them from partys.xml at run time; this static table is
// the fallback when no Data root is at hand.
const std::vector<std::string> &playerSides();

// Reads the <PartyName> entries of a partys.xml, the same source the MFC code
// used. Returns an empty vector if the file cannot be read.
std::vector<std::string> readPlayerSides( const std::filesystem::path &partysXml );

// The four locator combos of the unit editor, MFC's CMeshFrame::LoadGunPointPropsComboBox,
// LoadGunPartPropsComboBox, LoadGunCarriagePropsComboBox and LoadPlatformPropsComboBox
// (MeshFrm.cpp:1545..1699), computed from the Locators children of the open project
// (the combat model's skeleton nodes) instead of the live combat object. `nItemType`
// is ETIT_MESH_PLATFORM_PROPS_ITEM (props 1 part, 2 and 3 gun carriage) or
// ETIT_MESH_GUN_PROPS_ITEM (props 1 shoot point, 2 shoot part). Returns false when the
// pair is no locator combo. "NA" ends every list, and is the only entry without a model.
// MFC also reset a value missing from the list to "NA"; that is an edit, left to the caller.
bool MeshLocatorStrings( const CTreeItem &root, int nItemType, int nPropId, std::vector<std::string> &strings );

// UnitSide.cpp FillVectorOfSides: appends the party names of the game's
// partys.xml (GetGameDataDir()) to a combo's strings, as MFC does. Without a
// game data directory, or when the file cannot be read, it appends playerSides().
void FillVectorOfSides( std::vector<std::string> &sides );

}
