#pragma once
// The stats half of the S06 exporters (wpn, mcp, trc, scp) and what they
// share. Each exporter ports its MFC frame's SaveRPGStats line for line: the
// engine's stats struct is filled from the project tree and written with
// tree.Add( "RPG", &stats ) through the engine's own CTreeAccessor and
// IDataTree, so the file is what the MFC editor wrote and what the game's
// readers read. These sources need the engine's headers and StreamIO; they
// are built into the EditorBridge archive, not the engine-free model tests.
//
// MFC's frames read the tree after CreateDefaultChilds had put every item's
// values into its default order (the batch export calls it after the load,
// the editor after opening), and then index values[i] and GetChildItem( type,
// n ). The exporters do the same on a copy of the project, so a value an
// older project lacks has its default, as it had in MFC.

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "../exporter.h"
#include "../tree_item.h"

struct IDataTree;

namespace NResourceModel
{

bool ExportWeapon( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportMine( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportTrench( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportSquad( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportSprite( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportInfantry( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportMesh( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportObject( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportFence( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportBuilding( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportBridge( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportParticle( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportEffect( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportRoad3D( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportRiver3D( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportTileSet( const Project &project, const SExportContext &context, SExportOutcome &outcome );
bool ExportMedal( const Project &project, const SExportContext &context, SExportOutcome &outcome );

// CMeshFrame::SetCombatMesh's locator half (MeshFrm.cpp:1762-1885): the root's
// Locators item gets one child per skeleton node of the combat .mod, named as
// the node, in skeleton order (nLocatorID is the index). The skeleton is read
// through the structure loader as the export reads it (D019), so no window or
// GPU is needed. A model that cannot be read leaves the item empty, as MFC's
// early return did, and answers false with szMessage naming the file. Counts
// the nodes in nNodes and names the file in szModFile.
bool RebuildMeshLocators( CTreeItem &root, const SExportContext &context, int &nNodes, std::string &szModFile, std::string &szMessage );

namespace NStatsExport
{

// The project re-read from its own save, with CreateDefaultChilds run on
// the root: the tree MFC's SaveRPGStats walked. Null with outcome.szError when
// the project has no typed root of the expected type.
std::unique_ptr<Project> PreparedCopy( const Project &project, int nRootType, const char *pszKind, SExportOutcome &outcome );

// CTreeItem::GetChildItem( nType, nIndex ): the nIndex-th child of that type,
// or null.
const CTreeItem *ChildItem( const CTreeItem &item, int nType, int nIndex = 0 );
// The same, failing the export with a message naming the missing child.
const CTreeItem *RequireChild( const CTreeItem &item, int nType, int nIndex, const char *pszWhat, SExportOutcome &outcome );

// values[nIndex] converted the way MFC's CVariant converts to int, float,
// bool and const char * (COI/Variant.cpp, Optimize*): a float to int
// truncates, a string goes through atoi/atof, a number to a string through
// "%i"/"%g". A value the item lacks (only possible before CreateDefaultChilds)
// reads as MFC's VT_NULL: 0 and "".
int ValueInt( const CTreeItem &item, int nIndex );
float ValueFloat( const CTreeItem &item, int nIndex );
bool ValueBool( const CTreeItem &item, int nIndex );
std::string ValueStr( const CTreeItem &item, int nIndex );

// Where MFC's Export put the stats file, relative to the export root's data/
// folder, with backslashes. The project's own_data/export_file_name (or the
// older export_dir + "1.xml") is relative to the kind's folder szAddDir, as
// CParentFrame::OnFileExportFiles stores it. Without one, MFC proposed
// szAddDir + the project's folder relative to the source root + "1.xml"; the
// port has no source root, so the project's own folder name stands for that
// relative path. bFileNamedAfterFolder: the shipped weapons are flat files
// named after their project folder (weapons\<name>.xml), not <name>\1.xml.
std::string StatsFileName( const Project &project, const SExportContext &context, const std::string &szAddDir, bool bFileNamedAfterFolder );

// File-name helpers the frame exporters share. Windows file systems ignored
// case and the shipped data keeps MFC-era mixed case, so a name is looked up
// ignoring case on Linux.
std::string ToSlashes( std::string s );
// The entry of dir named szName, ignoring case, or the plain join when there
// is none.
std::filesystem::path FoldedChild( const std::filesystem::path &dir, const std::string &szName );
// A whole path (any separators) with each component folded in turn; the part
// that does not exist is kept as written.
std::filesystem::path FoldedFile( const std::string &szPath );
// MFC's IsRelatedPath: neither a drive nor a root.
bool IsRelatedPath( const std::string &szPath );
// MakeFullPath( szFullDirName, szRelName ) of editor/frames.cpp, backslashes
// throughout; the caller converts.
std::string MakeFullPath( const std::string &szFullDirName, const std::string &szRelName );
// The stand-in picture of a missing frame, editor\invalid.tga of the data
// the export root holds; empty when there is none.
std::filesystem::path InvalidPicture( const SExportContext &context );
// GetFileChangeTime: the change time; min() for a file that is not there.
std::filesystem::file_time_type ChangeTime( const std::filesystem::path &file );

// The folder of a storage-relative file name, with its trailing backslash
// (MFC's GetDirectory).
std::string DirectoryOf( const std::string &szName );

// The project folder (MFC's GetDirectory( pszProjectName )), with a trailing
// separator.
std::string ProjectDirectory( const SExportContext &context );

// Creates szName (relative, backslashes) below the staging root through the
// engine's file storage, as MFC's Export did with CreateFileStream and
// CreateDataTreeSaver( WRITE ), and hands write the tree to fill. False with
// outcome.szError naming the path when it cannot be written; counts the file
// in outcome.nWritten when it is.
bool WriteStats( const SExportContext &context, const std::string &szName, const std::function<void( IDataTree * )> &write, SExportOutcome &outcome, const char *pszRootName = nullptr );

}

}
