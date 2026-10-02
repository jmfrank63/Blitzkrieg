#ifndef __EDITOR_BRIDGE_H__
#define __EDITOR_BRIDGE_H__

/* The editor's view of the engine. Flat C: opaque handles, plain structs,
   status codes. No engine header is included here, and no C++ exception
   crosses it - every entry point wraps its body and returns a status, because
   a throw through this boundary would leave the module it came from and unwind
   into Zig, which has no idea what to do with it. */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	BK_EDITOR_OK = 0,
	BK_EDITOR_BAD_ARGUMENT = 1,   /* the caller passed something impossible */
	BK_EDITOR_NO_SESSION = 2,     /* a null session reached an entry point */
	BK_EDITOR_NO_DEVICE = 3,      /* a real window, but no usable GPU device */
	BK_EDITOR_DATA_MISSING = 4,   /* a map, tileset or descriptor is not there */
	BK_EDITOR_REFUSED = 5,        /* the edit is not allowed; see the message */
	BK_EDITOR_FAILED = 6
} BkEditorStatus;

typedef struct BkEditorSession BkEditorSession;

/* The last message, owned by the bridge, valid until the next call on the same
   session. Never returns null: an empty string when there is nothing to say,
   and a fixed string when session is null - a start that failed before it had
   anywhere to put a message still has to be printable, and the caller holds a
   null session exactly then. */
const char *BkEditorLastMessage( BkEditorSession *session );

/* Starts the engine on a window the caller owns and keeps alive for the
   session. data_root is the directory holding Data and the shared libraries;
   null or "" means the running executable's own directory
   (NPlatform::Paths::BaseRoot), never the working directory. A relative
   data_root is taken relative to the working directory.

   window must not be null - that is BK_EDITOR_BAD_ARGUMENT, a caller bug.
   BK_EDITOR_NO_DEVICE means a real window on which the renderer would not
   start, which is what a runner without a GPU reports and a test may skip on.
   Keeping the two apart is deliberate: collapsing them would let a test that
   forgot its window skip on every machine unnoticed.

   On failure *out is set to whatever exists - null if it failed before
   allocating - so the caller can print BkEditorLastMessage(*out)
   unconditionally. */
BkEditorStatus BkEditorStart( void *window, const char *data_root, BkEditorSession **out );

/* What the caller needs to know about the map it just opened. */
typedef struct
{
	int width_tiles, height_tiles;
	int season;
	int player_count;
	int object_count;          /* objects + scenarioObjects, as read */
	int unknown_object_count;  /* in the map, not in the object database */
	int placed_object_count;   /* objects the engine actually holds, spans included */
	int bridge_span_count;     /* spans named by the map's bridges */
	int bridge_span_placed;    /* and how many of those the engine holds */
	int map_type;        /* nType */
	int attacking_side;  /* nAttackingSide */
} BkEditorMapSummary;

/* Opens a map and builds the engine state for it.

   path is an OS filesystem path ending in .bzm or .xml - what a file dialog
   hands back - and not a storage-relative name: the editor opens the file the
   user picked rather than the newer of a pair it inferred, which is what
   NMapFile::ReadNewest would do. It is still written with the engine's
   separator, because OpenFileStream splits on backslash only.

   An object whose type the database does not know is counted in
   unknown_object_count, kept in the snapshot and never placed, so it survives
   a save untouched. It is not an error: opening such a map is what crashes the
   MFC editor.

   out may be null if the caller only wants the status.

   A broken map is rejected as a whole. BK_EDITOR_BAD_ARGUMENT and
   BK_EDITOR_DATA_MISSING - no path, a file that is missing or will not read,
   or an engine that is not all there - change nothing: the map that was open
   stays open. BK_EDITOR_FAILED means the engine failed while the new map was
   being built into it; the engine is cleared and rebuilt in place, so the old
   map is gone too, and the session is left with no map open until the next
   successful open. */
BkEditorStatus BkEditorOpenMap( BkEditorSession *session, const char *path, BkEditorMapSummary *out );

/* Writes the open map to path; the format comes from the extension. Once the
   write itself succeeds, the map is read back from path and compared with
   what was meant, field by field; only then does this answer OK. A read
   failure or a difference is BK_EDITOR_FAILED with the reason in
   BkEditorLastMessage, and nothing at path is trusted to be right - which is
   why the editor always passes a temporary path here and swaps it over the
   real map itself only on success (D-19; the editor never hands this call
   the user's own map file to write into directly).

   What is written is the snapshot with the session's edits laid over it, never
   the engine's own copy: the engine's has UnpackFrameIndices applied, which
   picks a random visual variant per type, so writing it back would rewrite
   every frame index on the map. An object the database does not know goes out
   exactly as it came in.

   D-28: with a mod active (BkEditorSetMod), the written map's szMODName and
   szMODVersion are stamped from that mod's own name and version, the same
   way the MFC editor records its chosen mod (TemplateEditorFrame1.cpp) -
   only when they differ, so an already-matching map is not marked dirty by
   a save that changed nothing else. With no mod active the two fields are
   left exactly as they were read: this call never invents or clears a mod
   name the map did not already carry (the preservation invariant every
   other untouched field of the map keeps). */
BkEditorStatus BkEditorSaveMap( BkEditorSession *session, const char *path );

/* Closes the open map (File > Close, 03-15 gap fix): the world's objects and
   the terrain leave the scene, the AI editor is cleared, and every per-map
   table of the session is reset - CloseSessionMap, the same steps
   BkEditorSetMod takes before it swaps the object database, here without the
   swap. Nothing is saved: the editor has already asked about unsaved changes
   (D-23) before it calls this. The mod, the object database and the camera's
   yaw are left as they are. BK_EDITOR_OK with no map open too (nothing to
   close); BK_EDITOR_REFUSED only when the engine is not started. Every
   map-needing entry point answers "no map is open" afterwards, exactly as
   before the first BkEditorOpenMap. */
BkEditorStatus BkEditorCloseMap( BkEditorSession *session );

/* The edits. Each one changes the map and the engine together or neither: a
   refusal leaves the session exactly as it was, so the editor never saves
   something it did not show.

   BK_EDITOR_REFUSED means the map or the engine said no and the reason is in
   BkEditorLastMessage - an object still referred to by a bridge or a start
   command, or a position the engine will not put the object at. It is an
   ordinary answer, not a failure. (Since M2 a start command or reserve
   position no longer refuses a delete: see BkEditorDeleteObject.) BkEditorAddObject also refuses every type
   the catalogue marks not placeable - a single soldier among them, with a
   squad to place instead named in the message, and (M2, D-05) a bridge span,
   a trench piece and a fence, which the Bridge, Entrenchment and Fence tools
   draw as a whole. Such an object a loaded map already holds still loads,
   draws and moves.

   A link ID is the only name an object has here, and a map does not promise
   one per object: 0 means "no link ID", and shipped maps carry hundreds of
   terrain objects under it. Moving, turning, re-owning or deleting a link ID
   that more than one object of the map carries is refused, because the bridge
   cannot tell which of them would change; they are saved as they were read.

   Positions are floats because a map's are, in map units (see
   BkEditorScreenToWorld); the engine takes whole units and the bridge rounds
   once, on its way in. */
BkEditorStatus BkEditorAddObject( BkEditorSession *session, const char *name,
                                  float x, float y, int dir, int player, int *out_link_id );
/* All three at once. The single-field calls below are this one with the other
   two read out of the map, and a caller dragging an object while turning it
   wants the pair applied together rather than as two edits either of which can
   be refused on its own. If any part is refused, none of it is kept: the map
   and the engine both go back to what they were. */
BkEditorStatus BkEditorPlaceObject( BkEditorSession *session, int link_id,
                                    float x, float y, int dir, int player );
BkEditorStatus BkEditorMoveObject( BkEditorSession *session, int link_id, float x, float y );
/* The selection's group move (M3, D-25): every member of link_ids moved by
   ONE (dx, dy) delta, in MAP units - the same units every object position is
   in, and what the caller's drag pointer already answers - as ONE edit of the
   log. *out_token names it for BkEditorUndoEdit/RedoEdit, so a drag gesture
   (one call per frame, tokens merged by the caller) is one undo step, and its
   undo puts every member's whole record back raw - positions, directions,
   owners, the engine included. A squad member names the squad's own link ID
   (BkEditorObjectAt's rule), and a squad record re-places whole, so the
   soldiers keep their offsets by construction. BK_EDITOR_BAD_ARGUMENT for a
   null array, a count below one, a non-finite delta or a link ID named twice;
   BK_EDITOR_REFUSED, changing nothing, for a member that is an unknown type
   or shares its link ID, or whose destination is off the map - one bad member
   refuses the whole move (the M1 rule: the map never holds half a move). */
BkEditorStatus BkEditorMoveObjects( BkEditorSession *session, const int *link_ids, int count,
                                    float dx, float dy, int *out_token );
BkEditorStatus BkEditorTurnObject( BkEditorSession *session, int link_id, int dir );
BkEditorStatus BkEditorSetObjectPlayer( BkEditorSession *session, int link_id, int player );
/* Deletes the object as the MFC editor's delete does (D-04): it also leaves
   every start command's unit list (a command left with no unit is erased), is
   cleared from a start command's target (set to 0), and takes with it every
   reserve position naming it as artillery or truck. Reinforcement groups and
   the AI general's mobile reinforcements name SCRIPT IDs and are never edited.
   BK_EDITOR_OK, and BkEditorLastMessage then says what else changed ("also
   removed from start command 2; reserve position 1 erased") and notes a
   script ID a group or the AI general still names - empty when nothing but the
   object went. One call, one undo step: BkEditorRestoreObject puts back the
   object and every one of those changes exactly. BK_EDITOR_REFUSED, changing
   nothing, for the three things the game's loaders and M3's links depend on: a
   bridge span, a trench piece, and an object carrying a passenger; and for an
   object whose link ID other objects share. An object the database does not
   know CAN be deleted (05-05, D-33): Check Map's Fix all offers its removal
   explicitly, replacing the MFC's silent RemoveNonExistingObjects, and the
   restore brings the record back byte for byte - every other edit of one is
   still refused. */
BkEditorStatus BkEditorDeleteObject( BkEditorSession *session, int link_id );
/* Puts a deleted object back as it was: same record, same link ID, same place
   in its list, and in the engine where it stood. Undo of a delete, and redo of
   an add. BK_EDITOR_REFUSED when there is no such deleted object, when its
   link ID is in use again, or when the engine will not take the object back -
   and then nothing is restored, in the map or the engine. Deleted objects are
   forgotten by the next BkEditorOpenMap. */
BkEditorStatus BkEditorRestoreObject( BkEditorSession *session, int link_id );
/* value is 0 or 1, the two sides, or 2, neutral; anything else is
   BK_EDITOR_BAD_ARGUMENT. A player outside the table is BK_EDITOR_REFUSED. */
BkEditorStatus BkEditorSetDiplomacy( BkEditorSession *session, int player, int value );

/* What the engine is holding for an object, which is deliberately not read out
   of the map: it is how a caller - or a test - checks that the two agree.
   player is -1 where the object's kind has no owner, which is not the same as
   belonging to player -1. BK_EDITOR_REFUSED means the map may hold the object
   but the engine does not. */
typedef struct
{
	float x, y;
	int dir;
	int player;
} BkEditorObjectState;

BkEditorStatus BkEditorEngineObjectState( BkEditorSession *session, int link_id, BkEditorObjectState *out );

/* The placement rule of the MFC's placer (ObjectPlacerState.cpp:355-365),
   answered as a question so the caller snaps before it edits: where would
   FitVisOrigin2AIGrid put (x, y) for the object type `name`? Fit Objects To
   Grid off (the session's own flag) answers the input unchanged. So do the
   kinds the rule does not fit - units and squads (the MFC excepts them), and
   the segment-based kinds whose frame-less origin the base stats cannot
   answer (fence, entrenchment, bridge, terraobj: their GetOrigin overrides
   read a vector, and the MFC's own frame-less call reads beside it,
   ObjectPlacerState.cpp:360) - the tools fit buildings and generic objects.
   BK_EDITOR_REFUSED when the database does not know the name; either out
   pointer may be null. */
BkEditorStatus BkEditorSnapToGrid( BkEditorSession *session, const char *name, float x, float y, float *out_x, float *out_y );

/* The map as the bridge holds it - the snapshot with the session's edits in
   it - one record per object, objects before scenario objects, in file order.
   Like BkEditorCatalogue, out_count is always the total, and a buffer too
   short for it is BK_EDITOR_REFUSED with nothing written past capacity. */
typedef struct
{
	int link_id;
	char name[64];     /* truncated at 63, always terminated */
	float x, y;        /* the map's vPos, not the engine's */
	int dir;           /* nDir as the map holds it */
	int player;        /* the map's owner, which the engine may not share */
	int scenario;      /* 1: scenarioObjects, 0: objects */
	int known;         /* 0: the database does not know the type */
	int script_id;     /* the map's nScriptID: -1 none, else 0..32000 (04-09) */
	float hp;          /* the map's fHP, the record's own 0..1 (M3, D-26) */
	int frame_index;   /* the map's nFrameIndex: a squad's formation, a terraobj's segment (M3) */
	int link_with;     /* the map's link.nLinkWith: the host's link ID, 0 none (M3, D-27) */
} BkEditorObjectRecord;
BkEditorStatus BkEditorObjects( BkEditorSession *session, BkEditorObjectRecord *out, int capacity, int *out_count );

/* One player's entry in the diplomacy table: 0 and 1 are the two sides, 2 is
   neutral. A player outside the table is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorDiplomacy( BkEditorSession *session, int player, int *out_value );

/* Terrain. One paint command, however many cells the brush covered: the
   function runs once per command on the bridge's own copy - the same
   deterministic one the map file tier tests - and the region it touched is
   pushed into the engine. The engine is never the source of the saved terrain,
   because the engine's own update and that function agree only inside the
   region. */
/* No noise field. Whether a tile is noisy belongs to the tile in the tileset,
   not to the brush: the preprocessing pass both the map and the engine run ends
   in CTerrainBuilder::SetNoise, which writes HasNoise(tile) across the region
   whatever was there before (RandomMapGen/TerrainBuilder.cpp:257-264), and
   CTerrain::SetTile derives it too. A value passed in here would be discarded
   without a word, so the field is gone rather than ignored. */
typedef struct { int x, y; unsigned char tile; } BkEditorPaintCell;
/* out_token names this paint for BkEditorUndoPaint and BkEditorRedoPaint. The
   bridge keeps the order: undo takes the newest applied paint, redo the most
   recently undone, and a new paint drops everything undone. A token out of
   that order is BK_EDITOR_REFUSED. Undo puts back exactly the tiles and
   crosses the paint recorded, in the map and the engine; redo puts back
   exactly what the paint left - it does not paint again. out_token may be
   null; it is -1 when nothing was painted (count 0, or a refusal). A token is
   valid until the next BkEditorOpenMap, which forgets every paint of the map
   before and numbers the new map's paints from 0 again, so an old token may
   name a new paint and must not be used.
   Every cell's tile must be one the open map's tileset has a terrain type for
   (the shipped tilesets skip indices: 1 is in none of them). A cell naming any
   other tile is BK_EDITOR_BAD_ARGUMENT, with the tile in BkEditorLastMessage,
   and nothing is painted - the check runs over all cells before the terrain
   is touched. A cell off the map is BK_EDITOR_REFUSED, also before anything
   is painted. */
BkEditorStatus BkEditorPaint( BkEditorSession *session, const BkEditorPaintCell *cells, int count, int *out_token );
BkEditorStatus BkEditorUndoPaint( BkEditorSession *session, int token );
BkEditorStatus BkEditorRedoPaint( BkEditorSession *session, int token );
/* The tile the engine holds at a cell, as it draws it - for the engine tier
   and for an eyedropper. BK_EDITOR_REFUSED when no map is open or the cell is
   off the map. */
BkEditorStatus BkEditorEngineTile( BkEditorSession *session, int x, int y, unsigned char *out_tile );

