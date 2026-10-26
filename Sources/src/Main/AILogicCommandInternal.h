#ifndef __AI_LOGIC_COMMAND_INTERNAL_H__
#define __AI_LOGIC_COMMAND_INTERNAL_H__
#pragma ONCE
#include "AILogicCommand.h"
#include "NetMessages.h"
#include "Transceiver.h"
#include "../Common/Actions.h"
#include "../zlib/zconf.h"
class CRegisterGroupCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_REGISTER_GROUP };
private:
	OBJECT_COMPLETE_METHODS( CRegisterGroupCommand );
	DECLARE_SERIALIZE;
	std::vector<int> unitsIDs;					// IDs of all obejcts in group
	WORD wID;														// ID of the group
public:
	CRegisterGroupCommand() { }
	CRegisterGroupCommand( IRefCount **pUnitsBuffer, const int nLen, const WORD wID, IAILogic *pAILogic );
	virtual void Execute( IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream );
	virtual void Restore( IDataStream *pStream );
	virtual bool NeedToBeStored() const { return true; }

	virtual int STDCALL operator&( IDataTree &ss );
};
class CUnregisterGroupCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_UNREGISTER_GROUP };
private:
	OBJECT_COMPLETE_METHODS( CUnregisterGroupCommand );
	DECLARE_SERIALIZE;
	WORD wGroup;												// ID of the group
public:
	CUnregisterGroupCommand() { }
	CUnregisterGroupCommand( const WORD wGroup );
	virtual void Execute( IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream );
	virtual void Restore( IDataStream *pStream );
	virtual bool NeedToBeStored() const { return true; }

	virtual int STDCALL operator&( IDataTree &ss );
};
class CGroupCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_GROUP_COMMAND };
private:
	OBJECT_COMPLETE_METHODS( CGroupCommand );
	DECLARE_SERIALIZE;
	SAIUnitCmd command;									// command itself
	int nObjId;													// object ID for 'target-object' command
	WORD wGroup;												// group ID, this command for
	bool bPlaceInQueue;									// do we need place this command in the group's queue
public:
	CGroupCommand() : nObjId( 0 ), wGroup( 0 ), bPlaceInQueue( false ) { }
	CGroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue, IAILogic *pAILogic );
	virtual void Execute( IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream );
	virtual void Restore( IDataStream *pStream );
	virtual bool NeedToBeStored() const { return true; }

	virtual int STDCALL operator&( IDataTree &ss );
};
class CUnitCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_UNIT_COMMAND };
private:
	OBJECT_COMPLETE_METHODS( CUnitCommand );
	DECLARE_SERIALIZE;
	SAIUnitCmd command;									// command itself
	WORD wID;														// group ID - result of this command :)
	int nPlayer;												// player number
public:
	CUnitCommand() : wID( 0 ), nPlayer( 0 ) { }
	CUnitCommand( const struct SAIUnitCmd *pCommand, const WORD wID, const int _nPlayer );
	virtual void Execute( IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream );
	virtual void Restore( IDataStream *pStream );
	virtual bool NeedToBeStored() const { return true; }

	virtual int STDCALL operator&( IDataTree &ss );
};
class CShowAreasCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_SHOW_AREAS };
private:
	OBJECT_COMPLETE_METHODS( CShowAreasCommand );
	DECLARE_SERIALIZE;
	WORD wGroupID;											// group ID to show area
	int nAreaType;											// type of the area to show
	bool bShow;													// show or hide area
public:
	CShowAreasCommand() : wGroupID( 0 ), nAreaType( 0 ), bShow( false ) { }
	CShowAreasCommand( const WORD _wGroupID, const int _nAreaType, const bool _bShow )
		: wGroupID( _wGroupID ), nAreaType( _nAreaType ), bShow( _bShow ) {  }
	virtual void Execute( IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream ) {  }
	virtual void Restore( IDataStream *pStream ) {  }
	virtual bool NeedToBeStored() const { return true; }

	virtual int STDCALL operator&( IDataTree &ss );
};
class CControlSumCheckCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_CHECK_SUM };
private:	
	OBJECT_COMPLETE_METHODS( CControlSumCheckCommand );
	DECLARE_SERIALIZE;

	int nPlayer;
	uLong ulCheckSum;

	static std::vector< std::list<uLong> > checkSums;
