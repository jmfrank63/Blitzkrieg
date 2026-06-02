#ifndef __INTERFACESCREENBASE_H__
#define __INTERFACESCREENBASE_H__
#include "..\Main\iMain.h"
#include "..\GFX\GFX.h"
#include "..\SFX\SFX.h"
#include "..\Input\Input.h"
#include "..\Scene\Scene.h"
#include "..\UI\UI.h"
#include "..\Misc\HPTimer.h"
#include "..\Main\TextSystem.h"
template <class TInterface, int NInterfaceTypeID>
class CInterfaceCommandBase : public IInterfaceCommand
{
	NTimer::STime timeDelayed;
protected:
	CInterfaceCommandBase() : timeDelayed( 0 ) {  }
	virtual ~CInterfaceCommandBase() {  }
	virtual void PreCreate( IMainLoop *pML ) {  }
	virtual void PostCreate( IMainLoop *pML, TInterface *pInterface ) {  }
public:
	virtual void STDCALL Exec( IMainLoop *pML )
	{
		PreCreate( pML );
		TInterface *pInterface = CreateObject<TInterface>( NInterfaceTypeID );
		pInterface->Init();
		pML->ClearResources();
		pInterface->StartInterface();
		PostCreate( pML, pInterface );
	}
	virtual void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	virtual NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
};
class CInterfaceScreenBase : public IInterfaceBase
{
	DECLARE_SERIALIZE;

	int nHelpContextNumber;								// for intermisssion intefaces help

	CVec2 vLastCursorPos;									// last cursor movement position
	bool bLastCursorScreenMoveRes;				// 
	bool bEnableStatistics;								// enable to show stats
	NHPTimer::STime time;									// stats collecting precision time
	float fTotalTime;											// total time since start
	int nFrameCounter;										// total frame counter since start
	int nTriCounter;											// triangle counter
	float fAveFPS;												// average FPS (for a last second)
	float fAveTPS;												// average TPS -~-
	int nCPUFreq;													// main CPU frequence
	std::list<SGameMessage> messages;			// сообщения наверх
	std::string szBindSection;						// this interface bind section
	const std::string szInterfaceType;		// interface type - "InterMission", "Mission"
	CPtr<IText> pLastToolTip;							// last setuped tooltip
	NTimer::STime timeToolTip;						// time, last tooltip appeared
	NTimer::STime timeToolTipShowTime;		// time to show tooltip in the case of the cursor inactivity
	NTimer::STime timeToolTipHideTime;		// time to hide tooltip in the case of the cursor inactivity

	bool bInterfaceClosed;
	CPtr<IUIScreen> pStoredScreen;				// stored UI screen (from under-interface)
	int PlayOverInterface( const char *pszName, const DWORD dwAddFlags, const bool bFadeIn );
	void EnableMessageProcessingDelayed( const bool bEnable, const NTimer::STime &timeToPerform );
	void RestoreScreen();
protected:
	CPtr<IGFX> pGFX;
	CPtr<ISFX> pSFX;
	CPtr<IInput> pInput;
	CPtr<IScene> pScene;
	CPtr<ICamera> pCamera;
	CPtr<ICursor> pCursor;
	CPtr<IGameTimer> pTimer;
	CObj<IUIScreen> pUIScreen;
	bool ToggleShowStats() { bEnableStatistics = !bEnableStatistics; return bEnableStatistics; }
	void AddStatistics();
	bool ProcessAndAdd( const SGameMessage &msg )
	{
		if ( !ProcessMessage( msg ) )
			AddMessage( msg );
		return messages.empty();
	}
	void AddMessage( const SGameMessage &msg );
	void SetBindSection( const char *pszBindSection ) { szBindSection = pszBindSection; }
	bool ChangeResolution();
	void AddDelayedCommand( IInterfaceCommand *pCmd, const NTimer::STime &timeToPerform );
	void RemoveTransition() { if ( pScene ) pScene->RemoveSceneObject( 0 ); }
	virtual bool OpenCurtains();
	void OpenCurtainsForced();
	void StoreScreen();
	virtual void SuspendAILogic( bool bSuspend );
	virtual bool STDCALL StepLocal( bool bAppActive ) { return bAppActive; }
	virtual void STDCALL DrawAdd() {  }
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg ) = 0;
	virtual bool STDCALL OnCursorMove( const CVec2 &vPos );
	void SetTutorialNumber( const int _nHelpContext ) { nHelpContextNumber = _nHelpContext; }
	void ShowTutorialIfNotShown();
	bool ShowTutorial();
	void CloseInterface( const bool bCurtains = false );
	virtual ~CInterfaceScreenBase() {  }
public:
	CInterfaceScreenBase( const std::string &_szInterfaceType );
	virtual bool STDCALL Init();
	virtual void STDCALL Done();
	virtual void STDCALL Step( bool bAppActive );
	virtual bool STDCALL ProcessUIMessage( const SGameMessage &msg );
	virtual bool STDCALL ProcessTextMessage( const STextMessage &msg );
	virtual bool STDCALL GetMessage( SGameMessage *pMsg );
	virtual void STDCALL OnGetFocus( bool bFocus );
	virtual void STDCALL StartInterface();
	virtual int STDCALL FinishInterface( IInterfaceCommand *pCmdNextInterface );
	virtual int STDCALL FinishInterface( const int nInterfaceCommandTypeID, const char *pszCommandConfig );
	virtual void STDCALL SetWindowText( const int nElementID, IText *pText );
	virtual void STDCALL SetWindowText( const int nElementID, const WORD *pszText );
};
#endif // __INTERFACESCREENBASE_H__
