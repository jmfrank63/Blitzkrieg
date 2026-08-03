#ifndef __SINGLEPLAYERTRANSCEIVER_H__
#define __SINGLEPLAYERTRANSCEIVER_H__
#pragma ONCE
#include "Transceiver.h"
#include "../AILogic/AILogic.h"
interface ICommandsHistory;
class CSinglePlayerTransceiver : public CTRefCount<ITransceiver>
{
	OBJECT_SERVICE_METHODS( CSinglePlayerTransceiver );
	DECLARE_SERIALIZE;
	CPtr<IAILogic> pAILogic;							// shortcut to AI logic
	
	CPtr<ICommandsHistory> pCmdsHistory;
	long nCommonSegment;
	bool bHistoryPlaying;
public:
	virtual void STDCALL Init( ISingleton *pSingleton, const int nMultiplayerType );
	virtual void STDCALL Done() { }
	virtual void STDCALL PreMissionInit();
	virtual void SetLatency( int nSegments ) {  }
	virtual void STDCALL DoSegments();
	virtual int STDCALL CommandRegisterGroup( IRefCount **pUnitsBuffer, const int nLen );
	virtual void STDCALL CommandUnregisterGroup( const WORD wGroup );
	virtual void STDCALL CommandGroupCommand( const SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue );
	virtual int STDCALL CommandUnitCommand( const struct SAIUnitCmd *pCommand );
	virtual void STDCALL CommandShowAreas( int nGroupID, int nAreaType, bool bShow );
	virtual void STDCALL AddCommandToSend( IAILogicCommand *pCommand );	

	virtual int STDCALL GetNumberOfPlayers() const { return 1; }
	
	virtual bool STDCALL JoinToServer( const char *pszIPAddress, const int nPort, bool bPasswordRequired, const char* pszPassword ) { return false; }
	virtual void STDCALL CreateServer() { }
	virtual void STDCALL InitByCreateServersList() { }
	
	virtual void STDCALL CommandClientTogglePause();
	virtual void STDCALL CommandClientSpeed( const int nChange );
	virtual void STDCALL CommandClientDropPlayer( const WORD *pszPlayerNick ) { }
	
	virtual void STDCALL CommandTimeOut( const bool bSet ) { }
	
	virtual NTimer::STime STDCALL GetMultiplayerTime() { return 0; }

	virtual void STDCALL LoadAllGameParameters();
	
	virtual void STDCALL SetTotalOutOfSync() { }
	
	virtual void STDCALL GameFinished() { }
};
#endif // __SINGLEPLAYERTRANSCEIVER_H__