public:	
	static WORD wMask;
public:
	CControlSumCheckCommand() { }
	CControlSumCheckCommand( const int _nPlayer, const uLong _ulCheckSum ) 
		: nPlayer( _nPlayer ), ulCheckSum( _ulCheckSum ) { }
	
	virtual void Execute( IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream );
	virtual void Restore( IDataStream *pStream );
	virtual bool NeedToBeStored() const { return false; }

	static void Check( const int nOurNumber );
	static void Init( const WORD wMask );

	virtual int STDCALL operator&( IDataTree &ss );
};
class CDropPlayerCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_DROP_PLAYER };
private:
	OBJECT_COMPLETE_METHODS( CDropPlayerCommand );
	DECLARE_SERIALIZE;

	int nPlayerToDrop;
public:
	CDropPlayerCommand() : nPlayerToDrop( -1 ) { }
	explicit CDropPlayerCommand( const int _nPlayerToDrop ) : nPlayerToDrop( _nPlayerToDrop ) { }

	virtual void Execute( interface IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream );
	virtual void Restore( IDataStream *pStream );
	virtual bool NeedToBeStored() const { return true; }

	virtual int STDCALL operator&( IDataTree &ss );
};
// A call into the mission script that the player's input makes: Call HQ, the
// answer to a script's AskClient("GetSelectedUnits()"), a console script line.
// It goes through the transceiver like an order, so a replay holds it and plays
// it at its segment. The answer's objects travel as unique IDs and become
// pointers again when it runs.
class CScriptCallCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_SCRIPT_CALL };
	enum EKind
	{
		CALL		= SCRIPT_CALL,					// szCall is the whole call, e.g. "CallHQ()"
		OBJECTS	= SCRIPT_CALL_OBJECTS,	// szCall is a function's name, objectIDs its arguments
		CONSOLE	= SCRIPT_CALL_CONSOLE,	// a console line, under the AI's cheat gate
	};
private:
	OBJECT_COMPLETE_METHODS( CScriptCallCommand );
	DECLARE_SERIALIZE;
	int nKind;
	std::string szCall;
	std::vector<int> objectIDs;
public:
	CScriptCallCommand() : nKind( CALL ) { }
	CScriptCallCommand( const int nKind, const char *pszCall, IRefCount **pObjects, const int nLen, IAILogic *pAILogic );
	virtual void Execute( IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream );
	virtual void Restore( IDataStream *pStream );
	virtual bool NeedToBeStored() const { return true; }

	virtual int STDCALL operator&( IDataTree &ss );
};
// What the client changes in the AI besides orders and script calls: the lock
// of the player's aviation appear point while planes are picked (where they
// come from) and the war fog cheat (what every side sees).
class CClientActionCommand : public IAILogicCommand
{
public:
	enum { tidTypeID = NGM_ID_COMMAND_CLIENT_ACTION };
	enum EAction
	{
		LOCK_AVIATION_APPEAR_POINT		= 0,
		UNLOCK_AVIATION_APPEAR_POINT	= 1,
		TOGGLE_WAR_FOG								= 2,
	};
private:
	OBJECT_COMPLETE_METHODS( CClientActionCommand );
	DECLARE_SERIALIZE;
	int nAction;
public:
	CClientActionCommand() : nAction( LOCK_AVIATION_APPEAR_POINT ) { }
	explicit CClientActionCommand( const int _nAction ) : nAction( _nAction ) { }
	virtual void Execute( IAILogic *pAILogic );
	virtual void Store( IDataStream *pStream );
	virtual void Restore( IDataStream *pStream );
	virtual bool NeedToBeStored() const { return true; }

	virtual int STDCALL operator&( IDataTree &ss );
};
#endif // __AI_LOGIC_COMMAND_INTERNAL_H__
