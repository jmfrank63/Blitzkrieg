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
}
#endif // __MAP_FILE_H__