/* Altitudes (M3, D-19): the terrain's vertex heights, editable as a region
   under the preservation invariant. The heights are WORLD z units, exactly
   what SVertexAltitude::fHeight holds; the shades are the bridge's own
   business - an edit sets the heights and runs the engine's shade recompute
   over the region grown by the shade kernel (one vertex per side), and undo
   restores the recorded region raw, so nothing outside it moves. */
/* Vertex indices, half-open: [x0, x1) x [y0, y1) in terrain-VERTEX
   coordinates - altitudes are indexed by terrain vertex, one more per axis
   than the map's tiles (a 512-tile map has 513 vertices per axis). This is
   the region BkEditorAltitudes reads and BkEditorSetAltitudes writes. */
typedef struct { int x0, y0, x1, y1; } BkEditorAltitudeRegion;
/* Two-pass read of the z values (world units) over the region, row-major.
   Like BkEditorObjects, out_count is always the total and a buffer too short
   for it is BK_EDITOR_REFUSED with nothing written past capacity; heights may
   be null when capacity is 0, to ask for the count. A null region, an empty
   or inverted one, or a count that does not match the region is
   BK_EDITOR_BAD_ARGUMENT; a region off the open map, or none open, is
   BK_EDITOR_REFUSED. */
BkEditorStatus BkEditorAltitudes( BkEditorSession *session, const BkEditorAltitudeRegion *region,
                                  float *heights, int capacity, int *out_count );
/* Sets the heights (world units) over the region, row-major: count must equal
   the region's vertex count. Every height must be finite. Null pointers, an
   empty or inverted region, a count mismatch or a non-finite height is
   BK_EDITOR_BAD_ARGUMENT and nothing changes; a region off the open map, or
   none open, is BK_EDITOR_REFUSED and nothing changes - not the map, not the
   engine, not the history. out_token names the edit for BkEditorUndoEdit and
   BkEditorRedoEdit (it may be null; it is -1 after a refusal or a failure).
   Undo puts back exactly what the edit's region - the edit rectangle grown
   by one vertex per side - held, so a redo of it restores the same bytes. */
BkEditorStatus BkEditorSetAltitudes( BkEditorSession *session, const BkEditorAltitudeRegion *region,
                                     const float *heights, int count, int *out_token );

/* File > New (M3, D-23): the engine builds a map in memory - CMapInfo::Create
   of the given size and season, every tile the season's most common tile,
   zero altitudes with the season's shades, the default diplomacies - and it
   opens as the session's map, never-saved (there is no path until the first
   BkEditorSaveMap; Save As is the caller's first save, exactly a shipped
   map's rule). The summary answers what the new map is. */
/* size_x and size_y are in PATCHES per axis, 1..32; season is 0..3
   (Summer/Winter/Africa/Spring); szName is the map's name (what a Save As
   starts from and the title shows - the map itself carries no name field);
   szModFolder is "" to keep the current mod (RMGC_CURRENT_MOD_FOLDER's own
   meaning), the literal "none" for no mod, or a bare folder name that
   BkEditorMods must list - a mod that is not the active one is switched to
   first, exactly BkEditorSetMod's own steps. */
typedef struct
{
	int size_x, size_y;
	int season;
	char szName[64];      /* truncated at 63, always terminated */
	char szModFolder[64]; /* "", "none", or a bare mod folder name */
} BkEditorNewMapParams;
/* A size outside 1..32, a season outside 0..3, a null params or out, or a
   mod folder that is neither "", "none" nor installed is BK_EDITOR_BAD_ARGUMENT
   (an unknown mod) or refused with the reason in BkEditorLastMessage, and
   nothing changes: not the session's map, not its mod, not the engine. The
   unsaved-changes question is the caller's (the MFC editor's NeedSaveChanges
   ran before its dialog); this entry builds, it does not ask. */
BkEditorStatus BkEditorNewMap( BkEditorSession *session, const BkEditorNewMapParams *params,
                                BkEditorMapSummary *out );

/* The Heights tool (M3, D-18): one stroke step - the DrawShadeState machine
   (DrawShadeState.cpp:186-336) riding the D-19 altitude region primitive.
   A stroke is a series of steps sharing click_x/click_y; stroke_start marks
   the first, which is where the session takes the click modes' frozen
   targets. Every step is one edit of the log (BkEditorUndoEdit/RedoEdit);
   the core merges a gesture's steps into one undo step. */
/* action: 0 raise, 1 lower, 2 level. level_mode: 0 zero, 1 click tile,
   2 instant average (the MFC's own default, LEVEL_TO_2), 3 click average.
   brush is the MFC slider's own 2..16; the pattern the bridge scales from
   editor\profile.tga spans brush*2 vertices per axis, its corner above-left
   of the cursor's tile by the MFC's own arithmetic. height_speed is the
   profile gradient's ceiling, WORLD z units; level_ratio_percent the level
   step, percent of the distance to the mode's target. pos_x/pos_y are the
   cursor now and click_x/click_y the stroke's start, both WORLD (Vis) units;
   ctrl_held keeps a height the IsValidHeight predicate refuses, the MFC's
   MK_CONTROL override. */
typedef struct
{
	int action;
	int level_mode;
	int brush;
	float height_speed;
	float level_ratio_percent;
	float pos_x, pos_y;
	float click_x, click_y;
	int stroke_start;
	int ctrl_held;
} BkEditorHeightsStrokeParams;
/* A null params or a brush outside 2..16 is BK_EDITOR_BAD_ARGUMENT; a cursor
   off the map, a click-tile stroke whose reference left the map, and the
   invalid-height rollback (the pattern subtracted back unless ctrl_held,
   exactly DrawShadeState.cpp:261) are BK_EDITOR_REFUSED with the reason in
   BkEditorLastMessage - a refused step changes nothing: not the map, not the
   engine, not the history. out_token may be null; it is -1 after a refusal
   or a failure. */
BkEditorStatus BkEditorHeightsStroke( BkEditorSession *session, const BkEditorHeightsStrokeParams *params,
                                      int *out_token );

/* Generate heights (M3, D-18, the MFC's Hills/Rocks/Dunes): the engine's own
   noise - NPerlinNoise::Init, a CHField of the altitudes' own size,
   fBmDefVals[type] with featSize = granularity - with every altitude scaled
   into [min_z, max_z] by the MFC's formula (TabTerrainAltitudesDialog.cpp:330-346).
   type: 0 TG_FBM (Hills), 3 TG_HYBRID (Rocks), 4 TG_RIDGED (Dunes) - the
   hidden MULTI/HETERO radios are not features. The confirmation is the
   caller's. One edit of the log over the whole vertex sheet. min_z and max_z
   are WORLD z units per vertex (the MFC's fParameters[2]/[3]). A type outside
   the three, a non-finite float or a granularity <= 0 is BK_EDITOR_BAD_ARGUMENT;
   no map open is BK_EDITOR_REFUSED; out_token as BkEditorHeightsStroke's. */
BkEditorStatus BkEditorGenerateHeights( BkEditorSession *session, int type, float granularity,
                                        float min_z, float max_z, int *out_token );

/* Set Zero (M3, D-18): every height to 0 with the shades recomputed, one edit
   of the log over the whole vertex sheet. The confirmation is the caller's.
   BK_EDITOR_REFUSED when no map is open; out_token as BkEditorHeightsStroke's. */
BkEditorStatus BkEditorSetZeroHeights( BkEditorSession *session, int *out_token );

/* Update Map (M3, D-20, Ctrl+U in the MFC editor): the OnButtonUpdate
   composite - the session layer's UpdateMapInSession - as ONE undoable edit - the engine's own UpdateAllHeights and
   UpdateTerrain, the full crosses and shades recompute, the roads'/rivers'/
   sounds' z refresh, and when Fit Objects To Grid is on the snap of every
   sprite object with passability. progress_fn (which may be null) is called
   with (step, total, user) once per step - total is the MFC's own count,
   7 + the snapped objects - and must not call back into this bridge. Undo
   restores everything the composite captured, raw. BK_EDITOR_REFUSED when no
   map is open or the tileset has no terrain types; out_token as
   BkEditorHeightsStroke's. */
typedef void (*BkEditorProgressFn)( int step, int total, void *user );
BkEditorStatus BkEditorUpdateMap( BkEditorSession *session, BkEditorProgressFn progress_fn, void *user,
                                  int *out_token );

/* Fill Entire Map (M3, D-22): every tile becomes the terrain type tile_index's
   own (what a paint of it writes), the crosses recomputed over the whole map -
   ONE undoable paint of the log, the session layer's FillEntireMapInSession.
   The confirmation is the caller's. A
   tile_index the map's tileset has no terrain type for is BK_EDITOR_REFUSED
   (a paint's own rule), changing nothing. The MFC's update-rect typo
   (TemplateEditorFrame1.cpp:4951) is not copied: the region is the full map. */
BkEditorStatus BkEditorFillEntireMap( BkEditorSession *session, int tile_index, int *out_token );

/* The terrain-mode toggles (M3, D-20): Instant Update Map Mode (0/1 - off by
   default, the MFC's own initial state) runs the objects-Z refresh over every
   height stroke; Fit Objects To Grid (0/1 - ON by default, the MFC's own)
   snaps non-unit objects on place and move. A view setting: no map data, no
   history, kept until the next call. A null session is BK_EDITOR_NO_SESSION. */
BkEditorStatus BkEditorSetTerrainModes( BkEditorSession *session, int instant_update, int fit_to_grid );

/* The Layers menu (M3, D-32): what the renderer draws. One value per MFC
   toggle (TemplateEditorFrame1.cpp:5318-5777). Terrain, Grid, Terrain Noise,
   Black Stripes, Units, Objects, Bounding Boxes, Shadows, Haze, War Fog and
   Depth Complexity are IScene::ToggleShow flags; Wire Frame is
   IGFX::SetWireframe, not a scene flag (the renderer's fill mode, applied
   inside every frame - IGFX::SetWireframe is a render state and the GPU
   renderer refuses one outside a scene); Units Passability is the world's
   ToggleAIInfo (the passability marks the terrain draws); Unit Fire Ranges is
   not a toggle but a mode (BkEditorSetFireRangeMode) - its bit in the state
   reads says whether any range is shown. The numbers are the bit positions of
   BkEditorLayers and the order of the core's Layer enum: they never change. */
typedef enum
{
	BK_EDITOR_LAYER_TERRAIN = 0,
	BK_EDITOR_LAYER_GRID = 1,
	BK_EDITOR_LAYER_WIREFRAME = 2,
	BK_EDITOR_LAYER_DEPTH_COMPLEXITY = 3,
	BK_EDITOR_LAYER_TERRAIN_NOISE = 4,
	BK_EDITOR_LAYER_BLACK_STRIPES = 5,
	BK_EDITOR_LAYER_UNITS = 6,
	BK_EDITOR_LAYER_OBJECTS = 7,
	BK_EDITOR_LAYER_BOUNDING_BOXES = 8,
	BK_EDITOR_LAYER_SHADOWS = 9,
	BK_EDITOR_LAYER_HAZE = 10,
	BK_EDITOR_LAYER_WAR_FOG = 11,
	BK_EDITOR_LAYER_UNITS_PASSABILITY = 12,
	BK_EDITOR_LAYER_UNIT_FIRE_RANGES = 13,
	BK_EDITOR_LAYER_COUNT = 14
} BkEditorLayer;

/* BkEditorSetFireRangeMode's modes. */
#define BK_EDITOR_FIRE_OFF      0
#define BK_EDITOR_FIRE_SELECTED 1
#define BK_EDITOR_FIRE_FILTER   2

/* Shows or hides one layer. Renderer state only: no map data is touched, the
   document is never dirtied, nothing is recorded in the history - the shape of
   BkEditorSetMapType, which the engine has no say in either. shown is 0/1.
   The engine is driven to the wanted state whatever it was (IScene::ToggleShow
   flips and answers the new state, so it is asked at most twice), and the
   state is remembered: every map the session opens or creates afterwards
   comes up with it re-applied, which the MFC editor did not do (its menu check
   marks and the scene's own flags drifted apart across an open).
   BK_EDITOR_BAD_ARGUMENT for a layer outside 0..BK_EDITOR_LAYER_COUNT-1 or for
   BK_EDITOR_LAYER_UNIT_FIRE_RANGES (that is a mode, not a toggle).
   BK_EDITOR_REFUSED with no map open, or for a layer the renderer cannot draw
   (BkEditorLayers' mask) - nothing changes then. */
BkEditorStatus BkEditorSetLayerShow( BkEditorSession *session, int layer, int shown );

/* IGFX::SetWireframe, as the Layers menu's Wire Frame: BkEditorSetLayerShow's
   BK_EDITOR_LAYER_WIREFRAME with the MFC's own name. */
BkEditorStatus BkEditorSetWireframe( BkEditorSession *session, int on );

/* The layer state the session holds: out_bits has one bit per BkEditorLayer
   (bit n is layer n, set = shown), out_mask the layers the session can
   actually drive in this renderer (a layer outside it is refused by
   BkEditorSetLayerShow and its bit never changes - the Layers menu greys it).
   Both fixed-size, no map need be open; either pointer may be null, a null
   session is BK_EDITOR_NO_SESSION. */
BkEditorStatus BkEditorLayers( BkEditorSession *session, unsigned *out_bits, unsigned *out_mask );

/* Unit Fire Ranges (MFC ShowFireRange, TemplateEditorFrame1.cpp:2115): the
   shoot areas of a group of units are shown. mode is BK_EDITOR_FIRE_OFF (every
   range hidden), BK_EDITOR_FIRE_SELECTED (the units named in link_ids - the
   caller's selection; only the infantry and vehicles among them, as the MFC
   registered) or BK_EDITOR_FIRE_FILTER (every infantry or vehicle of the map
   whose database path the named filter passes; filter_name is a name from
   BkEditorObjectFilters as the files hold them - shipped and user). The
   previous group is dropped first, as the MFC did; a mode that finds no unit
   shows nothing, and that is no error. link_ids/count name the selection for
   BK_EDITOR_FIRE_SELECTED and are ignored by the other modes (link IDs the map
   does not hold, or that are no unit, are skipped). The layer's bit in
   BkEditorLayers is set while the mode is not off. Re-asked after every open or
   new map by the caller (the AI forgets its groups with the map).
   BK_EDITOR_BAD_ARGUMENT for a mode outside 0..2, a negative count, a null
   link_ids with a positive count, or - in filter mode - a null, empty or
   unknown filter name (the message names it); nothing changes then.
   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorSetFireRangeMode( BkEditorSession *session, int mode, const char *filter_name, const int *link_ids, int count );

/* The tiles BkEditorPaint takes on the open map: every index its tileset has a
   terrain type for, once each, ascending - what a brush's palette offers.
   Like BkEditorObjects, out_count is always the total, and a buffer too short
   for it is BK_EDITOR_REFUSED with nothing written past capacity; out may be
   null when capacity is 0, to ask for the count. BK_EDITOR_REFUSED too when
   no map is open. */
BkEditorStatus BkEditorTilesetTiles( BkEditorSession *session, unsigned char *out, int capacity, int *out_count );

/* One tile of the open map's tileset, for the Brush's tile picker (03-15 gap
   fix). terrain is the name (<name>, STerrTypeDesc::szName) of the first
   terrain type of the tileset's description that lists the tile - "Snow",
   "Ice", "Asphalt" - and terrain_index that terrain type's position in the
   description, so a picker can group tiles by it in the tileset's own order.
   tileset is the tileset's own name in the data storage
   (STerrainInfo::szTilesetDesc, e.g. "terrain\sets\2\tileset"): the same
   tile index in another tileset is another picture, so a caller keys a cache
   of BkEditorTilePicture's pictures on it. variant_count is the terrain
   type's own count of tiles (<tiles>, STileTypeDesc::tiles) - the tile
   properties the MFC's palette context menu shows
   (TabTileEditDialog.cpp:317-318). Both strings are cut to fit and
   always NUL-terminated.

   BK_EDITOR_BAD_ARGUMENT for a null out or a tile outside 0..255 (a paint
   cell's tile is an unsigned char). BK_EDITOR_REFUSED when no map is open, or
   for a tile no terrain type of the tileset lists - one BkEditorTilesetTiles
   does not offer and BkEditorPaint refuses; out is zeroed then. Every index
   the tileset lists answers, tile 0 included: the MFC's `> 0` guard
   (TabTileEditDialog.cpp:316), which left its first tile without properties,
   is not copied. */
