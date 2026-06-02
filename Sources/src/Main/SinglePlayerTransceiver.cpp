#include "StdAfx.h"

#include "iMainCommands.h"
#include "SinglePlayerTransceiver.h"
#include "AILogicCommandInternal.h"
#include "CommandsHistoryInterface.h"

#include "..\Input\Input.h"
int CSinglePlayerTransceiver::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	if ( saver.IsReading() ) 
	{
		pAILogic = GetSingleton<IAILogic>();
		pCmdsHistory = GetSingleton<ICommandsHistory>();
	}

	saver.Add( 101, &nCommonSegment );
	saver.Add( 102, &bHistoryPlaying );

	return 0;
}
void CSinglePlayerTransceiver::Init( ISingleton *pSingleton, const int nMultiplayerType )
{
	pAILogic = GetSingleton<IAILogic>( pSingleton );
	pCmdsHistory = GetSingleton<ICommandsHistory>( pSingleton );
	
	RemoveGlobalVar( "MultiplayerGame" );
	GetSingleton<IGameTimer>()->GetGameTimer()->Reset();
	GetSingleton<IGameTimer>()->GetGameSegmentTimer()->Set( 0xffffffff );
	bHistoryPlaying = false;

	nCommonSegment = 0;
}
void CSinglePlayerTransceiver::PreMissionInit()
{
	RemoveGlobalVar( "MultiplayerGame" );	
	GetSingleton<IGameTimer>()->GetGameTimer()->Reset();
	GetSingleton<IGameTimer>()->GetGameSegmentTimer()->Set( 0xffffffff );

	nCommonSegment = 0;
}
void CSinglePlayerTransceiver::LoadAllGameParameters()
{ 
	bHistoryPlaying = GetGlobalVar( "History.Playing", 0 ) != 0;
}
void CSinglePlayerTransceiver::DoSegments()
{
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	while ( const wchar_t *pszString = pBuffer->Read(CONSOLE_STREAM_NET_CHAT) )
	{
		NStr::DebugTrace("Got type: %S\n", pszString);
		pszString = pBuffer->Read(CONSOLE_STREAM_NET_CHAT);
		NStr::DebugTrace("Got msg: %S\n", pszString);
	}
	IGameTimer *pGameTimer = GetSingleton<IGameTimer>();
	ISegmentTimer *pGameSegment = pGameTimer->GetGameSegmentTimer();
	pGameSegment->BeginSegments( pGameTimer->GetGameTime() );
	while ( pGameSegment->NextSegment() )
	{
		if ( pAILogic && !pAILogic->IsSuspended() )
		{
			pCmdsHistory->ExecuteSegmentCommands( nCommonSegment, this );			
			pAILogic->Segment();
			++nCommonSegment;
		}
	}
}
int CSinglePlayerTransceiver::CommandRegisterGroup( IRefCount **pUnitsBuffer, const int nLen )
{
	if ( !bHistoryPlaying )
	{
		const WORD wID = pAILogic->GenerateGroupNumber();
		pAILogic->RegisterGroup( pUnitsBuffer, nLen, wID );

		pAILogic->SubstituteUniqueIDs( pUnitsBuffer, nLen );		
		pCmdsHistory->AddCommand( nCommonSegment, new CRegisterGroupCommand( pUnitsBuffer, nLen, wID, pAILogic ) );

		return wID;
	}
	else
		return 0;
}
void CSinglePlayerTransceiver::CommandUnregisterGroup( const WORD wGroup )
{
	if ( !bHistoryPlaying )
	{
		pAILogic->UnregisterGroup( wGroup );
		pCmdsHistory->AddCommand( nCommonSegment, new CUnregisterGroupCommand( wGroup ) );
	}
}
void CSinglePlayerTransceiver::CommandGroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue )
{
	if ( !bHistoryPlaying )
	{
		SAIUnitCmd cmd( *pCommand );
		cmd.bFromAI = false;
		pAILogic->GroupCommand( &cmd, wGroup, bPlaceInQueue );

		pCmdsHistory->AddCommand( nCommonSegment, new CGroupCommand( pCommand, wGroup, bPlaceInQueue, pAILogic ) );
	}
}
int CSinglePlayerTransceiver::CommandUnitCommand( const struct SAIUnitCmd *pCommand )
{
	if ( !bHistoryPlaying )
	{
		const int nGroup = pAILogic->GenerateGroupNumber();
		SAIUnitCmd cmd( *pCommand );
		cmd.bFromAI = false;
		pAILogic->UnitCommand( &cmd, nGroup, 0 );

		pCmdsHistory->AddCommand( nCommonSegment, new CUnitCommand( pCommand, nGroup, 0 ) );

		return nGroup;
	}
	else
		return 0;
}
void CSinglePlayerTransceiver::CommandShowAreas( int nGroupID, int nAreaType, bool bShow )
{
	if ( !bHistoryPlaying )
	{
		pAILogic->ShowAreas( nGroupID, EActionNotify(nAreaType), bShow );
		pCmdsHistory->AddCommand( nCommonSegment, new CShowAreasCommand( nGroupID, nAreaType, bShow ) );
	}
}
void CSinglePlayerTransceiver::AddCommandToSend( IAILogicCommand *pCommand )
{
	pCommand->Execute( pAILogic );
}
void CSinglePlayerTransceiver::CommandClientTogglePause() 
{ 
	GetSingleton<IInput>()->AddMessage( SGameMessage(CMD_GAME_PAUSE) );
}
void CSinglePlayerTransceiver::CommandClientSpeed( const int nChange ) 
{ 
	GetSingleton<IInput>()->AddMessage( SGameMessage(nChange == 1 ? CMD_GAME_SPEED_INC : CMD_GAME_SPEED_DEC) );
}
