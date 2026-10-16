#ifndef __BK_MAP_OBJ2MOD_H__
#define __BK_MAP_OBJ2MOD_H__
#include <string>

namespace NObjToMod
{
// bzmconvertor's ConvertObjToMod: reads the OBJ, the optional skeleton and
// animation sidecars (an empty name means none), validates them, and unless
// bValidateOnly writes the .mod (".mod" is appended to a name without it).
// Prints the one summary line on success; false with the reason already on
// stderr. Writing needs the StreamIO globals (NDataOnly::StartStreamIO);
// validating needs nothing.
bool Run( const std::string &szObjInput, const std::string &szModOutput, const std::string &szSkeletonInput,
	const std::string &szAnimationInput, bool bValidateOnly );
}
#endif // __BK_MAP_OBJ2MOD_H__
