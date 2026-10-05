#ifndef __EDITOR_BRIDGE_RESOURCE_H__
#define __EDITOR_BRIDGE_RESOURCE_H__

/* The resource editor's view of the engine. Flat C: opaque handles, plain
   structs, status codes, no engine header included and no C++ exception
   crossing - the same ABI shape bridge.h establishes for the map editor, with
   the resource editor's own entry points (BkRes*) added beside the shared
   lifecycle ones.

   A BkResSession is a BkEditorSession in disguise. The resource bridge reuses
   BkEditorStart/BkEditorStop/BkEditorSetOverlay/BkEditorCaptureFrame/
   BkEditorPaths/BkEditorSetMod/BkEditorMods/BkEditorActiveMod/BkEditorFrame
   (and the window-resize entry point) unchanged, through bridge.h's own
   declarations: a resource editor process starts the engine the same way the
   map editor does, points its ImGui overlay at the same device, and reads the
   same roots. What it does NOT reuse are the map-shaped entry points
   (BkEditorOpenMap, BkEditorAddObject, BkEditorPaint, etc.); it operates on a
   resource PROJECT instead, through the group listed below.

   Error codes come from BkEditorStatus in bridge.h: BK_EDITOR_OK and the five
   failure codes. No new status enum: the resource editor's codes map one for
   one, and callers that already handle the map editor's codes handle these
   without a translation layer. BkEditorLastMessage is the one message
   surface - owned by the session, valid until the next call on it; never
   null, even on a null session (a start that failed before allocating one
   still has to be printable).

   Line-ending contract: CRLF, exactly like every other text file in the
   repository (.gitattributes). */

#include "bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An opaque handle. Aliases BkEditorSession - a resource bridge session IS an
   editor session - so the shared lifecycle entry points in bridge.h take and
   return this handle without a cast. The typedef is new rather than a
   #define because a #define would make BkResSession and BkEditorSession the
   same name to the preprocessor, which would collide at the forward
   declaration bridge.h already carries. */
typedef BkEditorSession BkResSession;

/* ---- Projects ---------------------------------------------------------- */

/* A kind code, one per project extension the MFC editor registered (21 total,
   from MEM005 in the memory store): wpn/Weapon, mcp/Mine, trc/Trench,
   scp/Squad, spt/Sprite, unt/Animation(Infantry), msh/Mesh(Unit),
   obt/Object, fnc/Fence, bld/Build, bdg/Bridge, pcp/Particle, eff/Effect,
   til/TileSet, 3rd/3dRoad, 3rv/3dRiver, mip/Mission, chc/Chapter,
   cgc/Campaign, mdc/Medal, gui/GUIFrame. The integer values match the
   NResourceModel::EResourceKind enum; a value outside the registered set is
   BK_EDITOR_BAD_ARGUMENT. */
typedef int BkResKind;

/* BkResNew: fresh project of the given kind, with a Root item created by the
   kind's own CTreeItemFactory - the MFC editor's File > New. Nothing is on
   disk yet; the first BkResSave sets the path. BK_EDITOR_BAD_ARGUMENT for an
   unknown kind. BK_EDITOR_REFUSED when the engine is not started. */
BkEditorStatus BkResNew( BkResSession *session, BkResKind kind );

/* BkResOpen: reads an existing project from path through NResourceXml and
   builds the tree from it. BK_EDITOR_BAD_ARGUMENT for a null path.
   BK_EDITOR_DATA_MISSING when the file is not readable or does not parse.
   BK_EDITOR_REFUSED when the engine is not started. */
BkEditorStatus BkResOpen( BkResSession *session, const char *path );

/* BkResSave: writes the open project to path through NResourceXml, safe-save
   style (write to a temporary beside the destination, read back, compare,
   rename only on match - exactly BkEditorSaveMap's own invariant). The
   extension decides the format, as the MFC editor's File > Save As used it.
   BK_EDITOR_REFUSED when no project is open; BK_EDITOR_FAILED when the
   read-back comparison fails. */
BkEditorStatus BkResSave( BkResSession *session, const char *path );

/* BkResClose: drops the open project (no save). The lock, if any, is
   released by the owning process rather than by this call. BK_EDITOR_OK
   with no project open too. */
BkEditorStatus BkResClose( BkResSession *session );

/* BkResKind: the kind of the open project, written through *out. */
BkEditorStatus BkResKindOf( BkResSession *session, BkResKind *out );

/* BkResLock / BkResLockOwner / BkResLockTakeOver: MFC's cooperative lock
   (CParentFrame::LockFile, D-08): an empty `locked_<user>` file in the
   project's folder, where <user> is the login name (the test seam
   BK_RESOURCE_EDITOR_USER overrides it). BkResLock creates it; a lock this
   user already holds is OK. BK_EDITOR_REFUSED when another user's
   `locked_*` is present; the owners are in BkEditorLastMessage and
   BkResLockOwner then, and the host warns and either stays read-only or
   calls BkResLockTakeOver, which removes every other `locked_*` and takes
   the lock. BkResLockOwner writes the owners of every `locked_*` in the
   folder, comma-separated, or "" when there is none. BkResClose removes
   this session's lock file. */
