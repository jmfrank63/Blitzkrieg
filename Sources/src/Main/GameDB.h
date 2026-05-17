#ifndef __GAMEDB_H__
#define __GAMEDB_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ensure interface macro is defined
#ifndef interface
#define interface struct
#endif
// Ensure STDCALL macro is defined  
#ifndef STDCALL
#define STDCALL __stdcall
#endif
//
#include "..\Misc\Basic.h"
#include "..\zlib\zlib.h"
// Ensure IGDB is available
#include "..\Misc\Basic.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EObjVisType
{
	SGVOT_UNKNOWN   = 0,
	SGVOT_SPRITE    = 1,									// sprite object
	SGVOT_MESH      = 2,									// mesh objects
	SGVOT_EFFECT		= 3,									// effect object
	SGVOT_FLASH			= 4,									// flash object
	SGVOT_NONVISUAL	= 100,								// non-visual object
	SGVOT_FORCE_DWORD = 0x7fffffff
};
enum EObjGameType
{
	SGVOGT_UNKNOWN			= 0,
	SGVOGT_UNIT					= 1,							// dynamic unit with AI behavior. can be updated each frame
	SGVOGT_BUILDING			= 2,							// static building. can be boarded by soldiers soldiers, etc.
	SGVOGT_FORTIFICATION= 3,							// DOT, DZOT, etc... ground fortifications
	SGVOGT_ENTRENCHMENT = 4,							// entrenchment segment. can be boarded by soldiers soldiers, etc.
	SGVOGT_TANK_PIT			= 5,							// tank pit
	SGVOGT_BRIDGE				= 6,							// bridge segment. can pass through it
	SGVOGT_MINE					= 7,							// mine (anti-infantry or anti-tank)
	SGVOGT_OBJECT				= 8,							// other objects...
	SGVOGT_FENCE				= 9,							// fence
	SGVOGT_TERRAOBJ			= 10,							// terrain object
	SGVOGT_EFFECT				= 11,							// effect (boom, smoke, etc.)
	SGVOGT_PROJECTILE		= 12,							// projectile effect
	SGVOGT_SHADOW				= 13,							// shadow ...
	SGVOGT_ICON					= 14,							// icon object: bar, text, picture
	SGVOGT_SQUAD				= 15,							// squad - composite object
	SGVOGT_FLASH				= 16,							// flash after the shot
	SGVOGT_FLAG					= 17,
	
	SGVOGT_SOUND				= 100,						// sound object - non-visual!!!

