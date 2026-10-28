#ifndef __TURN_PRELOAD_H__
#define __TURN_PRELOAD_H__
#pragma ONCE
// The pictures of all four camera steps in memory (Remastered only,
// docs/superpowers/specs/2026-10-26-camera-turn-design.md, "the turn
// preload"): a turn then only switches what is drawn. On the second tutorial
// a turn spent its seconds asking the data storage whether a turned or lit
// folder exists (each a walk of the file system) and loading the packs and
// textures of the new side; both are done once here, for the mission.
namespace NTurnPreload
{
	// Whether the data storage holds the stream, asked once a mission.
	bool Exists( const std::string &szName );
	// Loads the sprite pack `<szPack>.san` and its texture into the shared
	// caches, where they stay until the mission's resources are cleared; true
	// if the pack is there. A pack is loaded once a mission.
	bool Warm( const std::string &szPack );
	// Whether Warm found the pack missing this mission.
	bool Missing( const std::string &szPack );
	// BK_TURN_TRACE: the pictures a camera turn draws, as a count and an
	// order-free sum of their names' hashes, so a tier can tell that a turn
	// draws the same pictures with the preload as without it. TakeDrawn
	// returns them and starts the next count.
	void NoteDrawn( const std::string &szPack );
	void TakeDrawn( int *pnCount, unsigned int *pnSum );
	// The Game's world starts the memo with its mission and clears it at the
	// mission's end; until then Exists asks the storage and Warm does nothing.
	void Begin();
	// Forgets what was asked and loaded (the caches keep the pictures until
	// they are cleared themselves).
	void Clear();
};
#endif // __TURN_PRELOAD_H__