typedef struct { int terrain_index; int variant_count; char terrain[64]; char tileset[128]; } BkEditorTile;
BkEditorStatus BkEditorDescribeTile( BkEditorSession *session, int tile, BkEditorTile *out );

/* One tile's picture, for the Brush's tile picker (03-15 gap fix): the tile's
   diamond cut out of the tileset's texture, the way the MFC editor's tile
   palette cut its thumbnails (MapEditor/TabTileEditDialog.cpp,
   CreateImageList) - the cell the tileset description's four corners
   (<tilemaps>, STileMapsDesc: maps0 top, maps1 right, maps2 left, maps3
   bottom) span, flipped the way a tile whose corners name the cell the other
   way round is drawn, and transparent outside the diamond (the MFC palette
   masked it with editor\terrain\tilemask.tga to the same shape). RGBA8,
   top row first - BkEditorObjectPicture's layout - scaled down only when a
   side exceeds max_side, keeping the shape (a shipped tile is 64x32).

   The corners are the description's own, read from the tileset's .xml, not
   the engine's loaded copy: CTerrain::LoadLocal pulls those in by a few
   texels that depend on the screen's width (CorrectUVMaps), which would make
   the picture's size depend on the window. The texture is the tileset's
   "_h.dds" (the uncompressed one the MFC palette and the minimap builder
   read), else "_c.dds", else "_l.dds". The decoded texture and the
   description are kept for the session, for the tileset last asked about,
   so a picker asking for every tile decodes the texture once; BkEditorSetMod
   drops them (the same name may be another file under the new mod).

   BK_EDITOR_BAD_ARGUMENT for a null output, max_side outside 8..256 or a tile
   outside 0..255. BK_EDITOR_REFUSED when no map is open, for a tile the
   tileset does not list (see BkEditorDescribeTile), when the tileset's .xml
   or texture will not load, or when capacity_bytes is too small for the
   picture - *out_width/*out_height are still set to the real size then, as
   BkEditorObjectPicture does, and nothing is written. BK_EDITOR_FAILED when
   the picture could not be cut or scaled. */
BkEditorStatus BkEditorTilePicture( BkEditorSession *session, int tile,
                                    unsigned char *out_rgba, int capacity_bytes, int max_side,
                                    int *out_width, int *out_height );

/* The Minimap panel and Create Minimap Images (M3 05-07, D-14..D-17). Every
   read here is a view over the session's map: none of them touches the map
   document, the history or the engine's state (the engine tier asserts the
   map is not dirty after each). All the lists are two-pass like
   BkEditorObjects: out_count is always the total, a buffer too short for it is
   BK_EDITOR_REFUSED with nothing written past capacity, and out may be null
   when capacity is 0, to ask for the count - after the sizing pass's
   BK_EDITOR_REFUSED "count returned", read only that many. */

/* Tile indices, half-open [x0, x1) x [y0, y1) in TILE coordinates (a 512-tile
   map is 512 wide), row-major with row 0 at the TOP of the map: tile row 0 is
   the map's far (highest world y) edge, exactly as BkEditorWorldToTile and
   BkEditorEngineTile number the rows. What is read is the map as it will be
   saved (the session's snapshot), which the engine's own terrain agrees with
   (BkEditorTerrainMatchesEngine). A null region, an empty or inverted one, or
   an area over 2^31 is BK_EDITOR_BAD_ARGUMENT; a region off the map, or no map
   open, is BK_EDITOR_REFUSED. */
typedef struct { int x0, y0, x1, y1; } BkEditorTileRegion;
BkEditorStatus BkEditorTiles( BkEditorSession *session, const BkEditorTileRegion *region,
                              unsigned char *out_tiles, int capacity, int *out_count );

/* The live minimap's terrain colour of every tile index the open map's tileset
   has (out_count is the tileset's tilemap count, at most 256): 0x00RRGGBB, the
   average of the tileset texture's UV rectangle (the "_h.dds" the MFC editor's
   minimap and CMapInfo::CreateMiniMapImage both sample, else "_c" or "_l")
   with CreateMiniMapImage's own arithmetic. The MFC panel colours a tile by its
   TERRAIN TYPE (CMiniMapTerrain::UpdateColor), so tile t answers the average
   of the first tile of the first terrain type that lists t; a tile no terrain
   type lists answers its own average. The texture and description come from
   BkEditorTilePicture's per-tileset cache. */
BkEditorStatus BkEditorMinimapTileColors( BkEditorSession *session, unsigned int *out_rgb, int capacity, int *out_count );

/* One marker the MFC minimap draws for a map object (MiniMapTypes.cpp,
   CUnitsSelection::Update): the object's AI-tile rectangle - half-open
   [x0, x1) x [y0, y1), in AI tiles (two per terrain tile per axis, y up from
   the map's south edge, clamped to the map) - its passability rectangle, or
   five AI tiles square around its position when it has none or is smaller;
   color_index 0..16 indexes the 17-colour player table (a player outside
   0..16 is 16); squad is 1 for a squad's own marker. Soldiers a squad
   carries are not objects of the map, so a squad is the one marker for them;
   an object whose type the database does not know has none. */
typedef struct { int link_id; int x0, y0, x1, y1; int color_index; int squad; } BkEditorMinimapUnit;
BkEditorStatus BkEditorMinimapUnits( BkEditorSession *session, BkEditorMinimapUnit *out, int capacity, int *out_count );

/* The fire-range areas the AI shows now (IAILogic::UpdateShootAreas - what
   CScene::SetAreas copies for the MFC panel): empty unless a group of units
   has its areas on, which is what the Layers menu's fire-range layer does.
   Centre and radii are AI (map) units (64 per terrain tile, y up from the
   south edge); kind is SShootArea::EShootAreaType (0 ballistic, 1 anti-air,
   2 line - never answered, the MFC skips it - 3 range); angles are the
   engine's 0..65535 turns, equal when the area is a full circle; rgb is
   SShootArea::GetColor() as 0x00RRGGBB. */
typedef struct { int kind; float cx, cy, radius, min_radius; int start_angle, finish_angle; unsigned int rgb; } BkEditorMinimapArea;
BkEditorStatus BkEditorMinimapAreas( BkEditorSession *session, BkEditorMinimapArea *out, int capacity, int *out_count );

/* The map's own pre-built minimap picture, for Game mode (D-16): map_path is
   the map file's path (the one BkEditorOpenMap was given, any separator) -
   <map>.tga is tried first, then <map>_h.dds, through the engine's image
   decoders, never a decoder of the caller's. RGBA8, top row first, scaled
   down only when a side exceeds max_side (BkEditorTilePicture's layout and
   REFUSED-with-the-size rules). BK_EDITOR_REFUSED when the map has no
   picture or it will not decode (a malformed image is a refusal, never a
   crash); BK_EDITOR_BAD_ARGUMENT for a null pointer or max_side outside
   8..2048. */
BkEditorStatus BkEditorMinimapImage( BkEditorSession *session, const char *map_path,
                                     unsigned char *out_rgba, int capacity_bytes, int max_side,
                                     int *out_width, int *out_height );

/* Map > Create Minimap Images (D-17): CMapInfo::CreateMiniMapImage on the SAVED
   map file at map_path, one call with the MFC's own four image parameters
   (TemplateEditorFrame1.cpp:300): <map>_large at 512x512 and <map> at 256x256,
   each as DDS (the engine's "_c/_l/_h" trio) and as TGA, written beside the
   map file. Afterwards every file is verified to exist and to have the size
   asked for; a missing or wrong one is BK_EDITOR_FAILED naming it. The map
   document is never touched (not dirty, no history).
   map_path must be the full path of a .bzm or .xml that exists and is not
   inside the installation's Data folder - the shipped data is never written -
   else BK_EDITOR_REFUSED naming why (the editor routes a shipped or
   never-saved map through Save As first). */
BkEditorStatus BkEditorCreateMiniMapImage( BkEditorSession *session, const char *map_path );

/* A world point (world units, not map units) to the tile it falls in - the
   brush's other half, through the engine's own conversion. Screen to world
   is BkEditorScreenToWorld; the two compose. BK_EDITOR_REFUSED means the
   point is not on the map. */
BkEditorStatus BkEditorWorldToTile( BkEditorSession *session, float wx, float wy, int *out_x, int *out_y );

/* Compares the engine's terrain against the copy that will be saved, and names
   the first difference in BkEditorLastMessage. For the engine tier: it walks
   the whole map, and the editor has no reason to call it. */
BkEditorStatus BkEditorTerrainMatchesEngine( BkEditorSession *session );

/* Compares what the engine draws against the objects the map holds, and names
   the first difference in BkEditorLastMessage: a drawn unit or squad that is
   no object of the map, or an object of the map that is not drawn - and the
   engine's link table: an object's link ID names it there, a deleted one's
   names nothing. For the engine tier, like BkEditorTerrainMatchesEngine. */
BkEditorStatus BkEditorWorldMatchesMap( BkEditorSession *session );

/* What the editor can place. name is a fixed buffer rather than a pointer, so
   nothing crosses the ABI that the caller has to free; a key longer than 63
   characters is truncated. out_count is always what the database holds, not
   how many fitted, so a caller given BK_EDITOR_REFUSED for a short buffer can
   size one and ask again.

   placeable is 1 when BkEditorAddObject takes the type and 0 when it refuses
   it whatever the position: a sound or a tank pit, which no map holds, and a
   single soldier (every infantry SGVOGT_UNIT), which the game plays only
   inside a squad - a map's infantry is its SGVOGT_SQUAD records. The MFC
   editor's palette never offered a single soldier either
   (TabSimpleObjectsDialog.cpp CommonFilterName drops units\Humans). Since M2
   a bridge span (6), a trench piece (4) and a fence (9) are 0 as well: each
   only makes sense inside its bridge, trench or fence run, which its own tool
   draws. */
/* One placeable-or-not object of the database. `path` is the object's
   szPath - the lowercased folder path with its trailing separator
   (GameDB.cpp lowercases at load) - which is what the object filters match
   against, exactly the MFC editor's own argument to FilterName
   (MiniMapTypes.cpp:205 passes pDesc->szPath; the palette list itself was
   built from szPath, TabSimpleObjectsDialog.cpp:718). `name` (the key) is
   what the palette places by. A path longer than the buffer is truncated
   like a long name. */
typedef struct { char name[64]; char path[128]; int game_type; int placeable; } BkEditorCatalogueEntry;
BkEditorStatus BkEditorCatalogue( BkEditorSession *session, BkEditorCatalogueEntry *out, int capacity, int *out_count );

/* D-29: the palette's own picture for one object - the icon.tga in its
   data-storage folder (<szPath>\icon.tga, opened through the data storage so
   a mounted mod's own icon wins), the same file the MFC editor's palette
   loaded (TabSimpleObjectsDialog.cpp:716-760), decoded with the engine's own
   image processor and, only when larger, scaled down keeping the aspect to
   fit within max_side pixels on each side (IImageProcessor::CreateScaleBySize,
   ISM_LANCZOS3) - never upscaled past its own size. Written into out_rgba as
   RGBA8, top row first, exactly *out_width * *out_height * 4 bytes.

   User-requested addition (03-09 Task 4): a single soldier with no icon.tga
   of its own (e.g. Allies_Bren) borrows the icon.tga of a squad that lists it
   as a member (e.g. gb_bren_43, RPGStats.h SSquadRPGStats::memberNames, read
   from every SGVOGT_SQUAD object's own data) - built once per session
   (SEditorSession::squadIconOwnerBySoldier) since it scans the whole object
   database. Deterministic when more than one squad lists the same soldier:
   the alphabetically first squad name wins. Still BK_EDITOR_REFUSED for a
   name no squad lists either (terrain pieces, effects, the entrenchment, the
   single-unit-formation squad type itself).

   BK_EDITOR_BAD_ARGUMENT for a null name or output, max_side outside 8..256,
   or a name the object database does not know. BK_EDITOR_REFUSED naming the
   object when neither it nor a squad that lists it has an icon.tga - not
   every shipped object or squad has one, and this is the ordinary way of
   saying so, not a failure - or when capacity_bytes is too small for the
   decoded picture: *out_width/*out_height are still set to the real size (so
   a caller can size a buffer and ask again) but nothing is written.
   BK_EDITOR_REFUSED too when the engine is not started, with the sizes left
   at 0. */
BkEditorStatus BkEditorObjectPicture( BkEditorSession *session, const char *name,
                                      unsigned char *out_rgba, int capacity_bytes, int max_side,
                                      int *out_width, int *out_height );

/* A mod as BkEditorMods lists it, or BkEditorActiveMod reports it: folder is
   the directory name under <BaseRoot>mods, exactly as it is on disk (never
   lower-cased - BkEditorSetMod keeps it as given, the same way
   BkEditorTestMapPath's own mod_folder argument does; the game's -mod=
   parser lower-cases its own copy for the generated-data key, a step this
   struct has no part in). name and version are mod.xml's own MODName and
   MODVersion, read the same way the game's mod-list screen reads them
   (GameTT/InterfaceIMModsList.cpp). */
typedef struct { char folder[64]; char name[64]; char version[32]; } BkEditorMod;

/* Every installed mod: a directory under <BaseRoot>mods whose data holds a
   mod.xml (STORAGE_TYPE_COMMON over "data\*.pak", the same pattern
   BkEditorSetMod mounts), sorted by folder name. A directory with no such
   mod.xml is not a mod and is left out - it is not an error, since a mods
   folder may hold anything.

   Like BkEditorCatalogue, out_count is always the total, and a buffer too
   short for it is BK_EDITOR_REFUSED with nothing written past capacity; out
   may be null when capacity is 0, to ask for the count. BK_EDITOR_REFUSED
   too when the engine is not started. A missing mods directory - a fresh
   installation with none installed - is zero mods, not a refusal. */
BkEditorStatus BkEditorMods( BkEditorSession *session, BkEditorMod *out, int capacity, int *out_count );

/* Mounts folder's data as the MOD storage and reloads the object database
   from it, mirroring CICChangeMOD::Exec (Main/MainLoopCommands.cpp:391-431)
   without the main loop it has none of: the open map is closed first (its
   object database is about to change from under it - nothing here saves
   it), the MOD storage is swapped, FilesInspector re-inspects the new
   storage set, the shared managers other than IGFX are cleared the way
   CMainLoop::ClearResources(true) clears them (IGFX::Clear and the font
   SetFont it restores are skipped on purpose: the editor's own overlay
   lives on that device, and this call never owns a window to redraw), and
   IObjectsDB::LoadDB rebuilds the catalogue from the new storage set.

   folder is a bare directory name under <BaseRoot>mods, as BkEditorMods
   lists it - never a path: a separator ('/' or '\\'), ".", "..", over 63
   characters, or one that fails NPlatform::Paths::IsRelativeDataName is
   BK_EDITOR_BAD_ARGUMENT, and nothing changes. null or "" clears the mod (the
   base game) - always BK_EDITOR_OK, since there is nothing to validate. A
   folder that does not exist, or whose data has no mod.xml, is
   BK_EDITOR_REFUSED naming the folder in BkEditorLastMessage, and - because
   this check runs before anything is touched - the session's mod, its open
   map and the object database are all left exactly as they were.

   Never calls IUserProfile::SetMOD: the editor has no game profile of its
   own to remember a mod in, and a refused switch must never look like it
   changed the player's real profile. BK_EDITOR_REFUSED too when the engine
   is not started. */
BkEditorStatus BkEditorSetMod( BkEditorSession *session, const char *folder );

/* The session's active mod - folder[0] == 0, name and version empty, when
   none is active. BK_EDITOR_REFUSED means the engine is not started; out is
   then zeroed. BK_EDITOR_BAD_ARGUMENT for a null out. */
