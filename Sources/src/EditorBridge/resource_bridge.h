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

/* Renames a node: CTreeItem::ChangeItemName, the displayed name only (the
   default name stays what the class gave it). The project writes it as the
   item's display_name. BK_EDITOR_BAD_ARGUMENT for a null or empty name or
   one longer than BkResNodeRecord's display_name holds; BK_EDITOR_REFUSED
   when the node is unknown or no project is open. */
BkEditorStatus BkResSetNodeName( BkResSession *session, int node, const char *name );

/* Records whether a node is expanded in the tree: the item's "expand"
   attribute, which MFC's SaveTree took from the tree control's
   TVIS_EXPANDED state and InsertChildItems opened the node by. BkResNodes
   answers it in BkResNodeRecord.expand. BK_EDITOR_REFUSED when the node is
   unknown or no project is open. */
BkEditorStatus BkResSetNodeExpand( BkResSession *session, int node, int expand );

/* ---- References ------------------------------------------------------- */

/* An entry of a reference list, as NResourceModel::EReferenceType has the
   20 kinds (research summary). name is the choice offered; token the
   stable key the project writes. */
typedef struct { int token; char name[128]; } BkResReferenceEntry;

/* Two-pass read of a reference list by type: out_count is always the total
   and a short buffer is BK_EDITOR_REFUSED. The lists are the ones MFC's
   CReferenceDialog::InitLists filled, walked by NResourceModel::References
   over <BaseRoot>Data and, while a mod is active, the mod's data folder
   after it (an entry the mod repeats is listed once). token is the entry's
   index in its list. The one exception is type 5 (E_ACTIONS_REF), which
   MFC's InitLists left empty: its list is CMultySelDialog's, the action
   types of Data/Editor/actions.ini in file order, and token is the action's
   id, the bit the property's hex mask sets (MultySelDialog.cpp OnOK).
   BK_EDITOR_BAD_ARGUMENT for a type outside 0..19; BK_EDITOR_REFUSED when
   the engine is not started. */
BkEditorStatus BkResRefList( BkResSession *session, int type, BkResReferenceEntry *out, int capacity, int *out_count );

/* Two-pass read of a property's strings (SProp::szStrings, MFC's
   SCOIProperties::szStrs), in order: a DT_COMBO's or DT_BOOL's choices,
   or a DT_BROWSE / DT_BROWSEDIR's source folder [0] and file filter [1].
   BkResPropRecord.combo_count is their count. token is the index; name is
   the string. BK_EDITOR_REFUSED when the node or property is unknown, no
   project is open, or the buffer is short. */
BkEditorStatus BkResPropStrings( BkResSession *session, int node, int prop_id, BkResReferenceEntry *out, int capacity, int *out_count );

/* ---- Geometry edits ---------------------------------------------------- */

/* A 2D point, map/scene units; the kind-specific get/set pairs below hand
   these around in bare arrays so one entry point serves every kind.
   Every geometry channel is stored where MFC keeps it (the table in the
   phase 6 spec's geometry section): the building / object / bridge
   project's own_data and desc on the root, the RPG copy of the map
   crosses, or fields of the tree items (a squad formation's ZeroPos and
   units, a fence segment's or bridge part's LockedTiles, a particle track's
   Key_frames, the position values of crosses and effect parts). The values are MFC's as stored
   (desc positions relative to the zero point, krest_pos in view units,
   angles in degrees), written with MFC's %g, so six significant digits
   survive a save. On a node where MFC has no home for the channel a get or
   set is BK_EDITOR_REFUSED. */
typedef struct { float x, y; } BkResPoint2;

/* A 3D vector (particle and effect keyframes carry z too). */
typedef struct { float x, y, z; } BkResVec3;

/* The passability cell type (one byte in the map's own data: free / blocked
   / water / etc.). The get returns the whole grid; the set replaces it. The
   actual size (width, height) is written through *out_w / *out_h. */
BkEditorStatus BkResGetPassabilityCells( BkResSession *session, int node, unsigned char *out, int capacity, int *out_w, int *out_h );
BkEditorStatus BkResSetPassabilityCells( BkResSession *session, int node, const unsigned char *in, int w, int h );

