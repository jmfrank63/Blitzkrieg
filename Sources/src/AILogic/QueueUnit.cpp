#include "StdAfx.h"

#include "QueueUnit.h"
#include "StatesFactory.h"
#include "UnitStates.h"
#include "Commands.h"
#include "CommonUnit.h"
extern NTimer::STime curTime;
CDecksSet< CObj<CAICommand> > CQueueUnit::cmds( SConsts::AI_START_VECTOR_SIZE );
void CQueueUnit::Init()
{
	bCmdFinished = false;
	pState = GetStatesFactory()->ProduceRestState( this );
	pCmdCurrent = 0;
	lastChangeStateTime = 0;
}
void CQueueUnit::Clear() 
{ 
	cmds.Clear();
}
bool CQueueUnit::IsEmptyCmdQueue() const 
{
	return cmds.IsEmpty( GetID() );
}
CAICommand* CQueueUnit::GetCurCmd() const 
{
	return pCmdCurrent;
}
void CQueueUnit::CheckCmdsSize( const int id )
{
	if ( id >= cmds.GetDecksNum() )
		cmds.IncreaseDecksNum( id * 1.5 );
}
void CQueueUnit::DelCmdQueue( const int id )
{
	for ( int i = cmds.begin( id ); i != cmds.end(); i = cmds.GetNext( i ) )
		cmds.GetEl( i ) = 0;
	cmds.DelDeck( id );
}
void CQueueUnit::InitWCommands( CQueueUnit *pUnit )
{
	FreezeByState( false );
	
	const WORD wThisID = GetID();
	const WORD wUnitID = pUnit->GetID();

	DelCmdQueue( wThisID );
	
	if ( pUnit->GetCurCmd() && CanCommandBeExecuted( pUnit->GetCurCmd() ) )
		cmds.Push( wThisID, pUnit->GetCurCmd() );

	while( !cmds.IsEmpty(wUnitID) )
	{
		CObj<CAICommand> pCmd = cmds.Peek( wUnitID );
		PopCmd( wUnitID );

		if ( CanCommandBeExecuted( pCmd ) )
			cmds.Push( wThisID, pCmd );
	}
}
void CQueueUnit::InsertUnitCommand( CAICommand *pCommand )
{
	CPtr<CAICommand> pGarbage = pCommand;
	
	FreezeByState( false );
	
	if ( !cmds.IsEmpty( GetID() ) && cmds.Peek( GetID() )->ToUnitCmd().cmdType == ACTION_COMMAND_DISAPPEAR )
		return;

	if ( CanCommandBeExecuted( pCommand ) &&
			 GetStatesFactory()->CanCommandBeExecuted( pCommand ) )
	{
		if ( pCmdCurrent.IsValid() )
		{
			CAICommand *pOldCommand = new CAICommand( *pCmdCurrent );
			pOldCommand->SetFromAI( true );
			cmds.PushFront( GetID(), pOldCommand );
		}

		if ( pCmdCurrent.IsValid() )
		{
			pState->TryInterruptState( 0 );// грубо прервать состояние
			NullSegmTime();
		}

		cmds.PushFront( GetID(), pCommand );
	}
}
void CQueueUnit::PushFrontUnitCommand( CAICommand *pCommand )
{
	CPtr<CAICommand> pGarbage = pCommand;
	
	FreezeByState( false );
	
	if ( !cmds.IsEmpty( GetID() ) && cmds.Peek( GetID() )->ToUnitCmd().cmdType == ACTION_COMMAND_DISAPPEAR )
		return;

	if ( CanCommandBeExecuted( pCommand ) && GetStatesFactory()->CanCommandBeExecuted( pCommand ) )
	{
		if ( pCmdCurrent.IsValid() )
		{
			pState->TryInterruptState( 0 );
			NullSegmTime();
		}

		cmds.PushFront( GetID(), pCommand );
	}
}
void CQueueUnit::UnitCommand( CAICommand *pCommand, bool bPlaceInQueue, bool bOnlyThisUnitCommand )
{
	CPtr<CAICommand> pGarbage = pCommand;
	
	FreezeByState( false );
	
	CPtr<CAICommand> pCmd = pCommand;
	if ( !CanCommandBeExecutedByStats( pCommand ) )
	{
 		const EActionCommand &cmd = pCommand->ToUnitCmd().cmdType;					
		if ( cmd == ACTION_COMMAND_MOVE_TO || cmd == ACTION_COMMAND_SWARM_TO )
			SendAcknowledgement( pCommand, ACK_CANNOT_MOVE_NEED_TO_BE_TOWED_TO_MOVE );

		return;
	}

	const SAIUnitCmd &unitCmd = pCmd->ToUnitCmd();
	NI_ASSERT_T( unitCmd.cmdType != ACTION_COMMAND_DIE, "Can't process DIE command in CQueueUnit" );
	
	if ( !cmds.IsEmpty( GetID() ) && cmds.Peek( GetID() )->ToUnitCmd().cmdType == ACTION_COMMAND_DISAPPEAR )
		return;
	
	if ( bPlaceInQueue )
		cmds.Push( GetID(), pCmd.GetPtr() );
	else if ( bCmdFinished || !pState )
	{
		DelCmdQueue( GetID() );

		bCmdFinished = true;

		cmds.Push( GetID(), pCmd.GetPtr() );
	}
	else 
	{
		if ( pCmd && pCmd->ToUnitCmd().cmdType == ACTION_COMMAND_STOP_THIS_ACTION ) // прервать насильно
		{
			if ( !bCmdFinished )
			{
				pState->TryInterruptState( 0 );
				NullSegmTime();
			}
		}
		else // спрашиваем у текущего состояния можно ли его прервать
		{
			switch ( pState->TryInterruptState( pCommand ) )
			{
				case TSIR_YES_IMMIDIATELY:
					NullSegmTime();
				case TSIR_YES_WAIT:
					DelCmdQueue( GetID() );
					cmds.Push( GetID(), pCmd.GetPtr() );

					break;
				case TSIR_YES_MANAGED_ALREADY:
					break;
				case TSIR_NO_COMMAND_INCOMPATIBLE:
					break;
			}
		}
	}
}
void CQueueUnit::SetCommandFinished()
{
	bCmdFinished = true;
	pCmdCurrent = 0;
}
void CQueueUnit::PopCmd( const int nID )
{
	cmds.Peek( nID ) = 0;
	cmds.Pop( nID );
}
void CQueueUnit::Segment()
{
	if ( !bCmdFinished && pState.IsValid() && IsRestState( pState->GetName() ) && !cmds.IsEmpty( GetID() ) )
	{
		CObj<CAICommand> pCmd = cmds.Peek( GetID() );
		PopCmd( GetID() );
		switch ( pState->TryInterruptState( pCmd ) )
		{
			case TSIR_YES_IMMIDIATELY:
				NullSegmTime();
				cmds.PushFront( GetID(), pCmd.GetPtr() );
				break;
			case TSIR_YES_WAIT:
				cmds.PushFront( GetID(), pCmd.GetPtr() );
				pState->Segment();
				break;
			case TSIR_YES_MANAGED_ALREADY:
				break;
			case TSIR_NO_COMMAND_INCOMPATIBLE:
				break;
		}
	} 
	else if ( bCmdFinished ) 
	{
		bCmdFinished = false;
		CPtr<IUnitState> pNewState;
		do
		{
			if ( cmds.IsEmpty(GetID()) )
			{
				pCmdCurrent = 0;
				pNewState = GetStatesFactory()->ProduceRestState( this );
				lastChangeStateTime = curTime;

				FreezeByState( false );
			}
			else // очередь команд не пуста
			{
				CObj<CAICommand> pCmd = cmds.Peek( GetID() );
				
				const EActionCommand cmdType = pCmd->ToUnitCmd().cmdType;
				if ( cmdType & 0x8000 )
				{
					pCmd = new CAICommand( *(cmds.Peek( GetID() )) );
					pCmd->ToUnitCmd().cmdType = EActionCommand( cmdType & ~0x8000 );
					pCmd->ToUnitCmd().vPos = checked_cast<CCommonUnit*>(this)->GetCenter();
				}
				PopCmd( GetID() );

				if ( pCmd && pCmd->ToUnitCmd().cmdType == ACTION_COMMAND_DISAPPEAR )
				{
					checked_cast<CCommonUnit*>(this)->Disappear();
					return;
				}
				else if ( pCmd && pCmd->ToUnitCmd().cmdType == ACTION_COMMAND_STOP_THIS_ACTION )
				{
					if ( pState.IsValid() && !bCmdFinished )
					{
						pState->TryInterruptState( 0 );
						NullSegmTime();
					}

					pState = 0;
				}
				else if ( CanCommandBeExecuted( pCmd ) &&
									GetStatesFactory()->CanCommandBeExecuted( pCmd ) )
				{
					pNewState = GetStatesFactory()->ProduceState( this, pCmd );
					if ( pNewState )
						pCmdCurrent = pCmd;

					lastChangeStateTime = curTime;

					FreezeByState( false );
				}
				else
				{
				}
			}
		} while( pNewState == 0 );

		pState = pNewState;
	}
	else
		pState->Segment();
}
CAICommand* CQueueUnit::GetNextCommand() const
{
	if ( cmds.IsEmpty( GetID() ) )
		return 0;
	else
		return cmds.Peek( GetID() );
}
CAICommand* CQueueUnit::GetLastCommand() const
{
	if ( cmds.IsEmpty( GetID() ) )
		return 0;
	else
		return cmds.GetLastEl( GetID() );
}
void CQueueUnit::KillStatesAndCmdsInfo()
{
	DelCmdQueue( GetID() );
	pCmdCurrent = 0;
	pState = 0;
	bCmdFinished = false;
}
void CQueueUnit::SetCurState( IUnitState *_pState )
{
	pState = _pState;
}
const int CQueueUnit::GetBeginCmdsIter() const
{
	return cmds.begin( GetID() );
}
const int CQueueUnit::GetEndCmdsIter() const
{
	return cmds.end();
}
const int CQueueUnit::GetNextCmdsIter( const int nIter ) const
{
	return cmds.GetNext( nIter );
}
CAICommand* CQueueUnit::GetCommand( const int nIter ) const
{
	return cmds.GetEl( nIter );
}