BkEditorStatus BkEditorActiveMod( BkEditorSession *session, BkEditorMod *out );

/* The two host roots the editor started on: NPlatform::Paths::BaseRoot() (the
   installation - Data, the modules) and UserRoot() (where the profile,
   config and cache live), each with the OS's own separator and a trailing
   one, as NPlatform::Paths itself returns them - not the engine's backslash
   form BkEditorOpenMap and BkEditorTestMapPath take.

   BK_EDITOR_REFUSED with "the engine is not started" before BkEditorStart
   has succeeded: the roots are whatever BkEditorStart set them to, and
   before that they are either unset or left over from another caller.
   BK_EDITOR_REFUSED too - never truncated - when a root does not fit the
   fixed buffer; BK_EDITOR_BAD_ARGUMENT for a null out. */
typedef struct { char base_root[1024]; char user_root[1024]; } BkEditorPathSet;
BkEditorStatus BkEditorPaths( BkEditorSession *session, BkEditorPathSet *out );

/* Where a test-launch copy of the current map goes so the game finds it: the
   profile's own generated-data root for the given mod
   (NProfile::GeneratedDirectory + NGeneratedData::ModKey, lower-cased first -
   see GeneratedData.h), then "maps", then file_name - written in the
   engine's form (backslashes), because that is what BkEditorSaveMap and
   Game's own command line take. mod_folder may be null or "" for the base
   game, exactly as NGeneratedData::ModKey reads it.

   file_name must be a bare name (no '/' or '\\'), pass
   NPlatform::Paths::IsRelativeDataName and end in ".bzm" - anything else,
   including an empty name or profile, is BK_EDITOR_BAD_ARGUMENT with nothing
   engine-specific about it: this call reaches no engine state at all, only
   NPlatform::Paths and NProfile's own sanitizers, reused rather than
   re-derived (security: path traversal through a profile, mod or file name
   the caller did not choose).

   On success the directories exist (created if they did not) and a stale
   sibling with the same stem and the other extension (.xml) has been
   removed, because the game loads the newer of a same-stem .xml/.bzm pair
   (GameTT/iMissionInternal.cpp) - a leftover from an older test copy must
   never outrank the one this call is about to write. BK_EDITOR_REFUSED, with
   out[0] left at 0, when capacity is too short for the path; nothing is
   created or removed in that case. */
BkEditorStatus BkEditorTestMapPath( BkEditorSession *session, const char *profile, const char *mod_folder,
                                    const char *file_name, char *out, int capacity );

/* The camera, placed in world units, and one frame drawn into the window the
   session was started on. BkEditorOpenMap places the camera on the map's
   middle, so a frame before the first BkEditorSetCamera already looks at the
   map; before any map is open the camera is CCamera's default placement.
   BkEditorResize places the camera again at its current anchor.
   BK_EDITOR_REFUSED from BkEditorFrame is a device that would not begin a
   scene, which is a thing that happens rather than a bug. */
BkEditorStatus BkEditorSetCamera( BkEditorSession *session, float wx, float wy );
BkEditorStatus BkEditorFrame( BkEditorSession *session );

/* The zoom is not the camera's distance: it is NSceneScreenScale's global-var
   driven orthographic rescale (Scene/SceneScreenScale.h), the same one the
   game's Mission screen drives through GFX.World.ZoomSteps. anchor_x/anchor_y
   are world units (ICamera::GetAnchor - what BkEditorSetCamera places).
   zoom_steps is the step count as applied - already clamped to
   [0, max_zoom_steps] for the window's current size, so a stale count from
   before a resize never reads back out of range. scale is screen pixels per
   unzoomed pixel (NSceneScreenScale::GetGameplayScale): 1.0 at zoom_steps 0.
   yaw_degrees is the camera's yaw in degrees - the game's own 45 plus
   whatever BkEditorSetYaw last set (D-12), 45 until then. */
typedef struct
{
	float anchor_x, anchor_y;
	int zoom_steps;
	int max_zoom_steps;
	float scale;
	float yaw_degrees;
} BkEditorView;

/* BK_EDITOR_REFUSED means the engine is not started; out is then left zeroed. */
BkEditorStatus BkEditorViewState( BkEditorSession *session, BkEditorView *out );

/* Zooms by delta_steps steps (positive in, negative out), anchored so the
   world point under the screen point (sx, sy) - screen pixels - stays under
   it: the game's own CInterfaceMission::ApplyZoomStep recipe. The result is
   clamped to [0, the window's current max_zoom_steps] - out to the unzoomed
   view, in no further than the game ever goes - so a request past either end
   is not an error; BkEditorViewState says whether it clamped. sx, sy off the
   window still zoom, anchored at whatever GetPos3 answers for that point.
   BK_EDITOR_REFUSED with no map open; BK_EDITOR_BAD_ARGUMENT for a
   non-finite sx or sy. */
BkEditorStatus BkEditorZoomAt( BkEditorSession *session, int delta_steps, float sx, float sy );

/* The same recipe with an absolute step count instead of a delta, anchored at
   the screen's centre - for Home/Reset view (D-13) and for restoring a
   session-remembered view (D-15). Same clamp and refusal rules as
   BkEditorZoomAt. */
BkEditorStatus BkEditorSetZoom( BkEditorSession *session, int steps );

/* D-12: degrees of yaw offset from the game's own 45 - the camera keeps the
   game's pitch and distance, only the yaw turns. Wrapped into [0, 360) rather
   than refused, since every value names a real angle; a non-finite degrees is
   BK_EDITOR_BAD_ARGUMENT. Re-places the camera at its current anchor
   (ICamera::GetAnchor) with the new yaw and runs ITerrain::ResetPosition, the
   same pair BkEditorResize does after a placement change - a stale terrain
   layout is what left the ground thousands of pixels away once before
   (session.cpp's SetSessionCamera comment). BkEditorViewState's yaw_degrees
   reports 45 + this offset.

   The terrain is laid out on a fixed isometric screen grid
   (Scene/TerrainInternal.cpp, CTerrain::MovePatches) and buildings/infantry
   are single-direction billboard sprites (Main/GameDB.h), so only offset 0 is
   correct: measured (engine-tier TestYawMeasurement, 03-06-SUMMARY.md) at 30,
   90, 180 and 270 on coldwinter, neither one follows the camera - the ground
   quad stays fixed in screen space and is progressively clipped away by the
   yaw (0.5% black at +0 rising to 99.2% at +180), while the sprites stay
   upright and in their pre-rotation screen positions, floating with no
   visible ground once the terrain clips out from under them. Picking still
   agreed with the terrain for every offset (2-3 of 2-3 on-screen objects each
   time), because both walk the same unrotated projection - so the mismatch is
   real but not something today's tests based on picking alone would catch.
   Recorded in 03-06-SUMMARY.md, along with whether this call ships rotation
   input in M1, is re-planned as engine work, or is deferred (the plan's
   checkpoint). BK_EDITOR_REFUSED means the engine is not started. */
BkEditorStatus BkEditorSetYaw( BkEditorSession *session, float degrees );

/* The editor's own drawing - its ImGui - goes into the engine's frame rather
   than into a renderer of its own. overlay runs on the thread that calls
   BkEditorFrame, inside the engine's Flip, after the scene and before present,
   with that frame's SDL_GPUCommandBuffer and colour target (an SDL_GPUTexture
   of width x height pixels). No render pass is open: the callback opens and
   ends its own. It must not call back into the bridge. A null overlay
   removes it, and BkEditorStop removes it too, since the renderer outlives the
   session and the callback and user data die with the caller's own state.
   BK_EDITOR_REFUSED means the engine is not started or its renderer has no
   such hook. */
typedef void (*BkEditorOverlay)( void *user, void *command_buffer, void *target, unsigned int width, unsigned int height );
BkEditorStatus BkEditorSetOverlay( BkEditorSession *session, BkEditorOverlay overlay, void *user );

/* The engine's SDL_GPUDevice and the SDL_GPUTextureFormat of the overlay's
   target, for building the overlay's pipelines against the device that will
   draw them. BK_EDITOR_REFUSED means the renderer has no SDL GPU device; both
   outputs are then null and 0. */
BkEditorStatus BkEditorGpuDevice( BkEditorSession *session, void **out_device, unsigned int *out_format );

/* The screen is the window: BkEditorStart sets the engine's mode to the
   window's size, so a mouse position is a screen position with no scale. Call
   BkEditorResize after the window's size changed. The screen then becomes the
   window's current size in points - which is pixels, because the editor's
   window has no high pixel density - the projection is set again, and the
   camera is placed again at its anchor (its distance depends on the screen's
   height), as the game does after a resolution change. The
   window itself is left alone: never moved to another display, sized or
   shown again, and no frame is presented. width and height must be the
   window's size as the caller just saw it; anything else is
   BK_EDITOR_BAD_ARGUMENT and changes nothing, since a caller out of step with
   its window would put the screen and the mouse out of step too.
   BK_EDITOR_FAILED means the renderer would not follow the window (inside a
   frame, or a renderer that cannot). */
BkEditorStatus BkEditorResize( BkEditorSession *session, int width, int height );
/* The size the engine draws at, which BkEditorScreenToWorld and
   BkEditorObjectAt take their points in. */
BkEditorStatus BkEditorScreenSize( BkEditorSession *session, int *out_width, int *out_height );

/* Draws one frame, as BkEditorFrame does, and writes it to path_tga as it was
   presented - the scene with the overlay over it - as an uncompressed 32-bit
   TGA of the screen's size, top row first, alpha opaque. For the engine tier
   and the app's own check, which have no other way to see the overlay; the
   editor does not call it. BK_EDITOR_REFUSED is a device that would not begin
   a scene or a renderer that cannot capture; BK_EDITOR_FAILED a file that
   would not write. */
BkEditorStatus BkEditorCaptureFrame( BkEditorSession *session, const char *path_tga );

/* The object under a screen point, as a link ID. Bridges and entrenchments
   are passed over, as the MFC editor passes them over
   (TemplateEditorFrame1.cpp:3384-3400): they are edited as wholes in M2.
   A soldier is drawn and picked on his own but the map holds his squad, so a
   click on a soldier answers with his squad's link ID, as the MFC editor
   selects the whole squad (ObjectPlacerState.cpp:409). BK_EDITOR_REFUSED
   means nothing pickable is there. */
BkEditorStatus BkEditorObjectAt( BkEditorSession *session, float sx, float sy, int *out_link_id );

/* The rubber band's pick (M3, D-25): the link IDs of every object the
   screen rectangle (window pixels, any two opposite corners, normalized
   here) selects - the scene's own rectangle pick, the MFC rubber band's own
   rule, which takes an object only when the CENTRE of its picture (a
   sprite's picture box, a mesh's bounding-sphere centre) lies inside the
   rectangle; a picture that merely meets it, as BkEditorObjectAt's point
   pick would take, is not selected. The picks' filters are ObjectAt's: a
   soldier answers his squad's link ID (so a band over one soldier selects
   the whole squad), bridges and entrenchments answer nothing (M2 edits them
   as wholes), objects held back by Hide checked are not there, and
   duplicates are answered once. The window's gameplay scale (a larger
   window, a zoom step) is undone around each picture as the point pick
   undoes it, so the band answers the same at any window size. Two-pass, like
   BkEditorObjects: *out_count is always the total, and a buffer too short
   is BK_EDITOR_REFUSED with nothing written past capacity. The order is the
   pick's own, which the Selector's cycle walks. */
BkEditorStatus BkEditorPickObjects( BkEditorSession *session, float sx0, float sy0, float sx1, float sy1,
                                    int *out_link_ids, int capacity, int *out_count );
/* The Ctrl rubber band's pick (M3, D-25): the link IDs of every editable
   record whose tile position falls inside the rectangle of tiles, bridges
   and entrenchments passed over per D-25 - the game type is filtered at
   pick time, where the engine can answer it. The tiles are the world-cell
   tiles BkEditorWorldToTile answers (y measured from the terrain's far
   edge), and a record is inside when the engine's own GetTileIndex of its
   drawn position lands in the rectangle - the conversion is the engine's own
   both ways. An object of a type the database does not know is passed over:
   it is kept as it is and cannot be moved, so selecting it would promise an
   edit the bridge refuses. Two-pass, like BkEditorPickObjects. */
BkEditorStatus BkEditorPickObjectsInTiles( BkEditorSession *session, int tx0, int ty0, int tx1, int ty1,
                                           int *out_link_ids, int capacity, int *out_count );

/* A screen point to the world point under it, against the terrain the camera
   is looking at - so it wants a camera that has been placed. Composes with
   BkEditorWorldToTile to turn a click into a cell.

   Two units cross this ABI, and a point in one is not a point in the other.
   World units are the scene's: the camera (BkEditorSetCamera), this call and
   BkEditorWorldToTile. Map units are the file's and the AI's: every object
   position - BkEditorAddObject, BkEditorPlaceObject and the calls built on it,
   BkEditorObjects, BkEditorEngineObjectState. A map unit is sqrt 2 world
   units' worth smaller (AI2Vis, Formats/fmtTerrain.h), so a click's world
   point handed straight to BkEditorAddObject places the object at 0.7 of
   the way from the map's corner to where it was clicked - off the screen,
   drawn and picked there, which is how plan 5's smoke first met it.
   BkEditorWorldToMap is the one conversion. */
BkEditorStatus BkEditorScreenToWorld( BkEditorSession *session, float sx, float sy, float *wx, float *wy );

/* The other direction of BkEditorScreenToWorld: a world point (world units,
   not map units) to the screen point (pixels) it draws at, at whatever zoom
   is set now - through the same projection BkEditorFrame draws with. z is 0,
   matching BkEditorScreenToWorld's own convention: the ray-cast against the
   real terrain height GetPos3 tries first does not resolve in this bridge's
   headless session (no CMainLoop, no running mission - measured: it always
   falls through to the z=0-plane algebraic fallback), so BkEditorScreenToWorld's
   x,y already assume z=0 - composing the two at a nonzero height would not
   round-trip, and would draw off the ground a click actually resolves
   against. BK_EDITOR_REFUSED with no map open or no camera; BK_EDITOR_BAD_ARGUMENT
   for a null output or a non-finite wx or wy. */
BkEditorStatus BkEditorWorldToScreen( BkEditorSession *session, float wx, float wy, float *sx, float *sy );

/* A world point as the map position an object placed there takes, through
   the engine's own conversion (Vis2AIFast). Unrounded: the object calls
   round once, on their way in. Needs no map and no camera, and cannot be
   refused; only a missing out pointer is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorWorldToMap( BkEditorSession *session, float wx, float wy, float *mx, float *my );

/* The map's own two fields, not a player's: nType is the mission kind and
   nAttackingSide is which side attacks in it. The engine has no say in either,
   so they take no engine call and cannot be refused. The attacking side is 0
   or 1; anything else is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorSetMapType( BkEditorSession *session, int type );
BkEditorStatus BkEditorSetAttackingSide( BkEditorSession *session, int side );

/* The map's own sound list.

   Ground-truth correction (03-10): the map keeps two structurally similar
   but different sound lists. CMapInfo::soundsList (Formats/fmtSound.h's
   CMapSoundInfo - name and position only) is what the MFC editor's own
   sound dialog and IScene::InitMapSounds read - but CMapInfo::operator&
   (RandomMapGen/MapInfo_Methods.cpp, both the binary and the XML tree
   writer) never serialises it, and nothing in this codebase ever populates
   it from a loaded file (confirmed: MapFile/MapEquivalence.cpp's own
   comment on CompareMap says so outright - "soundsList is not serialised -
   CMapInfo::operator& writes `sounds` and derives this"). The field this
   bridge reads and writes is CMapInfo::sounds.sounds - a
   std::vector<SMapSoundInfo> (fmtMap.h:92-110, name/position/repeat/random
   repeat/mute/min+max radius) - saved under tag 17 ("MapSounds" in the XML
   tree), the one that actually round-trips through a save and reload. The
   engine is never told, the same as BkEditorSetMapType: nothing in this
   bridge's headless session ever starts a mission, which is the only time
   InitMapSounds (and so soundsList) matters. The game reads sounds.sounds
   only there: at mission start GameTT/iMissionInternal.cpp appends each
   entry's name and position to soundsList (03-15 gap fix - before it
   nothing read this list, and a placed sound was silent in the game).
   Repeat, random repeat, mute and the radii have no reader: the sound
   scene's map sounds follow the sound's own entry and its own timing.

   Positions are world (scene) units, not map units: unlike an object's
   vPos (BkEditorAddObject, converted through AI2Vis on its way into the
   engine), a sound's vPos is written and read back raw - matching the MFC
   editor's own (dead but explicit) marker code, which moved a sound's scene
   object straight to vPos with no conversion (TemplateEditorFrame1.cpp,
   markers placed at vPos in the scene). Radii are in vis tiles (fmtMap.h's
   own comment on nMinRadius/nMaxRadius); times are milliseconds
   (NTimer::STime, a DWORD). */
