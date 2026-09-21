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

   out may be null if the caller only wants the status. On failure the session
   keeps whatever map it had open before. */
BkEditorStatus BkEditorOpenMap( BkEditorSession *session, const char *path, BkEditorMapSummary *out );

/* Safe on a null session, and safe to call twice. */
BkEditorStatus BkEditorStop( BkEditorSession *session );

#ifdef __cplusplus
}
#endif

#endif /* __EDITOR_BRIDGE_H__ */
