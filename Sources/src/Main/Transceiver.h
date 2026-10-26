#ifndef __TRANSCEIVER_H__
#define __TRANSCEIVER_H__
#pragma ONCE
#include "../Main/iMainClassIDs.h"
// What ITransceiver::CommandScriptCall's text is (CScriptCallCommand).
enum EScriptCallKind
{
	SCRIPT_CALL						= 0,					// the whole call, e.g. "CallHQ()"
	SCRIPT_CALL_OBJECTS		= 1,					// a function's name; the objects are its arguments
	SCRIPT_CALL_CONSOLE		= 2,					// a console script line, under the AI's cheat gate
};
interface ITransceiver : public IRefCount
{
	enum { tidTypeID = MAIN_TRANSCEIVER };
	virtual void STDCALL Init( ISingleton *pSingleton, const int nMultiplayerType ) = 0;
	virtual void STDCALL Done() = 0;
	virtual void STDCALL PreMissionInit() = 0;
	virtual void SetLatency( int nSegments ) = 0;
	virtual void STDCALL DoSegments() = 0;
	virtual int STDCALL CommandRegisterGroup( IRefCount **pUnitsBuffer, const int nLen ) = 0;
	virtual void STDCALL CommandUnregisterGroup( const WORD wGroup ) = 0;
	virtual void STDCALL CommandGroupCommand( const struct SAIUnitCmd *pCommand, const WORD wGroup, bool bPlaceInQueue ) = 0;
	virtual int STDCALL CommandUnitCommand( const struct SAIUnitCmd *pCommand ) = 0;
	virtual void STDCALL CommandShowAreas( int nGroupID, int nAreaType, bool bShow ) = 0;
	virtual void STDCALL AddCommandToSend( interface IAILogicCommand *pCommand ) = 0;
	// The player's input that reaches the AI other than as an order, recorded like
	// the orders (CScriptCallCommand, CClientActionCommand): a call into the
	// mission script (nKind is an EScriptCallKind), the lock of the
	// aviation appear point while planes are picked, the war fog cheat.
	virtual void STDCALL CommandScriptCall( const int nKind, const char *pszCall, IRefCount **pObjects, const int nLen ) = 0;
	virtual void STDCALL CommandLockAviationAppearPoint( class CVec2 *pvAppearPoint ) = 0;
	virtual void STDCALL CommandUnlockAviationAppearPoint() = 0;
	virtual void STDCALL CommandToggleWarFog() = 0;
	
	virtual int STDCALL GetNumberOfPlayers() const = 0;
	
	virtual bool STDCALL JoinToServer( const char *pszIPAddress, const int nPort, bool bPasswordRequired, const char* pszPassword ) = 0;
	virtual void STDCALL CreateServer() = 0;
	virtual void STDCALL InitByCreateServersList() = 0;
	
	virtual void STDCALL CommandClientTogglePause() = 0;
	virtual void STDCALL CommandClientSpeed( const int nChange ) = 0;
	virtual void STDCALL CommandClientDropPlayer( const WORD *pszPlayerNick ) = 0;
	
	virtual void STDCALL CommandTimeOut( const bool bSet ) = 0;
	
	virtual NTimer::STime STDCALL GetMultiplayerTime() = 0;

	virtual void STDCALL LoadAllGameParameters() = 0;
	
	virtual void STDCALL SetTotalOutOfSync() = 0;

	virtual void STDCALL GameFinished() = 0;
	
	virtual interface INetDriver* STDCALL GetInGameNetDriver() const { return 0; }
};
#endif // __TRANSCEIVER_H__