typedef struct
{
	char name[64];
	float x, y, z;
	int repeat_ms, repeat_random_ms;
	int mute_in_combat;
	int min_radius, max_radius;
} BkEditorSoundRecord;

/* The snapshot's sound list, in file order. Like BkEditorObjects, out_count
   is always the total, and a buffer too short for it is BK_EDITOR_REFUSED
   with nothing written past capacity; out may be null when capacity is 0,
   to ask for the count. BK_EDITOR_REFUSED too when no map is open. */
BkEditorStatus BkEditorSounds( BkEditorSession *session, BkEditorSoundRecord *out, int capacity, int *out_count );

/* Adds one sound to the snapshot and the working copy together; the engine
   is untouched (see above). index is where it lands in the list: 0..count
   inserts there, -1 appends. Any other index, or a null record, is
   BK_EDITOR_BAD_ARGUMENT.

   record->name must be null-terminated within its 64 bytes (an unterminated
   name is BK_EDITOR_BAD_ARGUMENT, like a name over 63 characters) and must
   name a sound the object database knows - game type 100, SGVOGT_SOUND -
   or this is BK_EDITOR_REFUSED naming it: neither an unknown name nor an
   object of some other game type is a sound. A non-finite x, y or z is
   BK_EDITOR_BAD_ARGUMENT. record->x/y must land on the map (through the
   engine's own tile lookup, the same oracle BkEditorWorldToTile uses) or
   this is BK_EDITOR_REFUSED. A negative repeat_ms, repeat_random_ms,
   min_radius or max_radius, or a min_radius above max_radius, is
   BK_EDITOR_REFUSED. Every field of the record is written; there is
   nothing in the map's own sound record this struct does not already
   carry. A refusal changes nothing: not the snapshot, not the working
   copy.

   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorAddSound( BkEditorSession *session, int index, const BkEditorSoundRecord *record );

/* Replaces the sound at index in both copies together, the engine
   untouched. Same field rules as BkEditorAddSound. index outside
   0..count-1, or a null record, is BK_EDITOR_BAD_ARGUMENT. A refusal
   changes nothing. */
BkEditorStatus BkEditorSetSound( BkEditorSession *session, int index, const BkEditorSoundRecord *record );

/* Removes the sound at index from both copies together, the engine
   untouched. index outside 0..count-1 is BK_EDITOR_BAD_ARGUMENT.
   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorDeleteSound( BkEditorSession *session, int index );

/* M2 records (phase 4). Every record collection the editor edits below the
   object level goes through one small set of calls that read and put whole
   records, so the core's generic record command (record_edit: the record
   before and the record after) can undo an edit by putting the old record back
   through the same call. Units, for every M2 record: object positions, areas,
   start-command targets, reserve positions and parcels are MAP (AI) units;
   camera positions, camera anchors, sounds and road and river points are WORLD
   (Vis) units - BkEditorWorldToMap converts one way and is unrounded, the MFC
   editor's Vis2AI truncates with +0.3. */

typedef struct { float x, y, z; } BkEditorVec3;

/* The map's camera anchors, world units. (0, 0, 0) is the file's VNULL3 and
   means "not set": the game starts the camera at players[user] and falls back
   to neutral when that slot is unset or absent. player_count is the size of
   the map's playersCameraAnchors vector, 0..32; slots at or above it are
   zero. The editor never resizes the vector on open (the MFC editor did; not
   copied), so player_count is what the file had until an edit grows it. */
typedef struct
{
	BkEditorVec3 neutral;
	int player_count;
	BkEditorVec3 players[32];
} BkEditorCameraAnchorRecord;

/* The snapshot's camera anchors. BK_EDITOR_BAD_ARGUMENT for a null out;
   BK_EDITOR_REFUSED with no map open, and for a file whose vector holds more
   than 32 entries ("this map has more camera anchors than the editor edits":
   it saves byte-exact untouched). */
BkEditorStatus BkEditorCameraAnchors( BkEditorSession *session, BkEditorCameraAnchorRecord *out );

/* An exact put of the anchors into the snapshot and the working copy together:
   the vector becomes exactly player_count long, so an undo can restore a
   shorter vector than a set grew. Growing it (pad with unset slots, never
   shrink) is the caller's rule when it builds a new value. The engine is
   untouched: anchors matter only when a mission starts. A null record,
   player_count outside 0..32 or a non-finite coordinate is
   BK_EDITOR_BAD_ARGUMENT. A slot the call changes that is not unset must lie
   on the map or this is BK_EDITOR_REFUSED naming the slot. BK_EDITOR_REFUSED
   with no map open. A refusal changes nothing: not the snapshot, not the
   working copy. */
BkEditorStatus BkEditorSetCameraAnchors( BkEditorSession *session, const BkEditorCameraAnchorRecord *anchors );

/* The terrain's height at a world point (x, y), into *z, in world units:
   CVSOBuilder::UpdateZ on the working copy's altitudes, so a road, a river
   and an anchor all take the same z. A point off the map is BK_EDITOR_REFUSED;
   a null z is BK_EDITOR_BAD_ARGUMENT; BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorGroundHeight( BkEditorSession *session, float x, float y, float *z );

/* An object's script ID (04-09, D-15): -1 means none, otherwise 0..32000. It is
   what a reinforcement group names and what a Lua script finds the object by.
   The snapshot and the working copy change together and the engine is left
   alone: the AI takes a script ID only when an object is added, at mission
   start, and IAIEditor has no setter (C7), so a test reads the value back by
   saving, reopening and asking IAIEditor::GetObjectScriptID. An unknown link
   ID, link ID 0 (no link), a link ID more than one object carries, and a value
   outside -1..32000 are BK_EDITOR_REFUSED, naming why, and change nothing.
   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorSetObjectScriptID( BkEditorSession *session, int link_id, int script_id );

/* The properties' fields (M3, D-26): the masked fields of *edit applied to
   ONE object's record, both copies, the engine re-placed - ONE edit of the
   log, so one deactivate commit is one undo step. Mask bits: 1 player, 2 hp
   (the record's own 0..1; the properties' percent is the caller's scale),
   4 angle in DEGREES (the MFC properties dialog's unit, turned into the
   record's direction with the MFC's own formula), 8 formation (the squad
   record's frame index; refused for any other kind - a unit's frame index is
   its segment index, never a formation). The flag swap rides the player bit:
   a FLAG re-owned to player N becomes Flag_<the map's unit-creation party's
   general side> (partys.xml names the general side; "neutral" when the map's
   unit creation or the table is silent), the MFC properties' own swap
   (SEditorMApObject.cpp:426-470) - the record renames in place and the engine
   re-places it under the new type. BK_EDITOR_BAD_ARGUMENT for a null edit or
   a mask naming nothing; BK_EDITOR_REFUSED, changing nothing, for a record
   that cannot be edited (unknown or shared link ID), a player outside the
   diplomacy table, a non-finite hp or angle, a negative formation, a
   formation on a kind that carries none, or a flag type the database does
   not know. */
typedef struct
{
	int mask;          /* 1 player, 2 hp, 4 angle, 8 formation */
	int player;        /* 0..diplomacies-1 */
	float hp;          /* the record's own 0..1 */
	float angle;       /* degrees */
	int formation;     /* the squad's formation index, 0 or greater */
} BkEditorObjectFieldsEdit;
BkEditorStatus BkEditorSetObjectFields( BkEditorSession *session, int link_id, const BkEditorObjectFieldsEdit *edit, int *out_token );

/* CheckForInserting's rules (ObjectPlacerState.cpp:1325-1424) answered as a
   question (M3, D-27): can `source` link to `target`, and as what? *out_type
   is 0 garrison, 1 train coupling, 2 tow. Infantry only as passengers; a
   building needs its stats and a free slot (shoot slots + rest + medical); a
   trench piece takes infantry (the MFC's own checks are commented out there,
   ObjectPlacerState.cpp:1358-1372); a vehicle needs an entrance point and
   passenger room; a tractor or carrier tows an artillery gun with crew points
   it out-pulls; train cars couple with train cars. A read: nothing changes.
   BK_EDITOR_REFUSED names the rule that said no. */
BkEditorStatus BkEditorCanLink( BkEditorSession *session, int source, int target, int *out_type );

/* The drop's link (M3, D-27): the passenger record's nLinkWith becomes the
   host's link ID on both copies - the engine's garrison follows when the map
   loads, exactly the MFC editor's own save/load route - and a garrison moves
   the passenger beside the host (the MFC's GetCenter - 30, +30,
   ObjectPlacerState.cpp:827-831). ONE edit of the log. Refused with
   BkEditorCanLink's reason, changing nothing; the same refusals as the
   fields edit for a record that cannot be edited. */
BkEditorStatus BkEditorSetLink( BkEditorSession *session, int source, int target, int *out_token );

/* The properties' units list unlink (M3, D-27): the record's nLinkWith back
   to 0 (a palette-placed object is linked with nothing), both copies, the
   engine re-placed, ONE edit of the log. An already-unlinked object answers
   OK with *out_token -1. */
BkEditorStatus BkEditorUnlink( BkEditorSession *session, int link_id, int *out_token );

/* The Damage tool's hit (M3, D-29): the record's fHP moves by `delta` (the
   tool's percentage/100) with the MFC MapToolState's own clamps - 1.0 at the
   top, and a floor of 0.01 for a technics or a human object (a unit; its
   mesh or sprite kind answers IsTechnics/IsHuman), 0 for anything else -
   `mode` 0 damages (left click), 1 heals (right), 2 repairs to full
   (middle); a UNIT keeps 1% under any damage (D-29's own floor - the MFC's
   IsTechnics/IsHuman split the unit kind by vis type, and both are units),
   anything else may be hit to 0. The engine's live object takes the same
   share of its fMaxHP through IAIEditor::DamageObject. ONE edit of the log;
   the undo re-places the whole record, engine included. A squad floors at
   1% like a unit (the MFC damaged its soldiers, which IsHuman floors). A
   record with no engine object of its own carrying stats is REFUSED naming
   the stats - one the engine never took (a kind the engine does not place,
   a position off the terrain) or a missing stats pointer: the MFC's
   unguarded FindByVis result and pTmp->pRPG dereference
   (MapToolState.cpp:54-57) are NOT copied. A record
   the editor cannot edit is REFUSED too, and a percentage out of 0..1 is
   BK_EDITOR_BAD_ARGUMENT. The clamps leaving nothing to change answers OK
   with *out_token -1. */
BkEditorStatus BkEditorDamageObject( BkEditorSession *session, int link_id, float delta, int mode, int *out_token );

/* Players (M3, D-30). The map's diplomacy table holds one entry per player and
   the neutral player LAST: 0 and 1 are the two sides, 2 the neutral; the most
   a map holds is 16 players and the neutral (17 entries), the fewest two
   players and the neutral.

   BkEditorAddPlayer adds a player of `side` (0 or 1) just before the neutral
   entry, the MFC dialog's own insert: the new player takes the neutral's
   index, the neutral moves up by one and so do the objects of the neutral (an
   owner index at or above the old neutral's moves up, so the neutral's objects
   stay the neutral's). The player's unit creation and camera anchor are added
   when the file held entries at that place to shift.
   BkEditorDeletePlayer deletes player `player` (0 .. entries-2: never the
   neutral): the players above it move down by one with their unit creation,
   camera anchors and objects, and the deleted player's objects become the
   neutral's. Each is ONE edit of the log (out_token names it for
   BkEditorUndoEdit/RedoEdit; -1 after a refusal): undo puts the diplomacies,
   the unit creation, the camera anchors and every re-owned object back, raw,
   byte for byte. A flag that changes owner follows the properties' own swap
   (Flag_<the party's general side>). BK_EDITOR_REFUSED, changing nothing, for
   a side outside 0..1, a table already at 17 entries (or one that would fall
   below 3), the neutral entry or a player out of range; BK_EDITOR_BAD_ARGUMENT
   for a null out_token; BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorAddPlayer( BkEditorSession *session, int side, int *out_token );
BkEditorStatus BkEditorDeletePlayer( BkEditorSession *session, int player, int *out_token );

/* One player's Unit Creation Info (M3, D-30; the MFC's "Map Unit Creation
   Property", one entry of the map's SUnitCreationInfo): the party (a name of
   partys.xml), five aviation slots in the file's order - scouts, fighters,
   paradroppers, bombers, attack planes - each an aircraft name with a
   formation size and a plane count, the paratroop squad with its count, the
   relax time in seconds and the appear points. Appear points are MAP (AI)
   units, as the file holds them (the MFC's points list shows them divided by
   64). slot_count is the size of the map's unit-creation vector, 0..16:
   a put makes the vector exactly that long (an undo restores the old size
   byte for byte), so a caller raising it for a player the vector does not hold
   yet sets slot_count to at least player + 1; a put whose slot_count does not
   reach the player only sets the size (the entry is not stored, and not
   checked: it is the defaults a read of that player answered). */
typedef struct { char name[64]; int formation_size; int count; } BkEditorUcAircraft;
typedef struct
{
	int slot_count;
	char party[64];
	BkEditorUcAircraft aircraft[5];
	char paratroop_name[64];
	int paratroop_count;
	int relax_time;
	int appear_count;      /* 0..32 */
	BkEditorVec3 appear[32];
} BkEditorUnitCreationRecord;

/* The snapshot's unit creation of `player` (0..15). A player the vector does
   not hold yet reads as the defaults the game's own Validate fills in (and
   slot_count says how long the vector is). BK_EDITOR_REFUSED, naming why, for
   a map whose entry does not fit the record (a name of 64 characters or more,
   more than 32 appear points, a vector longer than 16) - it saves byte-exact
   untouched - and for a player outside 0..15; BK_EDITOR_BAD_ARGUMENT for a null
   out; BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorUnitCreation( BkEditorSession *session, int player, BkEditorUnitCreationRecord *out );

/* An exact put of one player's unit creation into the snapshot and the working
   copy together, validated like the MFC's MutableValidate and the manipulators'
   combos: the party must be in partys.xml, an aircraft a unit of an
   "aviation" folder of the object database, the paratroop squad a squad of a
   "squads" folder, a formation size 1..32, a plane and a paratroop count
   0..255, the relax time 1 or more (the file reader turns 0 into 20), every
   appear point on the map. A value the file held when the map was opened (or
   the entry holds now) is always accepted, so an undo of an edit of a file's
   own odd data cannot fail. The refusal names the field. The engine is
   untouched: unit creation matters only when a mission starts. BK_EDITOR_REFUSED
   changes nothing; BK_EDITOR_BAD_ARGUMENT for a null record, a player outside
   0..15, a slot_count outside 0..16, counts
   outside the record's arrays, or a non-finite point or unterminated name. */
BkEditorStatus BkEditorSetUnitCreation( BkEditorSession *session, int player, const BkEditorUnitCreationRecord *record );

/* The names a unit-creation field chooses from (M3, D-30): kind 0 the parties
   of partys.xml, 1 the aircraft (every unit of an "aviation" folder of the
   object database), 2 the paratroop squads (every squad of a "squads"
   folder), each in the order the source lists them. Two-pass like
   BkEditorListRmg: out_count is always the total, a capacity below it is
   BK_EDITOR_REFUSED after writing what fits, out may be null with capacity 0.
   BK_EDITOR_BAD_ARGUMENT for a null out_count, a negative capacity or a kind
   outside 0..2. */
