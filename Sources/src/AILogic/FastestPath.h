#ifndef __FASTEST_PATH_H__
#define __FASTEST_PATH_H__
#pragma ONCE
// The fastest path's A* (fastest_path.zig, a static library linked into
// AILogic): one call over a snapshot of the AI tile grid, no callbacks.
// Only fixed-width integers and pointers cross the boundary
// (docs/superpowers/specs/2026-10-28-fastest-path-design.md).
#include <stdint.h>

extern "C"
{
	enum
	{
		BK_PATH_STRAIGHT = 70, BK_PATH_DIAGONAL = 99,
		// A cell: the weight of entering the tile per unit of length (0: the
		// unit cannot enter it) and the point checking's "good tile" bit.
		BK_PATH_CELL_GOOD = 0x8000, BK_PATH_CELL_WEIGHT = 0x1FFF
	};
	enum EBkPathStatus
	{
		BK_PATH_GOAL = 0, BK_PATH_GOOD = 1, BK_PATH_BESIDE_GOAL = 2, BK_PATH_CLOSEST = 3,
		BK_PATH_BUDGET = 4, BK_PATH_INVALID = 5
	};
	struct SBkPathGrid
	{
		const uint16_t *pCells;
		int32_t nWidth, nHeight;
	};
	struct SBkPathRequest
	{
		int32_t nStartX, nStartY, nGoalX, nGoalY;
		int32_t nMaxSteps;
		int32_t nNodeBudget;
	};
	struct SBkPathPoint
	{
		int32_t x, y;
		uint32_t nCost;
	};
	struct SBkPathResult
	{
		int32_t nStatus;
		int32_t nLength;
		int32_t nExpanded;
	};
	void bk_path_find( const SBkPathGrid *pGrid, const SBkPathRequest *pRequest, SBkPathPoint *pOut, int32_t nOutCapacity, SBkPathResult *pResult );
}
#endif // __FASTEST_PATH_H__
