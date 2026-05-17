#include "stdafx.h"

#include "RMG_Types.h"
#include "..\Formats\FmtTerrain.h"
#include "Polygons_Types.h"
#include "Resource_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMLevelVSOParameter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &szProfileFileName );
	saver.Add( 2, &fHeight );
	saver.Add( 3, &bAdd );
	saver.Add( 4, &nMiddlePointsCount );
	saver.Add( 5, &bLevelEnds );
	saver.Add( 6, &bLevelPatches );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMLevelVSOParameter::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "ProfileFileName", &szProfileFileName );
	saver.Add( "Height", &fHeight );
	saver.Add( "ToUpper", &bAdd );
	saver.Add( "MiddlePointsCount", &nMiddlePointsCount );
	saver.Add( "LevelEnds", &bLevelEnds );
	saver.Add( "LevelPatches", &bLevelPatches );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMPatch::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &size );	
	saver.Add( 2, &szFileName );
	saver.Add( 3, &szPlace );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMPatch::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Size", &size );	
	saver.Add( "FileName", &szFileName );	
	saver.Add( "Place", &szPlace );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMContainer::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		nSeason = 0;
	}

	saver.Add( 1, &patches );
	saver.Add( 2, &( indices[0] ) );
	saver.Add( 3, &( indices[1] ) );
	saver.Add( 4, &( indices[2] ) );
	saver.Add( 5, &( indices[3] ) );
	saver.Add( 6, &size );
	saver.Add( 7, &nSeason );
	saver.Add( 8, &szSeasonFolder );
	saver.Add( 9, &usedScriptIDs );
	saver.Add( 10, &usedScriptAreas );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMContainer::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	if ( saver.IsReading() )
	{
		nSeason = 0;
	}

	saver.Add( "Patches", &patches );
	saver.Add( "North_0", &( indices[0] ) );
	saver.Add( "East_90", &( indices[1] ) );
	saver.Add( "South_180", &( indices[2] ) );
	saver.Add( "West_270", &( indices[3] ) );
	saver.Add( "Size", &size );
	saver.Add( "Season", &nSeason );
	saver.Add( "SeasonFolder", &szSeasonFolder );
	saver.Add( "UsedScriptIDs", &usedScriptIDs );
	saver.Add( "UsedScriptAreas", &usedScriptAreas );
	return 0;
}