/* Locked tiles (same grid shape): cell (x, y) is AI tile (x, y) of the
   item's LockedTiles. MFC stores only non-zero tiles, so a read grid ends at
   the furthest set tile. */
BkEditorStatus BkResGetLockedTiles( BkResSession *session, int node, unsigned char *out, int capacity, int *out_w, int *out_h );
BkEditorStatus BkResSetLockedTiles( BkResSession *session, int node, const unsigned char *in, int w, int h );

/* Object, building and fence transparency (S09, S10). All are the tile frame, like the
   locked tiles: cell (x, y) is tile (x, y), values 0..7 (anything above is
   BK_EDITOR_BAD_ARGUMENT), and a read grid ends at the furthest set tile.
   BkResGet/SetTransparencyCells is an object or building root's transparency
   grid (the desc visibility without the one-way tiles, which an object's
   trans-lines give and a building has none of);
   BkResGet/SetFenceTransparences a fence segment's Transparences list.
   BkResGet/SetPassabilityCells on an object or building root is the same tile frame: the
   set grid is cropped at save and desc passability gets the origin of the zero
   point the save ends with, so the pair stays consistent whatever order the
   edits came in. On a fence segment it is the locked tiles (MFC keeps no
   passability grid for a fence; the exporter builds one). */
BkEditorStatus BkResGetTransparencyCells( BkResSession *session, int node, unsigned char *out, int capacity, int *out_w, int *out_h );
BkEditorStatus BkResSetTransparencyCells( BkResSession *session, int node, const unsigned char *in, int w, int h );
BkEditorStatus BkResGetFenceTransparences( BkResSession *session, int node, unsigned char *out, int capacity, int *out_w, int *out_h );
BkEditorStatus BkResSetFenceTransparences( BkResSession *session, int node, const unsigned char *in, int w, int h );

/* The sprite's place: an object or building root's own_data sprite_pos or a
   fence segment's SpritePos (x, y; the z stays as stored). A building's Move
   object drag changes this and the zero point together; the points stay put
   relative to the zero point. */
BkEditorStatus BkResGetSpritePos( BkResSession *session, int node, BkResPoint2 *point );
BkEditorStatus BkResSetSpritePos( BkResSession *session, int node, const BkResPoint2 *point );

/* One-way transparency lines: a point list, two points (Point1, Point2) per
   line; an odd count is BK_EDITOR_BAD_ARGUMENT. */
