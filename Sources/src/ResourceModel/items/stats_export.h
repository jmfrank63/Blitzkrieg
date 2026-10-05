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
bool WriteStats( const SExportContext &context, const std::string &szName, const std::function<void( IDataTree * )> &write, SExportOutcome &outcome );

}

}
