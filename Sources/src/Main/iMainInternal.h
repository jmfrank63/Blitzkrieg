#ifndef __IMAININTERNAL_H__
#define __IMAININTERNAL_H__
#pragma ONCE
#include "iMain.h"
#include "../Input/Input.h"
#include "../Input/InputHelper.h"
#include "../GFX/GFX.h"
#include "../Scene/Scene.h"
#include "../AILogic/AILogic.h"
#include "../Misc/HPTimer.h"
#include "TextSystem.h"
#include "ScenarioTracker.h"
class CMainLoop : public IMainLoop
{
	OBJECT_MINIMAL_METHODS( CMainLoop );
	typedef DWORD STime;
	typedef std::list< CPtr<IInterfaceCommand> > CInterfaceCommandsList;
	typedef std::list< CPtr<IInterfaceBase> > CInterfacesList;
	typedef std::vector< CPtr<ISharedManager> > CManagersList;
	CPtr<IGFX> pGFX;
	CPtr<IInput> pInput;
	CPtr<IScene> pScene;
	CPtr<ICamera> pCamera;
	CPtr<ICursor> pCursor;
	CPtr<IAILogic> pAILogic;
	CInterfaceCommandsList cmds;					// interface (inter-frame) commands
	CInterfacesList interfaces;						// interfaces
	CManagersList managers;								// data managers (for save/load)
	bool bAppIsActive;										// is app active
	bool bWireFrame;											// wireframe mode
	bool bTextMode;												// text input mode
	bool bPaused;													// is app paused
	bool bDisableMessageProcessing;				// disable message processing
	bool bDeferResourcePurge;							// suppress the per-PopInterface unreferenced-purge (save-load keeps resources for merge reuse)
	int nResumeStreamingSteps;						// >0: resume ISFX streaming after this many StepApp frames
	std::string szBaseDir;
	int nAutoSavePeriod;									// auto save period (in msec)
	NTimer::STime timeLastAutoSave;				// last autosave time
	int nGuaranteeFPS;										// 
	int nGuaranteeFPSTime;								//
	int nNetAppID;												// app id to achive different apps on one port
	int nNetPort;													// network socket port
	CPtr<IImage> pScreenShotImage;
	CPtr<IScenarioTracker> pStoredScenarioTracker;
	NInput::CCommandRegistrator standardMsgs;
	void ProcessStandardMsgs( const SGameMessage &msg );
	void OnMultiplayerStateCommand( const SGameMessage &msg );
	virtual ~CMainLoop();
public:
	CMainLoop();
	const char* STDCALL GetBaseDir() const { return szBaseDir.c_str(); }
	void STDCALL ConfigureNet( const int nAppID, const int nPort ) { nNetAppID = nAppID; nNetPort = nPort; }
	bool STDCALL StepApp( bool bActive );
	void STDCALL Command( IInterfaceCommand *pCommand );
	void STDCALL Command( int nCommandID, const char *pszConfiguration );
	void STDCALL ResetStack();
	void STDCALL SetInterface( IInterfaceBase *pNewInterface );
	void STDCALL PushInterface( IInterfaceBase *pNewInterface );
	void STDCALL PopInterface();
	IInterfaceBase* STDCALL GetInterface() const;
	void STDCALL Pause( const bool _bPause, const int _nPauseReason );
	bool STDCALL IsPaused() const { return bPaused; }
	void STDCALL EnableMessageProcessing( const bool bEnable );
	void STDCALL ClearResources( const bool bClearAll );
	// Not part of IMainLoop: save-load (CICLoad) pops the whole interface
	// stack and then deserializes; purging per pop would empty the shared
	// managers right before their SDSM_MERGE deserialize could reuse the
	// resident resources (same-mission reload = zero disk I/O). The load
	// path defers the purge and runs one ClearResources(false) afterwards.
	void SetDeferResourcePurge( const bool bDefer ) { bDeferResourcePurge = bDefer; }
	void SetResumeStreamingAfterSteps( const int nSteps ) { nResumeStreamingSteps = Max( 0, nSteps ); }
	void STDCALL StoreScenarioTracker();
	void STDCALL RestoreScenarioTracker();
	void STDCALL SerializeConfig( const bool bRead, const DWORD dwSerialize );
	void STDCALL Serialize( IStructureSaver *pSS, interface IProgressHook *pHook );
};
struct SProgressMovieInfo
{
	std::string szMovieName;
	std::string szTextSource;
	CVec2 vTextTop;
	CVec2 vTextBottom;
	int nTextAlign;
	DWORD dwTextColor;
	int nFontSize;

	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "MovieName", &szMovieName );
		saver.Add( "TextSource", &szTextSource );
		saver.Add( "TextTop", &vTextTop );
		saver.Add( "TextBottom", &vTextBottom );
		saver.Add( "TextColor", &dwTextColor );
		saver.Add( "FontSize", &nFontSize );
		saver.Add( "TextAlign", &nTextAlign );
		return 0;
	}
};
class CProgressScreen : public IMovieProgressHook
{
	OBJECT_NORMAL_METHODS( CProgressScreen );
	int nNumSteps;                        // ����� ����� �����
	int nCurrentStep;                     // ������� ���
	int nNumFrames;                       // ����� ����� ������
	int nCurrFrame;                       // ������� ����
	int nMaxFrame;												// max frame to play
	CPtr<IGFX> pGFX;                      // ���� ������
	CPtr<IVideoPlayer> pVP;               // ��� ������
	CPtr<IGFXText> pGFXText;              // ����� ��� ��������� ������ ��������
	DWORD dwTextColor;                    // ���� ������
	CTRect<float> wndRect;
	int nTextAlign;
	int nFontSize;
	void Draw();
	void SetText( const SProgressMovieInfo *pInfo );
public:
	CProgressScreen() 
		: nNumSteps( 0 ), nCurrentStep( 0 ), nMaxFrame( 0 ), nFontSize( 1 ) {  }

	void Init( EProgressType nType );
	void Init( const std::string &szMovieName );
	void Stop();
	void STDCALL SetNumSteps( const int nRange, const float fPercentage = 1.0f );
	void STDCALL Step();
	void STDCALL Recover();
	void STDCALL SetCurrPos( const int nPos );
	int STDCALL GetCurrPos() const;
};
#endif // __IMAININTERNAL_H__
