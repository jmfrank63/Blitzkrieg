#include "StdAfx.h"

#include "iMainCommands.h"
#include "SinglePlayerTransceiver.h"
#include "AILogicCommandInternal.h"
#include "CommandsHistoryInterface.h"
#include "ReplayCore.h"

#include "../Input/Input.h"
#include "../StreamIO/RandomGen.h"
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
			CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
			// The console's script lines, which the AI read inside its Segment:
			// read here they become commands a replay holds. A playback drops the
			// viewer's, as it ignores the viewer's orders.
			while ( const char *pszCommand = pBuffer->ReadASCII( CONSOLE_STREAM_SCRIPT ) )
			{
				if ( !bHistoryPlaying )
					RecordAndExecute( new CScriptCallCommand( CScriptCallCommand::CONSOLE, pszCommand, 0, 0, pAILogic ) );
			}
			pCmdsHistory->ExecuteSegmentCommands( nCommonSegment, this );
			pAILogic->Segment();
			++nCommonSegment;
			if ( NReplay::IsStateHashSegment( nCommonSegment ) )
				pCmdsHistory->AddStateHash( nCommonSegment, pAILogic->GetStateHash() );
		}
	}
}
int CSinglePlayerTransceiver::CommandRegisterGroup( IRefCount **pUnitsBuffer, const int nLen )
{
	if ( !bHistoryPlaying )
	{
		CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
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
		CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
		pAILogic->UnregisterGroup( wGroup );
		pCmdsHistory->AddCommand( nCommonSegment, new CUnregisterGroupCommand( wGroup ) );
	}
}
void CSinglePlayerTransceiver::CommandGroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue )
{
	if ( !bHistoryPlaying )
	{
		CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
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
		CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
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
		CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
		pAILogic->ShowAreas( nGroupID, EActionNotify(nAreaType), bShow );
		pCmdsHistory->AddCommand( nCommonSegment, new CShowAreasCommand( nGroupID, nAreaType, bShow ) );
	}
}
void CSinglePlayerTransceiver::AddCommandToSend( IAILogicCommand *pCommand )
{
	CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
	pCommand->Execute( pAILogic );
}
void CSinglePlayerTransceiver::RecordAndExecute( IAILogicCommand *pCommand )
{
	CPtr<IAILogicCommand> pHold = pCommand;
	{
		CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
		pCommand->Execute( pAILogic );
	}
	pCmdsHistory->AddCommand( nCommonSegment, pCommand );
}
void CSinglePlayerTransceiver::CommandScriptCall( const int nKind, const char *pszCall, IRefCount **pObjects, const int nLen )
{
	if ( !bHistoryPlaying )
		RecordAndExecute( new CScriptCallCommand( nKind, pszCall, pObjects, nLen, pAILogic ) );
}
void CSinglePlayerTransceiver::CommandLockAviationAppearPoint( CVec2 *pvAppearPoint )
{
	CSimulationRandomScope simulation( GetSingleton<IRandomGen>() );
	*pvAppearPoint = pAILogic->LockAvitaionAppearPoint();
	// A playback's viewer still sees where planes would come from, but leaves the
	// lock as the recording has it: the point is only read while locked and is
	// taken again whenever the lock begins.
	if ( bHistoryPlaying )
		pAILogic->UnlockAviationAppearPoint();
	else
		pCmdsHistory->AddCommand( nCommonSegment, new CClientActionCommand( CClientActionCommand::LOCK_AVIATION_APPEAR_POINT ) );
}
void CSinglePlayerTransceiver::CommandUnlockAviationAppearPoint()
{
	if ( !bHistoryPlaying )
		RecordAndExecute( new CClientActionCommand( CClientActionCommand::UNLOCK_AVIATION_APPEAR_POINT ) );
}
void CSinglePlayerTransceiver::CommandToggleWarFog()
{
	if ( !bHistoryPlaying )
		RecordAndExecute( new CClientActionCommand( CClientActionCommand::TOGGLE_WAR_FOG ) );
}
void CSinglePlayerTransceiver::CommandClientTogglePause()
{ 
	GetSingleton<IInput>()->AddMessage( SGameMessage(CMD_GAME_PAUSE) );
}
void CSinglePlayerTransceiver::CommandClientSpeed( const int nChange ) 
{ 
	GetSingleton<IInput>()->AddMessage( SGameMessage(nChange == 1 ? CMD_GAME_SPEED_INC : CMD_GAME_SPEED_DEC) );
}