typedef struct { char name[64]; } BkEditorUcName;
BkEditorStatus BkEditorUnitCreationChoices( BkEditorSession *session, int kind, BkEditorUcName *out, int capacity, int *out_count );

/* The map's script file (04-10, D-20): CMapInfo::szScriptFile, the name of the
   Lua file the game loads from the map's own folder (the game adds ".lua"). The
   MFC editor stored a bare name; empty means None. */
typedef struct { char name[64]; } BkEditorScriptFileRecord;

/* The snapshot's script file name, NUL-terminated in out->name. A value that
   does not fit (64 characters or more) is BK_EDITOR_REFUSED naming why: the file
   keeps it byte-exact and it is not editable. BK_EDITOR_BAD_ARGUMENT for a null
   out; BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorScriptFile( BkEditorSession *session, BkEditorScriptFileRecord *out );

/* An exact put of the script file name into the snapshot and the working copy
   together; the engine is untouched (the script loads only when a mission
   starts). The name must be NUL-terminated within the 64 bytes
   (BK_EDITOR_BAD_ARGUMENT otherwise, and for a null record). A value NEW to the
   map must be empty (None) or a bare name - letters, digits, '_', '-' and '.'
   only, no folder, no ".lua", the rule NMapRecords::IsBareScriptName holds - or
   this is BK_EDITOR_REFUSED "a script is named without folder or .lua". The one
   exception is the value the file held when it was opened, whatever it is: an
   undo must be able to put a verbatim path back. A refusal changes nothing.
   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorSetScriptFile( BkEditorSession *session, const BkEditorScriptFileRecord *record );

/* Script areas (04-10, D-21): the map's scriptAreas, a list the game's Lua finds
   by NAME (GetScriptAreaParams). The record holds the area as the file does, in
   MAP (AI) units: type 0 a rectangle (centre cx, cy and half size hx, hy), 1 a
   circle (centre cx, cy and radius r) - the file's own enum values. The index is
   the area's place in the list; a new area appends, nothing is renumbered. */
typedef struct
{
	char name[64];
	int type;
	float cx, cy, hx, hy, r;
} BkEditorScriptAreaRecord;

/* The snapshot's script areas in file order. out_count is always the total; a
   capacity below it is BK_EDITOR_REFUSED after writing what fits, never past
   capacity; out may be null with capacity 0 to ask for the total. A map with an
   area whose name does not fit the record (64 characters or more) is
   BK_EDITOR_REFUSED naming why: it saves byte-exact while nobody edits it.
   BK_EDITOR_BAD_ARGUMENT for a null out_count or a negative capacity;
   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorScriptAreas( BkEditorSession *session, BkEditorScriptAreaRecord *out, int capacity, int *out_count );

/* Inserts an area at index (0..count; -1 appends), into the snapshot and the
   working copy together; the engine is untouched (an area matters only when a
   mission starts). Names are non-empty and unique, compared case-sensitively -
   the game keys areas by name and a duplicate would silently replace the first
   (Pitfall 14): an empty name is BK_EDITOR_REFUSED "an area needs a name", a name
   another area holds "an area named <n> exists". The two exceptions are a name
   the file itself held twice when it was opened (an undo may put such an area
   back) and, for a set, the area's own name. A centre off the map and a negative
   size are BK_EDITOR_REFUSED; a null record, an unterminated name, a type other
   than 0 and 1, a non-finite number or an index out of range is
   BK_EDITOR_BAD_ARGUMENT. A refusal changes nothing. The values are stored as
   given: convert a drag with BkEditorScriptAreaFromVis first. */
BkEditorStatus BkEditorAddScriptArea( BkEditorSession *session, int index, const BkEditorScriptAreaRecord *record );

/* Replaces the area at index (0..count-1) with record, by the same rules; a
   centre that has not changed is not checked against the map, so a file's own
   odd area can be edited and put back. */
BkEditorStatus BkEditorSetScriptArea( BkEditorSession *session, int index, const BkEditorScriptAreaRecord *record );

/* Removes the area at index; the areas after it move down one. An index out of
   range is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorDeleteScriptArea( BkEditorSession *session, int index );

/* The conversions of a drag and of a handle, no map touched (D-21): the MFC
   editor's rule, NMapGeometry::AreaFromVis, MoveArea and ResizeArea - world (Vis)
   units in, AI units out, the truncation with Vis2AI's +0.3 applied once.
   FromVis: type 0 takes the rectangle of a drag from (wx0, wy0) to (wx1, wy1)
   (centre the middle, half size half the extent), type 1 the circle (centre
   the first point, radius the distance to the last); name may be empty here.
   Moved: the area with its centre at world point (wx, wy). Resized: the area
   with its rectangle corner or circle edge at world point (wx, wy). A null
   pointer, an unterminated name, a type other than 0 and 1, a non-finite
   number or a world point beyond +-1e6 is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorScriptAreaFromVis( BkEditorSession *session, int type, float wx0, float wy0, float wx1, float wy1,
                                          const char *name, BkEditorScriptAreaRecord *out );
BkEditorStatus BkEditorScriptAreaMoved( BkEditorSession *session, const BkEditorScriptAreaRecord *area, float wx, float wy,
                                        BkEditorScriptAreaRecord *out );
BkEditorStatus BkEditorScriptAreaResized( BkEditorSession *session, const BkEditorScriptAreaRecord *area, float wx, float wy,
                                          BkEditorScriptAreaRecord *out );

/* Start commands (04-11, D-17): the map's startCommandsList, orders the game
   gives units when the mission starts (CAILogic::InitStartCommands). A command
   names its units by link ID (a soldier stands for his squad, which is what the
   map holds), an action type from Data/Editor/actions.ini, and a target: a
   unit's link ID (link_id, 0 for none - never a reference) or a point (x, y, MAP
   (AI) units). from_explosion is the file's own field, kept as it is: a set never
   changes it. The index is the command's place in the list; a new command
   appends and nothing is renumbered. */

/* One action type of Data/Editor/actions.ini, in the order the file lists it (a
   name a file repeats appears once, with its last value, as the MFC editor's
   list has it). */
typedef struct { char name[64]; int id; } BkEditorActionCommand;

/* The action types, and in *out_default_index the entry the MFC editor starts a
   new command at (entry 9, STOP; the last entry when the file lists fewer).
   out_count is always the total; a capacity below it is BK_EDITOR_REFUSED after
   writing what fits, never past capacity; out may be null with capacity 0 to
   ask for the total. A file that is not in the data (or lists nothing) is
   BK_EDITOR_REFUSED naming why, with out_count 0. BK_EDITOR_BAD_ARGUMENT for a
   null out_count or out_default_index or a negative capacity. */
BkEditorStatus BkEditorActionCommands( BkEditorSession *session, BkEditorActionCommand *out, int capacity, int *out_count,
                                       int *out_default_index );

typedef struct
{
	int cmd_type;
	int link_id;
	float x, y;
	int from_explosion;
	float number;
	int unit_count;
} BkEditorStartCommandRecord;

/* How many start commands the snapshot holds. BK_EDITOR_BAD_ARGUMENT for a null
   out; BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorStartCommandCount( BkEditorSession *session, int *out_count );

/* The command at index (0..count-1) and its unit link IDs, in the file's order.
   out->unit_count is always the total; a units capacity below it is
   BK_EDITOR_REFUSED after writing what fits, never past capacity (units may be
   null with unit_cap 0 to ask for the total: that sizing pass is REFUSED when
   the command has units, as a buffer too short is). An index out of range, a
   negative unit_cap or a null out is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorStartCommand( BkEditorSession *session, int index, BkEditorStartCommandRecord *out, int *units, int unit_cap );

/* Inserts a command at index (0..count; -1 appends) into the snapshot and the
   working copy together; the engine is untouched (commands run only when a
   mission starts). units holds record->unit_count link IDs. Rules, each a
   BK_EDITOR_REFUSED naming why with nothing changed: at least one unit; every
   unit a link ID above 0 naming an object of the objects or scenarioObjects
   lists that is a unit or a squad the database knows; no unit twice; link_id 0 or
   an existing object; cmd_type one of the listed action types (the list missing
   refuses); x and y on the map. A command the file itself held when it was
   opened is exempt from the rules: an undo of a delete puts back whatever was
   deleted. The record's from_explosion is stored as given (an undo needs it
   back). A unit that carries a script ID a reinforcement group holds - the game
   holds it back until a script brings it in - is not a refusal: the call answers
   BK_EDITOR_OK and BkEditorLastMessage names it (assumption A3: the game may
   dereference a held-back unit). A null record, a unit_count above 4096 or
   below 0, a null units with a count above 0, a non-finite number, a
   from_explosion other than 0 and 1 or an index out of range is
   BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorAddStartCommand( BkEditorSession *session, int index, const BkEditorStartCommandRecord *record, const int *units );

/* Replaces the command at index by the same rules, except that only what the set
   changes is judged (a file's own odd command can be edited and put back), and
   that from_explosion is taken from the command already there, never from the
   record (D-17). */
BkEditorStatus BkEditorSetStartCommand( BkEditorSession *session, int index, const BkEditorStartCommandRecord *record, const int *units );

/* Removes the command at index; the ones after it move down one. An index out of
   range is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorDeleteStartCommand( BkEditorSession *session, int index );

/* Reserve positions (04-11, D-18): the map's reservePositionsList, where the game
   puts an artillery unit when a mission starts (CAILogic::InitReservePositions,
   which casts the link to a unit, so a squad or a non-unit would crash it). A
   position names the gun (artillery_link_id) and, for a towed gun, the truck
   that tows it (truck_link_id, 0 for none - never a reference), and a place
   (x, y, MAP (AI) units). The index is the position's place in the list; a new
   position appends (the MFC editor pushed it in front; the game keys them by
   link ID, so the order does not matter). */

/* What an object type can be in a reserve position, from its stats as the MFC
   editor's ObjectPlacerState classifies them: 0 nothing, 1 a self-propelled
   gun (a self-propelled or armoured unit, a super train, or an artillery piece
   without crew places), 2 a towed gun (an artillery piece with crew places), 3 a
   truck able to tow (a carrier or a tractor). A name the database does not know,
   a squad and anything else is 0 with BK_EDITOR_OK. BK_EDITOR_BAD_ARGUMENT for
   a null name or role. */
BkEditorStatus BkEditorReserveRole( BkEditorSession *session, const char *object_name, int *out_role );

typedef struct
{
	int artillery_link_id;
	int truck_link_id;
	float x, y;
} BkEditorReservePositionRecord;

/* How many reserve positions the snapshot holds. */
BkEditorStatus BkEditorReservePositionCount( BkEditorSession *session, int *out_count );

/* The position at index. An index out of range or a null out is
   BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorReservePosition( BkEditorSession *session, int index, BkEditorReservePositionRecord *out );

/* Inserts a position at index (0..count; -1 appends), both copies together, the
   engine untouched. ValidateReservePosition's rules, each a BK_EDITOR_REFUSED
   naming why with nothing changed: the gun a link ID above 0 naming a unit of the
   map (a squad and any non-unit are refused, in either role) whose role is 1 or
   2; a towed gun needs a truck ("a towed gun needs a truck"), a self-propelled
   gun takes none; a truck a unit of role 3 that the MFC editor's towing check
   passes (its towing force above the gun's weight); a position with both link IDs
   0 and link ID 0 as the gun; x and y on the map. A position the file held when
   it was opened is exempt, as for start commands. A null record, a non-finite
   number or an index out of range is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorAddReservePosition( BkEditorSession *session, int index, const BkEditorReservePositionRecord *record );

/* Replaces the position at index by the same rules; only what the set changes is
   judged (the gun and truck together, and the place). */
BkEditorStatus BkEditorSetReservePosition( BkEditorSession *session, int index, const BkEditorReservePositionRecord *record );

/* Removes the position at index. An index out of range is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorDeleteReservePosition( BkEditorSession *session, int index );

/* The AI general (04-12, D-19): the map's SAIGeneralMapInfo, one side at a time. A side
   holds the script IDs of its mobile reinforcement groups and its parcels; a parcel is a
   circle (centre, radius, in MAP (AI) units) of type 1 (defence) or 2 (reinforce) with a
   defence direction, and holds reinforce points STORED RELATIVE to the parcel centre and
   rotated by minus the defence direction (the MFC editor's formula, NMapGeometry). The
   game gives a general to sides 0 and 1 only, and never to the player's own. The
   snapshot and the working copy change together and the engine is left alone. */

typedef struct
{
	int side_count;  /* how many sides the map has (the size of sidesInfo) */
	int mobile_count;
	int parcel_count;
	int point_count; /* over all the parcels of the side */
} BkEditorAISideInfo;

typedef struct
{
	int type;        /* 1 defence, 2 reinforce (the file's EPatchType) */
	float cx, cy;    /* centre, AI units */
	float radius;    /* AI units; the editor's default and minimum is 256 (4 map tiles) */
	int defence_dir; /* 0..65535, a turn being 65535 (MFC scale) */
	int first_point; /* this parcel's points are points[first_point .. first_point + point_count) */
	int point_count;
} BkEditorAIParcel;

typedef struct
{
	float x, y; /* relative to the parcel centre, AI units, rotated by minus its direction */
	int dir;    /* 0..65535 */
} BkEditorAIPoint;

/* Reads side `side`: *info always, then the mobile script IDs, the parcels and the
   points, each up to its capacity (a capacity of 0 with a null array asks for the
   count). The info holds the totals; an array smaller than its total is
   BK_EDITOR_REFUSED after writing what fits, never past capacity - the sizing pass of a
   two-pass read. A side at or above side_count reads empty, with the current side_count
   (so the editor can tell how many sides an edit would add). BK_EDITOR_BAD_ARGUMENT for
   a null info, a negative side or capacity, or a null array with a capacity above 0;
   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorAIGeneralSide( BkEditorSession *session, int side, BkEditorAISideInfo *info,
                                      int *mobile, int mobile_capacity,
                                      BkEditorAIParcel *parcels, int parcel_capacity,
                                      BkEditorAIPoint *points, int point_capacity );

/* A raw put of one whole side together with the side count, both copies: the sides are
   resized to side_count (the lower ones the map lacked come out empty, which is how a
   click on side 3 of a one-side map makes sides 1 and 2; a smaller count drops the sides
   above it, which is how undo takes them away again), then side `side` is set when it is
   below side_count. A side at or above side_count must be empty (BK_EDITOR_REFUSED).
   BK_EDITOR_BAD_ARGUMENT for a negative or too large side (0..1023) or side_count
   (0..1024), a negative count, a null array with a count above 0, a parcel's
   point range outside the points array, or a direction outside 0..65535.
   BK_EDITOR_REFUSED naming why, nothing changed, for what an edit ADDS: a type that is
   not 1 or 2, a radius that is not above 0, a centre or radius that is not finite, a
   centre off the map, a point that is not finite, a script ID outside 0..32000 or
   twice. A parcel or script ID the side holds now, or held when the file was opened, is
   exempt, so a file's own odd data can be put back by an undo. */
BkEditorStatus BkEditorSetAIGeneralSide( BkEditorSession *session, int side, int side_count,
                                         const int *mobile, int mobile_count,
                                         const BkEditorAIParcel *parcels, int parcel_count,
                                         const BkEditorAIPoint *points, int point_count );

/* Reinforcement groups (04-09, D-16): the map's SReinforcementGroupInfo, keyed
   by group ID, each holding the script IDs of the objects the game holds back
   for it (an object of the map's objects list whose script ID a group holds is
   never placed at mission start; a script brings it in). The snapshot and the
   working copy change together and the engine is left alone. The file writes
   the groups in ID order whatever order they were put in. */

