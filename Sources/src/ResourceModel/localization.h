#pragma once
// MFC-free port of CLocalizationItem (Sources/src/editor/localization.{h,cpp}).
// The MFC item is three DT_BROWSE properties (Name/Description/Statistics)
// pointing at name.txt, desc.txt and stats.txt beside the project. The model
// part is the three-file read; the bytes are returned raw, never re-encoded,
// because the comparator treats them as opaque.

#include <filesystem>
#include <string>

namespace NResourceModel
{

struct SLocalizationItem
{
	std::string name;       // raw bytes of name.txt
	std::string desc;       // raw bytes of desc.txt
	std::string stats;      // raw bytes of stats.txt (empty if absent)
	bool hasName = false;
	bool hasDesc = false;
	bool hasStats = false;  // stats.txt is optional
};

// Reads name.txt, desc.txt and the optional stats.txt under localeDir. File
// names match case-insensitively. Returns true when name.txt and desc.txt both
// exist.
bool loadLocalization( const std::filesystem::path &localeDir, SLocalizationItem *pOut );

}
