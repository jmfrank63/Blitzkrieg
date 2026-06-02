#ifndef __MULTIPLAYER_H__
#define __MULTIPLAYER_H__
#pragma ONCE
#include "iMainClassIDs.h"
interface IAILogicCommand;
interface IMultiplayer : public IRefCount
{
	enum { tidTypeID = LAN_MULTIPLAYER };

	enum EGamePlayingCommands
	{ 
		GPC_SEGMENT_FINISHED,
		GPC_PLAYER_REMOVED,
		GPC_AI_COMMAND,
		GPC_START_GAME,
		GPC_PLAYER_LAG,
		GPC_PAUSE,
		GPC_GAME_SPEED,
		GPC_TIMEOUT,
		GPC_PLAYER_ALIVE,
	};
	
	enum EMultiplayerStates { EMS_SERVERS_LIST, EMS_GAME_CREATION, EMS_PLAYING, EMS_NONE };

	class CCommand : public IRefCount
	{
		OBJECT_COMPLETE_METHODS( CCommand );
	public:
		EGamePlayingCommands eCommandType;
		CPtr<IAILogicCommand> pAILogicCommand;
		int nPlayer;
		int nParam;

		CCommand() : nPlayer( -1 ) { }
		CCommand( EGamePlayingCommands _eCommandType, const int _nPlayer, const int _nParam, IAILogicCommand *_pAILogicCommand )
			: eCommandType( _eCommandType ), pAILogicCommand( _pAILogicCommand ), nPlayer( _nPlayer ), nParam( _nParam ) { }
	};

	virtual void STDCALL Init() = 0;
	virtual void STDCALL Segment() = 0;
	virtual void STDCALL InitServersList() = 0;
	virtual bool STDCALL InitJoinToServer( const char *pszIPAddress, const int nPort, bool bPasswordRequired, const char* pszPassword ) = 0;

	virtual const EMultiplayerStates GetState() = 0;
	
	virtual CCommand* STDCALL GetCommand() = 0;
	virtual void STDCALL SendClientCommands( IDataStream *pPacket ) = 0;
	virtual void STDCALL SendInGameChatMessage( const WORD *pszType, const WORD *pszMessage ) = 0;
	
	virtual int STDCALL GetNumberOfPlayers() const = 0;
	
	virtual void STDCALL TogglePause() = 0;
	virtual void STDCALL GameSpeed( const int nChange ) = 0;
	virtual void STDCALL DropPlayer( const int nLogicID ) = 0;
	
	virtual void STDCALL CommandTimeOut( const bool bSet ) = 0;
	
	virtual void STDCALL SendAliveMessage() = 0;
	virtual void STDCALL FinishGame() = 0;
	
	virtual interface INetDriver* STDCALL GetInGameNetDriver() const = 0;
};
#endif // __MULTIPLAYER_H__
