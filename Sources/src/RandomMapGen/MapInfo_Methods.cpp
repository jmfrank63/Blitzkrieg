#include "stdafx.h"

#include "MapInfo_Types.h"
#include "VA_Types.h"
#include "..\formats\FmtTerrain.h"
#include "..\StreamIO\ProgressHook.h"
#include "Resource_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SQuickLoadMapInfo::FillFromMapInfo( const SLoadMapInfo &rLoadMapInfo )
{
	playerParties.resize( rLoadMapInfo.unitCreation.units.size() );
	for ( int nPlayerIndex = 0; nPlayerIndex < rLoadMapInfo.unitCreation.units.size(); ++nPlayerIndex )
	{
		playerParties[nPlayerIndex] = rLoadMapInfo.unitCreation.units[nPlayerIndex].szPartyName;
	}
	diplomacies = rLoadMapInfo.diplomacies;
	size.x = rLoadMapInfo.terrain.patches.GetSizeX();
	size.y = rLoadMapInfo.terrain.patches.GetSizeY();
	nType = rLoadMapInfo.nType;
	nAttackingSide = rLoadMapInfo.nAttackingSide;

	szMODName = rLoadMapInfo.szMODName;
	szMODVersion = rLoadMapInfo.szMODVersion;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SQuickLoadMapInfo::FillFromRMTemplate( const SRMTemplate &rRMTemplate )
{
	playerParties.resize( rRMTemplate.unitCreation.units.size() );
	for ( int nPlayerIndex = 0; nPlayerIndex < rRMTemplate.unitCreation.units.size(); ++nPlayerIndex )
	{
		playerParties[nPlayerIndex] = rRMTemplate.unitCreation.units[nPlayerIndex].szPartyName;
	}
	diplomacies = rRMTemplate.diplomacies;
	size = rRMTemplate.size;
	nType = rRMTemplate.nType;
	nAttackingSide = rRMTemplate.nAttackingSide;

	szMODName = rRMTemplate.szMODName;
	szMODVersion = rRMTemplate.szMODVersion;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SQuickLoadMapInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &playerParties );
	saver.Add( 2, &diplomacies );
	saver.Add( 3, &size );
	saver.Add( 4, &nType );
	saver.Add( 5, &nAttackingSide );
	saver.Add( 7, &szMODName );
	saver.Add( 8, &szMODVersion );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SQuickLoadMapInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Parties", &playerParties );
	saver.Add( "Diplomacies", &diplomacies );
	saver.Add( "Size", &size );
	saver.Add( "GameType", &nType );
	saver.Add( "AttackingSide", &nAttackingSide );
	saver.Add( "MODName", &szMODName );
	saver.Add( "MODVersion", &szMODVersion );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::FillDefaultDiplomacies()
{
	FillDefaultDiplomacies( this );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::Clear()
{
	Clear( this );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::Create( const CTPoint<int> &rSize, int _nSeason, const std::string &rszSeasonFolder, int nPlayersCount, int _nType )
{
	return Create( this, rSize, _nSeason, rszSeasonFolder, nPlayersCount, _nType );
}

bool CMapInfo::IsValid()
{
	return IsValid( *this );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMapInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &terrain );
	saver.Add( 2, &objects );
	saver.Add( 3, &entrenchments );
	saver.Add( 4, &bridges );
	saver.Add( 5, &reinforcements );
	saver.Add( 6, &szScriptFile );
	saver.Add( 7, &scriptAreas );
	saver.Add( 8, &vCameraAnchor );
	saver.Add( 9, &nSeason );
	saver.Add( 10, &szSeasonFolder );
	saver.Add( 11, &diplomacies );
	saver.Add( 12, &reservePositionsList );
	saver.Add( 13, &unitCreation );
	saver.Add( 14, &startCommandsList );
	saver.Add( 15, &szChapterName );
	saver.Add( 16, &nMissionIndex );
	saver.Add( 17, &sounds );
	saver.Add( 18, &szForestCircleSounds );
	saver.Add( 19, &szForestAmbientSounds );
	saver.Add( 20, &scenarioObjects );
	saver.Add( 21, &aiGeneralMapInfo );
	saver.Add( 22, &playersCameraAnchors );
	saver.Add( 23, &nType );
	saver.Add( 24, &nAttackingSide );
	saver.Add( 26, &szMODName );
	saver.Add( 27, &szMODVersion );

	if ( saver.IsReading() )
	{
		// CRAP{ ����� ��� ������� diplomacies
		FillDefaultDiplomacies();
		// CRAP}
		if ( szSeasonFolder.empty() )
		{	
			szSeasonFolder = SEASON_FOLDERS[nSeason];
		}
		// CRAP{ set passability for roads as 1
		for ( TVSOList::iterator it = terrain.roads3.begin(); it != terrain.roads3.end(); ++it )
		{
			if ( it->fPassability == 0 ) 
				it->fPassability = 1;
		}
		// CRAP}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMapInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	
	saver.Add( "Terrain", &terrain );
	saver.Add( "Objects", &objects );
	saver.Add( "ScenarioObjects", &scenarioObjects );
	saver.Add( "Entrenchments", &entrenchments );
	saver.Add( "Bridges", &bridges );
	saver.Add( "ReinforcementGroup", &reinforcements );
	saver.Add( "Scripts", &szScriptFile );
	saver.Add( "ScriptAreas", &scriptAreas );
	saver.Add( "CameraAnchor", &vCameraAnchor );
	saver.Add( "PlayerCameraAnchors", &playersCameraAnchors );
	saver.Add( "Season", &nSeason );
	saver.Add( "SeasonFolder", &szSeasonFolder );
	saver.Add( "Diplomacies", &diplomacies );
	saver.Add( "ReservePositions", &reservePositionsList );
	saver.Add( "UnitCreations", &unitCreation );
	saver.Add( "StartCommands", &startCommandsList );
	saver.Add( "ChapterName", &szChapterName );
	saver.Add( "MissionIndex", &nMissionIndex );
	saver.Add( "MapSounds", &sounds );
	saver.Add( "ForestCircleSounds", &szForestCircleSounds );
	saver.Add( "ForestAmbientSounds", &szForestAmbientSounds );
	saver.Add( "AIGeneral", &aiGeneralMapInfo );
	saver.Add( "GameType", &nType );
	saver.Add( "AttackingSide", &nAttackingSide );
	saver.Add( "MODName", &szMODName );
	saver.Add( "MODVersion", &szMODVersion );

	if ( saver.IsReading() )
	{
		// CRAP{ ����� ��� ������� diplomacies
		FillDefaultDiplomacies();
		// CRAP}
		if ( szSeasonFolder.empty() )
		{
			szSeasonFolder = SEASON_FOLDERS[nSeason];
		}
		// CRAP{ set passability for roads as 1
		for ( TVSOList::iterator it = terrain.roads3.begin(); it != terrain.roads3.end(); ++it )
		{
			if ( it->fPassability == 0 ) 
				it->fPassability = 1;
		}
		// CRAP}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMapInfo::GetSelectedSeason()
{
	return GetSelectedSeason( nSeason, szSeasonFolder );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::RemoveObject( int nObjectIndex )
{
	return RemoveObject( this, nObjectIndex );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::RemoveObjects( const std::list<CVec2> &rClearPolygon )
{
	return RemoveObjects( this, rClearPolygon );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrain( const CTRect<int> &rUpdateRect )
{
	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		STilesetDesc tilesetDesc;
		SCrossetDesc crossetDesc;
		LoadDataResource( terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc );
		LoadDataResource( terrain.szCrossetDesc, "", false, 0, "crosset", crossetDesc );
		return UpdateTerrain( &terrain, rUpdateRect, tilesetDesc, crossetDesc, /*roadsetDesc,*/ CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( nSeason ) ) );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrainCrosses( const CTRect<int> &rUpdateRect )
{
	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		STilesetDesc tilesetDesc;
		SCrossetDesc crossetDesc;
		LoadDataResource( terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc );
		LoadDataResource( terrain.szCrossetDesc, "", false, 0, "crosset", crossetDesc );
		return UpdateTerrainCrosses( &terrain, rUpdateRect, tilesetDesc, crossetDesc/*, roadsetDesc*/ );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrainRivers( const CTRect<int> &rUpdateRect )
{
	return UpdateTerrainRivers( &terrain, rUpdateRect );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrainRoads3D( const CTRect<int> &rUpdateRect )
{
	return UpdateTerrainRoads3D( &terrain, rUpdateRect );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrainShades( const CTRect<int> &rUpdateRect )
{
	return UpdateTerrainShades( &terrain, rUpdateRect, CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( nSeason ) ) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMapInfo::UpdateObjects( const CTRect<int> &rUpdateRect )
{
	return UpdateObjects( this, rUpdateRect );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::PackFrameIndices()
{
	PackFrameIndices( this );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::UnpackFrameIndices()
{
	UnpackFrameIndices( this );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::RemoveNonExistingObjects( IDataStorage *pDataStorage, IObjectsDB *pObjectsDB, std::string *pszOutputString )
{
	return RemoveNonExistingObjects( this, pDataStorage, pObjectsDB, pszOutputString );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::TerrainHitTest( const CVec3 &rPoint, TERRAIN_HIT_TEST_TYPE type, std::vector<int> *pTerrainObjects )
{
	return TerrainHitTest( terrain, rPoint, type, pTerrainObjects );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SVectorStripeObject* CMapInfo::GetRiver( int nID )
{
	return GetRiver( terrain, nID );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SVectorStripeObject* CMapInfo::GetRoad3D( int nID )
{
	return GetRoad3D( terrain, nID );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::CreateMiniMapImage( const CRMImageCreateParameterList &rImageCreateParameterList, IProgressHook *pProgressHook )
{
	return CreateMiniMapImage( *( this ), rImageCreateParameterList, pProgressHook );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::AddSounds( TMapSoundInfoList *pSoundsList, DWORD dwSoundTypeBits )
{
	return AddSounds( *( this ), pSoundsList, dwSoundTypeBits );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetUsedLinkIDs( CUsedLinkIDs *pUsedLinkIDs )
{
	return GetUsedLinkIDs( *( this ), pUsedLinkIDs );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetUsedScriptIDs( CUsedScriptIDs *pUsedScriptIDs )
{
	return GetUsedScriptIDs( *( this ), pUsedScriptIDs );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetUsedScriptAreas( CUsedScriptAreas *pUsedScriptAreas )
{
	return GetUsedScriptAreas( *( this ), pUsedScriptAreas );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetTerrainTileIndices( const CVec3 &rPoint, CTPoint<int> *pPoint )
{
	return GetTerrainTileIndices( terrain, rPoint, pPoint );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetTileIndices( const CVec3 &rPoint, CTPoint<int> *pPoint )
{
	return GetTileIndices( terrain, rPoint, pPoint );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetAITileIndices( const CVec3 &rPoint, CTPoint<int> *pPoint )
{
	return GetAITileIndices( terrain, rPoint, pPoint );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::InvertYTile( CTPoint<int> *pPoint )
{
	InvertYTile( terrain, pPoint );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::InvertYPosition( CTPoint<float> *pPoint )
{
	InvertYPosition( terrain, pPoint );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::AddMapInfo( const CTPoint<int> &destPoint, const SLoadMapInfo &rSourceLoadMapInfo )
{
	return AddMapInfo( this, destPoint, rSourceLoadMapInfo );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::FillTerrain( int nTileIndex )
{
	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		STilesetDesc tilesetDesc;
		LoadDataResource( terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc );
		return FillTerrain( &terrain, tilesetDesc, nTileIndex );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::FillTileSet( const std::list<CVec2> &rInclusivePolygon, const std::list<std::list<CVec2> > &rExclusivePolygons, const CRMTileSet &rTileSet, std::unordered_map<LPARAM, float> *pDistances )
{
	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		STilesetDesc tilesetDesc;
		LoadDataResource( terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc );
		return FillTileSet( &terrain, tilesetDesc, rInclusivePolygon, rExclusivePolygons, rTileSet, pDistances );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::FillObjectSet( const std::list<CVec2> &rInclusivePolygon, const std::list<std::list<CVec2> > &rExclusivePolygons, const CRMObjectSet &rObjectSet, CArray2D<BYTE> *pTileMap )
{
	return FillObjectSet( this, rInclusivePolygon, rExclusivePolygons, rObjectSet, pTileMap );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::FillProfilePattern( const std::list<CVec2> &rInclusivePolygon, const std::list<std::list<CVec2> > &rExclusivePolygons, const SVAGradient &rGradient, const CTPoint<int> &rPatternSize, float fPositiveRatio, std::unordered_map<LPARAM, float> *pDistances )
{
	return FillProfilePattern( &terrain, rInclusivePolygon, rExclusivePolygons, rGradient, rPatternSize, fPositiveRatio, pDistances );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//obsolete
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
bool CMapInfo::AddRoad( const CTRect<int> &rRoadRect, int nRoadType, int nRoadDirection, std::vector<SRoadItem> *pRoad )
{
	return AddRoad( this, rRoadRect, nRoadType, nRoadDirection, pRoad );
}
/**/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
bool CMapInfo::MakeRoad( const SRoadPoint &rFrom, const SRoadPoint &rTo, int nRoadType, const SRoadMakeParameter &rRoadMakeParamerer, std::vector<SRoadItem> *pRoad )
{
	return MakeRoad( this, rFrom, rTo, nRoadType, rRoadMakeParamerer, pRoad );
}
/**/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
bool CMapInfo::UpdateTerrainRoads( const CTRect<int> &rUpdateRect )
{
	//��������� ���������� � ������������ �������� ������
	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		SRoadsetDesc roadsetDesc;
		{
			CPtr<IDataStream> pStream = pDataStorage->OpenStream( ( terrain.szRoadsetDesc + std::string( ".xml" ) ).c_str(), STREAM_ACCESS_READ );
			CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
			tree.Add( "roadset", &roadsetDesc );
		}
		return UpdateTerrainRoads( &terrain, rUpdateRect, roadsetDesc );
	}
	return false;
}
/**/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// basement storage  
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		STilesetDesc tilesetDesc;
		{
			CPtr<IDataStream> pStream = pDataStorage->OpenStream( ( terrain.szTilesetDesc + std::string( ".xml" ) ).c_str(), STREAM_ACCESS_READ );
			CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
			tree.Add( "tileset", &tilesetDesc );
		}
		SCrossetDesc crossetDesc;
		{
			CPtr<IDataStream> pStream = pDataStorage->OpenStream( ( terrain.szCrossetDesc + std::string( ".xml" ) ).c_str(), STREAM_ACCESS_READ );
			CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
			tree.Add( "crosset", &crossetDesc );
		}
		SRoadsetDesc roadsetDesc;
		{
			CPtr<IDataStream> pStream = pDataStorage->OpenStream( ( terrain.szRoadsetDesc + std::string( ".xml" ) ).c_str(), STREAM_ACCESS_READ );
			CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
			tree.Add( "roadset", &roadsetDesc );
		}
		bool result = ApplyTemplate( this, rRMTemplate, tilesetDesc,
																 crossetDesc,
																 roadsetDesc,
																 pRMPlacedPatches );
		if ( isUpdateTerrain && result )
		{
			return UpdateTerrain();
		}
		return result;
	}
/**/
/**
	const int nDiplomaciesSize = 3;
	pLoadMapInfo->diplomacies.SetSizes( nDiplomaciesSize, nDiplomaciesSize );
	for ( int nXIndex = 0; nXIndex < ( nDiplomaciesSize - 1 ); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < ( nDiplomaciesSize - 1 ); ++nYIndex )
		{
			pLoadMapInfo->diplomacies[nYIndex][nXIndex] = EDI_ENEMY;
		}
	}
	for ( int index = 0; index < ( nDiplomaciesSize - 1 ); ++index )
	{
		pLoadMapInfo->diplomacies[index][nDiplomaciesSize - 1] = EDI_NEUTRAL;
		pLoadMapInfo->diplomacies[nDiplomaciesSize - 1][index] = EDI_NEUTRAL;
	}

	for ( int index = 0; index < nDiplomaciesSize; ++index )
	{
		pLoadMapInfo->diplomacies[index][index] = EDI_FRIEND;
	}
/**/

	/**
	// CRAP{ ����� ��� ������� diplomacies
	if ( saver.IsReading() )
	{
		if ( (  diplomacies.GetSizeX() == 0 ) || 
				 ( diplomacies.GetSizeY() == 0 ) )
		{
			const int nDiplomaciesSize = 3;
			diplomacies.SetSizes( nDiplomaciesSize, nDiplomaciesSize );
			for ( int nXIndex = 0; nXIndex < ( nDiplomaciesSize - 1 ); ++nXIndex )
			{
				for ( int nYIndex = 0; nYIndex < ( nDiplomaciesSize - 1 ); ++nYIndex )
				{
					diplomacies[nYIndex][nXIndex] = EDI_ENEMY;
				}
			}
			for ( int index = 0; index < ( nDiplomaciesSize - 1 ); ++index )
			{
				diplomacies[index][nDiplomaciesSize - 1] = EDI_NEUTRAL;
				diplomacies[nDiplomaciesSize - 1][index] = EDI_NEUTRAL;
			}

			for ( int index = 0; index < nDiplomaciesSize; ++index )
			{
				diplomacies[index][index] = EDI_FRIEND;
			}
		}
	}
	// CRAP}
	/**/

	/**
	// CRAP{ ����� ��� ������� diplomacies
	if ( saver.IsReading() )
	{
		if ( (  diplomacies.GetSizeX() == 0 ) || 
				 ( diplomacies.GetSizeY() == 0 ) )
		{
			const int nDiplomaciesSize = 3;
			diplomacies.SetSizes( nDiplomaciesSize, nDiplomaciesSize );
			for ( int nXIndex = 0; nXIndex < ( nDiplomaciesSize - 1 ); ++nXIndex )
			{
				for ( int nYIndex = 0; nYIndex < ( nDiplomaciesSize - 1 ); ++nYIndex )
				{
					diplomacies[nYIndex][nXIndex] = EDI_ENEMY;
				}
			}
			for ( int index = 0; index < ( nDiplomaciesSize - 1 ); ++index )
			{
				diplomacies[index][nDiplomaciesSize - 1] = EDI_NEUTRAL;
				diplomacies[nDiplomaciesSize - 1][index] = EDI_NEUTRAL;
			}

			for ( int index = 0; index < nDiplomaciesSize; ++index )
			{
				diplomacies[index][index] = EDI_FRIEND;
			}
		}
	}
	// CRAP}
	/**/