BkEditorStatus BkResLock( BkResSession *session );
BkEditorStatus BkResLockOwner( BkResSession *session, char *out_owner, int capacity );
BkEditorStatus BkResLockTakeOver( BkResSession *session );

/* ---- Tree -------------------------------------------------------------- */

/* One node of the tree as the renderer sees it: the parent/child relation,
   the class name (what CTreeItemFactory::Create was called with), the
   displayed name (CTreeItem::szDisplayName), the expand bit and the child
   count. Fixed buffers like the map bridge's records; a long name is
   truncated at the capacity's limit and always NUL-terminated. */
typedef struct
{
	int id;
	int parent;
	int class_type;
	int expand;
	int child_count;
	char display_name[64];
} BkResNodeRecord;

/* Two-pass read of every tree node, root first, in CTreeItem::treeItemList
   order - like BkEditorObjects: out_count is always the total and a short
   buffer is BK_EDITOR_REFUSED with nothing written past capacity.
   BK_EDITOR_REFUSED when no project is open. */
BkEditorStatus BkResNodes( BkResSession *session, BkResNodeRecord *out, int capacity, int *out_count );

/* One property of a node: the identifier, the names (default/display), the
   domain type (NResourceModel::EDomenType), the value's kind
   (CVariant::EKind) and its text form - what the properties pane shows and
   edits. Fixed buffers again. */
typedef struct
{
	int id;
	int domain_type;
	int value_kind;
	int combo_count;
	char default_name[64];
	char display_name[64];
	char value_text[128];
} BkResPropRecord;

/* Two-pass read of every property of a node, SProp order. BK_EDITOR_REFUSED
   when the node is unknown or no project is open. */
BkEditorStatus BkResProps( BkResSession *session, int node, BkResPropRecord *out, int capacity, int *out_count );

/* Writes a property. value_text is the text form the properties pane edits
   it as; the bridge parses it against the domain type. BK_EDITOR_REFUSED
   when the node or property is unknown; BK_EDITOR_BAD_ARGUMENT when the
   text does not parse against the domain. */
BkEditorStatus BkResSetProp( BkResSession *session, int node, int prop_id, const char *value_text );

/* Inserts a new node of the named class at parent/index (one before index
   that was there slides up to index + 1). Writes the new node's id through
   *out_id. BK_EDITOR_REFUSED when the parent does not accept this class, or
   no project is open. BK_EDITOR_BAD_ARGUMENT for an unknown class. */
BkEditorStatus BkResInsertNode( BkResSession *session, int parent, int class_type, int index, int *out_id );

/* Deletes a node and every descendant; returns the serialised subtree
   (NResourceXml) through *out_blob so the caller can hand it to
   BkResRestoreNode to undo. Two-pass like the readers: pass a null blob and
   a 0 capacity to size; call again with a buffer at least *out_size long.
   BK_EDITOR_REFUSED when the node is the root (there is no undo of that
   from the resource editor's side) or no project is open. */
BkEditorStatus BkResDeleteNode( BkResSession *session, int node, unsigned char *out_blob, int capacity, int *out_size );

/* Puts a deleted subtree back at parent/index from its serialised blob;
   writes the restored root's id through *out_id. BK_EDITOR_FAILED when the
   blob does not parse. BK_EDITOR_REFUSED when the parent does not accept
   the restored class or no project is open. */
BkEditorStatus BkResRestoreNode( BkResSession *session, const unsigned char *blob, int size, int parent, int index, int *out_id );

/* Moves a node to a new parent/index. BK_EDITOR_REFUSED for a move that
   would make a cycle, or that the new parent will not accept, or when no
   project is open. */
BkEditorStatus BkResMoveNode( BkResSession *session, int node, int new_parent, int new_index );

/* ---- References ------------------------------------------------------- */

/* An entry of a reference list, as NResourceModel::EReferenceType has the
   20 kinds (research summary). name is the choice offered; token the
   stable key the project writes. */
typedef struct { int token; char name[128]; } BkResReferenceEntry;

/* Two-pass read of a reference list by type. BK_EDITOR_BAD_ARGUMENT for a
   type outside 0..19; BK_EDITOR_REFUSED when the engine is not started. */
BkEditorStatus BkResRefList( BkResSession *session, int type, BkResReferenceEntry *out, int capacity, int *out_count );

/* ---- Geometry edits ---------------------------------------------------- */

