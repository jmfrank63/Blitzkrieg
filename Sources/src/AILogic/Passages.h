#ifndef __PASSAGES_H__
#define __PASSAGES_H__
#pragma ONCE
// Narrow places on the fastest path (Remastered,
// docs/superpowers/specs/2026-10-28-fastest-path-design.md): a gap, a bridge,
// a street too narrow for two vehicles side by side. Each is an area with two
// mouths; a vehicle whose path crosses it waits before it until the area is
// free and every vehicle that came before it has passed, so only one is ever
// inside and none meets another head on in it. Simulation state: saved, the
// same on every machine of a multiplayer game.
class CAIUnit;
interface IStaticPath;

// A narrow place on one unit's path, kept by its path unit.
struct SPassageOnPath
{
	int nPassage;
	SVector vMin, vMax;										// the narrow tiles this path crosses

	int operator&( IStructureSaver &ss );
};

class CPassages
{
	DECLARE_SERIALIZE;

	struct SWaiter
	{
		CPtr<CAIUnit> pUnit;
		NTimer::STime arrival;

		int operator&( IStructureSaver &ss );
	};
	struct SPassage
	{
		SVector vMin, vMax;									// its narrow tiles, of every path that crossed it
		CPtr<CAIUnit> pOccupant;
		bool bInside;												// the occupant has come in
		NTimer::STime timeEntered;
		std::list<SWaiter> waiting;					// by arrival, then unit id

		int operator&( IStructureSaver &ss );
	};
	std::vector<SPassage> passages;
	enum { OCCUPY_TIMEOUT = 30000 };

	int Register( const SVector &vMin, const SVector &vMax );
	void Release( const int nPassage, const char *pszWhy );
	void RemoveWaiter( SPassage &passage, CAIUnit *pUnit );
public:
	void Clear() { passages.clear(); }
	bool IsEmpty() const { return passages.empty(); }

	// The narrow places along a vehicle's new big path, registered.
	void FindOnPath( CAIUnit *pUnit, IStaticPath *pPath, std::vector<SPassageOnPath> *pOnPath );
	// Whether the vehicle must stop this segment: it is before a passage that
	// is taken or that others reached first. Takes the passage when its turn
	// comes and gives it back once the vehicle is through.
	bool MustWait( CAIUnit *pUnit, std::vector<SPassageOnPath> *pOnPath );
	// The vehicle takes another way, or is gone: out of every queue.
	void Leave( CAIUnit *pUnit );
	// Gives back a passage whose occupant is dead or has stood in it too long.
	void Segment();
};
extern CPassages thePassages;
#endif // __PASSAGES_H__
