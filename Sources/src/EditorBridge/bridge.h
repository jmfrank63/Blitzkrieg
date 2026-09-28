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
   session. data_root is the directory holding Data and the shared libraries.

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
   exactly as it came in. */
BkEditorStatus BkEditorSaveMap( BkEditorSession *session, const char *path );

/* The edits. Each one changes the map and the engine together or neither: a
   refusal leaves the session exactly as it was, so the editor never saves
   something it did not show.

   BK_EDITOR_REFUSED means the map or the engine said no and the reason is in
   BkEditorLastMessage - an object still referred to by a bridge or a start
   command, or a position the engine will not put the object at. It is an
   ordinary answer, not a failure.

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
BkEditorStatus BkEditorTurnObject( BkEditorSession *session, int link_id, int dir );
BkEditorStatus BkEditorSetObjectPlayer( BkEditorSession *session, int link_id, int player );
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

/* The tiles BkEditorPaint takes on the open map: every index its tileset has a
   terrain type for, once each, ascending - what a brush's palette offers.
   Like BkEditorObjects, out_count is always the total, and a buffer too short
   for it is BK_EDITOR_REFUSED with nothing written past capacity; out may be
   null when capacity is 0, to ask for the count. BK_EDITOR_REFUSED too when
   no map is open. */
BkEditorStatus BkEditorTilesetTiles( BkEditorSession *session, unsigned char *out, int capacity, int *out_count );

/* A world point (world units, not map units) to the tile it falls in - the brush's other half, through the
   engine's own conversion. Screen to world is BkEditorScreenToWorld; the two
   compose. BK_EDITOR_REFUSED means the point is not on the map. */
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
   size one and ask again. */
typedef struct { char name[64]; int game_type; } BkEditorCatalogueEntry;
BkEditorStatus BkEditorCatalogue( BkEditorSession *session, BkEditorCatalogueEntry *out, int capacity, int *out_count );

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

/* Safe on a null session, and safe to call twice. Removes the overlay
   BkEditorSetOverlay installed, so it is never called after this returns. */
BkEditorStatus BkEditorStop( BkEditorSession *session );

#ifdef __cplusplus
}
#endif

#endif /* __EDITOR_BRIDGE_H__ */
