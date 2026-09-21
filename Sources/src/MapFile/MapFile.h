#ifndef __MAP_FILE_H__
#define __MAP_FILE_H__
#include <string>
class CMapInfo;
namespace NMapFile
{
// Reads one map. pszPath is a real filesystem path ending in .bzm or .xml,
// written with backslashes like every other path the engine handles. Returns
// false and fills pError when the file cannot be opened, the stream throws, or
// CMapInfo::IsValid() is false.
bool Read( const char *pszPath, CMapInfo *pMap, std::string *pError );

// Reads the newer of <base>.xml and <base>.bzm, as the game does. pszBase is a
// STORAGE-relative name with no extension ("maps\\Multiplayer\\coldwinter"),
// because the mtimes come from the registered storage; Read, by contrast,
// takes a filesystem path. Returns false when neither file is there.
bool ReadNewest( const char *pszBase, CMapInfo *pMap, std::string *pError );
}
#endif // __MAP_FILE_H__
