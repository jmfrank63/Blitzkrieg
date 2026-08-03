#ifndef __IMISSIONINTERNAL_H__
#define __IMISSIONINTERNAL_H__
#pragma ONCE
#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "../Input/Input.h"
#include "../Input/InputHelper.h"
#include "../Common/World.h"
#include "../AILogic/AILogic.h"
#include "../Formats/fmtMap.h"
#include "../Common/MapObject.h"
#include "../Common/InterfaceScreenBase.h"
#include "../UI/UI.h"
#include "iMission.h"
#include "../Main/TextSystem.h"
#include "ListControlWrapper.h"
typedef std::pair<IVisObj*, CVec2> CPickVisObj;
typedef std::list<CPickVisObj> CPickVisObjList;	// ��� ������� �� ����� �����, ������� �� �� ����� ref counting
typedef std::vector<IVisObj*> CVisObjList;
class CInterfaceMission : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CInterfaceMission );
	DECLARE_SERIALIZE;
public:
	class CMultiplayerScoresSmall
	{
		DECLARE_SERIALIZE;
		enum EScoresStateName
		{
			ESSN_NONE,
			ESSN_MP_REPLAY,
			ESSN_MP_GAME,
		};
	public:
		class CScoresState : public IRefCount
		{
			DECLARE_SERIALIZE;
			int timeBeforeCapture;
			int nSecondsBeforeCapture;
			int nGameTime;
			bool bVisible;											// is whole interface visible

		protected:
			bool bInitted;
			CPtr<IUIDialog> pDialog;
			CPtr<IGameTimer> pTimer;

			virtual void Show( IUIScreen * pUIScreen ) = 0;
			virtual void OnFrags( const int nFrags, const int nParty ) = 0;
			virtual void OnFlags( const int nFlags, const int nParty ) = 0;
			virtual void OnTimeBeforeCapture( const int nTime ) = 0;
			virtual void OnTime( const int nTime ) = 0;
			virtual void Init( IUIScreen * pUIScreen ) = 0;

		public:
			CScoresState() : timeBeforeCapture( -1 ), bVisible( false ), nSecondsBeforeCapture( -1 ), nGameTime( -1 ), bInitted( false ) {  }

			void ProcessMessage( const SGameMessage &msg, IUIScreen * pUIScreen );
			void StepLocal( IUIScreen * pUIScreen );
		};
		class CGameScoresState : public CScoresState
		{
			DECLARE_SERIALIZE;
			OBJECT_COMPLETE_METHODS( CGameScoresState );

			virtual void Show( IUIScreen * pUIScreen );
			virtual void OnFrags( const int nFrags, const int nPlayer );
			virtual void OnFlags( const int nFlags, const int nPlayer );
			virtual void OnTimeBeforeCapture( const int nTime );
			virtual void OnTime( const int nTime );
			void Init( IUIScreen *pUIScreen );
		public:
			
		};
		class CReplayScoresState : public CScoresState
		{
			DECLARE_SERIALIZE;
			OBJECT_COMPLETE_METHODS( CReplayScoresState );

			virtual void Show( IUIScreen * pUIScreen );
			virtual void OnFrags( const int nFrags, const int nPlayer );
			virtual void OnFlags( const int nFlags, const int nPlayer );
			virtual void OnTimeBeforeCapture( const int nTime );
			virtual void OnTime( const int nTime );
			void Init( IUIScreen *pUIScreen );
		public:
			
		};
	private:
		
		CPtr<CScoresState> pState;
	public:
		CMultiplayerScoresSmall() {  }
		void ProcessMessage( const SGameMessage &msg, IUIScreen * pUIScreen );
		void StepLocal( IUIScreen * pUIScreen );
		void Init();
		void Done();
	};
private:
	CMultiplayerScoresSmall multiplayerScoresSmall;
public:
	class CPlayerLaggedDialog
	{
		DECLARE_SERIALIZE;
	public:
		enum EDialogStateName
		{
			EWM_LAG,
			EWM_LOADING,
		};
		class CDialogState : public IRefCount
		{
			DECLARE_SERIALIZE;
		protected:
			bool bShowedWindow;
		public:
			CDialogState() : bShowedWindow( false ) {  }
			virtual void Hide( IUIScreen *pUIScreen ) = 0;
			virtual void AddPlayer( const int nParam, IUIScreen *pUIScreen ) = 0;
			virtual void RemovePlayer( const int nPlayer, IUIScreen *pUIScreen ) = 0;

			virtual EDialogStateName GetName() const = 0;
			virtual bool IsActive() const { return bShowedWindow; }
		};
		class CDialogStateLagged : public CDialogState
		{
			DECLARE_SERIALIZE;
			OBJECT_COMPLETE_METHODS( CDialogStateLagged );

			struct SPlayerLaggedInfo : public IRefCount
			{
				OBJECT_COMPLETE_METHODS( SPlayerLaggedInfo );
			public:
				int nPlayer;
				int nLaggedSeconds;
				SPlayerLaggedInfo() { }
				SPlayerLaggedInfo( const int _nPlayer, const int _nLaggedSeconds ) : nPlayer( _nPlayer ), nLaggedSeconds( _nLaggedSeconds ) {  }
				int GetID() const { return nPlayer; }
			};

			CListControlWrapper<SPlayerLaggedInfo, int /*nPlayer*/> players;

			void Show( IUIScreen *pUIScreen );
		public:
			virtual void Hide( IUIScreen *pUIScreen );
			virtual void AddPlayer( const int nParam, IUIScreen *pUIScreen );
			virtual void RemovePlayer( const int nPlayer, IUIScreen *pUIScreen );

			virtual EDialogStateName GetName() const { return EWM_LAG; }
		};
		class CDialogStateLoading : public CDialogState
		{
			DECLARE_SERIALIZE;
			OBJECT_COMPLETE_METHODS( CDialogStateLoading );

			struct SPlayerLoadingInfo : public IRefCount
			{
				OBJECT_COMPLETE_METHODS( SPlayerLoadingInfo );
			public:
				int nPlayer;
				SPlayerLoadingInfo() { }
				SPlayerLoadingInfo( const int _nPlayer ) : nPlayer( _nPlayer ) {  }
				int GetID() const { return nPlayer; }
			};
			
			CListControlWrapper<SPlayerLoadingInfo, int /*nPlayer*/> players;

			void Show( IUIScreen *pUIScreen );
		public:
			virtual void Hide( IUIScreen *pUIScreen );
			virtual void AddPlayer( const int nParam, IUIScreen *pUIScreen );
			virtual void RemovePlayer( const int nPlayer, IUIScreen *pUIScreen );

			virtual EDialogStateName GetName() const { return EWM_LOADING; }
		};
	private:
		CPtr<CDialogState> pState;
	public:
		void ProcessMessage( const SGameMessage &msg, IUIScreen * pUIScreen );
		void StepLocal( IUIScreen * pUIScreen );
	};