/* The group IDs of the snapshot, ascending. out_count is always the total; a
   capacity below it is BK_EDITOR_REFUSED after writing what fits, never past
   capacity. out may be null with capacity 0 to ask for the total.
   BK_EDITOR_BAD_ARGUMENT for a null out_count or a negative capacity;
   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorGroupIDs( BkEditorSession *session, int *out, int capacity, int *out_count );

/* The script IDs group id holds, in the order the file has them. out_count is
   the total (-1 when there is no such group, which is BK_EDITOR_REFUSED); a
   capacity below it is BK_EDITOR_REFUSED after writing what fits. ids may be
   null with capacity 0. A negative id, a negative capacity or a null
   out_count is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorGroup( BkEditorSession *session, int id, int *ids, int capacity, int *out_count );

/* Creates group id or replaces its script IDs with ids[0..count-1], in that
   order: an exact put, so undo can put back what a delete or an edit took. A
   negative id or count, a null ids with a count above 0, or a count no group
   could hold is BK_EDITOR_BAD_ARGUMENT. A script ID the put adds must be
   0..32000 and appear once (-1 would match every object without one,
   Pitfall 9): BK_EDITOR_REFUSED naming why, and nothing changes. An ID the
   group already holds is exempt, so a file's own odd data can be put back. */
BkEditorStatus BkEditorSetGroup( BkEditorSession *session, int id, const int *ids, int count );

/* Removes group id and its script IDs. A negative id is BK_EDITOR_BAD_ARGUMENT;
   one that is not there is BK_EDITOR_REFUSED and changes nothing. */
BkEditorStatus BkEditorDeleteGroup( BkEditorSession *session, int id );

/* The first group ID at or above from (a negative from counts as 0) that no
   group uses, into *out_id (C9: New offers this). BK_EDITOR_BAD_ARGUMENT for
   a null out_id. */
BkEditorStatus BkEditorFirstFreeGroupID( BkEditorSession *session, int from, int *out_id );

/* Hide checked (04-09, D-16): the view holds back every object of the map's
   objects list (not the scenario objects, as the game) whose script ID is one
   of script_ids[0..count-1], as the game holds them back for a reinforcement
   group. They leave the scene - visual, shadow and icons, as the MFC editor's
   own Hide checked took them out - and BkEditorObjectAt and BkEditorPickGroup
   no longer answer them; count 0 shows everything, and the
   set replaces the last one. An object with link ID 0 is not hidden (it names
   no one object). Any integers are fine: one no object carries hides nothing.
   This is a view setting, not an edit: it is never saved and never in any
   history, and BkEditorOpenMap and BkEditorCloseMap forget it. A negative
   count, or a null list with a count, is BK_EDITOR_BAD_ARGUMENT; so is a count
   above 65536. BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorSetHiddenScriptIDs( BkEditorSession *session, const int *script_ids, int count );

/* The edit log (04-05). An edit the bridge derives or compounds - a road or
   river edit now, bridges, fences and entrenchments later - hands out a token
   and keeps its own undo record: the records before and after, stored when
   the edit was made. BkEditorUndoEdit puts the before-state back and
   BkEditorRedoEdit the after-state, from what was stored - nothing is derived
   again (D-03). The order is the paints': undo takes only the newest applied
   edit, redo only the most recently undone, and a new edit drops everything
   undone; any other token is BK_EDITOR_REFUSED and changes nothing. A token is
   valid until the next BkEditorOpenMap or BkEditorCloseMap, which forget the
   log. BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorUndoEdit( BkEditorSession *session, int token );
BkEditorStatus BkEditorRedoEdit( BkEditorSession *session, int token );

/* Roads and rivers (D-07, D-08, D-09). kind 0 is a road (the map's roads3),
   kind 1 a river; any other kind is BK_EDITOR_BAD_ARGUMENT. Every point is
   WORLD (Vis) units; a width is world units (the tool's width w of 1..16 is
   w * fWorldCellSize / 2, Formats/fmtTerrain.h); opacity is 0..1.

   A road or river is edited as its control polyline and the width and
   opacity at its key points (one key point per control point). The sampled
   points are derived by the bridge, once per edit, with the MFC tool's own
   code (CVSOBuilder::CreateVSO, Update with a 30-unit step, UpdateZ), and both
   copies and the engine get exactly that record; the edit log keeps it.
   Every edit redraws the stripe in the engine (Remove + Add); a river edit
   also updates the AI's passability (IAIEditor::DeleteRiver with the record
   as it was, AddRiver with the new one, undo and redo included). Roads never
   touch the AI: the game works their passability out when it loads the map.
   Altitudes and shades never change.

   The saved nID of a new record is the bridge's own (one above every nID the
   map uses), never the engine's random one; the bridge maps between the two. */
typedef struct { char name[128]; } BkEditorVsoDescriptor;
/* One record: saved_id is the nID the file holds; desc the descriptor's full
   name as saved (the season folder, Roads3D\ or Rivers\, and the name);
   control_count the control points; key_count the key points. */
typedef struct { int saved_id; char desc[128]; int control_count; int key_count; } BkEditorVsoInfo;
/* A key point: its sampled position (world units, z the ground's), its
   normal (a unit vector across the stripe), its width (world units, from the
   centre line to each edge) and its opacity (0..1). A width handle sits at
   position +- normal * width. */
typedef struct { float x, y, z, nx, ny, nz, width, opacity; } BkEditorVsoKeyPoint;

/* The descriptors of a kind for the open map's season, as the MFC editor
   lists them: the names in <season folder>Roads3D\ or Rivers\ with no folder
   and no extension, sorted. out_count is always the total; a buffer too short
   is BK_EDITOR_REFUSED with nothing written past capacity, and out may be null
   when capacity is 0. BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorVsoDescriptors( BkEditorSession *session, int kind, BkEditorVsoDescriptor *out, int capacity, int *out_count );
/* How many roads (kind 0) or rivers (kind 1) the map holds. */
BkEditorStatus BkEditorVsoCount( BkEditorSession *session, int kind, int *out_count );
/* The record at index (0..count-1, else BK_EDITOR_BAD_ARGUMENT), in two
   passes like BkEditorObjects: info (never null) always gets the counts; the
   control points go to controls and the key points to keys when their
   capacities hold them, and a buffer too short for its count is
   BK_EDITOR_REFUSED with nothing written past capacity. Either array may be
   null with a capacity of 0. */
BkEditorStatus BkEditorVso( BkEditorSession *session, int kind, int index, BkEditorVsoInfo *info,
                            BkEditorVec3 *controls, int control_cap, BkEditorVsoKeyPoint *keys, int key_cap );
/* Adds a road (kind 0) or river (kind 1) through count control points (world
   units; z is ignored, every point is fitted to the ground), with the
   descriptor desc (a bare name from BkEditorVsoDescriptors), width_tiles 1..16
   and opacity 0..1 at every point. It is appended to the map's list; out_index
   is where it landed, out_token names the edit for BkEditorUndoEdit and
   BkEditorRedoEdit (either out may be null; both are -1 after a refusal).

   BK_EDITOR_BAD_ARGUMENT: a null desc or points (with count > 0), a count below
   0 or above 1024, width_tiles or opacity out of range, any non-finite value.
   BK_EDITOR_REFUSED, naming the reason: no map open, a desc that is not a bare
   name of this season's descriptors, a point off the map, and a line too short
   to be a road - fewer than two points at least 2 units apart, or shorter than
   one 30-unit sampling step (the game's loaders cannot take a record with
   fewer than two sampled points). A refusal changes nothing. */
BkEditorStatus BkEditorAddVso( BkEditorSession *session, int kind, const char *desc, const BkEditorVec3 *points, int count,
                               float width_tiles, float opacity, int *out_token, int *out_index );
/* Edits of the road or river at index, each one edit with a token for
   BkEditorUndoEdit (out_token may be null; -1 after a refusal). The record is
   resampled by the bridge keeping its key points' widths and opacities
   (CVSOBuilder::Update with key points kept, then UpdateZ); for a river the
   AI's tiles follow (DeleteRiver with the record as it was, AddRiver with the
   new one). Every entry: kind other than 0 or 1, index outside 0..count-1, a
   non-finite value or a mode other than 0..2 is BK_EDITOR_BAD_ARGUMENT;
   BK_EDITOR_REFUSED with no map open; a refusal changes nothing.

   The mode of a width or opacity edit is the MFC editor's width mode: 0 the
   key point alone, 1 it and every later one, 2 every point.

   BkEditorMoveVsoPoints: the line's control points, count of them - exactly
   the record's control count (else BK_EDITOR_BAD_ARGUMENT), world units, z
   ignored. REFUSED when a point would be off the map or two neighbours closer
   than 2 units.
   BkEditorSetVsoWidth: the width of key point key (0..key_count-1), world
   units from the centre line to the edge, above 0.
   BkEditorSetVsoOpacity: the opacity 0..1 at key point key. Like the MFC
   right-drag it resamples nothing: the key point (mode 0), every point from
   it on (1), every point (2).
   BkEditorInsertVsoPoint: the midpoint after control point control, or
   before it when it is the last, with the average width and opacity of the
   two key points it lies between.
   BkEditorDeleteVsoPoint: removes control point control; REFUSED ("a road
   needs at least 2 points") while only 2 remain. */
BkEditorStatus BkEditorMoveVsoPoints( BkEditorSession *session, int kind, int index, const BkEditorVec3 *points, int count, int *out_token );
BkEditorStatus BkEditorSetVsoWidth( BkEditorSession *session, int kind, int index, int key, float width_world, int mode, int *out_token );
BkEditorStatus BkEditorSetVsoOpacity( BkEditorSession *session, int kind, int index, int key, float opacity, int mode, int *out_token );
BkEditorStatus BkEditorInsertVsoPoint( BkEditorSession *session, int kind, int index, int control, int *out_token );
BkEditorStatus BkEditorDeleteVsoPoint( BkEditorSession *session, int kind, int index, int control, int *out_token );
/* The road or river under the world point (wx, wy), world units:
   CMapInfo::TerrainHitTest over the roads, then the rivers. cycle (0 or more)
   skips that many earlier hits, wrapping, so a repeated right press walks
   through overlapping lines. BK_EDITOR_REFUSED when nothing is there;
   *out_kind and *out_index are -1 then. */
BkEditorStatus BkEditorPickVso( BkEditorSession *session, float wx, float wy, int cycle, int *out_kind, int *out_index );
/* Deletes the road (kind 0) or river (kind 1) at index, the whole record, as
   one edit (out_token, may be null; -1 after a refusal). A river's tiles are
   unlocked in the AI first; undo puts the record back where it was and locks
   them again. index outside 0..count-1 is BK_EDITOR_BAD_ARGUMENT;
   BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorDeleteVso( BkEditorSession *session, int kind, int index, int *out_token );
/* For the engine tier: every road and river the map will save against the
   engine's own, found through the bridge's ID map (never by nID), in control
   points, sampled points, widths and opacities, and against the working copy.
   BK_EDITOR_FAILED naming the first difference. */
BkEditorStatus BkEditorVsoMatchesEngine( BkEditorSession *session );

/* Bridges (04-06, D-10..D-12): a bridge is one entry of the map's bridges
   list - the link IDs of its spans, in order - plus its span objects, and is
   drawn, picked, rotated, toggled and deleted as a whole. A drag is WORLD
   (Vis) units, as the pointer is; a span's position is MAP (AI) units, as an
   object's is. Every edit is one edit of the edit log (BkEditorUndoEdit /
   BkEditorRedoEdit take its token) and changes the bridges entry, its spans in
   both copies and the engine together, all or nothing; no edit ever leaves an
   entry naming a missing object (the game's LoadBridges dereferences every
   link). The span geometry is NMapGeometry::PlanBridge
   (Sources/src/MapFile/MapGeometry.h), the function the map-file tier builds
   its expected maps with.

   A bridge type: its name; direction 0 vertical, 1 horizontal (the drag must
   run along it); has_partner 1 when its rotated _01/_02 variant is in the
   object database; build_during_play_allowed 1 for a WoodenBig_Heavy_ type
   (the only ones the MFC editor lets be built during play). */
typedef struct { char name[64]; int direction; int has_partner; int build_during_play_allowed; } BkEditorBridgeDescriptor;
/* Every bridge type of the object database, sorted by name; out_count is
   always the total, a buffer too short is BK_EDITOR_REFUSED with nothing
   written past capacity, out may be null when capacity is 0. A null
   out_count is BK_EDITOR_BAD_ARGUMENT; BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorBridgeDescriptors( BkEditorSession *session, BkEditorBridgeDescriptor *out, int capacity, int *out_count );
/* One planned span: its position (MAP units), its packed frame type (1 begin,
   2 middle, 4 end, what the file holds) and its direction (0). */
typedef struct { float x, y; int type; int dir; } BkEditorPlannedPiece;
/* The spans a drag of the bridge type desc from (wx0, wy0) to (wx1, wy1)
   (WORLD units) would place, changing nothing - for the tool's ghost. Two
   passes like BkEditorBridgeDescriptors: out_count is always the planned
   count. BK_EDITOR_REFUSED naming the reason for a desc that is not a bridge
   type (or whose stats lack a begin, middle or end span) and a drag along the
   other axis than the type's direction ("this bridge runs horizontally ...");
   BK_EDITOR_BAD_ARGUMENT for a null desc or out_count, a desc of 64
   characters or more, a non-finite coordinate or one beyond +-1e6. */
BkEditorStatus BkEditorPlanBridge( BkEditorSession *session, const char *desc, float wx0, float wy0, float wx1, float wy1,
                                   BkEditorPlannedPiece *out, int capacity, int *out_count );
/* Draws a bridge of type desc along the drag (WORLD units): the planned spans
   become objects (HP 1, no script ID, player 0, direction 0, fresh link IDs;
   the file gets the packed frame type) and a new bridges entry is appended;
   out_index is that entry's index, out_token names the edit (either may be
   null; both -1 after a refusal). The refusals of BkEditorPlanBridge, and
   BK_EDITOR_REFUSED when the engine will not place a span (off the map): a
   refusal changes nothing. */
BkEditorStatus BkEditorDrawBridge( BkEditorSession *session, const char *desc, float wx0, float wy0, float wx1, float wy1,
                                   int *out_token, int *out_index );
/* One bridges entry: the type (its first span's name), how many spans the
   entry names, the box of their positions (MAP units) and whether it is built
   during play (a span with negative HP in the saved map). */
typedef struct { char desc[64]; int span_count; float min_x, min_y, max_x, max_y; int built_during_play; } BkEditorBridgeInfo;
/* Every bridges entry of the map, in list order; two passes like
   BkEditorBridgeDescriptors. */
BkEditorStatus BkEditorBridges( BkEditorSession *session, BkEditorBridgeInfo *out, int capacity, int *out_count );
/* The group under the screen point (sx, sy), window pixels: out_kind 1 and
   the bridges index for a bridge span, 2 and the entrenchments index for a
   trench piece. BK_EDITOR_REFUSED when neither is there (*out_kind and
   *out_index -1). BkEditorObjectAt keeps passing spans and pieces over, so
   the Select tool never takes one alone. A null out is
   BK_EDITOR_BAD_ARGUMENT; BK_EDITOR_REFUSED with no map open. */
BkEditorStatus BkEditorPickGroup( BkEditorSession *session, float sx, float sy, int *out_kind, int *out_index );
/* Deletes bridge index whole - its entry, then every span, in both copies and
   the engine - as one edit (out_token, may be null; -1 after a refusal); its
   undo puts the spans back and then the entry at the same index. index
   outside 0..count-1 is BK_EDITOR_BAD_ARGUMENT. BK_EDITOR_REFUSED for a
   bridge with a span the editor could not put back (a type the database does
   not know, a span the engine never held, a link ID the map shares): it is
   kept as read. */