/* A 2D point, map/scene units; the kind-specific get/set pairs below hand
   these around in bare arrays so one entry point serves every kind. */
typedef struct { float x, y; } BkResPoint2;

/* A 3D vector (particle and effect keyframes carry z too). */
typedef struct { float x, y, z; } BkResVec3;

/* The passability cell type (one byte in the map's own data: free / blocked
   / water / etc.). The get returns the whole grid; the set replaces it. The
   actual size (width, height) is written through *out_w / *out_h. */
BkEditorStatus BkResGetPassabilityCells( BkResSession *session, int node, unsigned char *out, int capacity, int *out_w, int *out_h );
BkEditorStatus BkResSetPassabilityCells( BkResSession *session, int node, const unsigned char *in, int w, int h );

/* Locked / unlocked tiles (bool grid, same shape). */
BkEditorStatus BkResGetLockedTiles( BkResSession *session, int node, unsigned char *out, int capacity, int *out_w, int *out_h );
BkEditorStatus BkResSetLockedTiles( BkResSession *session, int node, const unsigned char *in, int w, int h );

/* Transparency / one-way transparency lines (point lists). */
BkEditorStatus BkResGetTransparencyLines( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetTransparencyLines( BkResSession *session, int node, const BkResPoint2 *in, int count );

/* The zero point: one 2D vector per node (returned/set through *point). */
BkEditorStatus BkResGetZeroPoint( BkResSession *session, int node, BkResPoint2 *point );
BkEditorStatus BkResSetZeroPoint( BkResSession *session, int node, const BkResPoint2 *point );

/* The entrance point, same shape as the zero point. */
BkEditorStatus BkResGetEntrance( BkResSession *session, int node, BkResPoint2 *point );
BkEditorStatus BkResSetEntrance( BkResSession *session, int node, const BkResPoint2 *point );

/* Shoot / fire / smoke / directed-explosion points carry an angle + a cone;
   the Record struct bundles them. */
typedef struct { BkResPoint2 at; int angle; int cone; } BkResAimedPoint;
BkEditorStatus BkResGetShootPoints( BkResSession *session, int node, BkResAimedPoint *out, int capacity, int *out_count );
BkEditorStatus BkResSetShootPoints( BkResSession *session, int node, const BkResAimedPoint *in, int count );
BkEditorStatus BkResGetFirePoints( BkResSession *session, int node, BkResAimedPoint *out, int capacity, int *out_count );
BkEditorStatus BkResSetFirePoints( BkResSession *session, int node, const BkResAimedPoint *in, int count );
BkEditorStatus BkResGetSmokePoints( BkResSession *session, int node, BkResAimedPoint *out, int capacity, int *out_count );
BkEditorStatus BkResSetSmokePoints( BkResSession *session, int node, const BkResAimedPoint *in, int count );
BkEditorStatus BkResGetDirectedExplosionPoints( BkResSession *session, int node, BkResAimedPoint *out, int capacity, int *out_count );
BkEditorStatus BkResSetDirectedExplosionPoints( BkResSession *session, int node, const BkResAimedPoint *in, int count );

/* Formation positions: the slots of one squad formation (scp), one Point2
   per soldier in the order of CSquadFormationPropsItem::units. Coordinates
   are MFC's: absolute AI world units (fWorldCellSize per cell) in the
   SquadFrm view, the same space as SUnit::vPos and vZeroPos. z is dropped
   because SquadFrm always sets it to 0; the export, not this list, subtracts
   vZeroPos to get the offsets the game reads. The owner is normally a
   formation props node (ETIT_SQUAD_FORMATION_PROPS_ITEM), but like every
   geometry channel the bridge accepts any node.
   Bridge span marks: the span anchor crosses of a bridge (bdg) - BridgeFrm's
   vBeginPos, vCenterKrest, vEndPos and the front/back marks it draws at an
   anchor + m_fFront / m_fBack - as a flat Point2 list in the same AI world
   units, z dropped (BridgeFrm keeps it 0). The owner is normally a begin,
   center or end spans node (ETIT_BRIDGE_*_SPANS_ITEM).
   Both are two-pass reads like BkResNodes: out_count is always the total and
   a short buffer is BK_EDITOR_REFUSED. A set replaces the whole list; an
   un-set list reads as count 0. */
BkEditorStatus BkResGetFormationPositions( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetFormationPositions( BkResSession *session, int node, const BkResPoint2 *in, int count );
BkEditorStatus BkResGetBridgeSpanMarks( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetBridgeSpanMarks( BkResSession *session, int node, const BkResPoint2 *in, int count );

/* Mission objectives, chapter / campaign crosses, particle / effect
   keyframes - the point / 3D-vector lists each kind owns. */
BkEditorStatus BkResGetMissionObjectives( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetMissionObjectives( BkResSession *session, int node, const BkResPoint2 *in, int count );
BkEditorStatus BkResGetChapterCrosses( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetChapterCrosses( BkResSession *session, int node, const BkResPoint2 *in, int count );
BkEditorStatus BkResGetCampaignCrosses( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetCampaignCrosses( BkResSession *session, int node, const BkResPoint2 *in, int count );
BkEditorStatus BkResGetParticleKeyframes( BkResSession *session, int node, BkResVec3 *out, int capacity, int *out_count );
BkEditorStatus BkResSetParticleKeyframes( BkResSession *session, int node, const BkResVec3 *in, int count );
BkEditorStatus BkResGetEffectKeyframes( BkResSession *session, int node, BkResVec3 *out, int capacity, int *out_count );
BkEditorStatus BkResSetEffectKeyframes( BkResSession *session, int node, const BkResVec3 *in, int count );

/* ---- Export ----------------------------------------------------------- */

/* One warning line of an export - what the MFC editor's output pane
   collected. Fixed buffer; truncated at capacity, NUL-terminated. */
typedef struct { char text[256]; } BkResWarning;

/* The report BkResExport / BkResExportStatsOnly / BkResBatch all write.
   warnings is a two-pass list like the trees above: warning_count is the
   total, a short buffer answers BK_EDITOR_REFUSED. */
typedef struct
{
	int written;
	int skipped;
	int warning_count;
	BkResWarning *warnings;
	int warnings_capacity;
} BkResExportReport;

/* Exports the open project to the game's runtime resource folder (what the
   game reads). flags carry the MFC editor's bake toggles (compress, pack,
   force overwrite). BK_EDITOR_REFUSED when no project is open. */
BkEditorStatus BkResExport( BkResSession *session, int flags, BkResExportReport *report );

/* Exports only the stats sidecars, without rebaking meshes/textures - a
   fast iteration path the MFC editor's Stats button used. */
BkEditorStatus BkResExportStatsOnly( BkResSession *session, int flags, BkResExportReport *report );

/* Batch exports every project under src_folder (or the ones of kind
   `kind`) into dst_folder. Kind values follow BkResKind; -1 means all. */
BkEditorStatus BkResBatch( BkResSession *session, int kind, const char *src_folder, const char *dst_folder, int flags, BkResExportReport *report );

/* ---- MOD -------------------------------------------------------------- */

/* The mod's settings as the resource editor edits them: the mod name,
   version, and a few bake knobs the MFC editor's Mod Settings dialog
   showed. */
typedef struct
{
	char name[64];
	char version[32];
	int bake_compressed;
	int bake_packed;
} BkResModSettings;

BkEditorStatus BkResModSettingsGet( BkResSession *session, BkResModSettings *out );
BkEditorStatus BkResModSettingsSet( BkResSession *session, const BkResModSettings *in );

/* Packs the active mod's folder (base root/mods/<folder>) into a
   distributable .zip beside it. BK_EDITOR_REFUSED when no mod is active. */
BkEditorStatus BkResPackMod( BkResSession *session, const char *out_zip_path );

/* ---- Preview --------------------------------------------------------- */

/* Builds an empty preview IScene with the game camera, no terrain (except
   road3d/river3d, which the preview keeps because the kind draws itself on
   top of road/river geometry). BK_EDITOR_NO_DEVICE on a host without a GPU
   (test-resource-bridge skips with "no GPU device" in that case). */
BkEditorStatus BkResPreviewBegin( BkResSession *session, BkResKind kind );

/* Exports the open project into the preview's temp storage and builds it
   through IVisObjBuilder, so the drawn frame is what the game would draw.
   BK_EDITOR_REFUSED when no project is open or BkResPreviewBegin has not
   been called. */
BkEditorStatus BkResPreviewShow( BkResSession *session );

/* Tears the preview scene down. BK_EDITOR_OK with none active, too. */
BkEditorStatus BkResPreviewStop( BkResSession *session );

/* Starts (run=1) or stops (run=0) the preview's own animation playback -
   the running of a particle source, the walk cycle of an infantry mesh,
   etc. BK_EDITOR_REFUSED when the preview is not open. */
BkEditorStatus BkResPreviewPlayback( BkResSession *session, int run );

/* Moves the preview camera: wx/wy is the anchor in world units, zoom the
   camera's distance step. */
BkEditorStatus BkResPreviewCamera( BkResSession *session, float wx, float wy, int zoom );

/* ---- Import ----------------------------------------------------------- */

/* Builds a project from an existing runtime resource folder (the game's
   own stats + meshes) - the reverse of BkResExport. kind picks which
   project kind to build; path is the folder to read from.
   BK_EDITOR_DATA_MISSING when the folder is empty or malformed. */
BkEditorStatus BkResImportFromGame( BkResSession *session, BkResKind kind, const char *path );

#ifdef __cplusplus
}
#endif

#endif