private:
	CPlayerLaggedDialog laggedDialog;
	class CTimeoutDialog
	{
		DECLARE_SERIALIZE;
	public:
		CTimeoutDialog() {  }

		void ProcessMessage( const SGameMessage &msg, IUIScreen * pUIScreen );
		void StepLocal( IUIScreen * pUIScreen );
	};
	CTimeoutDialog timeoutDialog;
	CPtr<IClientAckManager> pAckManager;	// acknowledgements manager
	CPtr<IAILogic> pAILogic;							// singleton AI logic shortcut
	CPtr<IFrameSelection> pFrameSelection;	// frame selection - shortcut from scene
	CObj<IWorldClient> pWorld;						// world - all game data
	CVec3 vCameraStartPos;								// camera start position
	std::string szCurrMapName;						// map name
	bool bCycledLaunch;										// cycled launch or normal
	CVisObjList preselectedObjects;
	CPickVisObjList selectedObjects;			// currently picked objects
	CPickVisObjList::iterator itCurrSelected;
	CPickVisObj *pSelectedObject;
	CPtr<IGFXText> pTextPause;						// pause
	bool bForceRotation;
	int nStartPauseCounter;								// mission start pause counter
	NInput::CCommandRegistrator missionMsgs;
	bool bEditMode;												// edit mode (obsolete)
	bool bEnableStatistics;								// enable statistics drawing
	int nDirection;												// rotation direction
	CVec3 vLastAnchor;										// last camera anchor for sync
	int nFPSAveragePeriod;
	virtual bool OpenCurtains();
	void BeginSelection( const CVec2 &vPos );
	void EndSelection( const CVec2 &vPos );
	bool PickObjects( CPickVisObjList *pPickedObjects, const CVec2 &pos, EObjGameType type, bool bVisible = false );
	bool PickObjects( CPickVisObjList *pPickedObjects, const CTRect<float> &rect, EObjGameType type, bool bVisible = false );
	void ResetSelection( IVisObj *pObj = 0 );
	void ResetSelectionLocal();
	void ResetPreSelection();
	bool PreSelectObjects( const CVec2 &pos, EObjGameType type );
	bool PreSelectObjects( const CTRect<float> &rect, EObjGameType type );
	void PreSelectObjects( const CPickVisObjList &picked );
	void AddPreSelectedObjects();

	bool SelectFirstObject( const CVec2 &pos );
	void SelectNextObject();
	bool DropObject( const CVec2 &pos );
	void DoAction( const SGameMessage &msg );
	void BeginAction( const SGameMessage &msg );
	void GetPos3( CVec3 *pPos, const CVec2 &pos );
	void GetPos3( CVec3 *pPos, float x, float y );
	void VisualizeFeedback( const int nFeedBack, const int nParam );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool STDCALL ProcessMessageLocal( const SGameMessage &msg );
	virtual bool STDCALL StepLocal( bool bAppActive );
	virtual void STDCALL DrawAdd();
	virtual bool STDCALL OnCursorMove( const CVec2 &vPos );
	void SetMissionStatusObject( bool bStatus );
	bool MakeMapShot();
	virtual ~CInterfaceMission();
public:
	CInterfaceMission();
	virtual bool STDCALL Init();
	virtual void STDCALL Done();
	virtual void STDCALL OnGetFocus( bool bFocus );
	bool NewMission( const std::string &_szMapName, bool _bCycledLaunch );
	void CheckResolution();
	void ConfigureInterfacePreferences();
};
class CICMission : public CInterfaceCommandBase<CInterfaceMission, MISSION_INTERFACE_MISSION>
{
	OBJECT_NORMAL_METHODS( CICMission );
	DECLARE_SERIALIZE;
	std::string szMapName;
	bool bCycledLaunch;
	virtual void PreCreate( IMainLoop *pML );
	virtual void PostCreate( IMainLoop *pML, CInterfaceMission *pInterface );
	CICMission() : bCycledLaunch( false ) {  }
public:
	virtual void STDCALL Configure( const char *pszConfig );
};
#endif // __IMISSIONINTERNAL_H__
