// The trench exporter: CTrenchFrame::SaveRPGStats (Sources/src/editor/
// TrenchFrm.cpp:123-370) for the stats, and ExportFrameData's graphics half:
// the segment models copied beside the stats by basename and 1/1w/1a converted
// from the first model's folder.
//
// MFC's up-to-date check compares the sources with the export's "1.tga", which
// the export never writes (it writes 1_c.dds and the like), so that file was
// always missing and every export ran. The port exports every time as well and
// counts nothing as skipped.
//
// MFC copied each segment model into the editor's temp folder, read its
// bounding box from chunk 4 of the .mod and built it with IVisObjBuilder to
// read its locators: the fire places. The box is read here the same way; the
// locators come from SExportContext::meshFirePlaces, which needs the engine's
// mesh builder.
#include "StdAfx.h"

#include <filesystem>
#include <list>

#include "../stats_export.h"
#include "../../image_export.h"
#include "../tree_item_types.h"
#include "trench.h"
#include "../../../Main/RPGStats.h"
#include "../../../Formats/fmtMesh.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;

struct SMySegment
{
	int nIndex;
	SEntrenchmentRPGStats::SSegmentRPGStats segment;
};
inline bool operator<( const SMySegment &m1, const SMySegment &m2 ) { return m1.nIndex < m2.nIndex; }

// MFC's MakeFullPath( GetDirectory( pszProjectName ), szRel ) or the path as
// is, with the separators this file system uses.
std::string SourcePath( const SExportContext &context, const std::string &szRel )
{
	const bool bRelative = szRel.empty() || ( szRel[0] != '\\' && szRel[0] != '/' && szRel.find( ':' ) == std::string::npos );
	std::string szFull = bRelative ? ProjectDirectory( context ) + szRel : szRel;
	for ( char &c : szFull )
		if ( c == '\\' )
			c = '/';
	return szFull;
}

// CTrenchFrame::LoadRPGStats, which the batch export ran before
// ExportFrameData: a segment without an index (an older project) gets its
// running number over all four folders.
void AssignTrenchIndices( CTreeItem &rootItem )
{
	int nIndex = 0;
	for ( int nTrenchIndex = 0; nTrenchIndex < 4; nTrenchIndex++ )
	{
		const CTreeItem *pTrenchParts = ChildItem( rootItem, ETIT_TRENCH_SOURCES_ITEM, nTrenchIndex );
		if ( pTrenchParts == nullptr )
			continue;
		for ( const auto &pPart : const_cast<CTreeItem *>( pTrenchParts )->MutableChildren() )
		{
			if ( auto *pTrenchProps = dynamic_cast<CTrenchSourcePropsItem *>( pPart.get() ) )
				if ( pTrenchProps->nTrenchIndex == -1 )
					pTrenchProps->nTrenchIndex = nIndex;
			nIndex++;
		}
	}
}

