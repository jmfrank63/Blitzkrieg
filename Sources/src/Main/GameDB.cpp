#include "StdAfx.h"

#include "GameDB.h"
#include <float.h>

#include "..\Formats\fmtTerrain.h"
#include "RPGStats.h"
#include "GameStats.h"
#include "..\zlib\zlib.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIPrice
{
	int nType;														// RPG Type
	float fPrice;													// AI Price
	//
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "Type", &nType );
		saver.Add( "Price", &fPrice );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CObjectsDB : public IObjectsDB
{
	OBJECT_NORMAL_METHODS( CObjectsDB );
	//
	typedef std::unordered_map<const IGDBObject*, const SCommonRPGStats*, SDefaultPtrHash> CObjectsRPGMap;
	typedef std::unordered_map<std::string, SCommonRPGStats*> CAddStatsMap;
	typedef std::unordered_map<std::string, SBasicGameStats*> CGameStatsMap;
	typedef std::unordered_map<int, SAIExpLevel> CAIExpLevelsMap;
	typedef std::unordered_map<std::string, const SAIExpLevel*> CAIExpLevelsByNameMap;
	//
	typedef std::unordered_map<std::string, const SGDBObjectDesc*> CObjDescMap;
	std::vector<SGDBObjectDesc> objects;	// all object entries
	CObjDescMap objmap;										// objects map
	CObjectsRPGMap rpgs;									// RPG stats
	CAddStatsMap addstats;								// additional stats
	CGameStatsMap gamestats;							// game stats
	//
	std::vector<SAIPrice> aiPrices;				// AI prices
	CAIExpLevelsMap aiExpLevels;					// AI exp levels
	SAIExpLevel defaultExpLevel;
	
	CAIExpLevelsByNameMap aiExpLevelsByName;	//
	//
	void AddObject( const std::string &szFullName, EObjVisType eVisType, EObjGameType eGameType );
	void LoadAIPrices();
	void LoadRepairCostAdjust();
	const IGDBObject* GetExpLevelsByName( const std::string &szName ) { return aiExpLevelsByName[szName]; }
	//
	void LoadAsXML( SWin32Time timeXML );
public:
	CObjectsDB() { }
	~CObjectsDB();
	virtual const char* STDCALL GetName() { return "ObjectsDB"; }
	virtual const char* STDCALL GetParentName() { return "Game"; }
	//
	virtual const IGDBObject* STDCALL Get( const char *pszName, const char *pszParentName );
	virtual const SGDBObjectDesc* STDCALL GetDesc( int nIndex ) const { return &( objects[nIndex] ); }
	virtual const SGDBObjectDesc* STDCALL GetDesc( const char *pszName ) const 
	{ 
		std::unordered_map<std::string, const SGDBObjectDesc*>::const_iterator pos = objmap.find( pszName );
		return pos != objmap.end() ? pos->second : 0; 
	}
	//
	virtual int STDCALL GetIndex( const SGDBObjectDesc *pObject ) const
	{
		return ( (pObject == 0) ? -1 : ( pObject - &( objects[0] ) ) );
	}
	
	virtual int STDCALL GetIndex( const char *pszName ) const
	{
		return GetIndex( GetDesc( pszName ) );
	}

	virtual int STDCALL GetNumDescs() const { return objects.size(); }
	virtual const SGDBObjectDesc* STDCALL GetAllDescs() const { return &( objects[0] ); }
	// additional object info retrieving
	virtual const IGDBObject* STDCALL GetRPGStats( const IGDBObject *pObject );
	virtual const IGDBObject* STDCALL GetAddStats( const char *pszName, EAddStatsType type );
	virtual const IGDBObject* STDCALL GetGameStats( const char *pszName, EGameStatsType type );
	virtual const IGDBObject* STDCALL GetExpLevels( const int nUnitType ) const;
	//
	virtual bool STDCALL LoadDB();
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IObjectsDB* STDCALL CreateObjectsDB()
{
	return new CObjectsDB();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CObjectsDB::~CObjectsDB()
{
	// delete RPS stats
	ClearComplexContainer( rpgs );
	// delete weapon RPG stats
	ClearComplexContainer( addstats );
	// delete game stats
	ClearComplexContainer( gamestats );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectsDB::LoadAsXML( SWin32Time timeXML )
{
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	// load XML file
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( "objects.xml", STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ, "ObjectDB" );
		tree.Add( "Objects", &objects );
	}
	// write binary file
	const std::string szGDBFileName = std::string( pStorage->GetName() ) + "objects.gdb";
	if ( CPtr<IDataStream> pStream = CreateFileStream(szGDBFileName.c_str(), STREAM_ACCESS_WRITE) )
	{
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSS;
		saver.Add( 1, &timeXML );
		saver.Add( 2, &objects );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObjectsDB::LoadDB()
{
	objects.clear();
	objmap.clear();
	ClearComplexContainer( rpgs );
	ClearComplexContainer( addstats );
	ClearComplexContainer( gamestats );
	aiPrices.clear();
	aiExpLevels.clear();
	aiExpLevelsByName.clear();
	//
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	SStorageElementStats statsXML, statsGDB;
	Zero( statsXML );
	pStorage->GetStreamStats( "objects.xml", &statsXML );
	Zero( statsGDB );
	pStorage->GetStreamStats( "objects.gdb", &statsGDB );
	if ( (statsXML.nSize == 0) && (statsGDB.nSize == 0) ) 
		return false;
	else if ( (statsXML.nSize > 0) && (statsGDB.nSize > 0) ) 
	{
		if ( CPtr<IDataStream> pStream = pStorage->OpenStream("objects.gdb", STREAM_ACCESS_READ) )
		{
			SWin32Time timeXML;
			CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::READ );
			CSaverAccessor saver = pSS;
			saver.Add( 1, &timeXML );
			if ( statsXML.mtime != timeXML ) 
			{
				saver = 0;
				pSS = 0;
				pStream = 0;
				LoadAsXML( statsXML.mtime );
			}
			else
				saver.Add( 2, &objects );
		}
	}
	else
		LoadAsXML( statsXML.mtime );
	//
	{
		// MOD objects database
		std::vector<SGDBObjectDesc> modobjects;
		if ( CPtr<IDataStream> pMODStream = GetSingleton<IDataStorage>()->OpenStream("modobjects.xml", STREAM_ACCESS_READ) )
		{
			CTreeAccessor tree = CreateDataTreeSaver( pMODStream, IDataTree::READ, "ObjectDB" );
			tree.Add( "Objects", &modobjects );
		}
		objects.reserve( objects.size() + modobjects.size() );
		// load object descriptors database
		objmap.clear();
		for ( std::vector<SGDBObjectDesc>::iterator it = objects.begin(); it != objects.end(); ++it )
		{
			NStr::ToLower( it->szPath );
			objmap.insert( CObjDescMap::value_type(it->szKey, &(*it)) );
		}
		// check - one can add only NEW objects to database
		for ( std::vector<SGDBObjectDesc>::iterator it = modobjects.begin(); it != modobjects.end(); ++it )
		{
			if ( objmap.find(it->szKey) == objmap.end() ) 
			{
				objects.push_back( *it );

				SGDBObjectDesc &desc = objects.back();
				NStr::ToLower( desc.szPath );
				objmap.insert( CObjDescMap::value_type(desc.szKey, &desc) );
			}
			else
			{
				NI_ASSERT_T( objmap.find(it->szKey) != objmap.end(), NStr::Format("Object \"%s\" already registered in objects DB", it->szKey.c_str()) );
			}
		}
	}
	// load all RPG objects
	LoadAIPrices();
	LoadRepairCostAdjust();

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** automagic class for str <=> enum type conversions
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGDBAutoMagic
{
	typedef std::unordered_map<int, std::string> CIntToStrMap;
	typedef std::unordered_map<std::string, int> CStrToIntMap;
	// vis type
	CIntToStrMap vistostr;
	CStrToIntMap strtovis;
	// game type
	CIntToStrMap gametostr;
	CStrToIntMap strtogame;
public:
	CGDBAutoMagic()
	{
		// vis
		// to str
		vistostr[SGVOT_UNKNOWN	] = "unknown";
		vistostr[SGVOT_SPRITE		] = "sprite";
		vistostr[SGVOT_MESH			] = "mesh";
		vistostr[SGVOT_EFFECT		] = "effect";
		vistostr[SGVOT_NONVISUAL] = "nonvisual";
		// to vis
		for ( CIntToStrMap::const_iterator it = vistostr.begin(); it != vistostr.end(); ++it )
			strtovis[it->second] = it->first;
		// game
		// to str
		gametostr[SGVOGT_UNKNOWN			] = "unknown";
		gametostr[SGVOGT_UNIT					] = "unit";
		gametostr[SGVOGT_BUILDING			] = "building";
		gametostr[SGVOGT_FORTIFICATION] = "fortification";
		gametostr[SGVOGT_OBJECT				] = "object";
		gametostr[SGVOGT_FENCE				] = "fence";
		gametostr[SGVOGT_ENTRENCHMENT	] = "entrenchment";
		gametostr[SGVOGT_TANK_PIT			] = "tank_pit";
		gametostr[SGVOGT_TERRAOBJ			] = "terraobj";
		gametostr[SGVOGT_EFFECT				] = "effect";
		gametostr[SGVOGT_PROJECTILE		] = "projectile";
		gametostr[SGVOGT_MINE					] = "mine";
		gametostr[SGVOGT_BRIDGE				] = "bridge";
		gametostr[SGVOGT_SQUAD				] = "squad";
		gametostr[SGVOGT_SOUND				] = "sound";
		gametostr[SGVOGT_FLAG					] = "flag";
		// to game
		for ( CIntToStrMap::const_iterator it = gametostr.begin(); it != gametostr.end(); ++it )
			strtogame[it->second] = it->first;
	}
	// vis type
	EObjVisType GetVisType( const std::string &type ) { return EObjVisType( strtovis[type] ); }
	const std::string& GetVisType( EObjVisType type ) { return vistostr[type]; }
	// game type
	EObjGameType GetGameType( const std::string &type ) { return EObjGameType( strtogame[type] ); }
	const std::string& GetGameType( EObjGameType type ) { return gametostr[type]; }
};
static CGDBAutoMagic automagic;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SGDBObjectDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "name", &szKey );
	saver.Add( "path", &szPath );
	if ( saver.IsReading() )
	{
		std::string szString;
		// vis type
		saver.Add( "type", &szString );
		eVisType = automagic.GetVisType( szString );
		// game type
		saver.Add( "game_type", &szString );
		eGameType = automagic.GetGameType( szString );
	}
	else
	{
		// vis type
		std::string szString = automagic.GetVisType( eVisType );
		saver.Add( "type", &szString );
		// game type
		szString = automagic.GetGameType( eGameType );
		saver.Add( "game_type", &szString );
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SGDBObjectDesc::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szKey );
	saver.Add( 2, &szPath );
	saver.Add( 3, &eVisType );
	saver.Add( 4, &eGameType );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** additional information retrieving
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const IGDBObject* CObjectsDB::Get( const char *pszName, const char *pszParentName )
{
	if ( _stricmp(pszParentName, "Desc") == 0 )
		return GetDesc( pszName );
	else if ( _stricmp(pszName, "Weapon") == 0 )
		return GetAddStats( pszParentName, IObjectsDB::WEAPON );
	else if ( _stricmp(pszName, "Ack") == 0 )
		return GetAddStats( pszParentName, IObjectsDB::ACKS );	
	else if ( _stricmp(pszName, "AIExpLevel") == 0 )
		return GetExpLevelsByName( pszParentName );
	else if ( _stricmp(pszName, "Mission") == 0 )
		return GetGameStats( pszParentName, IObjectsDB::MISSION );	
	else if ( _stricmp(pszName, "Chapter") == 0 )
		return GetGameStats( pszParentName, IObjectsDB::CHAPTER );	
	else if ( _stricmp(pszName, "Campaign") == 0 )
		return GetGameStats( pszParentName, IObjectsDB::CAMPAIGN );	
	else if ( _stricmp(pszName, "Medal") == 0 )
		return GetGameStats( pszParentName, IObjectsDB::MEDAL );	
	else
	{		
		const SGDBObjectDesc *pDesc = static_cast<const SGDBObjectDesc*>( GetDesc( pszParentName ) );
		return pDesc != 0 ? GetRPGStats( pDesc ) : 0;
	}
	NI_ASSERT_TF( 0, NStr::Format("Can't find GDB object \"%s\" of parent \"%s\"", pszName, pszParentName), return 0 );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
TYPE* ReadRPGStats( const SGDBObjectDesc *pObj, const char *pszStatsName, const char *pszAdd = "\\1.xml" )
{
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (pObj->szPath + pszAdd).c_str(), STREAM_ACCESS_READ );
	if ( pStream != 0 )
	{
		TYPE *pRPG = new TYPE();
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( pszStatsName, pRPG );
		return pRPG;
	}
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const IGDBObject* CObjectsDB::GetRPGStats( const IGDBObject *pGDBObject )
{
	CObjectsRPGMap::iterator pos = rpgs.find( pGDBObject );
	if ( pos != rpgs.end() )
		return pos->second;
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );
	// crete new object
	const SGDBObjectDesc *pObj = static_cast<const SGDBObjectDesc*>( pGDBObject );
	//
	SCommonRPGStats *pRPG = 0;
	const char *pszStatsName = "desc";
	switch ( pObj->eGameType )
	{
		case SGVOGT_UNIT:
			if ( pObj->eVisType == SGVOT_MESH )
				pRPG = ReadRPGStats<SMechUnitRPGStats>( pObj, "RPG" );
			else if ( pObj->eVisType == SGVOT_SPRITE )
				pRPG = ReadRPGStats<SInfantryRPGStats>( pObj, "RPG" );
			else
			{
				NI_ASSERT_SLOW_TF( false, NStr::Format("Unknown stats type for \"%s\"", pObj->szKey.c_str()), return 0 );
			}
			break;
		case SGVOGT_TANK_PIT:
			pRPG = ReadRPGStats<SMechUnitRPGStats>( pObj, "RPG" );
			break;
		case SGVOGT_BUILDING:
		case SGVOGT_FORTIFICATION:
			pRPG = ReadRPGStats<SBuildingRPGStats>( pObj, "desc" );
			break;
		case SGVOGT_FENCE:
			pRPG = ReadRPGStats<SFenceRPGStats>( pObj, "RPG" );
			break;
		case SGVOGT_ENTRENCHMENT:
			pRPG = ReadRPGStats<SEntrenchmentRPGStats>( pObj, "RPG" );
			break;
		case SGVOGT_BRIDGE:
			pRPG = ReadRPGStats<SBridgeRPGStats>( pObj, "RPG" );
			break;
		case SGVOGT_MINE:
			pRPG = ReadRPGStats<SMineRPGStats>( pObj, "RPG" );
			break;
		case SGVOGT_SQUAD:
			pRPG = ReadRPGStats<SSquadRPGStats>( pObj, "RPG" );
			break;
		case SGVOGT_TERRAOBJ:
			/*
			if ( pObj->eVisType == SGVOT_MESH ) 
				pRPG = ReadRPGStats<STerraObjSetRPGStats>( pObj, "RPG" );
			else
			*/
				pRPG = ReadRPGStats<SObjectRPGStats>( pObj, "desc" );
			break;
		case SGVOGT_SOUND:
			pRPG = ReadRPGStats<SSoundRPGStats>( pObj, "RPG", ".xml" );
			break;
		default:
			pRPG = ReadRPGStats<SObjectRPGStats>( pObj, "desc" );
	}
	//
	NI_ASSERT_SLOW_T( pRPG != 0, NStr::Format("Can't read RPG stats for \"%s\"", pObj->szKey.c_str()) );
	// set parent name
	pRPG->szParentName = pObj->szKey;
	// получим все ссылки
	pRPG->RetrieveShortcuts( this );
	// провалидируем...
	pRPG->Validate();
	// преобразовать из человеческих единиц в AI
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );
	pRPG->ToAIUnits();
	// store in the map
	rpgs[pGDBObject] = pRPG;
	pRPG->GetCheckSum();
	//
	return pRPG;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
TYPE* GetAddStatsLocal( const char *pszName, const char *pszPath, const char *pszEntry )
{
	// crete new object
	TYPE *pStats = new TYPE();
	// load data
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (std::string(pszPath) + pszName + ".xml").c_str(), STREAM_ACCESS_READ );
	if ( pStream != 0 )
	{
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( pszEntry, pStats );
	}
	//
	return pStats;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const IGDBObject* CObjectsDB::GetAddStats( const char *pszName, IObjectsDB::EAddStatsType type )
{
	CAddStatsMap::const_iterator pos = addstats.find( pszName );
	if ( pos != addstats.end() )
		return pos->second;
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );
	// create new object
	SCommonRPGStats *pStats = 0;
	switch ( type )
	{
		case IObjectsDB::WEAPON:
			pStats = GetAddStatsLocal<SWeaponRPGStats>( pszName, "weapons\\", "RPG" );
			break;
		case IObjectsDB::ACKS:
			pStats = GetAddStatsLocal<SAckRPGStats>( pszName, "", "RPG" );
		default:
			NI_ASSERT_TF( pStats != 0, NStr::Format("Can't find add stats \"%s\" of type %d", pszName, type), return 0 );
	}
	//
	if ( pStats == 0 )
		return 0;
	// преобразовать из человеческих единиц в AI
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );
	pStats->ToAIUnits();
	// set parent name
	pStats->szParentName = pszName;
	addstats[pszName] = pStats;
	//
	return pStats;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
TYPE* GetGameStatsLocal( const std::string &szName )
{
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (szName + ".xml").c_str(), STREAM_ACCESS_READ );
	if ( pStream != 0 )
	{
		TYPE *pStats = new TYPE();
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "RPG", pStats );
		return pStats;
	}
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const IGDBObject* CObjectsDB::GetGameStats( const char *pszName, EGameStatsType type )
{
	std::string szName = pszName;
	NStr::ToLower( szName );
	CGameStatsMap::iterator pos = gamestats.find( szName );
	if ( pos != gamestats.end() )
		return pos->second;
	//
	SBasicGameStats *pStats = 0;
	switch ( type ) 
	{
		case IObjectsDB::MISSION:
			pStats = GetGameStatsLocal<SMissionStats>( szName );
			break;
		case IObjectsDB::CHAPTER:
			pStats = GetGameStatsLocal<SChapterStats>( szName );
			break;
		case IObjectsDB::CAMPAIGN:
			pStats = GetGameStatsLocal<SCampaignStats>( szName );
			break;
		case IObjectsDB::MEDAL:
			pStats = GetGameStatsLocal<SMedalStats>( szName );
			break;
		case IObjectsDB::BASIC:
			pStats = GetGameStatsLocal<SBasicGameStats>( szName );
			break;
		default:
			NI_ASSERT_TF( pStats != 0, NStr::Format("Can't find game stats \"%s\" of type %d", pszName, type), return 0 );
	}
	//
	if ( pStats == 0 )
		return 0;
	// retrieve all shortcuts
	pStats->RetrieveShortcuts( this );
	// set parent name
	pStats->szParentName = szName;
	// register in the map
	if ( type != IObjectsDB::BASIC ) 
		gamestats[szName] = pStats;
	//
	return pStats;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const IGDBObject* CObjectsDB::GetExpLevels( const int nUnitType ) const
{
	CAIExpLevelsMap::const_iterator pos = aiExpLevels.find( nUnitType );
	return pos !=  aiExpLevels.end() ? &( pos->second ) : &defaultExpLevel;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectsDB::LoadAIPrices()
{
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "units\\aiprices.xml", STREAM_ACCESS_READ );
	if ( pStream ) 
	{
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		// load and set prices to global vars
		saver.Add( "Prices", &aiPrices );
		for ( std::vector<SAIPrice>::const_iterator it = aiPrices.begin(); it != aiPrices.end(); ++it )
		{
			const std::string szVarName = NStr::Format( "AIPrice.%x", it->nType );
			SetGlobalVar( szVarName.c_str(), it->fPrice );
		}
		// load and set exp levels
		std::vector<SAIExpLevel> explevels;
		saver.Add( "ExpLevels", &explevels );
		for ( std::vector<SAIExpLevel>::const_iterator it = explevels.begin(); it != explevels.end(); ++it )
			aiExpLevels[it->eType] = *it;
		for ( CAIExpLevelsMap::const_iterator it = aiExpLevels.begin(); it != aiExpLevels.end(); ++it )
			aiExpLevelsByName[it->second.szTypeName] = &( it->second );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectsDB::LoadRepairCostAdjust()
{
	// to adjust repair cost of units;
	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );
	const float fRepairCostAdjust = constsTbl.GetFloat( "AI", "Engineers.RepairCostAdjust", 100.0f );
	SetGlobalVar( "RepairCostAdjust", fRepairCostAdjust );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjectsDB::AddObject( const std::string &szFullName, EObjVisType eVisType, EObjGameType eGameType )
{
	objects.push_back( SGDBObjectDesc() );
	SGDBObjectDesc &desc = objects.back();
	// fill descriptor
	desc.szKey = szFullName.substr( 0, szFullName.rfind('\\') );
	desc.szPath = desc.szKey + "\\";
	desc.eVisType = eVisType;
	desc.eGameType = eGameType;
	// register in the map
	objmap[desc.szKey] = &desc;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
