#if !defined(__MapInfo__Types__)
#define __MapInfo__Types__

#include "..\Formats\FmtMap.h"
#include "..\GFX\GFXTypes.h"

#include "RMG_Types.h"
#include "RP_Types.h"
#include "MiniMap_Types.h"
#include "..\zlib\zlib.h"
#include "..\Misc\CheckSums.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern const int RMGC_INVALID_LINK_ID_VALUE;			//0
extern const int RMGC_INVALID_SCRIPT_ID_VALUE;		//-1
extern const int RMGC_DEFAULT_SCRIPT_ID_VALUE;		//0
extern const int RMGC_INVALID_FRAME_INDEX_VALUE;	//-1

//��������� �������� ������������ ��� ������ � xml �������
extern const char *RMGC_TEMPLATES_XML_NAME;
extern const char *RMGC_TEMPLATES_FILE_NAME;

extern const char *RMGC_MINIMAP_XML_NAME;
extern const char *RMGC_MINIMAP_FILE_NAME;

extern const char *RMGC_TILESET_FILE_NAME;
extern const char *RMGC_CROSSSET_FILE_NAME;
extern const char *RMGC_ROADSET_FILE_NAME;
extern const char *RMGC_NOISE_FILE_NAME;

extern const char *RMGC_QUICK_LOAD_MAP_INFO_NAME;
extern const int RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER;