// CTrenchFrame::SaveRPGStats up to tree.Add.
bool FillRPGStats( SEntrenchmentRPGStats &rpgStats, const CTreeItem &rootItem, const SExportContext &context, SExportOutcome &outcome )
{
	const CTreeItem *pCommonProps = RequireChild( rootItem, ETIT_TRENCH_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	if ( pCommonProps == nullptr )
		return false;
	rpgStats.szKeyName = ValueStr( *pCommonProps, 0 );
	rpgStats.fMaxHP = float( ValueInt( *pCommonProps, 1 ) );

	const CTreeItem *pDefencesItem = RequireChild( rootItem, ETIT_TRENCH_DEFENCES_ITEM, 0, "Defences", outcome );
	if ( pDefencesItem == nullptr )
		return false;
	for ( int i = 0; i < 6; i++ )
	{
		const CTreeItem *pDefProps = RequireChild( *pDefencesItem, ETIT_TRENCH_DEFENCE_PROPS_ITEM, i, "defence", outcome );
		if ( pDefProps == nullptr )
			return false;
		int nIndex = 0;
		const std::string &szName = pDefProps->GetDisplayName();
		if ( szName == "Left" )
			nIndex = RPG_LEFT;
		else if ( szName == "Right" )
			nIndex = RPG_RIGHT;
		else if ( szName == "Top" )
			nIndex = RPG_TOP;
		else if ( szName == "Bottom" )
			nIndex = RPG_BOTTOM;
		else if ( szName == "Front" )
			nIndex = RPG_FRONT;
		else if ( szName == "Back" )
			nIndex = RPG_BACK;

		rpgStats.defences[ nIndex ].nArmorMin = ValueInt( *pDefProps, 0 );
		rpgStats.defences[ nIndex ].nArmorMax = ValueInt( *pDefProps, 1 );
		rpgStats.defences[ nIndex ].fSilhouette = ValueFloat( *pCommonProps, 4 );
	}

	std::list<SMySegment> segmentsToSort;

	for ( int nTrenchIndex = 0; nTrenchIndex < 4; nTrenchIndex++ )
	{
		const CTreeItem *pTrenchParts = RequireChild( rootItem, ETIT_TRENCH_SOURCES_ITEM, nTrenchIndex, "trench sources", outcome );
		if ( pTrenchParts == nullptr )
			return false;
		for ( const auto &pPart : pTrenchParts->GetChildren() )
		{
			const auto *pTrenchProps = dynamic_cast<const CTrenchSourcePropsItem *>( pPart.get() );
			if ( pTrenchProps == nullptr )
				continue;
			SMySegment my;
			my.nIndex = pTrenchProps->nTrenchIndex;

			const std::string szRel = ValueStr( *pTrenchProps, 0 );
			const std::string szFullName = SourcePath( context, szRel );
			std::error_code ec;
			if ( !std::filesystem::is_regular_file( szFullName, ec ) )
			{
				// MFC's message box; the segment is left out.
				outcome.warnings.push_back( "Error while saving project: Cannot copy file " + szFullName );
				continue;
			}

			{
				std::string szTemp = szRel;
				const std::string::size_type nPos = szTemp.find_last_of( "\\/" );
				if ( nPos != std::string::npos )
					szTemp = szTemp.substr( nPos + 1 );
				szTemp = szTemp.substr( 0, szTemp.rfind( '.' ) );
				my.segment.szModel = szTemp;
			}

			{
				SAABBFormat aabb;
				const std::filesystem::path model( szFullName );
				std::string szDir = model.parent_path().string();
				if ( szDir.empty() || szDir.back() != '/' )
					szDir += '/';
				CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
				CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( model.filename().string().c_str(), STREAM_ACCESS_READ ) : 0;
				if ( pStream == 0 )
				{
					outcome.szError = "Error saving RPG : Cannot open file " + szFullName;
					return false;
				}
				CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
				CSaverAccessor saver = pSaver;
				saver.Add( 4, &aabb );

				my.segment.vAABBCenter.x = aabb.vCenter.x;
				my.segment.vAABBCenter.y = aabb.vCenter.y;
				my.segment.vAABBHalfSize.x = aabb.vHalfSize.x;
				my.segment.vAABBHalfSize.y = aabb.vHalfSize.y;
				my.segment.vAABBHalfSize.z = aabb.vHalfSize.z;
			}

			{
				if ( !context.meshFirePlaces )
				{
					outcome.szError = "the fire places of " + szFullName + " need the engine's mesh builder, which this export has not got";
					return false;
				}
				std::vector<std::pair<float, float>> firePlaces;
				std::string szError;
				if ( !context.meshFirePlaces( szFullName, firePlaces, szError ) )
				{
					outcome.warnings.push_back( "Error saving RPG : Cannot create model " + szFullName + ( szError.empty() ? std::string() : ": " + szError ) );
					continue;
				}
				for ( const auto &firePlace : firePlaces )
					my.segment.fireplaces.push_back( CVec2( firePlace.first, firePlace.second ) );
			}

			SEntrenchmentRPGStats::EEntrenchSegmType nType = SEntrenchmentRPGStats::EST_FIREPLACE;
			if ( nTrenchIndex == 0 )
			{
				nType = SEntrenchmentRPGStats::EST_FIREPLACE;
				rpgStats.fireplaces.push_back( my.nIndex );
			}
			else if ( nTrenchIndex == 1 )
			{
				nType = SEntrenchmentRPGStats::EST_LINE;
				rpgStats.lines.push_back( my.nIndex );
			}
			else if ( nTrenchIndex == 2 )
			{
				nType = SEntrenchmentRPGStats::EST_TERMINATOR;
				rpgStats.terminators.push_back( my.nIndex );
			}
			else
			{
				nType = SEntrenchmentRPGStats::EST_ARC;
				rpgStats.arcs.push_back( my.nIndex );
			}

			my.segment.fCoverage = ValueFloat( *pTrenchProps, 1 );
			my.segment.eType = nType;
			segmentsToSort.push_back( my );
		}
	}

	segmentsToSort.sort();
	int nPrev = -1;
	for ( std::list<SMySegment>::iterator it = segmentsToSort.begin(); it != segmentsToSort.end(); ++it )
	{
		if ( it->nIndex != nPrev + 1 )
		{
			for ( int i = nPrev + 1; i < it->nIndex; i++ )
			{
				SEntrenchmentRPGStats::SSegmentRPGStats segment;
				segment.fCoverage = 0.0f;
				segment.vFirePlace = VNULL2;
				segment.vAABBCenter = VNULL2;
				segment.vAABBHalfSize = VNULL3;
				segment.eType = SEntrenchmentRPGStats::EST_LINE;
				rpgStats.segments.push_back( segment );
			}
		}
		rpgStats.segments.push_back( it->segment );
		nPrev = it->nIndex;
	}
	return true;
}

// A failure of one picture or copy is a line of MFC's output pane, not the end
// of the export: the stats are written and the rest still tried.
void Warn( SExportOutcome &outcome )
{
	if ( !outcome.szError.empty() )
		outcome.warnings.push_back( outcome.szError );
	outcome.szError.clear();
}

// CTrenchFrame::ExportFrameData after SaveRPGStats.
void ExportGraphics( const CTreeItem &rootItem, const std::string &szStatsFile, const SExportContext &context, SExportOutcome &outcome )
{
	const std::string szResultDir = DirectoryOf( szStatsFile );
	const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );
	int nCount = 0;
	for ( int nTrenchIndex = 0; nTrenchIndex < 4; nTrenchIndex++ )
	{
		const CTreeItem *pTrenchParts = ChildItem( rootItem, ETIT_TRENCH_SOURCES_ITEM, nTrenchIndex );
		if ( pTrenchParts == nullptr )
			continue;
		for ( const auto &pPart : pTrenchParts->GetChildren() )
		{
			const auto *pTrenchProps = dynamic_cast<const CTrenchSourcePropsItem *>( pPart.get() );
			if ( pTrenchProps == nullptr )
				continue;
			const std::string szRel = ValueStr( *pTrenchProps, 0 );
			const std::string szFullName = SourcePath( context, szRel );
			const std::string::size_type nPos = szRel.find_last_of( "\\/" );
			const std::string szShort = nPos != std::string::npos ? szRel.substr( nPos + 1 ) : szRel;
			// The missing model was reported when the stats were filled.
			std::error_code ec;
			if ( std::filesystem::is_regular_file( szFullName, ec ) )
			{
				NImageExport::CopyFileInto( context, szFullName, szResultDir + szShort, outcome );
				Warn( outcome );
			}
			if ( nCount == 0 )
			{
				const std::string szSourceDir = std::filesystem::path( szFullName ).parent_path().string() + "/";
				for ( const char *pszName : { "1", "1w", "1a" } )
				{
					NImageExport::ConvertAndSaveImage( context, szSourceDir + pszName + ".tga", szResultDir + pszName, gamma, outcome );
					Warn( outcome );
				}
			}
			nCount++;
		}
	}
}

}

bool ExportTrench( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_TRENCH_ROOT_ITEM, "trench", outcome );
	if ( !pProject )
		return false;
	AssignTrenchIndices( *pProject->root );
	SEntrenchmentRPGStats rpgStats;
	if ( !FillRPGStats( rpgStats, *pProject->root, context, outcome ) )
		return false;
	const std::string szFile = StatsFileName( project, context, "units\\technics\\common\\entrenchment\\", false );
	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "RPG", &rpgStats );
	}, outcome ) )
		return false;
	if ( context.bStatsOnly )
		return true;
	// The sprite the game builds for the trench, "1" beside the stats.
	outcome.szObjectName = DirectoryOf( szFile ) + "1";
	ExportGraphics( *pProject->root, szFile, context, outcome );
	return true;
}

}
