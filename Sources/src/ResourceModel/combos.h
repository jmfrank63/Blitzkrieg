#pragma once
// MFC-free port of the two combo lists the property grid fills from code:
// the AI class combo (Sources/src/editor/Reference.cpp LoadAIClassCombo) and
// the player-sides combo (Sources/src/editor/UnitSide.cpp FillVectorOfSides).

#include <filesystem>
#include <string>
#include <vector>

namespace NResourceModel
{

// "wheel", "halftrack", "track", "human", in the MFC order (the order is the
// AI_CLASS_* value order the project XML stores).
const std::vector<std::string> &aiClasses();

// Party names of the shipped Data/partys.xml (USSR, German, GB, African_GB).
// The MFC code read them from partys.xml at run time; this static table is
// the fallback when no Data root is at hand.
const std::vector<std::string> &playerSides();

// Reads the <PartyName> entries of a partys.xml, the same source the MFC code
// used. Returns an empty vector if the file cannot be read.
std::vector<std::string> readPlayerSides( const std::filesystem::path &partysXml );

}