//�������� ����� ����������� ����� �� ���������
extern const char *RMGC_DEFAULT_ANGLE_MASK;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//isUpdateTerain - ������ ��� �����������, ����� ������� ��������� ������� c isUpdateTerain = false, � ����� ������������
//isUpdateRoad - ����������
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SQuickLoadMapInfo
{
	std::vector<std::string> playerParties;												// ��� ������� ������ � ����� ���� ����� ���������� ������� � ������ ������
	std::vector<BYTE> diplomacies;																// ����������, 0, 1 - ���������� �������, 2 - ��������
	CTPoint<int> size;
	int nType;
	int nAttackingSide;

	std::string szMODName;
	std::string szMODVersion;

	SQuickLoadMapInfo() : nType( 0 ), nAttackingSide( 0 ) { }
	
	void FillFromMapInfo( const SLoadMapInfo &rLoadMapInfo );
	void FillFromRMTemplate( const SRMTemplate &rRMTemplate );

	// serializing...
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TRPGStats>
void TPackFrameIndex( IObjectsDB *pGDB, SMapObjectInfo *pInfo, const TRPGStats *pStats, const char *pszType )
{
	if ( pStats == 0 ) 
	{
		return;
	}
	const int nType = pStats->GetTypeFromIndex( pInfo->nFrameIndex );
	if ( nType == -1 ) 
	{
		NI_ASSERT_T( nType != -1, NStr::Format( "PackFrameIndex() Unknown type for %s \"%s\". Was frame index %d", pszType, pInfo->szName.c_str(), pInfo->nFrameIndex ) );
	}
	else
	{
		pInfo->nFrameIndex = nType;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TRPGStats>
void TUnpackFrameIndex( IObjectsDB *pGDB, SMapObjectInfo *pInfo, const TRPGStats *pStats, const char *pszType, int *pCurRandomSeed = 0 )
{
	if ( pStats == 0 ) 
		return;

	if ( pInfo->nFrameIndex == -1 ) 
	{
		NI_ASSERT_T( pInfo->nFrameIndex != -1, NStr::Format( "UnpackFrameIndex() Unknown type for %s \"%s\". Was frame index %d", pszType, pInfo->szName.c_str(), pInfo->nFrameIndex ) );
	}
	else
		pInfo->nFrameIndex = pStats->GetIndexFromType( pInfo->nFrameIndex, pCurRandomSeed );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMapInfo : public SLoadMapInfo
{
public:
	enum SEASON
	{
		SEASON_SUMMER	= 0,
		SEASON_WINTER	= 1,
		SEASON_AFRICA	= 2,
		SEASON_SPRING = 3,
		SEASON_COUNT	= 4,
	};
	static const int REAL_SEASONS_COUNT;

	enum TERRAIN_HIT_TEST_TYPE
	{
		THT_ROADS3D = 0,
		THT_RIVERS = 1,
		THT_COUNT = 2,
	};
	
	enum GAME_TYPE
	{
		TYPE_SINGLE_PLAYER	= 0,
		TYPE_FLAGCONTROL		= 1,
		TYPE_SABOTAGE				= 2,
		TYPE_COUNT					= 3,
		TYPE_NONE						= 4,
	};

	static const int REAL_SEASONS[SEASON_COUNT];
	static const int MOST_COMMON_TILES[SEASON_COUNT];
	static const char* SEASON_NAMES[SEASON_COUNT];
	static const char* SEASON_FOLDERS[SEASON_COUNT];

	static const DWORD SOUND_TYPE_BITS_RIVERS;
	static const DWORD SOUND_TYPE_BITS_BUILDINGS;
	static const DWORD SOUND_TYPE_BITS_FORESTS;
	static const DWORD SOUND_TYPE_BITS_ALL;

	static const char* TYPE_NAMES[TYPE_COUNT];

private:
	static const int RANDOM_SEED;
	static int nCurRandomSeed;

	// checksums
	static std::unordered_set<const IGDBObject*, SDefaultPtrHash> gdbObjects;

	typedef void ( CMapInfo:: *TGetCheckSumFunc )( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void UpdateCheckSum( TGetCheckSumFunc pCheckSumFunc, uLong *pResourcesCheckSum, uLong *pMapCheckSum );

	//
	void GetTerrainCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetObjectsCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetEntrenchmentsCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetBridgesCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetReinforcementsCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetScriptFileCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetScriptAreasCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetDiplomaciesCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetUnitCreationCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetStartCommandsListCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetReservePositionsListCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	void GetAIConstsCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum );

	void ProcessObjectCheckSum( const char *pszObjectName, NCheckSums::SCheckSumBufferStorage *pBufRes, NCheckSums::SCheckSumBufferStorage *pBufMap );
	
	//----------------------------------------------------------------------------------------------------
	static bool GetTileIndicesInternal( const CVec3 &rPoint, int *pnXPosition, int *pnYPosition, const CTPoint<int> &rTerrainSize, float fCellSize, bool isYReverse );
	static void PackFrameIndex( IObjectsDB *pGDB, SMapObjectInfo *pMapObjectInfo );
	static void UnpackFrameIndex( IObjectsDB *pGDB, SMapObjectInfo *pMapObjectInfo, int *pRandomSeed = 0 );

public:	
	//----------------------------------------------------------------------------------------------------
	CMapInfo()
	{
		vCameraAnchor = VNULL3;
		nSeason = CMapInfo::SEASON_SUMMER;
		nMissionIndex = -1;
		nType = TYPE_SINGLE_PLAYER;
		nAttackingSide = 0;
		FillDefaultDiplomacies();
	}
	
	/**
	CMapInfo& operator=( const CMapInfo &rMapInfo )
	{
		if( &rMapInfo != this )
		{
			terrain = rMapInfo.terrain;
			objects = rMapInfo.objects;
			scenarioObjects = rMapInfo.scenarioObjects;
			entrenchments = rMapInfo.entrenchments;
			bridges = rMapInfo.bridges;
			reinforcements.groups = rMapInfo.reinforcements.groups;
			szScriptFile = rMapInfo.szScriptFile;
			scriptAreas = rMapInfo.scriptAreas;
			vCameraAnchor = rMapInfo.vCameraAnchor;
			playersCameraAnchors = rMapInfo.playersCameraAnchors;
			nSeason = rMapInfo.nSeason;
			szSeasonFolder = rMapInfo.szSeasonFolder;
			diplomacies = rMapInfo.diplomacies;
			unitCreation = rMapInfo.unitCreation;
			startCommandsList = rMapInfo.startCommandsList;
			reservePositionsList = rMapInfo.reservePositionsList;
			soundsList = rMapInfo.soundsList;
			szForestCircleSounds = rMapInfo.szForestCircleSounds;
			szForestAmbientSounds = rMapInfo.szForestAmbientSounds;

			szChapterName = rMapInfo.szChapterName;
			nMissionIndex = rMapInfo.nMissionIndex;
			
			nType = rMapInfo.nType;
			nAttackingSide = rMapInfo.nAttackingSide;
			
			sounds.sounds = rMapInfo.sounds.sounds;
			
			aiGeneralMapInfo.sidesInfo = rMapInfo.aiGeneralMapInfo.sidesInfo;

			szMODName = rMapInfo.szMODName;
			szMODVersion = rMapInfo.szMODVersion;
		}
		return *this;
	}	
	/**/
	
	//----------------------------------------------------------------------------------------------------
	int GetSelectedSeason();
	void FillDefaultDiplomacies();
	void Clear();
	//�������� ������ ����� ��������� ������� � ������ ( � VIS ������ )
	bool Create( const CTPoint<int> &rSize, int _nSeason, const std::string &rzSeasonFolder, int nPlayersCount, int _nType );
	bool IsValid();

	//----------------------------------------------------------------------------------------------------
	// ��������� ����� �� �����
	bool Load( const char* pszMapName );
	//----------------------------------------------------------------------------------------------------
	// serializing...
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );

	//��� ���������� ������������� �� ������ ����
	//----------------------------------------------------------------------------------------------------
	//������� ������ �� �������
	bool RemoveObject( int nObjectIndex );
	//������� ������� � ������������� (� VIS ������)
	bool RemoveObjects( const std::list<CVec2> &rClearPolygon );

	//----------------------------------------------------------------------------------------------------
	//�������� ��� ������� �������� ���������� ����, ������������� ������ �����
	inline bool PointInMap( float x, float y, bool bAIPoint = false )
	{
		return PointInMap ( ( *this ), x, y, bAIPoint );
	}
	template<class PointType>
	bool PointInMap( const PointType &rPoint, bool bAIPoint = false )
	{
		return PointInMap( rPoint.x, rPoint.y, bAIPoint );
	}
	bool TerrainHitTest( const CVec3 &rPoint, TERRAIN_HIT_TEST_TYPE type, std::vector<int> *pTerrainObjects );
	const SVectorStripeObject* GetRiver( int nID );
	const SVectorStripeObject* GetRoad3D( int nID );

	//----------------------------------------------------------------------------------------------------
	//������������ ���� terrain ( � VIS ������ )
	bool UpdateTerrain( const CTRect<int> &rUpdateRect );
	//��������� ����� ������ ( � VIS ������ )
	bool UpdateTerrainCrosses( const CTRect<int> &rUpdateRect );
	//���������������� ID'����� ��� ( �� ������� ) ( � VIS ������ ) + ������ �����
	bool UpdateTerrainRivers( const CTRect<int> &rUpdateRect );
	//���������������� ID'����� ����� ( �� ������� ) ( � VIS ������ ) + ������ �����
	bool UpdateTerrainRoads3D( const CTRect<int> &rUpdateRect );
	//����������� ������������� ������ �� �����, ������������� � ������� ������ ( ������������ ������ - altitudes ) ( � VIS ������ )
	bool UpdateTerrainShades( const CTRect<int> &rUpdateRect );
	//���������������� ID'����� ������ ( �� ������� ) ( � VIS ������ )
	//���������� ����������� �������� linkID ��� ����������� �������������
	int UpdateObjects( const CTRect<int> &rUpdateRect );
	//���������� frameIndices � ����
	//���������� ���������� ������������ ��������
	void PackFrameIndices();
	//����������� ���� � frameIndices
	//���������� ���������� ������������� ��������
	void UnpackFrameIndices();
	//������� ������� ���� ��� �� ������������ � Data storage
	bool RemoveNonExistingObjects( IDataStorage *pDataStorage, IObjectsDB *pObjectsDB, std::string *pszOutputString = 0 );

	//----------------------------------------------------------------------------------------------------
	//������� �������� � minimap � ��������������� �����
	bool CreateMiniMapImage( const CRMImageCreateParameterList &rImageCreateParameterList, interface IProgressHook *pProgressHook = 0 );
	//������������� ����� ���, ����� � �����
	bool AddSounds( TMapSoundInfoList *pSoundsList, DWORD dwSoundTypeBits );
	bool GetUsedLinkIDs( CUsedLinkIDs *pUsedLinkIDs );
	bool GetUsedScriptIDs( CUsedScriptIDs *pUsedScriptIDs );
	bool GetUsedScriptAreas( CUsedScriptAreas *pUsedScriptAreas );

	//----------------------------------------------------------------------------------------------------
	//���������� ������������� �������������� ������������ terrain Y
	bool GetTerrainTileIndices( const CVec3 &rPoint, CTPoint<int> *pPoint );
	bool GetTileIndices( const CVec3 &rPoint, CTPoint<int> *pPoint );
	bool GetAITileIndices( const CVec3 &rPoint, CTPoint<int> *pPoint );

	//----------------------------------------------------------------------------------------------------
	//������� ���������
	void InvertYTile( CTPoint<int> *pPoint );
	void InvertYPosition( CTPoint<float> *pPoint );

	//----------------------------------------------------------------------------------------------------
	// ������ checksums
	// pResourcesCheckSum - �������, ������������ �� �����
	// pMapCheckSum - checksum �����
	void GetCheckSums( uLong *pResourcesCheckSum, uLong *pMapCheckSum );
	//----------------------------------------------------------------------------------------------------
	// ����������� ������� ��� �������������� ��������� ��������� CMapInfo
	static int GetSelectedSeason( int nSeason, const std::string &rszSeasonFolder );
	static void FillDefaultDiplomacies( SLoadMapInfo *pLoadMapInfo );
	static void Clear( SLoadMapInfo *pLoadMapInfo );
	static bool Create( SLoadMapInfo *pLoadMapInfo, const CTPoint<int> &rSize, int _nSeason, const std::string &rszSeasonFolder, int nPlayersCount, int _nType );
	static bool IsValid( const SLoadMapInfo rLoadMapInfo );

	//----------------------------------------------------------------------------------------------------
	static bool RemoveObject ( SLoadMapInfo *pLoadMapInfo, int nObjectIndex );
	static bool RemoveObjects( SLoadMapInfo *pLoadMapInfo, const std::list<CVec2> &rClearPolygon );

	//----------------------------------------------------------------------------------------------------
	static bool PointInMap( const SLoadMapInfo &rLoadMapInfo, float x, float y, bool bAIPoint = false );
	template<class PointType>
	static bool PointInMap( const SLoadMapInfo &rLoadMapInfo, const PointType &rPoint, bool bAIPoint = false )
	{
		return PointInMap( rLoadMapInfo, rPoint.x, rPoint.y, bAIPoint );
	}
	static bool TerrainHitTest( const STerrainInfo &rTerrainInfo, const CVec3 &rPoint, TERRAIN_HIT_TEST_TYPE type, std::vector<int> *pTerrainObjects );
	static const SVectorStripeObject* GetRiver( const STerrainInfo &rTerrainInfo, int nID );
	static const SVectorStripeObject* GetRoad3D( const STerrainInfo &rTerrainInfo, int nID );
	
	//----------------------------------------------------------------------------------------------------
	static bool UpdateTerrain				( struct STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect, const struct STilesetDesc &rTilesetDesc, const struct SCrossetDesc &rCrossetDesc, /*const struct SRoadsetDesc &rRoadsetDesc,*/ const struct SGFXLightDirectional &rSunlight );
	static bool UpdateTerrainCrosses( struct STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect, const struct STilesetDesc &rTilesetDesc, const struct SCrossetDesc &rCrossetDesc/*, const struct SRoadsetDesc &rRoadsetDesc*/ );
	static bool UpdateTerrainRivers	( struct STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect );
	static bool UpdateTerrainRoads3D( struct STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect );
	static bool UpdateTerrainShades	( struct STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect, const struct SGFXLightDirectional &rSunlight );
	static int  UpdateObjects				( struct SLoadMapInfo *pLoadMapInfo, const CTRect<int> &rUpdateRect );
	static void PackFrameIndices		( struct SLoadMapInfo *pLoadMapInfo );
	static void UnpackFrameIndices	( struct SLoadMapInfo *pLoadMapInfo );
	static bool RemoveNonExistingObjects( struct SLoadMapInfo *pLoadMapInfo, IDataStorage *pDataStorage, IObjectsDB *pObjectsDB, std::string *pszOutputString = 0 );

	//----------------------------------------------------------------------------------------------------
	static bool GetTerrainTileIndices( const STerrainInfo &rTerrainInfo, const CVec3 &rPoint, CTPoint<int> *pPoint );
	static bool GetTileIndices( const STerrainInfo &rTerrainInfo, const CVec3 &rPoint, CTPoint<int> *pPoint );
	static bool GetAITileIndices( const STerrainInfo &rTerrainInfo, const CVec3 &rPoint, CTPoint<int> *pPoint );
	
	//----------------------------------------------------------------------------------------------------
	static void InvertYTile( const STerrainInfo &rTerrainInfo, CTPoint<int> *pPoint );
	static void InvertYPosition( const STerrainInfo &rTerrainInfo, CTPoint<float> *pPoint );

	//----------------------------------------------------------------------------------------------------
	static bool CreateMiniMapImage( const SLoadMapInfo &rLoadMapInfo, const CRMImageCreateParameterList &rImageCreateParameterList, interface IProgressHook *pProgressHook = 0 );
	static bool AddSounds( const SLoadMapInfo &rLoadMapInfo, TMapSoundInfoList *pSoundsList, DWORD dwSoundTypeBits );
	static bool GetUsedLinkIDs( const SLoadMapInfo &rLoadMapInfo, CUsedLinkIDs *pUsedLinkIDs );
	static bool GetUsedScriptIDs( const SLoadMapInfo &rLoadMapInfo, CUsedScriptIDs *pUsedScriptIDs );
	static bool GetUsedScriptAreas( const SLoadMapInfo &rLoadMapInfo, CUsedScriptAreas *pUsedScriptAreas );

	//----------------------------------------------------------------------------------------------------
	//�������� ��������� ����
	bool AddMapInfo( const CTPoint<int> &rDestPoint, const SLoadMapInfo &rSourceLoadMapInfo );
	bool FillTerrain( int nTileIndex );
	bool FillTileSet( const std::list<CVec2> &rInclusivePolygon,
										const std::list<std::list<CVec2> > &rExclusivePolygons,
										const CRMTileSet &rTileSet,
										std::unordered_map<LPARAM, float> *pDistances = 0 );
	//CArray2D<BYTE> *pTileMap - � AI ������ ( � ��� ���� ������ ��� VIS ������ )
	bool FillObjectSet( const std::list<CVec2> &rInclusivePolygon,
											const std::list<std::list<CVec2> > &rExclusivePolygons,
											const CRMObjectSet &rObjectSet,
											CArray2D<BYTE> *pTileMap = 0 );
	bool FillProfilePattern( const std::list<CVec2> &rInclusivePolygon,
													 const std::list<std::list<CVec2> > &rExclusivePolygons,
													 const struct SVAGradient &rGradient,
													 const CTPoint<int> &rPatternSize,
													 float fPositiveRatio,
													 std::unordered_map<LPARAM, float> *pDistances = 0 );

	//----------------------------------------------------------------------------------------------------
	static bool AddMapInfo( SLoadMapInfo *pLoadMapInfo, const CTPoint<int> &rDestPoint, const SLoadMapInfo &rSourceLoadMapInfo );
	static bool FillTerrain( STerrainInfo *pTerrainInfo, const struct STilesetDesc &rTilesetDesc, int nTileIndex );
	static bool FillTileSet( STerrainInfo *pTerrainInfo, const struct STilesetDesc &rTilesetDesc, const std::list<CVec2> &rInclusivePolygon, const std::list<std::list<CVec2> > &rExclusivePolygons, const CRMTileSet &rTileSet, std::unordered_map<LPARAM, float> *pDistances = 0 );
	static bool FillObjectSet( SLoadMapInfo *pLoadMapInfo, const std::list<CVec2> &rInclusivePolygon, const std::list<std::list<CVec2> > &rExclusivePolygons, const CRMObjectSet &rObjectSet, CArray2D<BYTE> *pTileMap = 0 );
	static bool FillProfilePattern( STerrainInfo *pTerrainInfo, const std::list<CVec2> &rInclusivePolygon, const std::list<std::list<CVec2> > &rExclusivePolygons, const struct SVAGradient &rGradient, const CTPoint<int> &rPatternSize, float fPositiveRatio, std::unordered_map<LPARAM, float> *pDistances = 0 );

	//nGraph == ( -1 ) - random number will be used
	//nAngle == ( -1 ) - random number will be used
	static bool CreateRandomMap( struct SMissionStats *pMissionStats, const std::string &rszContextFileName, int nLevel, int nGraph = ( -1 ), int nAngle = ( -1 ), bool bSaveAsBZM = true, bool bSaveAsDDS = true, SRMUsedTemplateInfo *pRMUsedTemplateInfo = 0, interface IProgressHook *pProgressHook = 0 );

	//----------------------------------------------------------------------------------------------------
	static bool GetScenarioObjects( const std::string &rszMapInfoFileName, std::vector<SMapObjectInfo> *pMapObjects );
	//----------------------------------------------------------------------------------------------------
	
	//obsolete
	//static const DWORD ROAD_BITS_NULL_VALUE;
	//static const DWORD ROAD_BITS[32];
	//static const DWORD ROAD_CROSS_TILE_INDICES[31];
	//static const DWORD ROAD_EDGE_MAP_CROSS_TILE_INDICES[31];
	//static const DWORD ROAD_CROSS_BITS[31][11]; 
	//static const DWORD ROAD_BITS_DIMENSION;
	//static const DWORD ROAD_CROSS_BITS_DIMENSION;
	//static const DWORD ROAD_CROSS_BITS_CASES;

	//��������� ����� �����, �������� ������ ������ ( ����������� ������ )
	//bool UpdateTerrainRoads( const CTRect<int> &rUpdateRect );
	//static bool UpdateTerrainRoads( struct STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect, const struct SRoadsetDesc &rRoadsetDesc );
	//bool AddRoad( const CTRect<int> &rRoadRect, int nRoadType, int nRoadDirection, std::vector<SRoadItem> *pRoad = 0  );
	//bool MakeRoad( const SRoadPoint &rFrom, const SRoadPoint &rTo, int nRoadType, const SRoadMakeParameter &rRoadMakeParamerer, std::vector<SRoadItem> *pRoad = 0 );
	//static bool AddRoad					( SLoadMapInfo *pLoadMapInfo, const CTRect<int> &rRoadRect, int nRoadType, int nRoadDirection, std::vector<SRoadItem> *pRoad = 0 );
	//static bool MakeRoad				( SLoadMapInfo *pLoadMapInfo, const SRoadPoint &rFrom, const SRoadPoint &rTo, int nRoadType, const SRoadMakeParameter &rRoadMakeParamerer, std::vector<SRoadItem> *pRoad = 0 );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void RMGGetSeasonNameString( int nSeason, const std::string &rszSeasonFolder, std::string *pszSeasonName )
{
	NI_ASSERT_T( pszSeasonName != 0,
							 NStr::Format( "GetSeasonName, Wrong parameter pszSeasonName: %x\n", pszSeasonName ) );

	if ( nSeason == 0 )
	{
		std::string szSeasonFolder( rszSeasonFolder );
		std::string szSpringSeasonFolder( CMapInfo::SEASON_FOLDERS[CMapInfo::SEASON_SPRING] );
		NStr::ToLower( szSeasonFolder );
		NStr::ToLower( szSpringSeasonFolder );
		if ( szSeasonFolder.compare( szSpringSeasonFolder ) == 0 )
		{
			( *pszSeasonName ) = CMapInfo::SEASON_NAMES[CMapInfo::SEASON_SPRING];
			return;
		}
	}
	( *pszSeasonName ) = CMapInfo::SEASON_NAMES[nSeason];
}
	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void RMGGetUsedScriptIDsString( const CUsedScriptIDs &rUsedScriptIDs,  std::string *pszUsedScriptIDsString )
{
	NI_ASSERT_T( pszUsedScriptIDsString != 0,
							 NStr::Format( "RMGGetUsedScriptIDsString, Wrong parameter pszUsedScriptIDsString: %x\n", pszUsedScriptIDsString ) );
	
	pszUsedScriptIDsString->clear();
	for ( CUsedScriptIDs::const_iterator usedScripIDIterator = rUsedScriptIDs.begin(); usedScripIDIterator != rUsedScriptIDs.end(); ++usedScripIDIterator )
	{
		if ( usedScripIDIterator == rUsedScriptIDs.begin() )
		{
			( *pszUsedScriptIDsString ) += NStr::Format( "%d", *usedScripIDIterator );
		}
		else
		{
			( *pszUsedScriptIDsString ) += NStr::Format( ", %d", *usedScripIDIterator );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void RMGGetUsedScriptAreasString( const CUsedScriptAreas &rUsedScriptAreas,  std::string *pszUsedScriptAreasString )
{
	NI_ASSERT_T( pszUsedScriptAreasString != 0,
							 NStr::Format( "RMGGetUsedScriptAreasString, Wrong parameter pszUsedScriptAreasString: %x\n", pszUsedScriptAreasString ) );
	
	pszUsedScriptAreasString->clear();
	for ( CUsedScriptAreas::const_iterator usedScripAreaIterator = rUsedScriptAreas.begin(); usedScripAreaIterator != rUsedScriptAreas.end(); ++usedScripAreaIterator )
	{
		if ( usedScripAreaIterator == rUsedScriptAreas.begin() )
		{
			( *pszUsedScriptAreasString ) += NStr::Format( "<%s>", usedScripAreaIterator->c_str() );
		}
		else
		{
			( *pszUsedScriptAreasString ) += NStr::Format( ", <%s>", usedScripAreaIterator->c_str() );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // #if !defined(__MapInfo__Types__)