int SRMContainer::GetIndices( int nDirection, const std::string &rszPlace, std::vector<int> *pIndices ) const
{
	int nIndicesCount = 0;
	if ( pIndices && ( nDirection >= 0 ) && ( nDirection < 4 ) )
	{
		std::string szToCompare0 = rszPlace;
		NStr::ToLower( szToCompare0 );
		for ( int nIndex = 0; nIndex < indices[nDirection].size(); ++nIndex )
		{
			std::string szToCompare1 = patches[indices[nDirection][nIndex] ].szPlace;
			NStr::ToLower( szToCompare1 );
			if ( szToCompare0.empty() || szToCompare1.empty() || ( szToCompare0 == szToCompare1 ) )
			{
				++nIndicesCount;
				pIndices->push_back( indices[nDirection][nIndex] );
			}
		}
	}
	return nIndicesCount;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMContainer::GetSupportedSettings( std::list<std::string> *pSupportedSettingsList ) const
{
	NI_ASSERT_TF( pSupportedSettingsList != 0,
								NStr::Format(	"SRMContainer::GetSupportedSettings() Invalid Parameter pSupportedSettingsList: %x", pSupportedSettingsList ),
								return 0 );

	std::unordered_map<std::string, std::vector<int> > settings;
	
	std::string szAnySettingName( RMGC_ANY_SETTING_NAME );
	NStr::ToLower( szAnySettingName );
	settings[szAnySettingName].push_back( 0 );
	settings[szAnySettingName].push_back( 0 );
	settings[szAnySettingName].push_back( 0 );
	settings[szAnySettingName].push_back( 0 );
	
	for ( int nDirectionIndex = 0; nDirectionIndex < 4; ++nDirectionIndex )
	{
		for ( int nIndex = 0; nIndex < indices[nDirectionIndex].size(); ++nIndex )
		{
			std::string szSettingName = patches[indices[nDirectionIndex][nIndex] ].szPlace;
			NStr::ToLower( szSettingName );
			
			if ( szSettingName.empty() )
			{
				szSettingName = szAnySettingName;
			}
			
			if ( settings.find( szSettingName ) == settings.end() )
			{
				settings[szSettingName].push_back( 0 );
				settings[szSettingName].push_back( 0 );
				settings[szSettingName].push_back( 0 );
				settings[szSettingName].push_back( 0 );
			}
			
			std::vector<int> &rCounts = settings[szSettingName];
			rCounts[nDirectionIndex] += 1;
		}
	}
	
	const std::vector<int> &rAnySettingCounts = settings[szAnySettingName];
	if ( ( rAnySettingCounts[ANGLE_0] > 0 ) &&
			 ( rAnySettingCounts[ANGLE_90] > 0 ) &&
			 ( rAnySettingCounts[ANGLE_180] > 0 ) &&
			 ( rAnySettingCounts[ANGLE_270] > 0 ) )
	{
		pSupportedSettingsList->push_back( szAnySettingName );
		return 1;
	}

	int nSupportedSettings = 0;
	for ( std::unordered_map<std::string, std::vector<int> >::const_iterator settingIterator = settings.begin(); settingIterator != settings.end(); ++ settingIterator )
	{
		const std::vector<int> &rCounts = settingIterator->second;
		if ( ( ( rCounts[ANGLE_0] > 0 ) || ( rAnySettingCounts[ANGLE_0] > 0 ) ) &&
				 ( ( rCounts[ANGLE_90] > 0 ) || ( rAnySettingCounts[ANGLE_90] > 0 ) ) &&
				 ( ( rCounts[ANGLE_180] > 0 ) || ( rAnySettingCounts[ANGLE_180] > 0 ) ) &&
				 ( ( rCounts[ANGLE_270] > 0 ) || ( rAnySettingCounts[ANGLE_270] > 0 ) ) )
		{
			pSupportedSettingsList->push_back( settingIterator->first );
			++nSupportedSettings;
		}
	}
	return nSupportedSettings;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SRMContainer::IsSupportedSetting( const std::string &rszSettingName ) const
{
	std::vector<int> availiableIndices;
	return ( ( GetIndices( ANGLE_0, rszSettingName, &availiableIndices ) > 0 ) &&
					 ( GetIndices( ANGLE_90, rszSettingName, &availiableIndices ) > 0 ) &&
					 ( GetIndices( ANGLE_180, rszSettingName, &availiableIndices ) > 0 ) &&
					 ( GetIndices( ANGLE_270, rszSettingName, &availiableIndices ) > 0 ) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMGraphNode::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &rect );	
	saver.Add( 2, &szContainerFileName );	

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMGraphNode::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Rect", &rect );	
	saver.Add( "Container", &szContainerFileName );	

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMGraphLink::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &link );
	saver.Add( 2, &nType );
	saver.Add( 3, &szDescFileName );
	saver.Add( 4, &fRadius );
	saver.Add( 5, &nParts );
	saver.Add( 6, &fMinLength );
	saver.Add( 7, &fDistance );
	saver.Add( 8, &fDisturbance );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMGraphLink::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Link", &link );
	saver.Add( "Type", &nType );
	saver.Add( "Desc", &szDescFileName );
	saver.Add( "Radius", &fRadius );
	saver.Add( "Parts", &nParts );
	saver.Add( "MinLength", &fMinLength );
	saver.Add( "Distance", &fDistance );
	saver.Add( "Disturbance", &fDisturbance );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMGraph::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		nSeason = 0;
	}

	saver.Add( 1, &nodes );	
	saver.Add( 2, &links );	
	saver.Add( 3, &size );
	saver.Add( 4, &nSeason );
	saver.Add( 5, &szSeasonFolder );
	saver.Add( 6, &usedScriptIDs );
	saver.Add( 7, &usedScriptAreas );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMGraph::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	if ( saver.IsReading() )
	{
		nSeason = 0;
	}

	saver.Add( "Nodes", &nodes );	
	saver.Add( "Links", &links );	
	saver.Add( "Size", &size );
	saver.Add( "Season", &nSeason );
	saver.Add( "SeasonFolder", &szSeasonFolder );
	saver.Add( "UsedScriptIDs", &usedScriptIDs );
	saver.Add( "UsedScriptAreas", &usedScriptAreas );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMGraph::GetSupportedSettings( std::list<std::string> *pSupportedSettingsList ) const
{
	NI_ASSERT_TF( pSupportedSettingsList != 0,
								NStr::Format(	"SRMGraph::GetSupportedSettings() Invalid Parameter pSupportedSettingsList: %x", pSupportedSettingsList ),
								return 0 );

	std::unordered_map<std::string, int> settings;

	std::string szAnySettingName( RMGC_ANY_SETTING_NAME );
	NStr::ToLower( szAnySettingName );

	int nNodesCount = 0;
	int nAnyNodesCount = 0;
	for ( CRMGraphNodesList::const_iterator nodeIterator = nodes.begin(); nodeIterator != nodes.end(); ++nodeIterator )
	{
		SRMContainer container;
		if ( !LoadDataResource( nodeIterator->szContainerFileName, "", false ,0, RMGC_CONTAINER_XML_NAME, container ) )
		{
			return 0;
		}

		std::list<std::string> settingsList;
		if ( container.GetSupportedSettings( &settingsList ) > 0 )
		{
			bool bAnySetting = false;
			for ( std::list<std::string>::const_iterator settingItrator = settingsList.begin(); settingItrator != settingsList.end(); ++settingItrator )
			{
				if ( szAnySettingName == ( *settingItrator ) )
				{
					bAnySetting = true;
					break;
				}
			}
			if ( bAnySetting )
			{
				++nAnyNodesCount;
			}
			else
			{
				for ( std::list<std::string>::const_iterator settingItrator = settingsList.begin(); settingItrator != settingsList.end(); ++settingItrator )
				{
					if ( settings.find( *settingItrator ) == settings.end() )
					{
						settings[*settingItrator] = 0;
					}
					settings[*settingItrator] += 1;
				}
			}
		}
		++nNodesCount;
	}
	if ( nNodesCount == 0 )
	{
		return 0;
	}
	if ( nAnyNodesCount == nNodesCount )
	{
		pSupportedSettingsList->push_back( szAnySettingName );
		return 1;
	}
	int nSupportedSettings = 0;
	for ( std::unordered_map<std::string, int>::const_iterator settingIterator = settings.begin(); settingIterator != settings.end(); ++ settingIterator )
	{
		if ( settingIterator->second >= ( nNodesCount - nAnyNodesCount ) )
		{
			pSupportedSettingsList->push_back( settingIterator->first );
			++nSupportedSettings;
		}
	}
	return nSupportedSettings;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SRMGraph::IsSupportedSetting( const std::string &rszSettingName ) const
{
	int nNodesCount = 0;
	for ( CRMGraphNodesList::const_iterator nodeIterator = nodes.begin(); nodeIterator != nodes.end(); ++nodeIterator )
	{
		SRMContainer container;
		if ( !LoadDataResource( nodeIterator->szContainerFileName, "", false ,0, RMGC_CONTAINER_XML_NAME, container ) )
		{
			return false;
		}
		if ( !container.IsSupportedSetting( rszSettingName ) )
		{
			return false;
		}
		++nNodesCount;
	}
	return ( nNodesCount > 0 );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMObjectSetShell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &objects );
	saver.Add( 2, &fWidth );
	saver.Add( 3, &nBetweenDistance );
	saver.Add( 4, &fRatio );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMObjectSetShell::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Objects", &objects );
	saver.Add( "Width", &fWidth );
	saver.Add( "BetweenDistance", &nBetweenDistance );
	saver.Add( "Ratio", &fRatio );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTileSetShell::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &tiles );
	saver.Add( 2, &fWidth );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTileSetShell::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Tiles", &tiles );
	saver.Add( "Width", &fWidth );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMFieldSet::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &objectsShells );
	saver.Add( 2, &tilesShells );
	saver.Add( 3, &szProfileFileName );
	saver.Add( 4, &fHeight );
	saver.Add( 5, &patternSize );
	saver.Add( 6, &fPositiveRatio );
	saver.Add( 7, &nSeason );
	saver.Add( 8, &szSeasonFolder );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMFieldSet::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "ObjectsShells", &objectsShells );
	saver.Add( "TilesShells", &tilesShells );
	saver.Add( "ProfileFileName", &szProfileFileName );
	saver.Add( "Height", &fHeight );
	saver.Add( "PatternSize", &patternSize );
	saver.Add( "PositiveRatio", &fPositiveRatio );
	saver.Add( "Season", &nSeason );
	saver.Add( "SeasonFolder", &szSeasonFolder );
	
	return 0;
}