	SGVOGT_FORCE_DWORD = 0x7fffffff
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// general descriptor
struct SGDBObjectDesc : public IGDBObject
{
	std::string szKey;										// object key name
	std::string szPath;										// path to game resources for this object
	EObjVisType eVisType;									// visualization type (sprite/mesh/particles/etc.)
	EObjGameType eGameType;								// game type (unit/building/object/effect/etc.)
	//
	virtual const char* STDCALL GetName() const { return szKey.c_str(); }
	virtual const char* STDCALL GetParentName() const { return "Desc"; }
	virtual const uLong STDCALL GetCheckSum() const { return 0L; }
	//
	bool IsObj() const { return (eGameType == SGVOGT_UNIT) || (eGameType == SGVOGT_BUILDING) || (eGameType == SGVOGT_OBJECT) || (eGameType == SGVOGT_FENCE) || (eGameType == SGVOGT_ENTRENCHMENT) || (eGameType == SGVOGT_FORTIFICATION); }
	bool IsHuman() const { return (eGameType == SGVOGT_UNIT) && (eVisType == SGVOT_SPRITE); }
	bool IsTechnics() const { return (eGameType == SGVOGT_UNIT) && (eVisType == SGVOT_MESH); }
	bool IsDestructable() const { return (eGameType == SGVOGT_UNIT) || (eGameType == SGVOGT_BUILDING) || (eGameType == SGVOGT_OBJECT) || (eGameType == SGVOGT_FENCE) || (eGameType == SGVOGT_ENTRENCHMENT) || (eGameType == SGVOGT_FORTIFICATION); }
	//
	int operator&( IDataTree &ss );
	int operator&( IStructureSaver &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// objects database
// Note: IGDB is defined in Basic.h
interface IObjectsDB : public ::IGDB
{
	// type ID
	enum { tidTypeID = 1 };
	// add stats type
	enum EAddStatsType
	{
		WEAPON,
		ACKS
	};
	// game stats type
	enum EGameStatsType
	{
		MISSION,
		CHAPTER,
		CAMPAIGN,
		MEDAL,
		BASIC
	};
	//
	virtual const IGDBObject* STDCALL Get( const char *pszName, const char *pszParentName ) = 0;
	virtual const SGDBObjectDesc* STDCALL GetDesc( int nIndex ) const = 0;
	virtual const SGDBObjectDesc* STDCALL GetDesc( const char *pszName ) const = 0;
	virtual int STDCALL GetIndex( const SGDBObjectDesc *pObject ) const = 0;
	virtual int STDCALL GetIndex( const char *pszName ) const = 0;
	virtual int STDCALL GetNumDescs() const = 0;
	virtual const SGDBObjectDesc* STDCALL GetAllDescs() const = 0;
	// additional object info retrieving
	virtual const IGDBObject* STDCALL GetRPGStats( const IGDBObject *pObject ) = 0;
	virtual const IGDBObject* STDCALL GetAddStats( const char *pszName, EAddStatsType type ) = 0;
	virtual const IGDBObject* STDCALL GetGameStats( const char *pszName, EGameStatsType type ) = 0;
	virtual const IGDBObject* STDCALL GetExpLevels( const int nUnitType ) const = 0;
	//
	virtual bool STDCALL LoadDB() = 0;
};
IObjectsDB* STDCALL CreateObjectsDB();
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGDB
{
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
	inline const TYPE* GetRPGStats( IObjectsDB *pGDB, int nIndex )
	{
		const SGDBObjectDesc* pDesc = pGDB->GetDesc( nIndex );
		NI_ASSERT_SLOW_TF( pDesc != 0, NStr::Format("Can't find desc by index %d", nIndex), return 0 );
		const IGDBObject *pStats = pGDB->GetRPGStats( pDesc );
		NI_ASSERT_SLOW_TF( pStats != 0, NStr::Format("Can't find stats for \"%s\"", pDesc->szKey.c_str()), return 0 );
		NI_ASSERT_SLOW_TF( dynamic_cast<const TYPE*>(pStats) != 0, NStr::Format("RPG stats for \"%s\" is not a right class", pDesc->szKey.c_str()), return 0 );
		return static_cast<const TYPE*>( pStats );
	}
template <class TYPE>
	inline const TYPE* GetRPGStats( int nIndex ) { return GetRPGStats<TYPE>( GetSingleton<IObjectsDB>(), nIndex ); }
template <class TYPE>
	inline const TYPE* GetRPGStats( IObjectsDB *pGDB, const SGDBObjectDesc *pDesc  )
	{
		const IGDBObject *pStats = pGDB->GetRPGStats( pDesc );
		NI_ASSERT_SLOW_TF( dynamic_cast<const TYPE*>(pStats) != 0, NStr::Format("RPG stats for \"%s\" is not a right class", pDesc->szKey.c_str()), return 0 );
		return static_cast<const TYPE*>( pStats );
	}
template <class TYPE>
	inline const TYPE* GetRPGStats( const SGDBObjectDesc *pDesc  ) { return GetRPGStats<TYPE>( GetSingleton<IObjectsDB>(), pDesc ); }
template <class TYPE>
	inline const TYPE* GetRPGStats( IObjectsDB *pGDB, const char *pszName )
	{
		const SGDBObjectDesc *pDesc = pGDB->GetDesc( pszName );
		NI_ASSERT_SLOW_TF( pDesc != 0, NStr::Format("Can't find object \"%s\" descriptor", pszName), return 0 );
		const IGDBObject *pStats = pGDB->GetRPGStats( pDesc );
		NI_ASSERT_SLOW_TF( dynamic_cast<const TYPE*>(pStats) != 0, NStr::Format("RPG stats for \"%s\" is not a right class", pszName), return 0 );
		return static_cast<const TYPE*>( pStats );
	}
template <class TYPE>
	inline const TYPE* GetRPGStats( const char *pszName ) { return GetRPGStats<TYPE>( GetSingleton<IObjectsDB>(), pszName ); }
template <class TYPE>
	inline const TYPE* GetAddStats( IObjectsDB *pGDB, const char *pszName, IObjectsDB::EAddStatsType type )
	{
		const IGDBObject *pStats = pGDB->GetAddStats( pszName, type );
		NI_ASSERT_SLOW_TF( dynamic_cast<const TYPE*>(pStats) != 0, NStr::Format("Add stats for \"%s\" is not a right class", pszName), return 0 );
		return static_cast<const TYPE*>( pStats );
	}
template <class TYPE>
	inline const TYPE* GetAddStats( const char *pszName, IObjectsDB::EAddStatsType type ) { return GetAddStats<TYPE>( GetSingleton<IObjectsDB>(), pszName, type ); }
template <class TYPE>
	inline const TYPE* GetGameStats( IObjectsDB *pGDB, const char *pszName, IObjectsDB::EGameStatsType type )
	{
		const IGDBObject *pStats = pGDB->GetGameStats( pszName, type );
		if ( pStats == 0 )
			return 0;
		NI_ASSERT_SLOW_TF( dynamic_cast<const TYPE*>(pStats) != 0, NStr::Format("Game stats for \"%s\" is not a right class", pszName), return 0 );
		return static_cast<const TYPE*>( pStats );
	}
template <class TYPE>
	inline const TYPE* GetGameStats( const char *pszName, IObjectsDB::EGameStatsType type ) { return GetGameStats<TYPE>( GetSingleton<IObjectsDB>(), pszName, type ); }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GAMEDB_H__
