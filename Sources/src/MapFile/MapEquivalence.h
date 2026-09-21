#ifndef __MAP_EQUIVALENCE_H__
#define __MAP_EQUIVALENCE_H__
#include <string>
struct SLoadMapInfo;
struct STerrainInfo;
namespace NMapFile
{
// Compares two maps field by field, in the declaration order of SLoadMapInfo.
// Returns true when they are equivalent. On a difference, fills pWhere with a
// path like "terrain.patches[3][7].basecrosses[2].tile" and returns false; it
// stops at the first difference, because a sweep over 1,755 maps wants a name,
// not a diff. Floats compare with ==: nothing in this tier recomputes one, so
// any difference is a bug rather than a rounding artefact.
bool AreEquivalent( const SLoadMapInfo &rLeft, const SLoadMapInfo &rRight, std::string *pWhere );

// True when two terrains have the same altitudes, height and shade, at every
// vertex. The paint tests assert this on its own: the terrain function must
// never touch them.
bool CompareAltitudeArrays( const STerrainInfo &rLeft, const STerrainInfo &rRight );

// The size the comparator was written against; the test prints it so the
// static_assert in MapEquivalence.cpp can be updated when it fires.
unsigned long LoadMapInfoSize();
}
#endif // __MAP_EQUIVALENCE_H__