BkEditorStatus BkEditorDeleteBridge( BkEditorSession *session, int index, int *out_token );
/* D-11: rotates bridge index - its type's _01/_02 partner, rebuilt about the
   same centre along the other axis with the same number of spans, at the same
   index of the list, built during play carried over - as one edit
   (out_token, may be null). BK_EDITOR_REFUSED, changing nothing, naming the
   reason: "no rotated variant of <type>" when the partner is not in the
   object database, a span of the rotated bridge off the map, a bridge whose
   spans the editor could not put back. index outside 0..count-1 is
   BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorRotateBridge( BkEditorSession *session, int index, int *out_token );
/* D-12: toggles bridge index between intact and built during play (every
   span's HP in the saved map 1 or -1; the engine shows it intact, marked with
   the MFC editor's specular 0xFF0000FF) as one edit. BK_EDITOR_REFUSED ("only
   WoodenBig_Heavy bridges can be built during play") for any other type.
   index outside 0..count-1 is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorToggleBridgeBuild( BkEditorSession *session, int index, int *out_token );

/* Fences (04-07, D-14): the MFC Fences tab. A fence run is a drag along one
   axis (WORLD units) that places one fence every second AI tile; the
   direction is in the frame index the file holds, ( 1 << dir ) | 0x00010000,
   dir 1 or 3 for a horizontal drag (left, else right with the tile moved two
   to the right), 0 or 2 for a vertical one (up with the tile moved two up,
   else down). A drag that does not leave its first AI tile is one fence,
   direction 0, or 1 with ctrl. The whole run is one edit of the edit log
   (BkEditorUndoEdit / BkEditorRedoEdit take its token) and changes both
   copies and the engine together, all or nothing; a placed fence is an
   ordinary object (BkEditorPlaceObject, BkEditorDeleteObject). The geometry is
   NMapGeometry::PlanFences (Sources/src/MapFile/MapGeometry.h).

   A world point to the AI tile it falls in, through the engine's own
   ITerrainEditor::GetAITileIndex (half a world cell, rounded - not the
   truncation of CMapInfo::GetAITileIndices, which agrees with it only at tile
   corners). The tile is written even for a point off the map, which is
   BK_EDITOR_REFUSED. */
BkEditorStatus BkEditorWorldToAITile( BkEditorSession *session, float wx, float wy, int *out_x, int *out_y );
/* A fence type: its name. */
typedef struct { char name[64]; } BkEditorFenceDescriptor;
/* Every fence type of the object database with stats, sorted by name; two
   passes like BkEditorBridgeDescriptors. */
BkEditorStatus BkEditorFenceDescriptors( BkEditorSession *session, BkEditorFenceDescriptor *out, int capacity, int *out_count );
/* The fences a drag of the fence type desc from (wx0, wy0) to (wx1, wy1)
   (WORLD units) would place, changing nothing - for the tool's ghost; each is
   a BkEditorPlannedPiece: its position (MAP units), its packed frame type
   ( 1 << dir ) | 0x00010000, dir 0 (the fence's own direction is in its
   type). Two passes: out_count is always the planned count.
   BK_EDITOR_REFUSED naming the reason for a desc that is not a fence type (or
   whose stats lack a centre segment in one of four directions) and a run with
   an end off the map ("the fence run leaves the map"); BK_EDITOR_BAD_ARGUMENT
   for a null desc or out_count, a desc of 64 characters or more, a
   non-finite coordinate or one beyond +-1e6. */
BkEditorStatus BkEditorPlanFences( BkEditorSession *session, const char *desc, float wx0, float wy0, float wx1, float wy1, int ctrl,
                                   BkEditorPlannedPiece *out, int capacity, int *out_count );
/* Places the planned fences as objects (HP 1, no script ID, player 0, fresh
   link IDs; the file gets the packed frame type) as one edit; out_token names
   it (may be null; -1 after a refusal). The refusals of BkEditorPlanFences,
   and BK_EDITOR_REFUSED when the engine will not place a fence (on another
   object, off the map): a refusal changes nothing. */
BkEditorStatus BkEditorDrawFences( BkEditorSession *session, const char *desc, float wx0, float wy0, float wx1, float wy1, int ctrl,
                                   int *out_token );

/* Entrenchments (04-08, D-13): the MFC trench builder. A trench is one entry
   of the map's entrenchments list - sections, each the link IDs of its
   pieces - plus its piece objects of the "Entrenchment" type, drawn and
   deleted as a whole (one edit of the edit log, all or nothing; no edit ever
   leaves a section naming a missing object or a section empty: the game's
   LoadEntrenchments dereferences every link). Points are WORLD (Vis) units,
   the clicks of the polyline in order (the builder extends its path one click
   at a time as the MFC tool does: straight runs of line pieces, arcs of arc
   pieces at turns over 30 degrees); a piece's position is MAP (AI) units. The
   geometry is NMapGeometry::PlanEntrenchment
   (Sources/src/MapFile/MapGeometry.h), the function the map-file tier builds
   its expected maps with.

   The pieces the clicks would commit, changing nothing - for the tool's
   preview; each a BkEditorPlannedPiece: position (MAP units), packed type (1
   line, 2 fireplace, 4 terminator, 8 arc - what the file holds) and direction
   (0..65535). Two passes like BkEditorBridgeDescriptors: out_count is always
   the planned count; the order is the begin terminator, the end terminator,
   then one piece per step of the path. BK_EDITOR_REFUSED naming the reason for
   a path shorter than one piece ("the trench is shorter than one piece ..."),
   and for a database with no usable "Entrenchment" type; BK_EDITOR_BAD_ARGUMENT
   for a null out_count, null points with a count, a count outside 0..256 and a
   non-finite coordinate (z is ignored). */
BkEditorStatus BkEditorPlanEntrenchment( BkEditorSession *session, const BkEditorVec3 *points, int count,
                                         BkEditorPlannedPiece *out, int capacity, int *out_count );
/* Draws the entrenchment the clicks commit: the planned pieces become objects
   (HP 1, no script ID, the given player, fresh link IDs; the file gets the
   packed type) and a new entrenchments entry is appended with its sections;
   out_index is that entry's index, out_token names the edit (either may be
   null; both -1 after a refusal). The refusals of BkEditorPlanEntrenchment,
   BK_EDITOR_BAD_ARGUMENT for a player outside the map's players, and
   BK_EDITOR_REFUSED when the engine will not place a piece (off the map): a
   refusal changes nothing. */
BkEditorStatus BkEditorDrawEntrenchment( BkEditorSession *session, const BkEditorVec3 *points, int count, int player,
                                         int *out_token, int *out_index );
/* One entrenchments entry: how many pieces and sections it names, the player
   of its first piece and the box of its pieces' positions (MAP units). */
typedef struct { int piece_count; int section_count; int player; float min_x, min_y, max_x, max_y; } BkEditorEntrenchmentInfo;
/* Every entrenchments entry of the map, in list order; two passes like
   BkEditorBridgeDescriptors. */
BkEditorStatus BkEditorEntrenchments( BkEditorSession *session, BkEditorEntrenchmentInfo *out, int capacity, int *out_count );
/* Deletes entrenchment index whole - its entry, then every piece, in both
   copies and the engine - as one edit (out_token, may be null; -1 after a
   refusal); its undo puts the pieces back and then the entry at the same
   index. A piece is found with BkEditorPickGroup (kind 2); BkEditorDeleteObject
   still refuses a piece alone (D-04). index outside 0..count-1 is
   BK_EDITOR_BAD_ARGUMENT. BK_EDITOR_REFUSED for an entrenchment with a piece
   the editor could not put back (a link ID the map shares, a piece the engine
   never held): it is kept as read. */
BkEditorStatus BkEditorDeleteEntrenchment( BkEditorSession *session, int index, int *out_token );

/* Object filters (M3, D-31): the named conditions of folder words the
   palette's quick toggles, its filter combo and the Filters Composer share,
   and which later feed the Fields Composer's objects tab (05-10) and the
   fire-range filter (05-06). Read from the shipped Data/Editor/filter.xml
   through the engine's own data-tree reader - the MFC editor's exact
   LoadDataResource call (TabSimpleObjectsDialog.cpp:225) - and merged with
   the user file <UserRoot>mapeditor/filter.xml: a user filter replaces the
   shipped one of the same byte-equal name, user-only names are appended, and
   the shipped ones keep their presence. One filter is up to 8 word lists of
   up to 8 words of up to 31 characters each (an object's folder path passes
   when every word of some one list appears in it - the MFC editor's
   SSimpleFilter::Check).

   `user` is 1 when this entry came from the user file (or is overridden by
   it) and is therefore a candidate for BkEditorSaveObjectFilters; a shipped
   name the user has not touched is 0 and is not copied into the user file.

   The answers come ordered by name (byte order), not file order, so a read
   is order-stable across runs and platforms - the MFC editor's unordered_map
   iterated in whatever order the bucket array gave. out_count is always the
   total; a capacity below it is BK_EDITOR_REFUSED after writing what fits,
   never past capacity; out may be null with capacity 0 to ask for the total
   (that sizing pass is REFUSED when there are filters, as a short buffer
   is). A malformed or unreadable file reads empty and never fails the call -
   a broken filter file must not block the editor (the engine's own tree
   reader throws into this side's catch). Filters are installation data, not
   map data: no map need be open.

   BK_EDITOR_BAD_ARGUMENT for a null out_count or a negative capacity. */
#define BK_EDITOR_FILTER_MAX_LISTS 8
#define BK_EDITOR_FILTER_MAX_WORDS 8
#define BK_EDITOR_FILTER_WORD_LEN  32
typedef struct { int word_count; char words[BK_EDITOR_FILTER_MAX_WORDS][BK_EDITOR_FILTER_WORD_LEN]; } BkEditorObjectFilterWords;
typedef struct { char name[64]; int list_count; int user; BkEditorObjectFilterWords lists[BK_EDITOR_FILTER_MAX_LISTS]; } BkEditorObjectFilter;
BkEditorStatus BkEditorObjectFilters( BkEditorSession *session, BkEditorObjectFilter *out, int capacity, int *out_count );

/* Writes the given filters to <UserRoot>mapeditor/filter.xml in the shipped
   file's own XML shape, through the engine's own data-tree writer (the same
   reader reads both back). The caller decides what belongs in a user file:
   the entries BkEditorObjectFilters answered with user 1, plus everything
   authored or edited since - a shipped name the user has not modified is not
   copied, so the shipped file keeps answering for it. The directory is
   created when missing. count 0 (null filters) writes an empty filter set -
   every filter was deleted. token outs do not apply: filters are not map
   data and never enter the edit log.

   BK_EDITOR_BAD_ARGUMENT for a negative count, null filters with a count, an
   empty or unterminated name, a list_count outside 0..8, a word_count
   outside 0..8, or a word not NUL-terminated within its 32 bytes.
   BK_EDITOR_REFUSED naming why when the file cannot be written. Safe with no
   map open; a refusal changes nothing (the file is written once, whole). */
BkEditorStatus BkEditorSaveObjectFilters( BkEditorSession *session, const BkEditorObjectFilter *filters, int count );

/* The Fields tool (M3, D-21): one application of a field set over a drawn
   polygon as ONE undoable edit (out_token, -1 after a refusal). The MFC's
   CFieldsState::PlaceField pipeline, in the session layer's
   ApplyFieldInSession: the polygon (WORLD xy, z ignored; 3..64 points, the
   MFC's own UniquePolygon+area closing rule refuses degenerate ones) is cut
   by the map bounds, optionally randomized through the engine's own
   RandomizeEdges with the MFC dialog's exact arguments (min_length in cells,
   the MFC's own >= 2 rule; width 0..0.5; disturbance 0..1 - clamped here),
   then the field set's tile shells (FillTileSet, both copies, the engine's
   terrain redrawn over the covered patches), object shells (FillObjectSet
   into a scratch summer map, each object placed through the session's add
   path) and profile pattern (the set's own tga, FillProfilePattern, the
   objects' z back on the ground, full shades) fill the map.

   The season confirmation is the caller's - ask BkEditorFieldSetSeason,
   compare with the map's season, show the YES/NO popup (the MFC's
   IDS_INVALID_FIELD_SEASON flow); the bridge does not gate on season. The
   flags mirror the dialog's checkboxes: fill_terrain, place_objects,
   modify_heights, update_map_after (the whole Update Map composite runs
   after, nested in this one edit - still one undo step);
   check_passability_only reports what the object shells would place and
   what the AI's passability would take, changing nothing (token -1, no
   log entry); can_add_object_filter gates the placement by the object
   filter named in object_filter (a name from BkEditorObjectFilters, the
   D-31 filters matched against the objects' folder paths, the MFC's own
   FilterName argument).

   out_report (which may be null with capacity 0) answers every object the
   object shells produced and whether it was placed; out_report_count is
   always the total, a capacity below it is BK_EDITOR_REFUSED after writing
   what fits. A refusal - no map, bad arguments, an unknown field set or
   filter, a degenerate polygon - changes nothing; a mid-pipeline failure
   puts the tiles, the altitudes and the objects already added back and is
   BK_EDITOR_FAILED. Undo restores the whole composite raw, byte for byte.

   BK_EDITOR_BAD_ARGUMENT for a null params or out_token, a null or
   non-finite point, a name that is not a storage-relative bare name, or a
   polygon outside 3..64 points. BK_EDITOR_REFUSED with no map open. */
typedef struct {
	char field_set[192];         /* storage-relative bare name, as BkEditorListRmg lists them */
	int point_count;             /* 3..64 */
	const BkEditorVec3 *points;  /* WORLD (Vis) units, z ignored */
	int randomize;               /* 0/1: RandomizeEdges with the three below */
	float min_length;            /* cells, the MFC's own >= 2 */
	float width;                 /* 0..0.5 */
	float disturbance;           /* 0..1 */
	int fill_terrain;            /* 0/1 */
	int place_objects;           /* 0/1 */
	int modify_heights;          /* 0/1 */
	int update_map_after;        /* 0/1 */
	int check_passability_only;  /* 0/1: report only, nothing changes */
	int can_add_object_filter;   /* 0/1: gate the placement by object_filter */
	char object_filter[64];      /* a name from BkEditorObjectFilters, "" when unused */
} BkEditorFieldApplyParams;

typedef struct { char name[64]; float x, y; int placed; } BkEditorFieldObjectReport; /* x, y: AI (map) units */
BkEditorStatus BkEditorApplyField( BkEditorSession *session, const BkEditorFieldApplyParams *params,
                                   BkEditorFieldObjectReport *out_report, int report_capacity, int *out_report_count,
                                   int *out_token );

/* The field set's season (D-21): the loaded set's own
   CMapInfo::GetSelectedSeason answer, which the app compares with the map's
   season to show the YES/NO confirmation before applying. name is
   storage-relative, like BkEditorListRmg lists them. A name that is not in
   the data is BK_EDITOR_REFUSED; a null name or out_season is
   BK_EDITOR_BAD_ARGUMENT. No map need be open. */
BkEditorStatus BkEditorFieldSetSeason( BkEditorSession *session, const char *name, int *out_season );

/* One storage-relative RMG name (a bare name with the folder it lists from,
   no extension), as BkEditorListRmg answers and every composer entry takes. */
typedef struct { char name[192]; } BkEditorRmgName;

/* The RMG storage-folder scan (M3, D-08): the lists the Fields tool's
   field-set combo, Create Random Map (05-08) and the composers (05-09/10)
   share. kind: 0 field sets (Scenarios/FieldSets), 1 templates,
   2 graphs, 3 containers, 4 settings, 5 chapters. The mounted storages'
   folders are walked - the MFC editor read Editor\Default*.xml list files
   instead, which are not shipped, so its composers opened empty (D-08).
   Names are storage-relative (folder included, .xml stripped), lowercased,
   sorted, deduped; a folder no storage carries is an empty list, not an
   error. out_count is always the total; a capacity below it is
   BK_EDITOR_REFUSED after writing what fits; out may be null with capacity
   0 to ask for the total. Not map data: no map need be open.
   BK_EDITOR_BAD_ARGUMENT for a null out_count, a negative capacity, or a
   kind outside 0..5. */
BkEditorStatus BkEditorListRmg( BkEditorSession *session, int kind, BkEditorRmgName *out, int capacity, int *out_count );

/* Safe on a null session, and safe to call twice. Removes the overlay
   BkEditorSetOverlay installed, so it is never called after this returns. */
BkEditorStatus BkEditorStop( BkEditorSession *session );

#ifdef __cplusplus
}
#endif

#endif /* __EDITOR_BRIDGE_H__ */
