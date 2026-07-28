#ifndef __IM_STATS_H__
#define __IM_STATS_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
#include "..\Main\ScenarioTrackerTypes.h"
#include "..\Main\ScenarioTracker.h"
class CInterfaceStats : public CInterfaceInterMission
{
	DECLARE_SERIALIZE;
	OBJECT_NORMAL_METHODS( CInterfaceStats );
public:
	enum EPartyInfoType
	{
		E_NONE,
		E_SUMMARY,
		E_BEST_NO_PLAYER_DISPLAY,
		E_SUMMARY_DEVIDED_BY_1000,
	};

	enum EStatisticsGameType
	{
		EST_BOTH,
		EST_MULTIPLAYER,
		EST_SINGLEPLAYER,
	};

	class CSorter : public IUIListSorter 
	{
		OBJECT_NORMAL_METHODS( CSorter );
	public:
		virtual bool STDCALL operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const;
	};

	typedef std::pair< CPtr<IPlayerScenarioInfo>, CPtr<IScenarioStatistics> > CPlayerFullInfo;
	typedef std::vector< CPlayerFullInfo > CPlayersFullInformation;

	struct SStatConfugure
	{
		/*EScenarioTrackerMissionTypes*/ int eType[2];
		/*EStatisticsGameType*/ int eStatType;
		/*EPartyInfoType*/ int eAccumulateType;			// type of accumulate party stats
		int nIndexToCountBest; //best will be count by this value
		bool bLeaderIsGreatest;							// leader is count by greatest value if true (otherwise by smallest)

		SStatConfugure() {  }
		SStatConfugure( const  int _eStatType,
										const /*EPartyInfoType*/ int _eAccumulateType, 
										const int _eType1, const int _eType2 = -1,
										const int _nIndexToCountBest = 0,
										const bool bGreatest = true );
		
		bool IsToDisplay( const bool bMultiplayerGame ) const;
	};

	struct SPlayerStatInfo
	{
		double fVal[2];

		void Init( const CPlayerFullInfo &playerInfo, const SStatConfugure &config );
		std::wstring GetValue( const SStatConfugure &config ) const;
		std::wstring GetValueToSort( const SStatConfugure &config ) const;
	};

	class SPartyInfo
	{
		double fVal[2];											// summ val for party
		double fBestVal[2];									// best is by the first val.
		int nBestIndex;											// index of best player
	public:
		SPartyInfo() : nBestIndex( -1 ) { }

		void Init( const CPlayersFullInformation &info, const SStatConfugure &config );
		std::wstring GetPartyVal( const SStatConfugure &config ) const;
		std::wstring GetValForSort( const SStatConfugure &config ) const;
		int GetLeader() const { return nBestIndex; }	// leader of that statistics
		double GetLeaderValue( const SStatConfugure &config ) const { return fBestVal[config.nIndexToCountBest]; }
	};

	class CCommonStats
	{
	private:
		int nTime;
		int nObjectivesCompleted;
		int nDifficulty;
		int nUpgrades;
		int nSaves;
	public:
		void Init ( const CPlayerFullInfo &info );
		int GetNStats( const bool bMultiplayer ) const;
		std::string GetStatTitleKey( const int nIndex ) const;
		std::wstring GetStatValue( const int nIndex ) const;
	};
private:

	CPtr<CSorter> pSorter;
	std::vector<SStatConfugure> playerStatsConfigure;

	std::vector<CPlayersFullInformation> playerInfos;
	
	NInput::CCommandRegistrator commandMsgs;

	std::vector< CPtr<IUIListControl> > pPartyList;

	bool bPopupsShowed;		


	bool bTutorialWindow;
	bool bCheckReplay;
	int m_nStatsType;
	int nMedalIterator;			//������� ������ ������
	bool bStatsShown;
	bool bUpgradesShown;
	bool bNewUnitsShown;
	bool bNextChapterShown;
	bool bPlayerRankShown;
	bool bLastFullScreen;		//���������� ��� ������ ������, ���� true �� ��������� ��� ����������� full screen interface

	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceStats();
	CInterfaceStats() : CInterfaceInterMission( "InterMission" ), nMedalIterator( 0 ), bStatsShown( 0 ),
		bUpgradesShown( 0 ), bNewUnitsShown( 0 ), bNextChapterShown( 0 ), m_nStatsType( 0 ), bPlayerRankShown( 0 ),
		bCheckReplay( 0 ), bLastFullScreen( 0 ), bPopupsShowed( 0 ), bTutorialWindow( 0 ), playerInfos( 2 ), pPartyList( 2 ) {}

	void InitSorter();
	void AquireLists();
	void CollectPlayerStats( interface IPlayerScenarioInfo *pPlayer, interface IScenarioStatistics *pStats );
	void RepositionList();
	void FillCommonStatsList( const bool bMultiplayer, const CInterfaceStats::CCommonStats &commonStats );

	void OnDemoversionExit();

public:
	virtual bool STDCALL Init();
	virtual void STDCALL Done();
	virtual void STDCALL OnGetFocus( bool bFocus );
	void Create( const int nStatsType );
};
class CICStats : public CInterfaceCommandBase<CInterfaceStats, MISSION_INTERFACE_STATS>
{
	OBJECT_NORMAL_METHODS( CICStats );
	
	int /*EStatsComplexity*/ nCurrentMissionStats;

	virtual void PreCreate( IMainLoop *pML );
	virtual void PostCreate( IMainLoop *pML, CInterfaceStats *pIS );
	CICStats() {  }
public:
	virtual void STDCALL Configure( const char *pszConfig );
};
#endif		//__IM_STATS_H__