BkEditorStatus BkResGetTransparencyLines( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetTransparencyLines( BkResSession *session, int node, const BkResPoint2 *in, int count );

/* The zero point: one 2D vector per node (returned/set through *point). */
BkEditorStatus BkResGetZeroPoint( BkResSession *session, int node, BkResPoint2 *point );
BkEditorStatus BkResSetZeroPoint( BkResSession *session, int node, const BkResPoint2 *point );

/* A squad formation's direction (CSquadFormationPropsItem::fFormationDir,
   the FormationDir attribute; SquadFrm's direction arrow in Set Zero mode).
   x is the angle in radians as MFC stores it; y is ignored on a set and reads
   0. A set writes only the angle: MFC also turns the slots about the zero
   point (CalculateNewPositions), which the editor writes through
   BkResSetFormationPositions in the same undo step. The owner is a formation
   props node; any other node is BK_EDITOR_REFUSED. */
BkEditorStatus BkResGetFormationDirection( BkResSession *session, int node, BkResPoint2 *direction );
BkEditorStatus BkResSetFormationDirection( BkResSession *session, int node, const BkResPoint2 *direction );

/* The entrance point, same shape as the zero point. */
BkEditorStatus BkResGetEntrance( BkResSession *session, int node, BkResPoint2 *point );
BkEditorStatus BkResSetEntrance( BkResSession *session, int node, const BkResPoint2 *point );

/* Shoot / fire / smoke / directed-explosion points carry an angle + a cone;
   the Record struct bundles them. angle is the desc entry's Direction; cone
   is a fire slot's Angle and a fire / smoke / explosion point's
   VerticalAngle, rounded to whole degrees on read. On a building the points
   also live on tree children (a slot, fire, smoke or explosion item per
   point): a set copies each point's angle and cone to the same-index child's
   Direction and Angle / Vertical angle values, and leaves the child count to
   BkResInsertNode / BkResDeleteNode (the editor does both in one undo step).
   The five directed-explosion children are fixed, so that list has no insert
   or delete. */
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
   per soldier in the order of CSquadFormationPropsItem::units (SUnit::vPos).
   Coordinates are MFC's: absolute AI world units (fWorldCellSize per cell)
   in the SquadFrm view, the same space as vZeroPos. A slot keeps its z and
   Dir; a new one gets AddUnit's z = 0 and Dir = 0, and a shorter list drops
   the tail. The owner is a formation props node
   (ETIT_SQUAD_FORMATION_PROPS_ITEM); any other node is BK_EDITOR_REFUSED.
   Bridge span marks: the bridge root's (bdg) CBridgeFrame own data, always
   three points: Begin (x, y), End (x, y) and (Front, Back), the offsets of
   the front and back marks from an anchor along the bridge. Any other count
   is BK_EDITOR_BAD_ARGUMENT; a bridge saved without own data reads as count
   0. The z of Begin and End stays as read.
   Both are two-pass reads like BkResNodes: out_count is always the total and
   a short buffer is BK_EDITOR_REFUSED. A set replaces the whole list. */
BkEditorStatus BkResGetFormationPositions( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetFormationPositions( BkResSession *session, int node, const BkResPoint2 *in, int count );
BkEditorStatus BkResGetBridgeSpanMarks( BkResSession *session, int node, BkResPoint2 *out, int capacity, int *out_count );
BkEditorStatus BkResSetBridgeSpanMarks( BkResSession *session, int node, const BkResPoint2 *in, int count );

/* Map crosses: one point per child of a mission's Objectives node, a
   chapter's Missions or Place holders node, or a campaign's Chapters node,
   in child order - the children's position values (MFC's Get*Position).
   The list must carry exactly one point per child (BK_EDITOR_BAD_ARGUMENT
   otherwise). A save also writes them into the project's RPG copy when it
   has one; while a list is unedited it is read from there, as MFC's
   LoadRPGStats does. Without an RPG copy MFC keeps only the whole part of
   a cross across a reopen (the values' default type is int).
   Particle keyframes: the keys of one particle track (any CKeyFrameTreeItem
   node: Key_frames), x = time and y = value; z must be 0
   (BK_EDITOR_BAD_ARGUMENT otherwise) and reads as 0.
   Effect keyframes: one place per child of an effect's Animations, Meshes,
   Function Particles or Maya Particles node, its X / Y / Z position values.
   These are whole numbers in MFC (DT_DEC), so a fraction, or a count that
   is not one per child, is BK_EDITOR_BAD_ARGUMENT.
   On any other node these channels are BK_EDITOR_REFUSED. */
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

/* The report BkResExport / BkResExportStatsOnly / BkResBatch all write
   (null: no report). The export has happened by the time it is filled, so
   unlike the two-pass reads above a short warnings buffer is not refused:
   at most warnings_capacity lines are copied and warning_count is always the
   total. written counts files moved into place (BkResBatch with
   BK_RES_EXPORT_OPEN_SAVE: projects re-saved); skipped counts files left as
   up to date and, in a batch, projects that failed. */
typedef struct
{
	int written;
	int skipped;
	int warning_count;
	BkResWarning *warnings;
	int warnings_capacity;
} BkResExportReport;

/* Export flags. FORCE is MFC's batch -f (export even when up to date);
   OPEN_SAVE is MFC's -os (BkResBatch only: open and re-save each project,
   export nothing). */
#define BK_RES_EXPORT_FORCE 1
#define BK_RES_EXPORT_OPEN_SAVE 2

/* Exports the open project into the export root's data/ folder (the export
   dir of BkResModSettings; spec "Saving and exporting"). The kind's exporter
   (NResourceModel::RegisterExporter, one per sub-editor slice) writes into a
   staging folder; only when it succeeds are the files moved into place, so
   a failed export leaves no half-written resource. The project must have
   been saved (its sources are relative to its file). BK_EDITOR_REFUSED when
   no project is open, the project has no path yet, the export root is the
   shipped Data/ folder, or the kind's exporter is not ported yet (the
   message says so; nothing is written). BK_EDITOR_FAILED when the exporter
   fails; its reason is the message. */
BkEditorStatus BkResExport( BkResSession *session, int flags, BkResExportReport *report );

/* BkResExport with the exporter told to write only the stats and leave the
   exported graphics untouched - MFC's Infantry-only "Export RPG stats only",
   offered for every kind (D-13). */
BkEditorStatus BkResExportStatsOnly( BkResSession *session, int flags, BkResExportReport *report );

/* The open Mission's map pictures (MinimapCreation.cpp's Create1Minimap):
   map_c.dds, map_l.dds and map_h.dds, 512x512, written beside the project
   file from its Final map (found ignoring case in the export root's maps
   folder, then the shipped Data's), through the engine's minimap code. Left
   alone, still OK, when map_h.dds is newer than the map. BK_EDITOR_REFUSED,
   with the message naming why, when the open project is not a Mission, has no
   path yet, lies inside the shipped Data (never written), has no Final map or
   its map is not found; BK_EDITOR_FAILED when the engine cannot read the map
   or write the pictures. The Mission export asks for the same pictures and
   for the map's .xml converted to .bzm through the export context. */
BkEditorStatus BkResMissionMinimap( BkResSession *session );

/* MFC's batch mode: every project file under src_folder (recursively) of
   kind `kind` (-1: all 21 extensions, in BkResKind order) is exported into
   dst_folder's data/ folder, or with BK_RES_EXPORT_OPEN_SAVE only opened and
   re-saved. The open project is not touched. As in MFC, a project that
   fails does not stop the batch: it is counted as skipped and its reason is
   a warning ("<path>: <reason>"). BK_EDITOR_BAD_ARGUMENT for a null folder
   or a kind outside -1..20; BK_EDITOR_DATA_MISSING when src_folder is not a
   folder; BK_EDITOR_REFUSED when dst_folder's data/ is the shipped Data/. */
BkEditorStatus BkResBatch( BkResSession *session, int kind, const char *src_folder, const char *dst_folder, int flags, BkResExportReport *report );

/* ---- MOD -------------------------------------------------------------- */

/* The fields of MFC's MOD Settings dialog (CMODDialog): the export dir is
   the mod's own folder (MFC's "Composer Destination Directory", default
   mods\mymod\); exports and mod.xml go into its data/ folder. name,
   version and desc are mod.xml's MODName, MODVersion and MODDesc. All
   fields are NUL-terminated; a longer value is truncated. */
typedef struct
{
	char export_dir[260];
	char name[64];
	char version[32];
	char desc[256];
} BkResModSettings;

/* The current settings. Until a Set the export dir is <BaseRoot>mods/<the
   active mod>/ (mods/mymod/ with none active, as MFC); name, version and
   desc are read from that folder's data/mod.xml by the engine's own reader
   and are empty when there is none. */
BkEditorStatus BkResModSettingsGet( BkResSession *session, BkResModSettings *out );

/* MFC's OnMODSettings: takes the export dir for this session, writes
   <export dir>/data/mod.xml with the engine's tree saver exactly as
   CEditorApp::WriteMODFile did, and seeds data/modobjects.xml from the data
   storage's editor\modobjects.xml when the mod has none. BK_EDITOR_BAD_ARGUMENT
   for a null input or an empty export dir; BK_EDITOR_REFUSED when the
   folder is the shipped Data/ or cannot be written. */
BkEditorStatus BkResModSettingsSet( BkResSession *session, const BkResModSettings *in );

/* "Compress MOD to PAK": zips the export root's data/ folder (MFC ran
   `zip -9 -R -D <pak> *.*` there) into out_zip_path with the zip writer in
   the build - deflate level 9 (stored when that is not smaller), forward
   slashes, no directory entries, CRC and DOS time. The archive is then
   mounted the way the game mounts a mod's *.pak (an engine storage over a
   folder holding only it) and every entry read back and compared; a
   mismatch removes it and answers BK_EDITOR_FAILED.
   BK_EDITOR_BAD_ARGUMENT for a null path; BK_EDITOR_REFUSED when the data/
   folder is missing or empty, or out_zip_path lies inside it. */
BkEditorStatus BkResPackMod( BkResSession *session, const char *out_zip_path );

/* Reads one file of the export root's data/ folder, given relative to it,
   with the engine's own reader and counts what it found: a .san through the
   structure saver as the Game's animation manager loads it (found is the
   number of sprite frames, at least one), any other file through the XML
   tree reader as the Game's stats and descriptor loaders do (found is 1 when
   <chunk> is there under the root element `root`, "" meaning base).
   BK_EDITOR_BAD_ARGUMENT for a null argument, BK_EDITOR_REFUSED when the
   engine is not started, BK_EDITOR_DATA_MISSING when the file is missing and
   BK_EDITOR_FAILED, with the reason, when the engine cannot read it or finds
   nothing. Nothing is written. */
BkEditorStatus BkResReadBack( BkResSession *session, const char *data_file, const char *root, const char *chunk, int *found );

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

/* MFC's Camera button (ParticleFrm.cpp OnButtonCamera, EffectFrm.cpp): swaps
   the scene's default camera (SetDefaultCamera: pitch -120 degrees, yaw 45)
   for the horizontal one (SetHorizontalCamera: pitch -90 degrees, yaw 45, the
   view straight down the ground plane's normal), which BkResPreviewCamera's
   anchor and zoom cannot express. horizontal=1 sets the horizontal camera,
   0 puts the preview's default placement back. Both keep the anchor the
   preview was begun with and the zoom step in force. BK_EDITOR_REFUSED when
   the preview is not open. */
BkEditorStatus BkResPreviewCameraMode( BkResSession *session, int horizontal );

/* The wire frame of the road and river previews (the frames' OnSwitchWireframeMode):
   a render state, drawn from the next frame on, back off by BkResPreviewStop.
   BK_EDITOR_REFUSED unless a .3rd or .3rv preview has begun. */
BkEditorStatus BkResPreviewWireframe( BkResSession *session, int on );

/* ---- Key-frame curves ------------------------------------------------- */

/* The knobs of one key-frame node (a CKeyFrameTreeItem: the Particle
   project's curves), which CKeyFrameEditor::SetDimentions took from the
   item: the range the curve is clamped to, the step the nodes snap to and
   whether the range grows with the keys (resize_mode, 0 or 1). The values are
   the ones the item's InitDefaultValues set; they are not stored in the
   project. */
typedef struct
{
	float min_x, max_x;
	float min_y, max_y;
	float step_x, step_y;
	int resize_mode;
} BkResKeyframeKnobs;

/* Reads the knobs of `node_id` into *out. BK_EDITOR_BAD_ARGUMENT for a null
   out or a node that is not a key-frame item; BK_EDITOR_REFUSED when no project
   is open or the node id is unknown. */
BkEditorStatus BkResGetKeyframeKnobs( BkResSession *session, int node_id, BkResKeyframeKnobs *out );

/* ---- Particle info ---------------------------------------------------- */

/* CParticleFrame::GetParticleInfo's four numbers: the particle source's
   SParticleSourceInfo, which the frame's status bar shows as "Max particles",
   "Size", "Average size" and "Average count". */
typedef struct
{
	float max_count;
	float max_size;
	float average_size;
	float average_count;
} BkResParticleInfo;

/* Runs the real .pcp exporter into the preview staging folder, builds the
   effect through IVisObjBuilder as BkResPreviewShow does (beginning the
   particle preview first when none is begun) and reads the info of the
   effect's last particle source into *out. BK_EDITOR_BAD_ARGUMENT for a null
   out; BK_EDITOR_REFUSED, with the reason in BkEditorLastMessage, when no
   project is open, the open project is not a .pcp, or it was never saved;
   BK_EDITOR_FAILED when the export or build fails or the effect holds no
   particle source with info. */
BkEditorStatus BkResGetParticleInfo( BkResSession *session, BkResParticleInfo *out );

/* ---- Particle source mode ---------------------------------------------- */

/* MFC kept simple against complex in the frame (bComplexSource, the Particle
   source toolbar button). The port reads it from the project: a .pcp is
   complex when the "Particle reference" of its complex source item is
   non-empty, which is also what the exporter writes as ComplexParticleSource.
   BkResParticleSourceMode reads that into *complex (1 or 0);
   BkResParticleSetSourceMode switches it: complex fills the reference with
   name, simple clears it. Both are BK_EDITOR_REFUSED with the reason in
   BkEditorLastMessage when no project is open or it is not a .pcp; setting
   complex with a null or empty name is refused too. They edit the value in
   place; the app records the same change as a property edit for undo. */
BkEditorStatus BkResParticleSourceMode( BkResSession *session, int *complex );
BkEditorStatus BkResParticleSetSourceMode( BkResSession *session, int complex, const char *name );

/* ---- Unit (mesh) preview ---------------------------------------------- */

/* One locator of the unit's skeleton: the node id is the skeleton node's
   index, the same as the tree's Locators child of that position. world is
   where the node sits in the preview scene now (the object's placement and
   direction applied); screen is world run through the preview scene's own
   transform (IScene::GetPos2), in the viewport's pixels. */
typedef struct
{
	int node_id;
	char name[64];
	float wx, wy, wz;
	float sx, sy;
} BkResLocator;

/* Shows one model variant of the previewed unit, replacing the one drawn:
   0 is the combat model, 1 the install model and 2 the transportable one -
   MFC's SetCombatMesh, SetInstallMesh and SetTransportableMesh, which built
   them from the 1st, 2nd and 3rd model name of Graphics Info. The install and
   transportable models are built with the combat model's texture, as MFC
   made them (it saved the alive summer texture for each). The direction and
   the locator display carry over. BK_EDITOR_REFUSED when the preview shows no
   unit (BkResPreviewBegin(msh) and BkResPreviewShow first), the variant is
   outside 0..2, or its model is unavailable (the name is empty or the file
   was not exported); the message names the file. */
BkEditorStatus BkResPreviewMeshVariant( BkResSession *session, int variant );

/* Turns the previewed unit to `angle` degrees (0..359; MFC's direction dock
   gave the engine's 16-bit direction, this is the same turn as degrees) and
   moves the locator sprites with it. BK_EDITOR_REFUSED when no unit shows. */
BkEditorStatus BkResPreviewDirection( BkResSession *session, int angle );

/* The Effect editor's direction dock (CEffectFrame, ID_SHOW_DIRECTION_BUTTON).
   The angle (radians, the dock's -pi..pi with 0 pointing right) is view state:
   it is not saved, not an undo step, starts at 45 degrees and returns to it
   whenever a project is opened or created. While the effect preview runs
   (BkResPreviewPlayback 1) a set applies UpdateEffectAngle's turn through the
   running effect's SetEffectDirection; Run applies the stored angle again;
   a stopped preview only stores it. BK_EDITOR_REFUSED unless a .eff project
   is open (Set), BK_EDITOR_BAD_ARGUMENT for a non-finite angle or null out. */
BkEditorStatus BkResEffectSetDirection( BkResSession *session, float angle );
BkEditorStatus BkResEffectGetDirection( BkResSession *session, float *angle );

/* UpdateEffectAngle's matrix for an angle, row-major 4x4 into out[16]: the
   same math the running effect receives, exposed so a test can pin it. */
BkEditorStatus BkResEffectDirectionMatrix( float angle, float *out );

/* MFC's OnShowLocatorsInfo: locators != 0 draws one editor\locator\1 sprite
   at every skeleton node, bounding_boxes != 0 turns on the scene's
   SCENE_SHOW_BBS. The two are independent here (MFC tied both to one toggle).
   BK_EDITOR_REFUSED when no unit shows. */
BkEditorStatus BkResPreviewShowLocators( BkResSession *session, int locators, int bounding_boxes );

/* The skeleton nodes of the shown model variant with their positions. Fills
   at most cap entries and always stores the total in *count, so a caller can
   size its buffer with cap = 0. BK_EDITOR_REFUSED when no unit shows. */
BkEditorStatus BkResMeshLocators( BkResSession *session, BkResLocator *out, int cap, int *count );

/* ---- Import ----------------------------------------------------------- */

/* Import from game data (D-13): builds a new, unsaved project of `kind`
   from a runtime resource folder (path, holding its 1.xml), the reverse of
   BkResExport. The stats are read by the engine's own operator& and put into
   the tree by the frame's GetRPGStats, ported line for line; graphics-source
   fields stay empty. Ported: weapon (wpn; path may also be the flat
   weapons\<name>.xml itself), mine (mcp), trench (trc: no segments, as in
   MFC), squad (scp: MFC never wrote this one, its load is commented out;
   the port does the inverse of its export), infantry (unt) and medal (mdc:
   MFC's GetRPGStats is commented out too; the port reads the name,
   description and picture back from the stats and keeps the file's place
   below medals\ as the project's export file name). The 3D road
   (3rd) and 3D river (3rv) take the runtime <name>.xml file itself as path
   (terrain\sets\1\roads3d\road_pavement.xml), not a folder holding 1.xml;
   a file of the other kind is refused naming its type. Every other
   kind answers BK_EDITOR_REFUSED, naming the kind, and keeps the open
   project: sprite (spt) because its export only composes .san packs and MFC
   has no reverse path, the rest until their sub-editor slice ports theirs.
   BK_EDITOR_BAD_ARGUMENT for a null path or an unknown kind;
   BK_EDITOR_DATA_MISSING when the stats file is missing or will not read. */
BkEditorStatus BkResImportFromGame( BkResSession *session, BkResKind kind, const char *path );

/* CTileSetFrame::OnImportTerrains / OnImportCrossets (TileSetFrm.cpp:557, 782):
   reads the tileset editor file `path` (an .xml holding a "tileset" tree, or a
   "crosset" tree when crossets != 0) and the .tga atlas of the same name
   beside it, cuts the atlas into 64 x 32 tiles with the tile mask
   (editor\terrain\tilemask.tga of the shipped Data folder) and writes them
   as <index>.tga into the project folder's terrains\ (crossets\) folder,
   never into Data. The open .til's Terrains (Crossets) item loses its
   children and takes one props item per terrain type (crosset) with its tile
   items, and its directory value becomes "terrains\\" ("crossets\\"). The
   whole tree is built and every tile written before the old children go, so
   a refusal leaves the project as it was. *out_count (may be null) is the
   number of tiles written. The editor records the replacement as delete,
   set-property and insert commands, which is one undo step; the tile files
   stay on disk. BK_EDITOR_REFUSED when no .til is open, the project has
   no path yet, or the file holds no terrain types (crossets); the message
   names the file. BK_EDITOR_DATA_MISSING for a missing or unreadable xml,
   atlas or mask. BK_EDITOR_FAILED when a tile lies outside the atlas, a
   crosset names a group the crosset does not have, or a tile cannot be
   written. BK_EDITOR_BAD_ARGUMENT for a null or empty path. */
BkEditorStatus BkResTileSetImport( BkResSession *session, const char *path, int crossets, int *out_count );

/* The thumbnail double-click (CTileSetFrame::DoubleClickOnThumbList): adds a
   tile props item named after the picture file (its name without folder and
   extension) at the end of the terrain's Tiles item (a CTileSetTilePropsItem
   with GetFreeTerrainIndex) or of a crosset group (a CCrossetTilePropsItem
   with GetFreeCrossetIndex), writes its id through *out_id and leaves the
   index to the pool: the lowest no tile of its list uses, so removing the
   item frees it. `parent` is a Tiles item, a crosset group item, or a terrain
   props item (its Tiles item is meant). BK_EDITOR_BAD_ARGUMENT when parent is
   any other item or the picture name is empty; BK_EDITOR_REFUSED when no .til
   is open, the node is unknown, or the list already has a tile of that name
   (MFC ignored the second click). The call is not recorded: the editor
   records it as an insert command carrying the name. */
BkEditorStatus BkResTileSetAddTile( BkResSession *session, int parent, const char *picture_path, int *out_id );

/* ---- GUI screens (kind gui, D031) ---------------------------------------
   BkResOpen opens a game UI screen - a <base> document, or a
   GUI_Composer_Project that carries windows (WindowPos) rather than the
   palette <childs> tree - as a gui project: kind 20 (BkResKindOf), no tree
   nodes (BkResNodes is BK_EDITOR_REFUSED), and the calls below. The screen is
   kept as its text; an edit rewrites only the bytes it changes, so BkResSave
   of an unedited screen is byte-identical. BkResExport writes the screen under
   a <base> root to <export root>/data/ui/<Screen>.xml (refusing a window
   without a WindowPos, naming file and line), the plain overlay the
   Game reads for a -mod (items/gui/gui_export.h); <Screen> is the saved
   file's name without extension. It is refused (BK_EDITOR_REFUSED, message
   naming the target path) when the export root is the shipped Data folder.
   A malformed screen is BK_EDITOR_DATA_MISSING with "<file>:<line>: <reason>".
   Every call is traced under BK_DEBUG_LOG=1 ("BkResGui: ..." on stderr).
   Window ids are the model's: the root is 0, every other id is handed out on
   open or insert and is never reused. Each call is BK_EDITOR_REFUSED when
   the open project is not a screen; an unknown window id is
   BK_EDITOR_BAD_ARGUMENT. Text outputs write *out_size = the size needed
   (without the terminator) whatever the capacity, and a buffer too small is
   BK_EDITOR_REFUSED with nothing written; a null buffer only asks the size. */

/* BkResGuiWindows: ten ints per window in document order - id, parent (-1 for
   the root), ClassTypeID, ElementID, PositionFlag, x, y, w, h (rounded to
   whole pixels), visible flag. *out_count is the number of windows; capacity
   counts windows (not ints). */
BkEditorStatus BkResGuiWindows( BkResSession *session, int *out, int capacity, int *out_count );

/* BkResGuiSetRects: count entries of six ints - id, PositionFlag, x, y, w, h.
   All or nothing: a bad entry leaves every window as it was. */
BkEditorStatus BkResGuiSetRects( BkResSession *session, const int *in, int count );

/* BkResGuiInsertTemplate: adds the window tree of a Data/Editor/UI template
   file under parent, with its WindowPos set to x, y; *out_id (may be null) is
   the new window's id (-1 on failure). BK_EDITOR_DATA_MISSING for an
   unreadable template, BK_EDITOR_FAILED for one that is not a window. */
BkEditorStatus BkResGuiInsertTemplate( BkResSession *session, int parent, const char *template_path, int x, int y, int *out_id );

/* BkResGuiDelete: removes the windows and their subtrees. BK_EDITOR_REFUSED
   when an id is the root or unknown. */
BkEditorStatus BkResGuiDelete( BkResSession *session, const int *ids, int count );

/* BkResGuiCopy / BkResGuiPaste: the clipboard is text. Copy writes the
   outermost windows among ids; Paste appends them under parent, each moved by
   (dx, dy), and writes the new top-level windows' ids to out_ids (capacity
   ints; *out_count is the total). BK_EDITOR_REFUSED for text that holds no
   windows.
   With unique_ids nonzero (a user's paste) a pasted window, nested ones
   included, whose ElementID the screen or an earlier window of the same paste
   already uses gets the next free id above it; ElementID -1 is never changed.
   Each change is three ints in out_changes - the pasted window's id, the old
   and the new ElementID - in document order; change_capacity counts changes
   and *out_change_count (may be null) is the total. unique_ids zero keeps
   every id, for an undo or redo that must restore the same bytes. */
BkEditorStatus BkResGuiCopy( BkResSession *session, const int *ids, int count, char *out, int capacity, int *out_size );
BkEditorStatus BkResGuiPaste( BkResSession *session, int parent, const char *clipboard, int dx, int dy, int unique_ids, int *out_ids, int capacity, int *out_count,
                              int *out_changes, int change_capacity, int *out_change_count );

/* BkResGuiGetAttr / BkResGuiSetAttr: one XML attribute of a window's element
   (Name, ElementID, PositionFlag ...). Get is BK_EDITOR_DATA_MISSING when the
   window has no such attribute. */
BkEditorStatus BkResGuiGetAttr( BkResSession *session, int id, const char *name, char *out, int capacity, int *out_size );
BkEditorStatus BkResGuiSetAttr( BkResSession *session, int id, const char *name, const char *value );

/* BkResGuiTemplates: the template palette as text, one line per file -
   <Folder>\t<File>\t<full path>\n - from <data>/Editor/UI/<Folder>/*.xml in
   folder-name then file-name order, then the files of user_folder (may be
   null) under the folder name "User". Needs no open screen. */
BkEditorStatus BkResGuiTemplates( BkResSession *session, const char *user_folder, char *out, int capacity, int *out_size );

#ifdef __cplusplus
}
#endif

#endif