void SRMFieldSet::ValidateFieldSet( const STilesetDesc &rTilesetDesc, int nDefaultTileIndex )
{
	for ( int nTilesSellIndex = 0; nTilesSellIndex < tilesShells.size(); ++nTilesSellIndex )
	{
		for ( int nTileIndex = 0; nTileIndex < tilesShells[nTilesSellIndex].tiles.size(); ++nTileIndex )
		if ( ( tilesShells[nTilesSellIndex].tiles[nTileIndex] < 0 ) || 
				 ( tilesShells[nTilesSellIndex].tiles[nTileIndex] >= rTilesetDesc.terrtypes.size() ) )
		{
			if ( ( nDefaultTileIndex < 0 ) || 
					 ( nDefaultTileIndex >= rTilesetDesc.terrtypes.size() ) )
			{
				tilesShells[nTilesSellIndex].tiles.SetWeight( nTileIndex, 0 );
			}
			else
			{
				tilesShells[nTilesSellIndex].tiles[nTileIndex] = nDefaultTileIndex;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMVSODesc::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &szVSODescFileName );
	saver.Add( 2, &fWidth );
	saver.Add( 3, &fOpacity );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMVSODesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Desc", &szVSODescFileName );
	saver.Add( "Width", &fWidth );
	saver.Add( "Opacity", &fOpacity );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SRMTemplate::FillDefaultDiplomacies()
{
	if ( diplomacies.empty() )
	{
		diplomacies.resize( 3 );
		diplomacies[0] = 0;
		diplomacies[1] = 1;
		diplomacies[2] = 2;

		unitCreation.units.resize( 2 );
		unitCreation.Validate();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTemplate::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( saver.IsReading() )
	{
		nSeason = 0;
	}

	saver.Add( 1, &fields );
	saver.Add( 2, &graphs );
	saver.Add( 3, &vso );
	saver.Add( 4, &size );
	saver.Add( 5, &nSeason );
	saver.Add( 6, &szSeasonFolder );
	saver.Add( 7, &usedScriptIDs );
	saver.Add( 8, &usedScriptAreas );
	saver.Add( 9, &nDefaultFieldIndex );
	saver.Add( 10, &szScriptFile );
	saver.Add( 11, &vCameraAnchor );
	saver.Add( 12, &diplomacies );
	saver.Add( 13, &unitCreation );
	saver.Add( 14, &szForestCircleSounds );
	saver.Add( 15, &szForestAmbientSounds );
	saver.Add( 16, &szChapterName );
	saver.Add( 17, &nMissionIndex );
	saver.Add( 18, &szPlace );
	saver.Add( 19, &nType );
	saver.Add( 20, &nAttackingSide );
	saver.Add( 22, &szMODName );
	saver.Add( 23, &szMODVersion );

	if ( saver.IsReading() )
	{
		FillDefaultDiplomacies();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTemplate::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	if ( saver.IsReading() )
	{
		nSeason = 0;
	}
	
	saver.Add( "Fields", &fields );
	saver.Add( "Graphs", &graphs );
	saver.Add( "VSO", &vso );
	saver.Add( "Size", &size );
	saver.Add( "Season", &nSeason );
	saver.Add( "SeasonFolder", &szSeasonFolder );
	saver.Add( "Place", &szPlace );
	saver.Add( "UsedScriptIDs", &usedScriptIDs );
	saver.Add( "UsedScriptAreas", &usedScriptAreas );
	saver.Add( "DefaultFieldIndex", &nDefaultFieldIndex );
	saver.Add( "Scripts", &szScriptFile );
	saver.Add( "CameraAnchor", &vCameraAnchor );
	saver.Add( "Diplomacies", &diplomacies );
	saver.Add( "UnitCreations", &unitCreation );
	saver.Add( "ForestCircleSounds", &szForestCircleSounds );
	saver.Add( "ForestAmbientSounds", &szForestAmbientSounds );
	saver.Add( "ChapterName", &szChapterName );
	saver.Add( "MissionIndex", &nMissionIndex );
	saver.Add( "GameType", &nType );
	saver.Add( "AttackingSide", &nAttackingSide );
	saver.Add( "MODName", &szMODName );
	saver.Add( "MODVersion", &szMODVersion );

	if ( saver.IsReading() )
	{
		FillDefaultDiplomacies();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTemplate::GetSupportedSettings( std::list<std::string> *pSupportedSettingsList ) const
{
	NI_ASSERT_TF( pSupportedSettingsList != 0,
								NStr::Format(	"SRMGraph::GetSupportedSettings() Invalid Parameter pSupportedSettingsList: %x", pSupportedSettingsList ),
								return 0 );

	std::unordered_map<std::string, int> settings;

	std::string szAnySettingName( RMGC_ANY_SETTING_NAME );
	NStr::ToLower( szAnySettingName );

	int nGraphsCount = 0;
	int nAnyGraphsCount = 0;
	for ( int nGraphIndex = 0; nGraphIndex <  graphs.size(); ++nGraphIndex )
	{
		if ( graphs.GetWeight( nGraphIndex ) > 0 )
		{
			SRMGraph graph;
			if ( !LoadDataResource( graphs[nGraphIndex], "", false, 1, RMGC_GRAPH_XML_NAME, graph ) )
			{
				return 0;
			}

			std::list<std::string> settingsList;
			if ( graph.GetSupportedSettings( &settingsList ) > 0 )
			{
				bool bAnySetting = false;
				for ( std::list<std::string>::const_iterator settingItrator = settingsList.begin(); settingItrator != settingsList.end(); ++settingItrator )
				{
					if ( szAnySettingName == ( *settingItrator ) )
					{
						bAnySetting = true;
						break;
					}
				}
				if ( bAnySetting )
				{
					++nAnyGraphsCount;
				}
				else
				{
					for ( std::list<std::string>::const_iterator settingItrator = settingsList.begin(); settingItrator != settingsList.end(); ++settingItrator )
					{
						if ( settings.find( *settingItrator ) == settings.end() )
						{
							settings[*settingItrator] = 0;
						}
						settings[*settingItrator] += 1;
					}
				}
			}
			++nGraphsCount;
		}
	}
	if ( nGraphsCount == 0 )
	{
		return 0;
	}
	if ( nAnyGraphsCount == nGraphsCount )
	{
		pSupportedSettingsList->push_back( szAnySettingName );
		return 1;
	}
	int nSupportedSettings = 0;
	for ( std::unordered_map<std::string, int>::const_iterator settingIterator = settings.begin(); settingIterator != settings.end(); ++ settingIterator )
	{
		if ( settingIterator->second >= ( nGraphsCount - nAnyGraphsCount ) )
		{
			pSupportedSettingsList->push_back( settingIterator->first );
			++nSupportedSettings;
		}
	}
	return nSupportedSettings;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SRMTemplate::IsSupportedSetting( const std::string &rszSettingName ) const
{
	int nGraphsCount = 0;
	for ( int nGraphIndex = 0; nGraphIndex < graphs.size(); ++nGraphIndex )
	{
		if ( graphs.GetWeight( nGraphIndex ) > 0 )
		{
			SRMGraph graph;
			if ( !LoadDataResource( graphs[nGraphIndex], "", false, 1, RMGC_GRAPH_XML_NAME, graph ) )
			{
				return false;
			}
			if ( !graph.IsSupportedSetting( rszSettingName ) )
			{
				return false;
			}
			++nGraphsCount;
		}
	}
	return ( nGraphsCount > 0 );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMFieldGraph::SPatch::FillBoundingPolygon( const CTRect<int> &rBoundingRect )
{
	boundingRect = rBoundingRect;
	boundingPolygon.resize( 4 );
	boundingPolygon[0] = CVec2( boundingRect.minx * fWorldCellSize, boundingRect.miny * fWorldCellSize );
	boundingPolygon[1] = CVec2( boundingRect.minx * fWorldCellSize, boundingRect.maxy * fWorldCellSize );
	boundingPolygon[2] = CVec2( boundingRect.maxx * fWorldCellSize, boundingRect.maxy * fWorldCellSize );
	boundingPolygon[3] = CVec2( boundingRect.maxx * fWorldCellSize, boundingRect.miny * fWorldCellSize );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMFieldGraph::AddPatch(  const CTRect<int> &rBoundingRect )
{
	patches.push_back( SPatch() );
	patches.back().FillBoundingPolygon( rBoundingRect );
	patches.back().linesIndices.resize( 4 );
	patches.back().marckedVertices.resize( 4 );
	patches.back().marckedVertices[0] = false;
	patches.back().marckedVertices[1] = false;
	patches.back().marckedVertices[2] = false;
	patches.back().marckedVertices[3] = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMFieldGraph::AddLine( const std::list<CVec2> &rPoints, int nID )
{
	lines.push_back( SLine() );
	lines.back().points = rPoints;
	lines.back().nID = nID;

	lines.back().begin.nPatchIndex = -1;
	lines.back().begin.nSideIndex = -1;
	lines.back().begin.nLineIndex = -1;
	
	lines.back().end.nPatchIndex = -1;
	lines.back().end.nSideIndex = -1;
	lines.back().end.nLineIndex = -1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRMFieldGraph::ConnectLineToPatch( int nPatchIndex, int nLineIndex, bool bBegin )
{
	SPatch &rPatch = patches[nPatchIndex];
	SLine &rLine = lines[nLineIndex];
	CVec2 vLinePoint = VNULL2;
	if ( bBegin )
	{
		rLine.begin.nPatchIndex = nPatchIndex;
		vLinePoint = rLine.points.front();
	}
	else
	{
		rLine.end.nPatchIndex = nPatchIndex;
		vLinePoint = rLine.points.back();
	}

	//���������� ������� � ������� ����� ����� ����� ��������
	//������� ������ �.� ������� ����� ���������� ������������ �����
	//
	// 1               2
	//  *-------------*
	//  |\     1     /|
	//  |  \       /  |
	//  |   vCenter   |
	//  |0     *      |2
	//  |    /   \    |
	//  |  /       \  |
	//  |/     3     \|
	//  *-------------*
	// 0               3
	// 
	int nSideIndex = 0;
	const CVec2 vLocalPoint = vLinePoint - ( ( rPatch.boundingPolygon[0] + rPatch.boundingPolygon[2] ) / 2.0f );
	if ( vLocalPoint.x > vLocalPoint.y )
	{
		//2 ��� 3
		if ( ( vLocalPoint.x * ( -1 ) ) > vLocalPoint.y )
		{
			nSideIndex = 3;
		}
		else
		{
			nSideIndex = 2;
		}
	}
	else
	{
		//0 ��� 1
		if ( ( vLocalPoint.x * ( -1 ) ) < vLocalPoint.y )
		{
			nSideIndex = 1;
		}
		else
		{
			nSideIndex = 0;
		}
	}

	//���������� ������� ���� ���������� ��������� ������
	int nSideLineIndex = 0;
	for ( ; nSideLineIndex < rPatch.linesIndices[nSideIndex].size(); ++nSideLineIndex )
	{
		SLine &rCurrentLine = lines[rPatch.linesIndices[nSideIndex][nSideLineIndex]];
		CVec2 vCurrentLinePoint = VNULL2;
		if ( rCurrentLine.begin.nPatchIndex == nPatchIndex )
		{
			vCurrentLinePoint = rCurrentLine.points.front();
		}
		else
		{
			vCurrentLinePoint = rCurrentLine.points.back();
		}
		float fDistance = 0.0f;
		if ( ( nSideIndex & 0x1 ) > 0 )
		{
			fDistance = ( vCurrentLinePoint - vLinePoint ).x;
		}
		else
		{
			fDistance = ( vCurrentLinePoint - vLinePoint ).y;
		}
		if ( nSideIndex > 1 )
		{
			fDistance *= ( -1 );
		}
		if ( fDistance > 0.0f )
		{
			break;
		}
	}

	if ( nSideLineIndex >= rPatch.linesIndices[nSideIndex].size() )
	{
		rPatch.linesIndices[nSideIndex].push_back( nLineIndex );
	}
	else
	{
		rPatch.linesIndices[nSideIndex].insert( rPatch.linesIndices[nSideIndex].begin() + nSideLineIndex, nLineIndex );
	}
	
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CRMFieldGraph::IndexLines()
{
	int nElements = 0;
	for ( int nPatchIndex = 0; nPatchIndex < patches.size(); ++nPatchIndex )
	{
		for ( int nSideIndex = 0; nSideIndex < 4; ++nSideIndex )
		{
			//patches[nPatchIndex].marckedVertices[nSideIndex] = false;
			for ( int nLineIndex = 0; nLineIndex < patches[nPatchIndex].linesIndices[nSideIndex].size(); ++nLineIndex )
			{
				++nElements;
				SLine &rLine = lines[patches[nPatchIndex].linesIndices[nSideIndex][nLineIndex]];
				if ( rLine.begin.nPatchIndex == nPatchIndex )
				{
					//NStr::DebugTrace("patch %d, side %d, begin line %d\n", nPatchIndex, nSideIndex, patches[nPatchIndex].linesIndices[nSideIndex][nLineIndex] );
					rLine.begin.nSideIndex = nSideIndex;
					rLine.begin.nLineIndex = nLineIndex;
				}
				else if ( rLine.end.nPatchIndex == nPatchIndex )
				{
					//NStr::DebugTrace("patch %d, side %d, end line %d\n", nPatchIndex, nSideIndex, patches[nPatchIndex].linesIndices[nSideIndex][nLineIndex] );
					rLine.end.nSideIndex = nSideIndex;
					rLine.end.nLineIndex = nLineIndex;
				}
				else
				{
					NI_ASSERT_T( false,
											 NStr::Format( "CRMFieldGraph::IndexLines() invalid patch: %d, side: %d", nPatchIndex, nSideIndex ) );
				}
			}
		}
	}
	return ( ( nElements / 2 ) +  ( patches.size() * 8 ) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMFieldGraph::AddBorderLines( const CTRect<int> &rBoundingRect )
{
	//�������� ������ ������ �� ����� �����
	bool bNeedBoundingPolygon = true;
	std::vector<std::vector<int> > borderPatchesInidces;
	borderPatchesInidces.resize( 4 );
	for ( int nPatchIndex = 0; nPatchIndex < patches.size(); ++nPatchIndex )
	{
		//��������� ����� ����� ������������ ���������� ��������
		std::vector<bool> sides;
		sides.resize( 4 );
		sides[0] = ( patches[nPatchIndex].boundingRect.minx == rBoundingRect.minx );
		sides[1] = ( patches[nPatchIndex].boundingRect.maxy == rBoundingRect.maxy );
		sides[2] = ( patches[nPatchIndex].boundingRect.maxx == rBoundingRect.maxx );
		sides[3] = ( patches[nPatchIndex].boundingRect.miny == rBoundingRect.miny );
		for ( int nSideIndex = 0; nSideIndex < 4; ++nSideIndex )
		{
			if ( sides[nSideIndex] )
			{
				bNeedBoundingPolygon = false;
				int nSidePatchIndex = 0;
				for ( ; nSidePatchIndex < borderPatchesInidces[nSideIndex].size(); ++nSidePatchIndex )
				{
					SPatch &rPatch = patches[borderPatchesInidces[nSideIndex][nSidePatchIndex] ];
					
					int nDistance = 0;
					if ( ( nSideIndex & 0x1 ) > 0 )
					{
						nDistance = rPatch.boundingRect.maxx - patches[nPatchIndex].boundingRect.maxx;
					}
					else
					{
						nDistance = rPatch.boundingRect.maxy - patches[nPatchIndex].boundingRect.maxy;
					}
					if ( nSideIndex > 1 )
					{
						nDistance *= ( -1 );
					}
					if ( nDistance > 0 )
					{
						break;
					}
				}
				if ( nSidePatchIndex >= borderPatchesInidces[nSideIndex].size() )
				{
					borderPatchesInidces[nSideIndex].push_back( nPatchIndex );
				}
				else
				{
					borderPatchesInidces[nSideIndex].insert( borderPatchesInidces[nSideIndex].begin() + nSidePatchIndex, nPatchIndex );
				}
			}
		}
	}
	
	//������ ����� ����� ��� ������
	std::vector<CVec2> boundingPolygon;
	boundingPolygon.resize( 4 );
	boundingPolygon[0] = CVec2( rBoundingRect.minx * fWorldCellSize, rBoundingRect.miny * fWorldCellSize );
	boundingPolygon[1] = CVec2( rBoundingRect.minx * fWorldCellSize, rBoundingRect.maxy * fWorldCellSize );
	boundingPolygon[2] = CVec2( rBoundingRect.maxx * fWorldCellSize, rBoundingRect.maxy * fWorldCellSize );
	boundingPolygon[3] = CVec2( rBoundingRect.maxx * fWorldCellSize, rBoundingRect.miny * fWorldCellSize );

	//��������� ����� �����
	for ( int nSideIndex = 0; nSideIndex < 4; ++nSideIndex )
	{
		for ( int nSidePatchIndex = 0; nSidePatchIndex < borderPatchesInidces[nSideIndex].size(); ++nSidePatchIndex )
		{
			if ( ( nSidePatchIndex + 1 ) < borderPatchesInidces[nSideIndex].size() )
			{
				const int nLineIndex = lines.size();
				lines.push_back( SLine() );
				SLine &rLine = lines[nLineIndex];
				rLine.nID = -1;
				rLine.begin.nPatchIndex = borderPatchesInidces[nSideIndex][nSidePatchIndex];
				rLine.end.nPatchIndex = borderPatchesInidces[nSideIndex][nSidePatchIndex + 1];

				SPatch &rBeginPatch = patches[ rLine.begin.nPatchIndex ];
				SPatch &rEndPatch = patches[ rLine.end.nPatchIndex ];
				rBeginPatch.linesIndices[nSideIndex].push_back( nLineIndex );
				rEndPatch.linesIndices[nSideIndex].insert( rEndPatch.linesIndices[nSideIndex].begin(), nLineIndex );
			}
			else
			{
				int nNextSideIndex = GetNextVetexIndex( nSideIndex );
				if ( !borderPatchesInidces[nNextSideIndex].empty() && ( borderPatchesInidces[nNextSideIndex][0] == borderPatchesInidces[nSideIndex][nSidePatchIndex] ) )
				{
					SPatch &rPatch = patches[ borderPatchesInidces[nSideIndex][nSidePatchIndex] ];
					rPatch.marckedVertices[nNextSideIndex] = true;
					//NStr::DebugTrace( "Patch %d, vertex %d, marked\n", borderPatchesInidces[nSideIndex][nSidePatchIndex], nNextSideIndex );
				}
				else
				{
					const int nLineIndex = lines.size();
					lines.push_back( SLine() );
					SLine &rLine = lines[nLineIndex];
					rLine.nID = -1;
					while ( true )
					{
						rLine.points.push_back( boundingPolygon[nNextSideIndex] );
						if ( !borderPatchesInidces[nNextSideIndex].empty() )
						{
							rLine.begin.nPatchIndex = borderPatchesInidces[nSideIndex][nSidePatchIndex];
							rLine.end.nPatchIndex = borderPatchesInidces[nNextSideIndex][0];

							SPatch &rBeginPatch = patches[ rLine.begin.nPatchIndex ];
							SPatch &rEndPatch = patches[ rLine.end.nPatchIndex ];
							rBeginPatch.linesIndices[nSideIndex].push_back( nLineIndex );
							rEndPatch.linesIndices[nNextSideIndex].insert( rEndPatch.linesIndices[nNextSideIndex].begin(), nLineIndex );
							
							break;
						}
						nNextSideIndex = GetNextVetexIndex( nNextSideIndex );
					}
				}
			}
		}	
	}
	
	if ( bNeedBoundingPolygon )
	{
		inclusivePolygons.push_back( std::list<CVec2>() );
		for ( std::vector<CVec2>::const_iterator vertexIterator = boundingPolygon.begin(); vertexIterator != boundingPolygon.end(); ++vertexIterator )
		{
			inclusivePolygons.back().push_back( *vertexIterator );
		}
		//NStr::DebugTrace( "Inclusive boundingPolygon\n" );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CRMFieldGraph::UnmarkAllVertices()
{
	for ( int nPatchIndex = 0; nPatchIndex < patches.size(); ++nPatchIndex )
	{
		for ( int nVertexIndex = 0; nVertexIndex < 4; ++nVertexIndex )	
		{
			patches[nPatchIndex].marckedVertices[nVertexIndex] = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRMFieldGraph::FindUnmarkedVertex( int *pPatchIndex, int *pVertexIndex )
{
	for ( int nPatchIndex = 0; nPatchIndex < patches.size(); ++nPatchIndex )
	{
		for ( int nVertexIndex = 0; nVertexIndex < 4; ++nVertexIndex )	
		{
			if ( !patches[nPatchIndex].marckedVertices[nVertexIndex] )
			{
				( *pPatchIndex ) = nPatchIndex;
				( *pVertexIndex ) = nVertexIndex;
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRMFieldGraph::GetPolygonAndMarkVertices( int nBeginPatchIndex, int nBeginVertexIndex, std::list<CVec2> *pPolygon, int nMaximumIterations )
{
	//������ �������
	SCaret currentCaret;
	currentCaret.nPatchIndex = nBeginPatchIndex;
	currentCaret.nSideIndex = nBeginVertexIndex;
	currentCaret.nLineIndex = -1;
	int nIterations = 0;
	do
	{
		if ( currentCaret.nLineIndex < 0 )
		{
			pPolygon->push_back( patches[currentCaret.nPatchIndex].boundingPolygon[currentCaret.nSideIndex] );
			
			patches[currentCaret.nPatchIndex].marckedVertices[currentCaret.nSideIndex] = true;
			currentCaret.nLineIndex = 0;
		}
		else if ( currentCaret.nLineIndex >= patches[currentCaret.nPatchIndex].linesIndices[currentCaret.nSideIndex].size() )
		{
			currentCaret.nSideIndex = GetNextVetexIndex( currentCaret.nSideIndex );
			currentCaret.nLineIndex = -1;
		}
		else
		{
			SLine &rLine = lines[patches[currentCaret.nPatchIndex].linesIndices[currentCaret.nSideIndex][currentCaret.nLineIndex] ];
			if ( rLine.begin.nPatchIndex == currentCaret.nPatchIndex )
			{
				for ( std::list<CVec2>::const_iterator pointIterator = rLine.points.begin(); pointIterator != rLine.points.end(); ++pointIterator )
				{
					pPolygon->push_back( *pointIterator );
				}
				currentCaret.nPatchIndex = rLine.end.nPatchIndex;
				currentCaret.nSideIndex = rLine.end.nSideIndex;
				currentCaret.nLineIndex = rLine.end.nLineIndex + 1;
			}
			else
			{
				for ( std::list<CVec2>::reverse_iterator pointIterator = rLine.points.rbegin(); pointIterator != rLine.points.rend(); ++pointIterator )
				{
					pPolygon->push_back( *pointIterator );
				}
				currentCaret.nPatchIndex = rLine.begin.nPatchIndex;
				currentCaret.nSideIndex = rLine.begin.nSideIndex;
				currentCaret.nLineIndex = rLine.begin.nLineIndex + 1;
			}
		}
		++nIterations;
		if ( nIterations > nMaximumIterations )
		{
			return false;
		}
	}
	while( ( currentCaret.nPatchIndex != nBeginPatchIndex ) ||
				 ( currentCaret.nSideIndex != nBeginVertexIndex ) ||
				 ( currentCaret.nLineIndex != ( -1 ) ) );
	
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CRMFieldGraph::FindPolygons( const CTRect<int> &rBoundingRect )
{
	//�������� ��������
	inclusivePolygons.clear();
	exclusivePolygons.clear();
	
	UnmarkAllVertices();

	//������� ������� ����� �� ���� �����
	//���� �� �������� �� ������ �����
	//�� � �������� �������� ������� ���� ����� inclusivePolygon
	AddBorderLines( rBoundingRect );
	
	//���������� � ����������� �����
	int nMaximumIterations = IndexLines();
	
	int nPatchIndex = 0;
	int nVertexIndex =0;
	//���� �� ���������� ���� �����
	while( FindUnmarkedVertex( &nPatchIndex, &nVertexIndex ) )
	{
		//�������� ������� � ����� ���� ������, ������� �� ����������
		std::list<CVec2> newPolygon;
		if ( !GetPolygonAndMarkVertices( nPatchIndex, nVertexIndex, &newPolygon, nMaximumIterations ) )
		{
			return false;
		}
		UniquePolygon<std::list<CVec2>, CVec2>( &newPolygon, RMGC_MINIMAL_VIS_POINT_DISTANCE );
		
		//���������� ����������� ����� �����
		const CVec2 vCenterPoint = ( ( patches[nPatchIndex].boundingPolygon[0] + patches[nPatchIndex].boundingPolygon[2] ) / 2.0f );
		EClassifyPolygon classifyPolygon = ClassifyPolygon( newPolygon, vCenterPoint );
		
		if ( classifyPolygon == CP_OUTSIDE )
		{
			inclusivePolygons.push_back( newPolygon );
			//NStr::DebugTrace( "Inclusive %d %d\n", nEmptyPatchIndex, nEmptyVertexIndex );
		}
		else
		{
			exclusivePolygons.push_back( newPolygon );
			//NStr::DebugTrace( "exclusive %d %d\n", nEmptyPatchIndex, nEmptyVertexIndex );
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTemplateUnitsTable::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &unitCreationInfo );
	saver.Add( 2, &unitPlaceHolders );
	saver.Add( 3, &bonuses );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMTemplateUnitsTable::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "UnitCreation", &unitCreationInfo );
	saver.Add( "RandomMissionBonus", &bonuses );
	std::vector<CRMUnitsPlaceHoldersMnemonicsHashMap> unitPlaceHoldersMnemonics;
	if ( saver.IsReading() )
	{
		saver.Add( "PlaceHolders", &unitPlaceHoldersMnemonics );
		unitPlaceHolders.clear();
		for ( int nPlayerIndex = 0; nPlayerIndex < unitPlaceHoldersMnemonics.size(); ++nPlayerIndex )
		{
			const CRMUnitsPlaceHoldersMnemonicsHashMap &rMnemonics = unitPlaceHoldersMnemonics[nPlayerIndex];
			unitPlaceHolders.push_back( CRMUnitsPlaceHoldersHashMap() );
			int nUnitsPlaceHoldersCount = 0;
			for ( CRMUnitsPlaceHoldersMnemonicsHashMap::const_iterator mnemonicIterator = rMnemonics.begin(); mnemonicIterator != rMnemonics.end(); ++mnemonicIterator )
			{
				int nUnitRPGType = GetUnitRPGType( mnemonicIterator->first );
				NI_ASSERT_T( ( nUnitRPGType != INVALID_UNIT_RPG_TYPE ) && ( !mnemonicIterator->second.empty() ),
										 NStr::Format( "SRMTemplateUnitsTable(): invalid mnemonic: %s or size: %d, player: %d",
																	 mnemonicIterator->first.c_str(),
																	 mnemonicIterator->second.size(),
																	 nPlayerIndex ) );
				unitPlaceHolders[nPlayerIndex][nUnitRPGType] = mnemonicIterator->second;
				++nUnitsPlaceHoldersCount;
			}
			NI_ASSERT_T( nUnitsPlaceHoldersCount == UNIT_RPG_TYPE_COUNT,
									 NStr::Format( "SRMTemplateUnitsTable(): invalid placeHolders size: %d, must be: %d, player: %d",
																 nUnitsPlaceHoldersCount,
																 UNIT_RPG_TYPE_COUNT,
																 nPlayerIndex ) );
		}
	}
	else
	{
		for ( int nPlayerIndex = 0; nPlayerIndex < unitPlaceHolders.size(); ++nPlayerIndex )
		{
			const CRMUnitsPlaceHoldersHashMap &rPlaceHolder = unitPlaceHolders[nPlayerIndex];
			unitPlaceHoldersMnemonics.push_back( CRMUnitsPlaceHoldersMnemonicsHashMap() );
			for ( CRMUnitsPlaceHoldersHashMap::const_iterator placeHolderIterator = rPlaceHolder.begin(); placeHolderIterator != rPlaceHolder.end(); ++placeHolderIterator )
			{
				std::string szMnemonic = GetUnitRPGMnemonic( placeHolderIterator->first );
				NI_ASSERT_T( !szMnemonic.empty(),
										 NStr::Format( "SRMTemplateUnitsTable(): invalid unit type: %d, player: %d",
																	 placeHolderIterator->first,
																	 nPlayerIndex ) );
				unitPlaceHoldersMnemonics[nPlayerIndex][szMnemonic] = placeHolderIterator->second;
			}
		}
		saver.Add( "PlaceHolders", &unitPlaceHoldersMnemonics );
	}
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMContext::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &levels );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMContext::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Levels", &levels );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SRMContext::IsValid( int nLevelsCount, int nPlayersCount )
{
	bool bValid = true;
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	if ( !pIDB )
	{
		return false;
	}

	//��������� ���������� �������� ���������
	if ( ( levels.size() <= 0 ) || ( levels.size() < nLevelsCount ) )
	{
		NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, invalid levels count: %d (%d). Press \"Continue\" for further information.",
																	levels.size(),
																	nLevelsCount ) );
		bValid = false;
	}
	
	for ( int nLevelIndex = 0; nLevelIndex < levels.size(); ++nLevelIndex )
	{
		//����� ������� ��� ��������������� ���������
		const SRMTemplateUnitsTable &rTemplateUnitsTable = levels[nLevelIndex];
	
		//��������� ���������� �������
		if ( ( rTemplateUnitsTable.unitCreationInfo.units.size() <= 0 ) || ( rTemplateUnitsTable.unitCreationInfo.units.size() < nPlayersCount ) )
		{
			NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, invalid players count in Unit Creation: %d (%d). Press \"Continue\" for further information.",
																		nLevelIndex,
																		rTemplateUnitsTable.unitCreationInfo.units.size(),
																		nPlayersCount ) );
			bValid = false;
		}
		if ( ( rTemplateUnitsTable.unitPlaceHolders.size() <= 0 ) || ( rTemplateUnitsTable.unitPlaceHolders.size() < nPlayersCount ) )
		{
			NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, invalid players count in Units Table: %d (%d). Press \"Continue\" for further information.",
																		nLevelIndex,
																		rTemplateUnitsTable.unitPlaceHolders.size(),
																		nPlayersCount ) );
			bValid = false;
		}
		
		for ( int nPlayerIndex = 0; nPlayerIndex < rTemplateUnitsTable.unitPlaceHolders.size(); ++nPlayerIndex )
		{
			//��������� UnitCreationInfo;
			//const SUnitCreation &rUnitCreation = rTemplateUnitsTable.unitCreationInfo.units[nPlayerIndex];
			
			//��������� ������� ������
			int nUnitsTableEntriesCount = 0;
			for ( CRMUnitsPlaceHoldersHashMap::const_iterator unitsIterator = rTemplateUnitsTable.unitPlaceHolders[nPlayerIndex].begin(); unitsIterator != rTemplateUnitsTable.unitPlaceHolders[nPlayerIndex].end(); ++unitsIterator )
			{
				if ( nUnitsTableEntriesCount >= SRMTemplateUnitsTable::UNIT_RPG_TYPE_COUNT )
				{
					NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, player: %d, to many unit table entries: %d (%d). Press \"Continue\" for further information.",
																				nLevelIndex,
																				nPlayerIndex,
																				nUnitsTableEntriesCount,
																				SRMTemplateUnitsTable::UNIT_RPG_TYPE_COUNT ), );
					bValid = false;
				}
				if ( unitsIterator->second.size() <= 0 )
				{
					NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, player: %d, table entry is empty: %s. Press \"Continue\" for further information.",
																				nLevelIndex,
																				nPlayerIndex,
																				SRMTemplateUnitsTable::GetUnitRPGMnemonic( unitsIterator->first ).c_str() ) );
					bValid = false;
				}
				
				for ( int nUnitIndex = 0; nUnitIndex < unitsIterator->second.size(); ++nUnitIndex )
				{
					CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( unitsIterator->second[nUnitIndex].c_str() );
					if ( !pDesc )
					{
						NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, player: %d, entry: %s, Can't find DB object description for \"%s\". Press \"Continue\" for further information.",
																					nLevelIndex,
																					nPlayerIndex,
																					SRMTemplateUnitsTable::GetUnitRPGMnemonic( unitsIterator->first ).c_str(),
																					unitsIterator->second[nUnitIndex].c_str() ) );
						bValid = false;
					}
					else
					{
						switch ( pDesc->eGameType )
						{
							case SGVOGT_UNIT:
							{
								if ( unitsIterator->first <= SRMTemplateUnitsTable::SQUAD_UNIT_RPG_TYPE ) 
								{
									NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, player: %d, entry: %s, Invalid conversion: Squad->Unit: \"%s\". Press \"Continue\" for further information.",
																								nLevelIndex,
																								nPlayerIndex,
																								SRMTemplateUnitsTable::GetUnitRPGMnemonic( unitsIterator->first ).c_str(),
																								unitsIterator->second[nUnitIndex].c_str() ) );
									bValid = false;
								}
								break;
							}
							case SGVOGT_SQUAD:
							{
								/**/
								if ( unitsIterator->first > SRMTemplateUnitsTable::SQUAD_UNIT_RPG_TYPE ) 
								{
									NStr::DebugTrace( "SRMSetting::IsValid, level: %d, player: %d, entry: %s, Cross-type conversion: Unit->Squad: \"%s\".\n",
																		 nLevelIndex,
																		 nPlayerIndex,
																		 SRMTemplateUnitsTable::GetUnitRPGMnemonic( unitsIterator->first ).c_str(),
																		 unitsIterator->second[nUnitIndex].c_str() );
								}
								/**/
								break;
							}
							default:
							{
								NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, player: %d, entry: %s, Invalid object type ( !unit, !squad ): \"%s\". Press \"Continue\" for further information.",
																							nLevelIndex,
																							nPlayerIndex,
																							SRMTemplateUnitsTable::GetUnitRPGMnemonic( unitsIterator->first ).c_str(),
																							unitsIterator->second[nUnitIndex].c_str() ) );
								bValid = false;
							}
						}				
					}
				}
				++nUnitsTableEntriesCount;
			}
			if ( nUnitsTableEntriesCount != SRMTemplateUnitsTable::UNIT_RPG_TYPE_COUNT )
			{
				NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, player: %d, not enougth unit table entries: %d (%d). Press \"Continue\" for further information.",
																			nLevelIndex,
																			nPlayerIndex,
																			nUnitsTableEntriesCount,
																			SRMTemplateUnitsTable::UNIT_RPG_TYPE_COUNT ) );
				bValid = false;
			}
		}
		
		//��������� ������:
		if ( rTemplateUnitsTable.bonuses.size() <= 0 )
		{
			NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, no bonuses: %d. Press \"Continue\" for further information.",
																		nLevelIndex,
																		rTemplateUnitsTable.bonuses.size() ) );
			bValid = false;
		}
		
		int nBonusesOverallWeight = 0;
		for ( int nBonusIndex = 0; nBonusIndex < rTemplateUnitsTable.bonuses.size(); ++nBonusIndex )
		{
			CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( rTemplateUnitsTable.bonuses[nBonusIndex].szRPGStats.c_str() );
			if ( !pDesc )
			{
				NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, Can't find bonus DB object description for \"%s\". Press \"Continue\" for further information.",
																			nLevelIndex,
																			rTemplateUnitsTable.bonuses[nBonusIndex].szRPGStats.c_str() ) );
				bValid = false;
			}
			if ( pDesc->eGameType != SGVOGT_UNIT )
			{
				NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, bonus is not a unit: \"%s\". Press \"Continue\" for further information.",
																			nLevelIndex,
																			rTemplateUnitsTable.bonuses[nBonusIndex].szRPGStats.c_str() ) );
				bValid = false;
			}
			nBonusesOverallWeight += rTemplateUnitsTable.bonuses[nBonusIndex].nWeight;
		}
		if ( nBonusesOverallWeight <= 0 )
		{
			NI_ASSERT_T( 0, NStr::Format( "SRMSetting::IsValid, level: %d, bonuses has invalid weight: %d. Press \"Continue\" for further information.",
																		nLevelIndex,
																		nBonusesOverallWeight ) );
			bValid = false;
		}
	}
	
	return bValid;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMSetting::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 2, &fields );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SRMSetting::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Fields", &fields );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string SRMTemplateUnitsTable::GetRandomBonus() const
{
	if ( bonuses.empty() ) 
	{
		return "";
	}
	
	CWeightVector<std::string> vw;
	for ( std::vector<SRandomMissionBonus>::const_iterator it = bonuses.begin(); it != bonuses.end(); ++it )
	{
		vw.push_back( it->szRPGStats, it->nWeight );
	}
	return vw.GetRandom( false );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
