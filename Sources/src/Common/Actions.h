#ifndef __ACTIONS_H__
#define __ACTIONS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Main\RPGStats.h"
#include "..\AILogic\AITypes.h"
#include "..\Anim\Animation.h"
#include "..\StreamIO\StreamIOHelper.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** notify structures
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EActionNotify
{
	// ( EActionNotify & 1 ) == 1 => идут в UpdateActions
	// ( EActionNotify & 1 ) == 0 => разбрасываются по различным функциям

	// actions
	ACTION_NOTIFY_IDLE								= 0x001,
	ACTION_NOTIFY_MOVE								= 0x011,
	ACTION_NOTIFY_PLACEMENT						= 0x000,
	ACTION_NOTIFY_ST_OBJ_PLACEMENT		= 0x010,

	ACTION_NOTIFY_AIM									= 0x021,
	ACTION_NOTIFY_MECH_SHOOT					= 0x020,
	ACTION_NOTIFY_INFANTRY_SHOOT			= 0x170,

	ACTION_NOTIFY_RPG_CHANGED					= 0x030,
	ACTION_NOTIFY_DIE									= 0x0e1, // умер, update на исчезновение не будет

	// Specific soldier actions
	ACTION_NOTIFY_CRAWL								= 0x031,
	ACTION_NOTIFY_IDLE_LYING					= 0x041,
	ACTION_NOTIFY_AIM_LYING						= 0x051,
	ACTION_NOTIFY_SHOOT_LYING					= 0x050, // только солдаты такое могут
	ACTION_NOTIFY_THROW								= 0x060,
	ACTION_NOTIFY_DIE_LYING						= 0x061, // умер, update на исчезновение не будет

	ACTION_NOTIFY_IDLE_TRENCH					= 0x071,
	ACTION_NOTIFY_AIM_TRENCH					= 0x081,
	ACTION_NOTIFY_SHOOT_TRENCH				= 0x070, // только солдаты такое могут
	ACTION_NOTIFY_THROW_TRENCH				= 0x080, // только солдаты такое могут
	ACTION_NOTIFY_DIE_TRENCH					= 0x090, // умер, update на исчезновение не будет

	ACTION_NOTIFY_SHOOT_BUILDING			=	0x0a0, // только солдаты такое могут
	ACTION_NOTIFY_THROW_BUILDING			= 0x160, // только солдаты такое могут
	ACTION_NOTIFY_DIE_BUILDING				=	0x0b0, // умер, update на исчезновение не будет

	ACTION_NOTIFY_IDLE_TRANSPORT			= 0x0c1,
	ACTION_NOTIFY_DIE_TRANSPORT				= 0x0d1, // умер, update на исчезновение не будет

	// Specific cannonry actions
	ACTION_NOTIFY_INSTALL_ROTATE			= 0x0181,
	ACTION_NOTIFY_UNINSTALL_ROTATE		= 0x0191,
	ACTION_NOTIFY_INSTALL_TRANSPORT		= 0x01a1,
	ACTION_NOTIFY_UNINSTALL_TRANSPORT = 0x01b1,

	// CRAP{ unnecessary, need to be deleted
	ACTION_NOTIFY_INSTALL							= 0x0f1,
	ACTION_NOTIFY_UNINSTALL						= 0x101,
	// CRAP}
	ACTION_NOTIFY_HIT									= 0x0f0,

	//
	ACTION_NOTIFY_NEW_PROJECTILE			= 0x100,
	ACTION_NOTIFY_DEAD_PROJECTILE			= 0x110,
	ACTION_NOTIFY_NEW_ST_OBJ					=	0x120,
	ACTION_NOTIFY_DELETED_ST_OBJ			= 0x130,
	ACTION_NOTIFY_NEW_UNIT						= 0x140,
	ACTION_NOTIFY_DISSAPEAR_UNIT			= 0x150,
	ACTION_NOTIFY_NEW_ENTRENCHMENT		= 0x1a0,
	ACTION_NOTIFY_NEW_FORMATION				= 0x1b0,

	ACTION_NOTIFY_ENTRANCE_STATE			= 0x190,	// войти куда-либо
	// всяческие support-действия типа постановка/снятие мин, ремонт/перезарядка техники и т.д.
	ACTION_NOTIFY_USE_UP							= 0x091,
	ACTION_NOTIFY_USE_DOWN						= 0x0a1,

	ACTION_NOTIFY_STOP								= 0x0b1,
	ACTION_NOTIFY_NEW_BRIDGE_SPAN			= 0x1c0,
	ACTION_NOTIFY_REVEAL_ARTILLERY		= 0x1d0,
	ACTION_NOTIFY_SET_CAMOUFLAGE			= 0x111,
	ACTION_NOTIFY_REMOVE_CAMOUFLAGE		= 0x121,
	ACTION_NOTIFY_SET_AMBUSH					= 0x131,
	ACTION_NOTIFY_REMOVE_AMBUSH				= 0x141,
	ACTION_NOTIFY_BREAK_TRACK					= 0x151,
	ACTION_NOTIFY_REPAIR_TRACK				= 0x161,
	ACTION_NOTIFY_UPDATE_DIPLOMACY		= 0x1e0,
	ACTION_NOTIFY_USE_SPYGLASS				= 0x171,

	ACTION_NOTIFY_REPAIR_STATE_BEGIN	=	0x181,
	ACTION_NOTIFY_REPAIR_STATE_END		=	0x191,
	ACTION_NOTIFY_RESUPPLY_STATE_BEGIN=	0x201,
	ACTION_NOTIFY_RESUPPLY_STATE_END	=	0x211,

	ACTION_NOTIFY_SHOOT_AREA					= 0x1f0,

	ACTION_NOTIFY_RANGE_AREA					= 0x200,

	ACTION_NOTIFY_TURRET_HOR_TURN			= 0x0e0,
	ACTION_NOTIFY_TURRET_VERT_TURN		= 0x220,

	ACTION_NOTIFY_DEAD_UNIT						= 0x230,

	ACTION_NOTIFY_SELECTABLE_ON				= 0x241,
	ACTION_NOTIFY_SELECTABLE_OFF			= 0x251,
	ACTION_NOTIFY_ANIMATION_CHANGED		= 0x250,

	ACTION_NOTIFY_SELECTABLE_CHANGED	= 0x261,
	
	ACTION_NOTIFY_INSTALL_MOVE				= 0x271,
	ACTION_NOTIFY_UNINSTALL_MOVE			= 0x281,
	
	ACTION_NOTIFY_CHANGE_VISIBILITY		= 0x301,
	//
	ACTION_NOTIFY_DELAYED_SHOOT				= 0x291,
	ACTION_NOTIFY_STATE_CHANGED				= 0x311,
	ACTION_NOTIFY_SILENT_DEATH				= 0x321,
	ACTION_NOTIFY_SERVED_ARTILLERY		= 0x331,

	ACTION_NOTIFY_OPEN_PARASHUTE			=	0x341,
	ACTION_NOTIFY_PARASHUTE						=	0x351,
	ACTION_NOTIFY_CLOSE_PARASHUTE			=	0x361,
	ACTION_NOTIFY_CHANGE_DBID					= 0x371,
	ACTION_NOTIFY_CHANGE_FRAME_INDEX	= 0x381,
	ACTION_NOTIFY_FALLING							= 0x391,

	ACTION_NOTIFY_STORAGE_CONNECTED   = 0x3a1,
	ACTION_NOTIFY_START_BUILD_PIT     = 0x3b1,
	ACTION_NOTIFY_START_LEAVE_PIT			= 0x3c1,
	ACTION_NOTIFY_THROW_LYING					= 0x260,

	ACTION_NOTIFY_STAYING_TO_LYING		= 0x3d1,
	ACTION_NOTIFY_LYING_TO_STAYING		= 0x3e1,

	ACTION_NOTIFY_SHELLTYPE_CHANGED   = 0x3f1,
	ACTION_NOTIFY_DEADPLANE				    = 0x401,
	ACTION_NOTIFY_LEVELUP							= 0x411,
	
	ACTION_NOTIFY_CHANGE_SELECTION		= 0x421,		// nParam == 1 - select specified unit, nParam == 0 - deselect specified unit
	
	ACTION_NOTIFY_SIDE_CHANGED				= 0x431,		// player belogness changed. nParam - new player index
	ACTION_NOTIFY_PRE_DISAPPEAR				= 0x441,
	
	ACTION_NOTIFY_ENTRENCHMENT_STARTED = 0x451,

	ACTION_NOTIFY_SELECT_CHECKED		  = 0x461,		
	ACTION_SET_SELECTION_GROUP		    = 0x471,		
	//ACTION_NOTIFY_RU_STORAGE_AREA			= 0x210,
	ACTION_NOTIFY_CHANGE_SCENARIO_INDEX = 0x481,
	// нужен только для отложенных updates
	ACTION_NOTIFY_GET_DEAD_UNITS_UPDATE = 0xfffffffe,

	ACTION_NOTIFY_NONE								= 0xffffffff,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMovingType
{
	MOVE_TYPE_MOVE = 0,										// на самом деле либо Move либо Turn
	MOVE_TYPE_DIVE = 1,										//for dive bombers
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsDyingAction( const EActionNotify eAction )
{
	return
		eAction == ACTION_NOTIFY_DIE ||
		eAction == ACTION_NOTIFY_DIE_LYING ||
		eAction == ACTION_NOTIFY_DIE_TRENCH ||
		eAction == ACTION_NOTIFY_DIE_BUILDING ||
		eAction == ACTION_NOTIFY_DIE_TRANSPORT;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSuspendedUpdate
{
	IRefCount *pObj;											// subject to change

	SSuspendedUpdate() : pObj( 0 ) { }
	SSuspendedUpdate( IRefCount *_pObj ) : pObj( _pObj ) { }

	virtual void Recall( IDataStream *pStream ) = 0;
	virtual void Pack( IDataStream *pStream ) const = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// animation action update
struct SAINotifyAction : public SSuspendedUpdate
{
	WORD typeID;															// action type
	int nParam;

	NTimer::STime time;

	//
	SAINotifyAction() : typeID( 0 ), nParam( -1 ), time() { }
	SAINotifyAction( const BYTE _typeID, IRefCount *pObj ) 
		: SSuspendedUpdate( pObj ), typeID( _typeID ), nParam( -1 ), time() { }

	virtual void Pack( IDataStream *pStream ) const
	{
		CStreamAccessor stream( pStream );
		stream << typeID << time << nParam;
	}
	
	virtual void Recall( IDataStream *pStream )
	{
		CStreamAccessor stream( pStream );
		stream >> typeID >> time >> nParam;
	}
};

struct SAINotifyDeadAtAll : public SSuspendedUpdate
{
	bool bRot;															// true - если потом придёт update на исчезновение

	//
	virtual void Pack( IDataStream *pStream ) const
	{
		CStreamAccessor stream( pStream );
		stream << bRot;
	}
	
	virtual void Recall( IDataStream *pStream )
	{
		CStreamAccessor stream( pStream );
		stream >> bRot;
	}
};

// RPG stats (hit points for now) update
struct SAINotifyRPGStats : public SSuspendedUpdate
{
	float fHitPoints;											// hit points
	float fMorale;
	int nMainAmmo, nSecondaryAmmo;				// патроны главной пушки и всего остального
	NTimer::STime time;

	//
	SAINotifyRPGStats() : fHitPoints( 0.0f ), fMorale( 0.0f ), nMainAmmo( 0 ), nSecondaryAmmo( 0 ), time() { }
	SAINotifyRPGStats( IRefCount *pObj, const float _fHitPoints ) 
		: SSuspendedUpdate( pObj ), fHitPoints( _fHitPoints ), fMorale( 0.0f ), nMainAmmo( 0 ), nSecondaryAmmo( 0 ), time() { }

	virtual void Pack( IDataStream *pStream ) const
	{
		CStreamAccessor stream( pStream );
		stream << fHitPoints << fMorale << nMainAmmo << nSecondaryAmmo << time;
	}
	
	virtual void Recall( IDataStream *pStream )
	{
		CStreamAccessor stream( pStream );
		stream >> fHitPoints >> fMorale >> nMainAmmo >> nSecondaryAmmo >> time;
	}
};

struct SAINotifyDiplomacy : public SSuspendedUpdate
{
	EDiplomacyInfo eDiplomacy;
	int nPlayer;

	//
	SAINotifyDiplomacy() : eDiplomacy( EDI_ENEMY ), nPlayer( 0 ) { }
	SAINotifyDiplomacy( EDiplomacyInfo _eDiplomacy, IRefCount *pObj, const int _nPlayer ) 
		: SSuspendedUpdate( pObj ), eDiplomacy( _eDiplomacy ), nPlayer( _nPlayer ) { }

	virtual void Pack( IDataStream *pStream ) const
	{
		CStreamAccessor stream( pStream );
		stream << eDiplomacy << nPlayer;
	}
	
	virtual void Recall( IDataStream *pStream )
	{
		CStreamAccessor stream( pStream );
		stream >> eDiplomacy >> nPlayer;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// update на вход/выход, юниты сначала входят не в стрелковую ячейку
struct SAINotifyEntranceState
{
	IRefCount *pInfantry;										// кто входит
	IRefCount *pTarget;											// куда входит
	bool bEnter;														// true - входит, false - выходит

	SAINotifyEntranceState() : pInfantry( 0 ), pTarget( 0 ), bEnter( false ) { }
	SAINotifyEntranceState( IRefCount *_pInfantry, IRefCount *_pTarget, const bool _bEnter ) : pInfantry( _pInfantry ), pTarget( _pTarget ), bEnter( _bEnter ) { }
};
// placement update
struct SAINotifyPlacement : public SSuspendedUpdate
{
	CVec2 center;													// (x, y)
	float z;															// height (mostly for planes)
	WORD dir;															// direction [0..65535) => [0..2pi), only for units
	DWORD dwNormal;												// нормаль
	float fSpeed;
	BYTE cSoil;														// параметры почвы: дым из-под колёс, следы и т.д.

	SAINotifyPlacement() : center( VNULL2 ), z( 0.0f ), dir( 0 ), dwNormal( 0 ), fSpeed( 0.0f ), cSoil( 0 ) { }
	SAINotifyPlacement(	IRefCount *pObj, const CVec2 &_center, const short _z, const WORD _dir, const float _fSpeed )
		: SSuspendedUpdate( pObj ), center( _center ), z( _z ), dir( _dir ), dwNormal( 0 ), fSpeed( _fSpeed ), cSoil( 0 ) { }

	virtual void Pack( IDataStream *pStream ) const
	{
		CStreamAccessor stream( pStream );
		stream << center.x << center.y << z << dir;
	}
	
	virtual void Recall( IDataStream *pStream )
	{
		CStreamAccessor stream( pStream );
		stream >> center.x >> center.y >> z >> dir;
	}

	// для использования в AILogic
	virtual int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &center );
		saver.Add( 2, &z );
		saver.Add( 3, &dir );
		saver.Add( 4, &dwNormal );
		saver.Add( 5, &fSpeed );
		saver.Add( 6, &cSoil );

		return 0;
	}
};
// information about new unit
struct SNewUnitInfo : public SAINotifyPlacement
{
	// placement of a unit
	float fResize;												//for resizing the object
	float fHitPoints;
	float fMorale;

	WORD dbID;														// database ID of a unit
	EDiplomacyInfo eDipl;									// diplomacy settings
	int nPlayer;
	int nFrameIndex;											// frame index for static objects
//
	SNewUnitInfo() : fResize( 0.0f ), fHitPoints( 0.0f ), fMorale( 0.0f ), dbID( 0 ), eDipl( EDI_ENEMY ), nPlayer( 0 ), nFrameIndex( 0 ) {  }

	virtual void Pack( IDataStream *pStream ) const
	{
		SAINotifyPlacement::Pack( pStream );		
		CStreamAccessor stream( pStream );
		stream << nFrameIndex << dbID << fResize << eDipl << fHitPoints << fMorale << nPlayer;
	}
	
	virtual void Recall( IDataStream *pStream )
	{
		SAINotifyPlacement::Recall( pStream );
		CStreamAccessor stream( pStream );
		stream >> nFrameIndex >> dbID >> fResize >> eDipl >> fHitPoints >> fMorale >> nPlayer;
	}
};

// hit update
struct SAINotifyHitInfo
{
	const SWeaponRPGStats *pWeapon;					// weapon shell was fired
	WORD wShell;														// shell index in the weapon
	WORD wDir;															// direction hit was from

	enum EHitType { EHT_NONE, EHT_HIT, EHT_MISS, EHT_REFLECT, EHT_GROUND, EHT_WATER, EHT_AIR };
	EHitType eHitType;											// тип попадани

	IRefCount *pVictim;											// if unit was hit
	CVec3 explCoord;

	SAINotifyHitInfo() : pWeapon( 0 ), wShell( 0 ), wDir( 0 ), eHitType( EHT_NONE ), pVictim( 0 ), explCoord( VNULL3 ) { }
	SAINotifyHitInfo( const SWeaponRPGStats *_pWeapon, const WORD _wShell, const WORD &_wDir, IRefCount *_pVictim, const CVec3 _explCoord )
		: pWeapon( _pWeapon ), wShell( _wShell ), wDir( _wDir ), eHitType( EHT_NONE ), pVictim( _pVictim ), explCoord( _explCoord ) { }

	SAINotifyHitInfo( const SWeaponRPGStats *_pWeapon, const WORD _wShell, const WORD &_wDir, const CVec3 _explCoord )
		: pWeapon( _pWeapon ), wShell( _wShell ), wDir( _wDir ), eHitType( EHT_NONE ), pVictim( 0 ), explCoord( _explCoord ) { }
};
// aiming: turret turning update
struct SAINotifyTurretTurn
{
	IRefCount *pObj;											// object turret belong to
	DWORD	nModelPart;											// turned bodypart
	WORD wAngle;													// final angle
	NTimer::STime endTime;								// final time

	SAINotifyTurretTurn() : pObj( 0 ), nModelPart( 0 ), wAngle( 0 ), endTime() { }
	SAINotifyTurretTurn( IRefCount *_pObj, const int &_nModelPart, const WORD &_wAngle, const NTimer::STime &_endTime )
		: pObj( _pObj ), nModelPart( _nModelPart ), wAngle( _wAngle ), endTime( _endTime ) { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAINotifyBaseShot
{
	int typeID;														// shot type
	IRefCount *pObj;											// юнит, который стрелял либо объект, из которого он стрелял
	BYTE cShell;													// shell number
	NTimer::STime time;										// time, this shot was...
	CVec3 vDestPos;												// destination point of this shot
	//
	SAINotifyBaseShot() : typeID( 0 ), pObj( 0 ), cShell( 0 ), time(), vDestPos( VNULL3 ) {  }
	SAINotifyBaseShot( const BYTE _typeID, IRefCount *_pObj, const BYTE _cShell, const NTimer::STime &_time, const CVec3 &_vDestPos )
		: typeID( _typeID ), pObj( _pObj ), cShell( _cShell ), time( _time ), vDestPos( _vDestPos ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAINotifyMechShot : public SAINotifyBaseShot
{
	BYTE cGun;														// gun number
	//
	SAINotifyMechShot() : cGun( 0 ) { }
	SAINotifyMechShot( const BYTE _typeID, IRefCount *_pObj, const BYTE _cGun, const BYTE _cShell, const NTimer::STime &_time, const CVec3 &_vDestPos )
		: SAINotifyBaseShot( _typeID, _pObj, _cShell, _time, _vDestPos ), cGun( _cGun ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAINotifyInfantryShot : public SAINotifyBaseShot
{
	const SWeaponRPGStats *pWeapon;				// оружие
	// если nSlot >= 0, то стрельбы из объекта, nSlot == -1, то стрельба в открытую
	short int nSlot;											// номер слота
	//
	SAINotifyInfantryShot() : pWeapon( 0 ), nSlot( -1 ) {  }
	SAINotifyInfantryShot( const BYTE _typeID, IRefCount *_pObj, const short int _nSlot, const BYTE _cShell, const NTimer::STime &_time, const CVec3 &_vDestPos )
		: SAINotifyBaseShot( _typeID, _pObj, _cShell, _time, _vDestPos ), nSlot( _nSlot ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAINotifySelections
{
	IRefCount *pObj;
	bool bSelectable;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAINotifyNewProjectile
{
	IRefCount *pObj;

	IRefCount *pSource;
	int nGun;
	int nShell;

	// время полёта снаряда
	NTimer::STime flyingTime;
	// время запуска снаряда
	NTimer::STime startTime;

	SAINotifyNewProjectile() : pObj( 0 ), pSource( 0 ), nGun( 0 ), nShell( 0 ), flyingTime(), startTime() { }
	SAINotifyNewProjectile( IRefCount *_pObj, IRefCount *_pSource, const int _nGun, const int _nShell, const NTimer::STime _flyingTime )
		: pObj( _pObj ), pSource( _pSource ), nGun( _nGun ), nShell( _nShell ), flyingTime( _flyingTime ), startTime() { }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** command action
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// общее правило:
//	команды, которые посылаются от пользователя, нумеруются от 0 до 999
//	команды, которые посылаются внутри AI нумеруются от 1000 до 32767 и начинаются на 4 таба правее (для того, чтобы было легко отличать)

// при добавлении новой пользовательской команды определить её pointtogo в GetGoPointByCommand дл
// корректной отдачи приказов формациям в окопах и зданиях

// если в команде выставлен старший бит 0x8000, то это self-action

// значения должны быть не больше, чем 65535!
enum EActionCommand
{
	ACTION_COMMAND_MOVE_TO					= 0,		// move to location
	ACTION_COMMAND_ATTACK_UNIT			= 1,		// attack unit
	ACTION_COMMAND_ATTACK_OBJECT		= 2,		// attack non-unit object
	ACTION_COMMAND_SWARM_TO					= 3,		// swarm to point (x, y)
	ACTION_COMMAND_LOAD							= 4,		// load units / attach for towing
	ACTION_COMMAND_UNLOAD						= 5,		// unload units / detach from towing
	ACTION_COMMAND_ENTER						= 6,		// enter to building/trench
	ACTION_COMMAND_LEAVE						= 7,		// leave building/trench
	ACTION_COMMAND_ROTATE_TO				= 8,		// rotate to point
	ACTION_COMMAND_STOP							= 9,		// stop all actions
	ACTION_COMMAND_PARADE						= 10,		// выстроиться в формацию
	//
	ACTION_COMMAND_DIE											= 1000,	// (AI only)
	//
	ACTION_COMMAND_LOAD_NOW									= 1001,	// (AI only)
	ACTION_COMMAND_UNLOAD_NOW								= 1002,	// (AI only)
	ACTION_COMMAND_LEAVE_NOW								= 1003,	// (AI only)
	//
	ACTION_MOVE_BY_DIR											= 1004,	// (AI only)
	ACTION_MOVE_BY_OWN_DIR									= 1005,	// (AI only)
	ACTION_COMMAND_WAIT_FOR_UNITS						= 1006,	// (AI only)
	ACTION_COMMAND_DISAPPEAR								= 1007,	// (AI only)
	ACTION_COMMAND_IDLE_TRENCH							= 1008,	// (AI only)
	ACTION_COMMAND_IDLE_BUILDING						= 1009,	// (AI only)
	ACTION_COMMAND_ENTER_TRANSPORT_NOW			= 1010,// (AI only)
	ACTION_COMMAND_IDLE_TRANSPORT						=	1011,	// (AI only)
	//
	ACTION_COMMAND_PLACEMINE				= 11,
	ACTION_COMMAND_CLEARMINE				= 12,
	ACTION_COMMAND_CLEARMINE_RADIUS					= 1012,	// (AI only)
	ACTION_COMMAND_PLACEMINE_NOW						= 1013,	// (AI only)
	ACTION_COMMAND_GUARD						= 13,
	ACTION_COMMAND_AMBUSH						= 14,

	ACTION_COMMAND_STOP_THIS_ACTION					= 1014,	// (AI only)

	ACTION_COMMAND_RANGE_AREA				= 15,
	ACTION_COMMAND_ART_BOMBARDMENT	= 16,
	ACTION_COMMAND_INSTALL					= 17,
	ACTION_COMMAND_UNINSTALL				= 18,
	//
	ACTION_COMMAND_CALL_BOMBERS			= 19,
	// fighters commands
	ACTION_COMMAND_CALL_FIGHTERS		= 20,
	ACTION_MOVE_FIGHTER_PATROL							= 1015,	// (AI only)
	//scouts
	ACTION_COMMAND_CALL_SCOUT				= 21,
	//
	ACTION_MOVE_PLANE_LEAVE									= 1017,	// (AI only)
	//drop paratroopers
	ACTION_COMMAND_PARADROP					= 22,		// call for paradropers
	ACTION_MOVE_DROP_PARATROOPERS						= 1018,	// (AI only)
	ACTION_MOVE_PARACHUTE										= 1019,	// (AI only)
	//
	ACTION_COMMAND_RESUPPLY					= 23,
	ACTION_MOVE_RESUPPLY_UNIT								= 1020,	// (AI only)

	ACTION_COMMAND_REPAIR						= 24,
	ACTION_MOVE_REPAIR_UNIT									= 1021,	// (AI only)

	ACTION_MOVE_SET_HOME_TRANSPORT					= 1022,	// (AI only)

	ACTION_MOVE_CATCH_TRANSPORT							= 1023,	// (AI only)

	ACTION_COMMAND_BUILD_FENCE_BEGIN= 26,
	ACTION_COMMAND_BUILD_FENCE_END					= 1024,	// (AI only)

	ACTION_COMMAND_ENTRENCH_BEGIN		=	27,
	ACTION_COMMAND_ENTRENCH_END							=	1025,	// (AI only)

	///ACTION_COMMAND_CATCH_ARTILLERY	=	28,		// assign gunners to artillery
	ACTION_MOVE_GUNSERVE										= 1026,	// (AI only)

	ACTION_COMMAND_USE_SPYGLASS			= 29,		// officer command - use spyglasses


	ACTION_MOVE_ATTACK_FORMATION						= 1027,	// (AI only)

	ACTION_COMMAND_TAKE_ARTILLERY		= 31,
	ACTION_COMMAND_DEPLOY_ARTILLERY	= 32,
	ACTION_MOVE_BEING_TOWED									= 1028,	// (AI only)
	ACTION_MOVE_LOAD_RU											= 1029,	// (AI only)

	ACTION_MOVE_LEAVE_TANK_PIT							= 1030,	// (AI only)

	ACTION_MOVE_SHTURMOVIK_PATROL						= 1031,	// (AI only)

	ACTION_MOVE_IDLE												= 1032,	// (AI only)
	ACTION_COMMAND_PLACE_ANTITANK		= 33,		// place anti-tank
	ACTION_COMMAND_DISBAND_FORMATION= 34,		// disband squad
	ACTION_COMMAND_FORM_FORMATION		= 35,		// form squad (after disbanding)

	ACTION_COMMAND_WAIT_TO_FORM							= 1033, // (AI only) - ждать, пока другие юниты не придут в нужное состояние, чтобы организовать формацию

	ACTION_COMMAND_CALL_SHTURMOVIKS		= 36,

	ACTION_COMMAND_SNEAK_ON									= 1034,
	ACTION_COMMAND_SNEAK_OFF								= 1035,

	ACTION_COMMAND_FOLLOW							= 39,
	ACTION_COMMAND_FOLLOW_NOW								= 1037, // (AI only) ехать за ведущим
	
	ACTION_COMMAND_PLANE_ADD_POINT		= 41,						// before plane leave target points are added
	ACTION_COMMAND_PLANE_TAKEOFF_NOW	= 42,					// commands plane to take off
	
	ACTION_COMMAND_CATCH_FORMATION					= 1038, // (AI only) присоединиться к формации
	ACTION_MOVE_PLANE_SCOUT_POINT						= 1039,	//(AI only) recon plane start scouting

	ACTION_COMMAND_RESUPPLY_HR				= 43,
	
	ACTION_COMMAND_SWARM_ATTACK_UNIT				= 1040, // (AI only) атака с самостоятельным перевыбором цели (например, при swarm)
	ACTION_MOVE_SWARM_ATTACK_FORMATION			= 1041, // (AI only) атака с самостоятельным перевыбором цели (например, при swarm)
//	ACTION_COMMAND_FIRE_MORALE_SHELL	= 44,		//выстрел моральным снарядом

	ACTION_COMMAND_ENTRENCH_SELF			= 45,
//	ACTION_COMMAND_ENTRENCH_UNIT		= 25,
//	ACTION_COMMAND_ENTER_TANKPIT		= 40,
	ACTION_COMMAND_SWARM_ATTACK_OBJECT			= 1042,
	ACTION_MOVE_PLANE_BOMB_POINT						= 1043,
	ACTION_COMMAND_CHANGE_SHELLTYPE		= 46,

//	ACTION_COMMAND_CREATE_RU_STORAGE= 30,

	ACTION_MOVE_BUILD_LONGOBJECT						= 1044,
	ACTION_MOVE_FLY_DEAD										= 1045,
	ACTION_COMMAND_REPEAR_OBJECT			= 47,
	ACTION_COMMAND_BUILD_BRIDGE				= 48,

	ACTION_MOVE_REPAIR_BRIDGE								= 1046,	// (AI only) engineers repair bridge
	ACTION_MOVE_CLEARMINE										= 1047,	// (AI only) for engineers
	ACTION_MOVE_PLACEMINE										= 1048,	// (AI only) for engineers
	ACTION_MOVE_PLACE_ANTITANK							= 1049,	// (AI only) for engineers
	ACTION_MOVE_REPAIR_BUILDING							= 1050,	// (AI only) for engineers

	
	ACTION_COMMAND_RESUPPLY_MORALE		= 49,					// car with this command gives morale support to units
	ACTION_COMMAND_STAND_GROUND				= 50,
	ACTION_MOVE_WAIT_FOR_TRUCKREPAIR				= 1051, // (AI only) for waiting something during swarm

	ACTION_SET_APPEAR_POINT									= 1052, // (AI only) AVIATION REMEMBER CURRENT APPEAR POINT
	ACTION_COMMAND_HEAL_INFANTRY			= 51,					

	ACTION_MOVE_TO_NOT_PRESIZE  						= 1053, // (AI general only)
	ACTION_COMMAND_PLACE_MARKER							= 1054, // (AI only) service command, client only

	ACTION_COMMAND_CHANGE_MOVEMENT		= 53,					// change movement order - move to point or mode parallel
	
	ACTION_COMMAND_ROTATE_TO_DIR						= 1055,	// повернуться в направлении, vPos задаёт вектор направления
	ACTION_COMMAND_USE											= 1056,
	ACTION_MOVE_ENTER_TRANSPORT_CHEAT_PATH	= 1057, // enter transport ignore locked tiles

	ACTION_COMMAND_ENTER_BUILDING_NOW				= 1058,
	ACTION_COMMAND_ENTER_ENTREHCMNENT_NOW		= 1059,
	
	ACTION_COMMAND_FILL_RU						= 54,
	ACTION_COMMAND_MOVE_TO_GRID				= 55,
	ACTION_COMMAND_CATCH_ARTILLERY		= 56,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIUnitCmd
{
	EActionCommand cmdType;								// command type
	CVec2 vPos;														// for ground pointing commands
	CPtr<IRefCount> pObject;							// for object pointing commands
	bool fromExplosion;										// for death from explosion
	// команда ACTION_COMMAND_CALL_BOMBERS - число бомберов
	// команда ACTION_COMMAND_ENTER:		 0 - войти в здание, 1 - войти в окоп
	// команда ACTION_COMMAND_ATTACK_OBJECT: 0 - атаковать не окоп, 1 - атаковать окоп
	float fNumber;
	bool bFromAI;			// если true, то команда пришла от клиента или от генерала
	//
	SAIUnitCmd() : cmdType( ACTION_COMMAND_MOVE_TO ), vPos( -1, -1 ), fromExplosion( false ), fNumber( 0 ), bFromAI( true ) { }
	explicit SAIUnitCmd( const EActionCommand &_cmdType )
		: cmdType( _cmdType ), vPos( -1, -1 ), fNumber( 0 ), fromExplosion( false ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, IRefCount *_pObject )
		: cmdType( _cmdType ), pObject( _pObject ), vPos( -1, -1 ), fNumber( 0 ), fromExplosion( false ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, IRefCount *_pObject, const float _fNumber )
		: cmdType( _cmdType ), pObject( _pObject ), fNumber( _fNumber ), vPos( -1, -1 ), fromExplosion( false ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, const CVec2 &_vPos )
		: cmdType( _cmdType ), vPos( _vPos ), fNumber( 0 ), fromExplosion( false ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, const float x, const float y )
		: cmdType( _cmdType ), vPos( x, y ), fNumber( 0 ), fromExplosion( false ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, const CVec2 &_vPos, const float _fNumber )
		: cmdType( _cmdType ), vPos( _vPos ), fNumber( _fNumber ), fromExplosion( false ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, const float x, const float y, const float _fNumber )
		: cmdType( _cmdType ), vPos( x, y ), fNumber( _fNumber ), fromExplosion( false ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, const float _fNumber )
		: cmdType( _cmdType ), fNumber( _fNumber ), vPos( -1, -1 ), fromExplosion( false ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, bool _fromExpl )
		: cmdType( _cmdType ), fromExplosion( _fromExpl ), vPos( -1, -1 ), fNumber( 0 ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, const float _fNumber, bool _fromExpl )
		: cmdType( _cmdType ), fromExplosion( _fromExpl ), vPos( -1, -1 ), fNumber( _fNumber ), bFromAI(true) { }
	SAIUnitCmd( const EActionCommand &_cmdType, const CVec2 &_vPos, const float _fNumber, const bool _fromExplosion )
		: cmdType( _cmdType ), vPos( _vPos ), fNumber( _fNumber ), fromExplosion( _fromExplosion ), bFromAI(true) { }
	//
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 2, &cmdType );
		saver.Add( 3, &pObject );
		saver.Add( 4, &vPos );
		saver.Add( 6, &fromExplosion );
		saver.Add( 7, &fNumber );
		saver.Add( 8, &bFromAI );

		return 0;
	}
	
	// pObject не сэйвится!
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;

		saver.Add( "Command", &cmdType );
		saver.Add( "Position", &vPos );
//		CPtr<IRefCount> pObject;
		saver.Add( "FromExplosionFlag", &fromExplosion );
		saver.Add( "NumberFlag", &fNumber );
		saver.Add( "FromAIFlag", &bFromAI );

		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// user input actions
// to achive RESET_USER_ACTION_xx one must bitwise OR correspending user action with 0x00000100
enum EUserAction
{
	USER_ACTION_UNKNOWN		= 0,
	//
	USER_ACTION_MOVE			= 1,						// go to point...
	USER_ACTION_ATTACK		= 2,						// attack object (unit/building/etc.)
	USER_ACTION_SWARM			= 3,						// swarm to point
	USER_ACTION_BOARD			= 4,						// board building/entrenchment/transport
	USER_ACTION_LEAVE			= 5,						// leave building/entrenchment/transport
	USER_ACTION_ROTATE		= 6,						// rotate to point
	USER_ACTION_INSTALL		= 7,						// install weapon
	USER_ACTION_UNINSTALL = 8,						// uninstall weapon
	USER_ACTION_GUARD			= 9,						// assign guard logic
	USER_ACTION_AMBUSH		= 10,						// assign ambush logic
	USER_ACTION_FORMATION	= 11,						// change formation
	USER_ACTION_RANGING		= 12,						// пристрелка области
	USER_ACTION_SUPPRESS	= 13,						// огонь на подавление (suppressing fire)
	USER_ACTION_FOLLOW		= 14,						// следовать за
	USER_ACTION_CHANGE_SHELL	= 15,				// change shell to fire
	USER_ACTION_ENTRENCH_SELF	= 16,				// entrench himself
	USER_ACTION_STAND_GROUND	= 17,				// stand ground - defence
	USER_ACTION_MOVE_TO_GRID	= 18,				// move to grid...
	USER_ACTION_CAPTURE_ARTILLERY = 19,		// squad: set some squad members to serve gun.
	// engineer special actions
	USER_ACTION_ENGINEER_PLACE_MINE_AP	= 20,	// place anti-personnel mines
	USER_ACTION_ENGINEER_PLACE_MINE_AT	= 21,	// place anti-tank mines
	USER_ACTION_ENGINEER_CLEAR_MINES		= 22,	// remove mines
	USER_ACTION_ENGINEER_BUILD_FENCE		= 23,	// build anti-personnel defence (wire fence)
	USER_ACTION_ENGINEER_BUILD_ENTRENCHMENT = 24,	// build entrenchment segment for personnel
	USER_ACTION_ENGINEER_REPAIR_BUILDING= 25,	// repair building...
	USER_ACTION_ENGINEER_REPAIR					= 26, // repair all damaged mech units
	//
	USER_ACTION_SUPPORT_RESUPPLY				= 27,	// resupply with ammo
	USER_ACTION_ENGINEER_BUILD_BRIDGE		= 28,	// build bridge
	USER_ACTION_ENGINEER_BUILD_ANTITANK	= 29,	// build anti-tank defence
	// officer special actions
	USER_ACTION_OFFICER_CALL_BOMBERS		= 30,	// call for bombers to bomb area
	USER_ACTION_OFFICER_CALL_FIGHTERS		= 31,	// call for fighter to defend area
	USER_ACTION_OFFICER_CALL_SPY				= 32,	// call spy plane to scout the area
	USER_ACTION_OFFICER_CALL_PARADROPERS= 33,	// call paradropper plane to drop paratroopers
	USER_ACTION_OFFICER_BINOCULARS			= 34,	// look at the binoculars
	USER_ACTION_OFFICER_CALL_GUNPLANES	= 35,	// call gunplane (штурмовик) to hunt in the area
	//
	USER_ACTION_HUMAN_RESUPPLY					= 36,	// resupply by humans
	USER_ACTION_FILL_RU									= 37,	// fill resource units
	// gunner special actions
	//USER_ACTION_GUNNER_ASSIGN_TO_GUN		= 40,	// assign gunners to gun
	USER_ACTION_HOOK_ARTILLERY					= 41,	// hook artillery for towing
	USER_ACTION_DEPLOY_ARTILLERY				= 42,	// deploy artillery after towing
	// formation changes
	// CRAP{ из-за чьих-то кривых ручёнок (не будем тыкать пальцем в Славика) нумерация формаций такая кривая
	USER_ACTION_FORMATION_0				= 50,		// change formation to '0'
	USER_ACTION_FORMATION_3				= 51,		// change formation to '1'
	USER_ACTION_FORMATION_2				= 52,		// change formation to '2'
	USER_ACTION_FORMATION_1				= 53,		// change formation to '3'
	USER_ACTION_FORMATION_4				= 54,		// change formation to '4'
	// CRAP}
	USER_ACTION_USE_SHELL_DAMAGE	= 55,		// use damage shell
	USER_ACTION_USE_SHELL_AGIT		= 56,		// use agitation shell
	USER_ACTION_USE_SHELL_SMOKE		= 57,		// use smoke shell
	//
	USER_ACTION_PLACE_MARKER			= 59,		// place marker on minimap
	USER_ACTION_CHANGE_MOVEMENT_ORDER	= 60,	// идти в точку или параллельно
	//
	USER_ACTION_DISBAND_SQUAD			= 61,		// disband squad
	USER_ACTION_FORM_SQUAD				= 62,		// form squad again (after disbanding)
	USER_ACTION_STOP							= 63,		// stop all actions
	// 
	USER_ACTION_CANCEL						= 100,	// cancel action
	USER_ACTION_SELECT_FRIEND			= 101,	// select friendly objects
	USER_ACTION_SELECT_NEUTRAL		= 102,	// select neutral objects
	USER_ACTION_SELECT_FOE				= 103,	// select foe objects
	//
	USER_ACTION_CALL_HQ						= 110,	// call to headquarter
	USER_ACTION_DO_SELFACTION			= 111,	// do self-action (specific for each unit in group! in accordance with priority)
	// special actions for user interface configuration
	USER_ACTION_CHANGE_ACTIONS_1	= 201,	// change actions (low DWORD)
	USER_ACTION_CHANGE_ACTIONS_2  = 202,	// change actions (high DWORD)
	USER_ACTION_CHANGE_ACTIONS_3	= 203,	// change actions (low DWORD)
	USER_ACTION_CHANGE_ACTIONS_4	= 204,	// change actions (high DWORD)

	USER_ACTION_ENABLE_PLANE_SCOUT				= 205,	// enable scout airplanes
	USER_ACTION_DISABLE_PLANE_SCOUT				= 206,	// disable scout airplanes
	USER_ACTION_ENABLE_PLANE_BOMBER				= 207,	// enable bomber airplanes
	USER_ACTION_DISABLE_PLANE_BOMBER			= 208,	// disable bomber airplanes
	USER_ACTION_ENABLE_PLANE_FIGHTER			= 209,	// enable fighter airplanes
	USER_ACTION_DISABLE_PLANE_FIGHTER			= 210,	// disable fighter airplanes
	USER_ACTION_ENABLE_PLANE_GUNPLANE			= 211,	// enable gunplane airplanes
	USER_ACTION_DISABLE_PLANE_GUNPLANE		= 212,	// disable gunplane airplanes
	USER_ACTION_ENABLE_PLANE_PARADROPER		= 213,	// enable paradroper airplanes
	USER_ACTION_DISABLE_PLANE_PARADROPER	= 214,	// disable paradroper airplanes
	USER_ACTION_DISABLE_PLANES						= 215,	// disable planes
	USER_ACTION_ENABLE_PLANES							= 216,	// enable planes

	USER_ACTION_HOURGLASS									= 222,	// hourglass - waiting for something

	USER_ACTION_CHOOSE_PLANE              = 524,

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline int GetAnimationFromAction( int nAction )
{
	switch ( nAction )
	{
		case ACTION_NOTIFY_IDLE:
			return ANIMATION_IDLE;
		case ACTION_NOTIFY_IDLE_LYING:
			return ANIMATION_IDLE_DOWN;
		case ACTION_NOTIFY_MOVE:
			return ANIMATION_MOVE;
		case ACTION_NOTIFY_CRAWL:
			return ANIMATION_CRAWL;
		case ACTION_NOTIFY_AIM:
			return ANIMATION_AIMING;
		case ACTION_NOTIFY_DIE:
			return ANIMATION_DEATH;
		case ACTION_NOTIFY_AIM_LYING:
			return ANIMATION_AIMING_DOWN;
		case ACTION_NOTIFY_DIE_LYING:
			return ANIMATION_DEATH_DOWN;
		case ACTION_NOTIFY_IDLE_TRENCH:
			return ANIMATION_IDLE;
		case ACTION_NOTIFY_AIM_TRENCH:
			return ANIMATION_AIMING_TRENCH;
		case ACTION_NOTIFY_THROW:
			return ANIMATION_THROW;
		case ACTION_NOTIFY_THROW_TRENCH:
			return ANIMATION_THROW_TRENCH;
		case ACTION_NOTIFY_MECH_SHOOT:
			return ANIMATION_SHOOT;
		case ACTION_NOTIFY_INFANTRY_SHOOT:
			return ANIMATION_SHOOT;
		case ACTION_NOTIFY_SHOOT_LYING:
			return ANIMATION_SHOOT_DOWN;
		case ACTION_NOTIFY_SHOOT_TRENCH:
			return ANIMATION_SHOOT_TRENCH;
		case ACTION_NOTIFY_USE_UP:
			return ANIMATION_USE;
		case ACTION_NOTIFY_USE_DOWN:
			return ANIMATION_USE_DOWN;
		case ACTION_NOTIFY_INSTALL:
			return ANIMATION_INSTALL;
		case ACTION_NOTIFY_UNINSTALL:
			return ANIMATION_UNINSTALL;
		case ACTION_NOTIFY_UNINSTALL_ROTATE:
			return ANIMATION_UNINSTALL_ROT;
		case ACTION_NOTIFY_INSTALL_ROTATE:
			return ANIMATION_INSTALL_ROT;
		case ACTION_NOTIFY_UNINSTALL_TRANSPORT:
			return ANIMATION_UNINSTALL;
		case ACTION_NOTIFY_INSTALL_TRANSPORT:
			return ANIMATION_INSTALL;
		case ACTION_NOTIFY_USE_SPYGLASS:
			return ANIMATION_BINOCULARS;
		case ACTION_NOTIFY_INSTALL_MOVE:
			return ANIMATION_INSTALL_PUSH;
		case ACTION_NOTIFY_UNINSTALL_MOVE:
			return ANIMATION_UNINSTALL_PUSH;

		case ACTION_NOTIFY_FALLING:
			return ANIMATION_THROW;
		case ACTION_NOTIFY_OPEN_PARASHUTE:
			return ANIMATION_USE;
		case ACTION_NOTIFY_PARASHUTE:
			return ANIMATION_IDLE;
		case ACTION_NOTIFY_CLOSE_PARASHUTE:
			return ANIMATION_USE_DOWN;
			
		case ACTION_NOTIFY_STAYING_TO_LYING:
			return ANIMATION_LIE;
		case ACTION_NOTIFY_LYING_TO_STAYING:
			return ANIMATION_STAND;
		case ACTION_NOTIFY_THROW_LYING:
			return ANIMATION_THROW_DOWN;
	}

	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsDeathAnimation( const int nAnimation )
{
	return ( nAnimation == ANIMATION_DEATH || nAnimation == ANIMATION_DEATH_DOWN );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// check, do we need animation + action or animation only (for the actions, which have animation)
inline bool DoWeNeedAction( const int nAction )
{
	switch ( nAction )
	{
		case ACTION_NOTIFY_IDLE:
		case ACTION_NOTIFY_IDLE_LYING:
		case ACTION_NOTIFY_MOVE:
		case ACTION_NOTIFY_CRAWL:
		case ACTION_NOTIFY_DIE:
		case ACTION_NOTIFY_DIE_LYING:
		case ACTION_NOTIFY_IDLE_TRENCH:
		case ACTION_NOTIFY_SHOOT_LYING:
		case ACTION_NOTIFY_SHOOT_TRENCH:
		case ACTION_NOTIFY_USE_UP:
		case ACTION_NOTIFY_USE_DOWN:
		case ACTION_NOTIFY_INSTALL:
		case ACTION_NOTIFY_UNINSTALL:
		case ACTION_NOTIFY_UNINSTALL_ROTATE:
		case ACTION_NOTIFY_INSTALL_ROTATE:
		case ACTION_NOTIFY_UNINSTALL_TRANSPORT:
		case ACTION_NOTIFY_INSTALL_TRANSPORT:
		case ACTION_NOTIFY_INSTALL_MOVE:
		case ACTION_NOTIFY_UNINSTALL_MOVE:
		case ACTION_NOTIFY_OPEN_PARASHUTE:
		case ACTION_NOTIFY_PARASHUTE:
		case ACTION_NOTIFY_CLOSE_PARASHUTE:
		case ACTION_NOTIFY_FALLING:
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsMovingAction( const EActionNotify updateType )
{
	return updateType == ACTION_NOTIFY_MOVE || updateType == ACTION_NOTIFY_CRAWL || updateType == ACTION_COMMAND_ROTATE_TO;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EFeedBack
{
	EFB_WIN		= 0,
	EFB_LOOSE = 1,
	EFB_DRAW = 2,
	EFB_MP_GAME_FINISHED,

	EFB_SCOUT_DISABLED,
	EFB_PARADROPERS_DISABLED,
	EFB_FIGHTERS_DISABLED,
	EFB_BOMBERS_DISABLED,
	EFB_SHTURMOVIKS_DISABLED,

	EFB_SCOUT_ENABLED,
	EFB_FIGHTERS_ENABLED,
	EFB_BOMBERS_ENABLED,
	EFB_PARADROPS_ENABLED,
	EFB_SHTURMOVIKS_ENABLED,

	EFB_AVIA_DISABLED,
	EFB_AVIA_ENABLED,

	EFB_OBJECTIVE_CHANGED,

	EFB_REINFORCEMENT_ARRIVED,						// "reinforcement has arrived" notification
	
	EFB_YOU_LOST_STORAGE,
	EFB_YOU_CAPTURED_STORAGE,
	
	EFB_UPDATE_TEAM_F_L_AGS,							// nParam == current team fLags
	EFB_UPDATE_TEAM_F_R_AGS,							// nParam == current team fRags
	EFB_UPDATE_TIME_BEFORE_CAPTURE,				// nParam == seconds before capture object
	
	EFB_ENEMY_AVIATION_CALLED,						// nParam == makelong( x y )
	EFB_ENEMY_STARTED_ANTIARTILLERY,			// nParam == makelong( x y )

	EFB_PLACE_MARKER,											// nParam == makelong( x y )
	EFB_BAD_WEATER,												// nParam == bool bStart
	
	EFB_AVIA_KILLED,											// nParam == SUCAviation::AIRCRAFT_TYPE | nType << 16. our player's = 1, our party's = 2, enemy's = 3
	
	EFB_ASK_FOR_WARFOG,
	
	EFB_AAGUN_FIRED,											// nParam == makelong( x y )
	EFB_REINFORCEMENT_CENTER,							// nParam == makelong( x y )
	EFB_SCENARIO_UNIT_DEAD,								// nParam == makelong( x y )
	EFB_SNIPER_DEAD,											// nParam == makelong( x y )
	EFB_TROOPS_PASSED,										// nParam == makelong( fromPlayer toPlayer )
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIFeedBack
{
	EFeedBack feedBackType;
	DWORD nParam;

	SAIFeedBack() : feedBackType( EFeedBack( 0 ) ), nParam( DWORD( -1 ) ) { }
	SAIFeedBack( const EFeedBack _feedBackType ) : feedBackType( _feedBackType ), nParam( -1 ) { }
	SAIFeedBack( const EFeedBack _feedBackType, DWORD _nParam )
		: feedBackType( _feedBackType ), nParam( _nParam ) { }
	
	SAIFeedBack( const SAIFeedBack &feedBack ) : feedBackType( feedBack.feedBackType ), nParam( feedBack.nParam ) { }
	const SAIFeedBack& operator=( const SAIFeedBack &feedBack ) { feedBackType = feedBack.feedBackType; nParam = feedBack.nParam; return *this; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAIAcknowledgment
{
	EUnitAckType            eAck;
	IRefCount								*pObj; // кто звучит
	int nSet;								// number of acknowledgement set

	SAIAcknowledgment() : eAck( EUnitAckType( 0 ) ), pObj( 0 ), nSet( 0 ) { }
	SAIAcknowledgment( 	EUnitAckType _eAck,
											IRefCount	*_pObj,
											const int _nSet )
	: eAck( _eAck ), pObj( _pObj ), nSet( _nSet )
	{
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// для посылки клиенту изменения состояния Bored у юнита
struct SAIBoredAcknowledgement
{
	EUnitAckType            eAck;
	IRefCount								*pObj;				// кто звучит
	bool										bPresent;			// новое состояние

	SAIBoredAcknowledgement() : eAck( EUnitAckType( 0 ) ), pObj( 0 ), bPresent( false ) { }
	SAIBoredAcknowledgement( 	EUnitAckType eAck,
														IRefCount	*pObj,
														bool bPresent )
	: eAck( eAck ), pObj( pObj ), bPresent( bPresent)
	{
	}

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ACTIONS_H__
