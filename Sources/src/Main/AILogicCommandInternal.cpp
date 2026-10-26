#include "StdAfx.h"

#include <cstdio>
#include <cstdlib>

#include "AILogicCommandInternal.h"
#include "iMainCommands.h"
#include "Transceiver.h"

#include "../AILogic/AILogic.h"
#include "../Input/Input.h"
CRegisterGroupCommand::CRegisterGroupCommand( IRefCount **pUnitsBuffer, const int nLen, const WORD _wID, IAILogic *pAILogic )
: wID( _wID )
{
	// SubstituteUniqueIDs leaves each ID in a pointer-sized slot; read as ints, a
	// 64-bit buffer gave every second ID as 0 and dropped the second half.
	unitsIDs.reserve( nLen );
	for ( int i = 0; i < nLen; ++i )
		unitsIDs.push_back( static_cast<int>( reinterpret_cast<intptr_t>( pUnitsBuffer[i] ) ) );
}
void CRegisterGroupCommand::Execute( IAILogic *pAILogic )
{
	std::vector<IRefCount*> unitsBuffer( unitsIDs.size() );
	int nLen = 0;
	int nUnknown = 0;
	for ( std::vector<int>::const_iterator it = unitsIDs.begin(); it != unitsIDs.end(); ++it )
	{
		unitsBuffer[nLen] = *it > 0 ? pAILogic->GetObjByUniqueID( *it ) : 0;
		if ( unitsBuffer[nLen] == 0 )
		{
			++nUnknown;
			const std::string szMessage = NStr::Format( "Wrong id (%d) passed, execute registergroup command", *it );
			GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, szMessage.c_str(), 0xffff0000, true );
		}

		++nLen;
	}
	// A replay's group must find every unit it was recorded with.
	if ( getenv( "BK_REPLAY_TRACE" ) != 0 )
		fprintf( stderr, "BK_REPLAY_TRACE: registered a group of %d units, %d unknown\n", nLen, nUnknown );

	pAILogic->RegisterGroup( &(unitsBuffer[0]), nLen, wID );
}
int CRegisterGroupCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &wID );
	saver.Add( 2, &unitsIDs );

	return 0;
}
int CRegisterGroupCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	std::string name = "RegisterGroup";

	saver.Add( "Name", &name );
	saver.Add( "GroupID", &wID );
	saver.Add( "Units", &unitsIDs );

	return 0;
}
void CRegisterGroupCommand::Store( IDataStream *pPacket )
{
	BYTE commandID = CRegisterGroupCommand::tidTypeID;
	pPacket->Write( &commandID, sizeof(commandID) );
	pPacket->Write( &wID, sizeof(wID) );
	int nNumObjects = unitsIDs.size();
	pPacket->Write( &nNumObjects, sizeof(nNumObjects) );
	pPacket->Write( &(unitsIDs[0]), unitsIDs.size() * sizeof(unitsIDs[0]) );
}
void CRegisterGroupCommand::Restore( IDataStream *pPacket )
{
	pPacket->Read( &wID, sizeof(wID) );
	int nNumObjects = 0;
	pPacket->Read( &nNumObjects, sizeof(nNumObjects) );
	unitsIDs.resize( nNumObjects );
	pPacket->Read( &(unitsIDs[0]), unitsIDs.size() * sizeof(unitsIDs[0]) );
}
CUnregisterGroupCommand::CUnregisterGroupCommand( const WORD _wGroup )
: wGroup( _wGroup )
{
}
void CUnregisterGroupCommand::Execute( IAILogic *pAILogic )
{
	pAILogic->UnregisterGroup( wGroup );
}
int CUnregisterGroupCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &wGroup );

	return 0;
}
int CUnregisterGroupCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	std::string name = "UnregisterGroup";

	saver.Add( "Name", &name );
	saver.Add( "GroupID", &wGroup );

	return 0;
}
void CUnregisterGroupCommand::Store( IDataStream *pPacket )
{
	BYTE commandID = CUnregisterGroupCommand::tidTypeID;
	pPacket->Write( &commandID, sizeof(commandID) );
	pPacket->Write( &wGroup, sizeof(wGroup) );
}
void CUnregisterGroupCommand::Restore( IDataStream *pPacket )
{
	pPacket->Read( &wGroup, sizeof(wGroup) );
}
CGroupCommand::CGroupCommand( const SAIUnitCmd *pCommand, const WORD _wGroup, bool _bPlaceInQueue, IAILogic *pAILogic )
: command( *pCommand ), wGroup( _wGroup ), bPlaceInQueue( _bPlaceInQueue )
{
	if ( pCommand->pObject != 0 )
		nObjId = pAILogic->GetUniqueIDOfObject( pCommand->pObject );
	else
		nObjId = 0;
	command.pObject = 0;
}
void CGroupCommand::Execute( IAILogic *pAILogic )
{
	if ( nObjId != 0 )
	{
		command.pObject = pAILogic->GetObjByUniqueID( nObjId );

		if ( command.pObject == 0 )
		{
			const std::string szMessage = NStr::Format( "Wrong id (%d) passed, execute group command", nObjId );
			GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, szMessage.c_str(), 0xffff0000, true );
		}
	}

	pAILogic->GroupCommand( &command, wGroup, bPlaceInQueue );
}
int CGroupCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &command );
	saver.Add( 2, &wGroup );
	saver.Add( 3, &nObjId );
	saver.Add( 4, &bPlaceInQueue );

	return 0;
}
int CGroupCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	std::string name = "GroupCommand";

	saver.Add( "Name", &name );
	saver.Add( "Command", &command );
	saver.Add( "TargetObjectID", &nObjId );
	saver.Add( "GroupID", &wGroup );
	saver.Add( "PlaceToQueueFlag", &bPlaceInQueue );

	return 0;
}
void CGroupCommand::Store( IDataStream *pPacket )
{
	CStreamAccessor packet = pPacket;

	const BYTE commandID = CGroupCommand::tidTypeID;	
	packet << commandID;
	const BYTE packedBoolCommandInfo = ((BYTE)command.fromExplosion << 1) | (BYTE)command.bFromAI;
	packet << WORD(command.cmdType) << command.vPos.x << command.vPos.y << command.fNumber << packedBoolCommandInfo;

	packet << wGroup << nObjId << BYTE( bPlaceInQueue ) << nObjId;
}
void CGroupCommand::Restore( IDataStream *pPacket )
{
	CStreamAccessor packet = pPacket;
	BYTE packedBoolCommandInfo;
	WORD cmdType;
	packet >> cmdType >> command.vPos.x >> command.vPos.y >> command.fNumber >> packedBoolCommandInfo;
	command.cmdType = EActionCommand( cmdType );
	command.fromExplosion = packedBoolCommandInfo >> 1;
	command.bFromAI = packedBoolCommandInfo & 2;

	BYTE cPlaceInQueue;
	packet >> wGroup >> nObjId >> cPlaceInQueue >> nObjId;
	bPlaceInQueue = cPlaceInQueue;

	command.pObject = 0;
}
CUnitCommand::CUnitCommand( const struct SAIUnitCmd *pCommand, const WORD _wID, const int _nPlayer )
: command( *pCommand ), wID( _wID ), nPlayer( _nPlayer )
{
}
void CUnitCommand::Execute( IAILogic *pAILogic )
{
	pAILogic->UnitCommand( &command, wID, nPlayer );
}
int CUnitCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &command );
	saver.Add( 2, &wID );
	saver.Add( 3, &nPlayer );

	return 0;
}
int CUnitCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	std::string name = "UnitCommand";

	saver.Add( "Name", &name );
	saver.Add( "Command", &command );
	saver.Add( "GroupID", &wID );
	saver.Add( "Player", &nPlayer );

	return 0;
}
void CUnitCommand::Store( IDataStream *pPacket )
{
	CStreamAccessor packet = pPacket;
	
	const BYTE commandID = CUnitCommand::tidTypeID;
	packet << commandID;
	packet << WORD(command.cmdType) << command.vPos.x << command.vPos.y << command.fNumber << BYTE(command.bFromAI);
	packet << wID << nPlayer;
}
void CUnitCommand::Restore( IDataStream *pPacket )
{
	CStreamAccessor packet = pPacket;

	WORD cmdType;
	BYTE cFromAI;
	packet >> cmdType >> command.vPos.x >> command.vPos.y >> command.fNumber >> cFromAI;
	command.cmdType = EActionCommand( cmdType );
	command.bFromAI = cFromAI;
	
	packet >> wID >> nPlayer;
}
void CShowAreasCommand::Execute( IAILogic *pAILogic )
{
	pAILogic->ShowAreas( wGroupID, EActionNotify(nAreaType), bShow );
}
int CShowAreasCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &wGroupID );
	saver.Add( 2, &nAreaType );
	saver.Add( 3, &bShow );

	return 0;
}
int CShowAreasCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	std::string name = "CShowAreasCommand";

	saver.Add( "Name", &name );
	saver.Add( "GroupID", &wGroupID );
	saver.Add( "AreaType", &nAreaType );
	saver.Add( "ShowFlag", &bShow );

	return 0;
}
std::vector< std::list<uLong> > CControlSumCheckCommand::checkSums;
WORD CControlSumCheckCommand::wMask;
void CControlSumCheckCommand::Init( const WORD _wMask )
{
	checkSums.clear();
	checkSums.resize( 16 );
	wMask = _wMask;
}
void CControlSumCheckCommand::Execute( IAILogic *pAILogic )
{
	checkSums[nPlayer].push_back( ulCheckSum );
}
void CControlSumCheckCommand::Store( IDataStream *pStream )
{
	BYTE commandID = CControlSumCheckCommand::tidTypeID;
	pStream->Write( &commandID, sizeof(commandID) );
	
	pStream->Write( &nPlayer, sizeof( nPlayer ) );
	pStream->Write( &ulCheckSum, sizeof( ulCheckSum ) );
}
void CControlSumCheckCommand::Restore( IDataStream *pStream )
{
	pStream->Read( &nPlayer, sizeof( nPlayer ) );
	pStream->Read( &ulCheckSum, sizeof( ulCheckSum ) );
}
void CControlSumCheckCommand::Check( const int nOurNumber )
{
	bool bFinished = false;
	while ( !bFinished )
	{
		uLong checkSum;

		if ( checkSums[nOurNumber].empty() )
			return;
		else
			checkSum = checkSums[nOurNumber].front();
		
		int nChecks = 0;
		int nOutOfSyncs = 0;
		for ( int i = 0; i < checkSums.size() && !bFinished; ++i )
		{
			if ( wMask & ( 1UL << i ) )
			{
				if ( checkSums[i].empty() )
					bFinished = true;
				else
				{
					++nChecks;
					if ( checkSums[i].front() != checkSum )
					{
						GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_MP_PLAYER_STATE_CHANGED, (i << 8) | 1 ) );
						++nOutOfSyncs;
					}
				}
			}
		}

		if ( nOutOfSyncs > 0 )
		{
			if ( nChecks == 2 || nOutOfSyncs == nChecks - 1 )
			{
				GetSingleton<ITransceiver>()->SetTotalOutOfSync();
				GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_MP_PLAYER_STATE_CHANGED, 6 ) );
				GetSingleton<IAILogic>()->NoWin();
			}
			else if ( nChecks > 2 && nOutOfSyncs > 1 && nOutOfSyncs < nChecks - 1 )
			{
				GetSingleton<ITransceiver>()->SetTotalOutOfSync();
				GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_MP_PLAYER_STATE_CHANGED, 7 ) );
				GetSingleton<IAILogic>()->NoWin();
			}

			return;
		}

		if ( !bFinished )
		{
			for ( int j = 0; j < checkSums.size(); ++j )
			{
				if ( !checkSums[j].empty() )
					checkSums[j].pop_front();
			}
		}
	}
}
int CControlSumCheckCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &nPlayer );
	saver.Add( 2, &ulCheckSum );

	int nCheckSumsSize;
	if ( !saver.IsReading() )
		nCheckSumsSize = checkSums.size();
	saver.Add( 3, &nCheckSumsSize );

	if ( saver.IsReading() )
	{
		checkSums.clear();
		checkSums.resize( nCheckSumsSize );
	}

	return 0;
}
int CControlSumCheckCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	std::string name = "CControlSumCheckCommand";

	saver.Add( "Name", &name );
	saver.Add( "Player", &nPlayer );
	saver.Add( "CheckSum", &ulCheckSum );

	return 0;
}
void CDropPlayerCommand::Execute( IAILogic *pAILogic )
{
	pAILogic->NeutralizePlayer( nPlayerToDrop );
}
void CDropPlayerCommand::Store( IDataStream *pStream )
{
	CStreamAccessor stream = pStream;
	
	BYTE commandID = CDropPlayerCommand::tidTypeID;
	stream << commandID << nPlayerToDrop;
}
void CDropPlayerCommand::Restore( IDataStream *pStream )
{
	CStreamAccessor stream = pStream;
	stream >> nPlayerToDrop;
}
int CDropPlayerCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "PlayerID", &nPlayerToDrop );

	return 0;
}
int CDropPlayerCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nPlayerToDrop );

	return 0;
}
CScriptCallCommand::CScriptCallCommand( const int _nKind, const char *pszCall, IRefCount **pObjects, const int nLen, IAILogic *pAILogic )
: nKind( _nKind ), szCall( pszCall != 0 ? pszCall : "" )
{
	if ( nLen <= 0 || pObjects == 0 )
		return;
	// SubstituteUniqueIDs leaves each ID in a pointer-sized slot (CRegisterGroupCommand).
	std::vector<IRefCount*> buffer( pObjects, pObjects + nLen );
	pAILogic->SubstituteUniqueIDs( &(buffer[0]), nLen );
	objectIDs.reserve( nLen );
	for ( int i = 0; i < nLen; ++i )
		objectIDs.push_back( static_cast<int>( reinterpret_cast<intptr_t>( buffer[i] ) ) );
}
void CScriptCallCommand::Execute( IAILogic *pAILogic )
{
	if ( nKind == OBJECTS )
	{
		// The call the client made before the port recorded it: the objects as
		// the script reads them, pointer values (CScripts::ReturnScriptIDs).
		std::string szCommand = szCall + "( ";
		int nKnown = 0;
		int nUnknown = 0;
		for ( std::vector<int>::const_iterator it = objectIDs.begin(); it != objectIDs.end(); ++it )
		{
			IRefCount *pObj = *it > 0 ? pAILogic->GetObjByUniqueID( *it ) : 0;
			if ( pObj == 0 )
			{
				++nUnknown;
				continue;
			}
			szCommand += NStr::Format( "%s%llu", nKnown == 0 ? "" : ",", static_cast<unsigned long long>( reinterpret_cast<uintptr_t>( pObj ) ) );
			++nKnown;
		}
		szCommand += " )";
		if ( getenv( "BK_REPLAY_TRACE" ) != 0 )
			fprintf( stderr, "BK_REPLAY_TRACE: script call %s of %d objects, %d unknown\n", szCall.c_str(), int( objectIDs.size() ), nUnknown );
		if ( nKnown > 0 )
			pAILogic->CallScriptFunction( szCommand.c_str() );
	}
	else
	{
		if ( getenv( "BK_REPLAY_TRACE" ) != 0 )
			fprintf( stderr, "BK_REPLAY_TRACE: script call %s%s\n", nKind == CONSOLE ? "console " : "", szCall.c_str() );
		if ( nKind == CONSOLE )
			pAILogic->CallConsoleScriptFunction( szCall.c_str() );
		else
			pAILogic->CallScriptFunction( szCall.c_str() );
	}
}
int CScriptCallCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nKind );
	saver.Add( 2, &szCall );
	saver.Add( 3, &objectIDs );

	return 0;
}
int CScriptCallCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	std::string name = "ScriptCall";

	saver.Add( "Name", &name );
	saver.Add( "Kind", &nKind );
	saver.Add( "Call", &szCall );
	saver.Add( "Objects", &objectIDs );

	return 0;
}
void CScriptCallCommand::Store( IDataStream *pPacket )
{
	CStreamAccessor packet = pPacket;

	const BYTE commandID = CScriptCallCommand::tidTypeID;
	packet << commandID << nKind;
	const int nLength = szCall.size();
	packet << nLength;
	if ( nLength > 0 )
		pPacket->Write( szCall.data(), nLength );
	const int nNumObjects = objectIDs.size();
	packet << nNumObjects;
	if ( nNumObjects > 0 )
		pPacket->Write( &(objectIDs[0]), nNumObjects * sizeof(objectIDs[0]) );
}
void CScriptCallCommand::Restore( IDataStream *pPacket )
{
	CStreamAccessor packet = pPacket;

	// A packet is another machine's: lengths beyond any call a client makes are refused.
	int nLength = 0;
	packet >> nKind >> nLength;
	szCall.clear();
	if ( nLength > 0 && nLength <= 65536 )
	{
		szCall.resize( nLength );
		pPacket->Read( &(szCall[0]), nLength );
	}
	int nNumObjects = 0;
	packet >> nNumObjects;
	objectIDs.clear();
	if ( nNumObjects > 0 && nNumObjects <= 65536 )
	{
		objectIDs.resize( nNumObjects );
		pPacket->Read( &(objectIDs[0]), nNumObjects * sizeof(objectIDs[0]) );
	}
}
void CClientActionCommand::Execute( IAILogic *pAILogic )
{
	if ( getenv( "BK_REPLAY_TRACE" ) != 0 )
		fprintf( stderr, "BK_REPLAY_TRACE: client action %d\n", nAction );
	switch ( nAction )
	{
		case LOCK_AVIATION_APPEAR_POINT:
			pAILogic->LockAvitaionAppearPoint();
			break;
		case UNLOCK_AVIATION_APPEAR_POINT:
			pAILogic->UnlockAviationAppearPoint();
			break;
		case TOGGLE_WAR_FOG:
			pAILogic->ToggleShow( 0 );
			break;
	}
}
int CClientActionCommand::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &nAction );

	return 0;
}
int CClientActionCommand::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	std::string name = "ClientAction";

	saver.Add( "Name", &name );
	saver.Add( "Action", &nAction );

	return 0;
}
void CClientActionCommand::Store( IDataStream *pPacket )
{
	CStreamAccessor packet = pPacket;

	const BYTE commandID = CClientActionCommand::tidTypeID;
	packet << commandID << nAction;
}
void CClientActionCommand::Restore( IDataStream *pPacket )
{
	CStreamAccessor packet = pPacket;
	packet >> nAction;
}
