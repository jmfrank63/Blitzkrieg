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

/* Writes the open map to path; the format comes from the extension.

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

   Positions are floats because a map's are; the engine takes whole units and
   the bridge rounds once, on its way in. */
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
   name a new paint and must not be used. */
BkEditorStatus BkEditorPaint( BkEditorSession *session, const BkEditorPaintCell *cells, int count, int *out_token );
BkEditorStatus BkEditorUndoPaint( BkEditorSession *session, int token );
BkEditorStatus BkEditorRedoPaint( BkEditorSession *session, int token );

/* A world point to the tile it falls in - the brush's other half, through the
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

/* The camera, and one frame drawn into the window the session was started on.
   BK_EDITOR_REFUSED from BkEditorFrame is a device that would not begin a
   scene, which is a thing that happens rather than a bug. */
BkEditorStatus BkEditorSetCamera( BkEditorSession *session, float wx, float wy );
BkEditorStatus BkEditorFrame( BkEditorSession *session );

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
   BkEditorWorldToTile to turn a click into a cell. */
BkEditorStatus BkEditorScreenToWorld( BkEditorSession *session, float sx, float sy, float *wx, float *wy );

/* The map's own two fields, not a player's: nType is the mission kind and
   nAttackingSide is which side attacks in it. The engine has no say in either,
   so they take no engine call and cannot be refused. The attacking side is 0
   or 1; anything else is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkEditorSetMapType( BkEditorSession *session, int type );
BkEditorStatus BkEditorSetAttackingSide( BkEditorSession *session, int side );

/* Safe on a null session, and safe to call twice. */
BkEditorStatus BkEditorStop( BkEditorSession *session );

#ifdef __cplusplus
}
#endif

#endif /* __EDITOR_BRIDGE_H__ */
