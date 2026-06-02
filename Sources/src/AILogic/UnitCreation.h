#ifndef _UNIT_CREATION_INTERNAL_
#define _UNIT_CREATION_INTERNAL_
#pragma ONCE
enum EGunplaneCalledAs
{
	EGCA_GUNPLANE,
	EGCA_DIVEBOMBER,
};
class CUnitCreation
{
	DECLARE_SERIALIZE;
	CPtr<IObjectsDB> pIDB;
	interface IPlaneCreation 
	{
		virtual const CVec2 &GetDestPoint() const = 0;
		virtual enum EActionCommand GetCommand() const = 0;
		virtual int GetNParam() const = 0;
		virtual void CalcPositions( const int nMax,
																const CVec2 & vAABBbox,
																const CVec2 & vDirection,
																std::vector<CVec2> *positions,
																CVec2 * pvOffset, const bool bRandom = false ) = 0;

	};
	class CPlaneCreation : public IPlaneCreation
	{
		CVec2 vDestPoint;
		enum EActionCommand eCmd;
		int nParam;
	public:
			CPlaneCreation( const CVec2 &vDestPoint, const enum EActionCommand eCmd, const int _nParam )
				: vDestPoint( vDestPoint ), eCmd( eCmd ), nParam( _nParam ) {  }
		virtual const CVec2 &GetDestPoint() const { return vDestPoint; }
		virtual enum EActionCommand GetCommand() const { return eCmd; }
		virtual int GetNParam() const { return nParam; }
	};
	class CLightPlaneCreation : public CPlaneCreation
	{
	public:
		CLightPlaneCreation( const CVec2 &vDestPoint, const enum EActionCommand eCmd, const int _nParam = 0 )
			: CPlaneCreation( vDestPoint, eCmd, _nParam ) {  }
		virtual void CalcPositions( const int nMax,
																const CVec2 & vAABBbox,
																const CVec2 & vDirection,
																std::vector<CVec2> *positions,
																CVec2 * pvOffset, const bool bRandom = false );
	};
	class CHeavyPlaneCreation : public CPlaneCreation
	{
		bool bNeedFormation;
	public:
		CHeavyPlaneCreation( const CVec2 &vDestPoint, const enum EActionCommand eCmd, const bool _bNeedFormation = false, const int _nParam = 0 )
			: CPlaneCreation( vDestPoint, eCmd, _nParam ), bNeedFormation( _bNeedFormation ) {  }
		virtual void CalcPositions( const int nMax,
																const CVec2 & vAABBbox,
																const CVec2 & vDirection,
																std::vector<CVec2> * positions,
																CVec2 * pvOffset, const bool bRandom = false );
	};

public:
	struct STankPitInfo
	{
		int operator&( IDataTree &ss );
		std::vector<std::string> sandBagTankPits;
		std::vector<std::string> digTankPits;
		const char * GetRandomTankPit( const class CVec2 &vSize, const bool bCanDig, float *pfResize ) const;
	};
	struct SPartyDependentInfo
	{
		std::string szPartyName;								// имя страны
		std::string szGeneralPartyName;					// имя General Side

		std::string szParatroopSoldierName;			// имя модельки, которая подменяет паращютиста
		std::string szGunCrewSquad;							// артиллеоисты
		std::string szHeavyMGSquad;							// пулеметчики
		std::string szResupplyEngineerSquad;		// грузчики у грузовиков с ресурсами
		int operator&( IDataTree &ss )
		{
			CTreeAccessor tree = &ss;
			tree.Add( "PartyName", &szPartyName );
			tree.Add( "GeneralPartyName", &szGeneralPartyName );

			tree.Add( "GunCrewSquad", &szGunCrewSquad );
			tree.Add( "HeavyMachinegunSquad", &szHeavyMGSquad );
			tree.Add( "ParatrooperSoldier", &szParatroopSoldierName );
			tree.Add( "ResupplyEngineerSquad", &szResupplyEngineerSquad );
			return 0;
		}
	};

	struct SCommonInfo
	{
		std::vector<std::string> antitankObjects;	// противотанковые заграждения
		std::string szAPFence;									// протиавопехотное проволочное заграждение
		std::string szMineAT;										// противотанковая мина
		std::string szMineAP;										// противопехотная мина
		std::string szEntrenchment;							// окоп
		int operator&( IDataTree &ss );
	};
	struct SFeedBack
	{
		int eEnable, eDisable;		
		SFeedBack( const int eEnable, const int eDisable )
			: eEnable( eEnable ), eDisable( eDisable ) {  }
	};
	std::vector<SFeedBack> feedbacks;

	struct SLocalInGameUnitCreationInfo
	{
		DECLARE_SERIALIZE;
	public:
		struct SPlaneInfo
		{
			DECLARE_SERIALIZE;
		public:
			std::string		szName;							// this 3 parameters enables planes
			int						nFormation;					// due to map.
			int						nPlanes;

			bool					bEnabledScript;						// для активации/деактивации по скрипту
			SPlaneInfo() : nFormation( 0 ), bEnabledScript ( true ), nPlanes( 0 ) { }
		};
		
		std::vector<SPlaneInfo> planes;
		
		std::string		szParatrooper;										// название парашютистов
		int nParadropSquadCount;												// количество сквадов паращютистов
		NTimer::STime timeLastCall;											// последний вызов самолетов
		NTimer::STime timeRelax;												// интервал вызова самолетов

		std::string szPartyName;												// название страны
		int nLastCalledAviaType;												// last called aviation type

		std::vector<CVec2> vAppearPoints;		// точки возможного появления (координата аэродрома)

		SLocalInGameUnitCreationInfo & operator=( const struct SUnitCreation &rSUnitCreation );
		SLocalInGameUnitCreationInfo( const struct SUnitCreation &rSUnitCreation );
		void Copy( const struct SUnitCreation &rSUnitCreation );
		SLocalInGameUnitCreationInfo() {  }
	};

private:
	int nAviationCallNumeber;
	bool bInit;														// for delaying initialization untill segment
	bool bMainButtonDisabled;

	std::vector<BYTE> bForceDisabled;						// авиация вообще не включается

	std::vector<SLocalInGameUnitCreationInfo> inGameUnits;
	std::vector<BYTE> bLockedFlags;
	std::vector<CVec2> vLockedAppearPoints;

	std::vector<SPartyDependentInfo> partyDependentInfo;
	SCommonInfo commonInfo;
	STankPitInfo tankPitInfo;							

	void CalcPositionsForHeavyPlanes( int nMax, const CVec2 & box, const CVec2 & direction, std::vector<CVec2> * positions, CVec2 * offset, bool bRandom =false )const;

	void DisableMainAviationButton( NTimer::STime time );
	void EnableAviationButtons( const bool bInit = false );

	void InitPlanePath( class CCommonUnit * pUnit, const CVec3 &vAppearPoint, const CVec3 &vGoToPoint );

	void InitConsts();
	const SPartyDependentInfo & GetPartyDependentInfo( const int nDipl ) const;
	void RegisterAviationCall( const int nPlayer, const int nAviaType );
	void CallPlane( const int nPlayer,
									 const int /*SUCAviation::AIRCRAFT_TYPE*/ nAviaType,
									 const WORD wGroupID,
									 CUnitCreation::IPlaneCreation * pCreation );

	bool IsAviaEnabledScript( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/nAvia ) const ;
	bool IsAviaEnabledMapParameters( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/nAvia ) const;

public:
	CUnitCreation();
	void Init();
	void Init( const struct SUnitCreationInfo &info );
	void Clear();
	
	const SLocalInGameUnitCreationInfo::SPlaneInfo& GetPlaneInfo( const int nPlayer, const int nAvia )
	{
		return inGameUnits[nPlayer].planes[nAvia];
	}
	bool IsAviaEnabled( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/nAvia ) const;


	int AddNewUnit( const std::string &name, IObjectsDB *pIDB, const float fHPFactor, const int x, const int y, const int z, const WORD dir, const BYTE player, bool bInitialization, bool IsEditor = false, bool bSendToWorld = true ) const;
	int AddNewUnit( const SUnitBaseRPGStats *pStats, const float fHPFactor, const int x, const int y, const int z, const WORD dbID, const WORD _dir, const BYTE player, const EObjVisType eVisType, bool bInitialization = false, bool bSendToWorld = true, bool IsEditor = false ) const;
	void GetCentersOfAllFormationUnits( const SSquadRPGStats *pStats, const CVec2 &vFormCenter, const WORD wFormDir, const int nFormation, const int nUnits, std::list<CVec2> *pCenters ) const;
	class CCommonUnit* AddNewFormation( const SSquadRPGStats *pStats, const int nFormation, const float fHP, const float x, const float y, const float z, const WORD wDir, const int nDiplomacy, bool bInitialization = false, bool bSendToWorld = true, const int nUnits = -1 ) const;
	class CCommonUnit* CreateSingleUnitFormation( class CSoldier *pSoldier ) const;
	
	void CallBombers( const struct SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl );
	void CallFighters( const struct SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl );
	void CallScout( const struct SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl );
	void CallParadroppers( const struct SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl );
	void CallShturmoviks( const SAIUnitCmd &unitCmd, const WORD wGroupID, int nDipl );

	void CreateMine( const enum SMineRPGStats::EType nType, const class CVec2 &vPoint, const int nDipl );
	class CFormation* CreateParatroopers( const class CVec3 &where, class CAIUnit * pPlane, const int nScriptID ) const;
	class CFormation* CreateResupplyEngineers( class CAITransportUnit *pUnit ) const;
	CFormation * CreateCrew( class CArtillery *pUnit, IObjectsDB *_pIDB = 0, const int nUnits = -1, const CVec3 vPos = CVec3(-1,-1,-1), const int nPlayer = -1, const bool bImmidiateAttach = true ) const;

	void SendFormationToWorld( CFormation * pUnit ) const;

	IObjectsDB* GetObjectDB() { return pIDB; }

	void LockAppearPoint( const int nPlayer, const bool bLocked );
	CVec2 GetRandomAppearPoint( const int nPlayer, const bool bLeave = false ) const;
	const SMechUnitRPGStats* GetPlaneStats( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/ nAvia ) const;
	const float GetPlaneFlyHeight( const int nPlayer, const int nAvia );
	const NTimer::STime GetPlaneRegenerateTime( const int nPlayer ) const;
	int GetNParadropers( const int nPlayer ) const;
	
	int GetParadropperDBID( const int nPlayer ) const;
	const char* GetWireFenceName() const;
	const char* GetRandomAntitankObjectName() const;
	const char* GetEntrenchmentName() const;
	
	const char *GetRandomTankPit( const class CVec2 &vSize, const bool bCanDig, float *pfResize ) const;

	void PlaneLandedSafely( const int nPlayer, const int /*SUCAviation::AIRCRAFT_TYPE*/ nAvia );
	void Segment();

	void EnableAviationScript( const int nPlayer, const int nAvia );
	void DisableAviationScript( const int nPlayer, const int nAvia );
	int GetLastCalledAviation( const int nPlayer ) const;

	void BadWeatherStarted();
	
	const CVec2 GetFirstIntercectWithMap( const int nPlayer );

	bool IsAntiTank( const SHPObjectRPGStats *pStats ) const;
	bool IsAPFence( const SHPObjectRPGStats *pStats ) const;
};
#endif // _UNIT_CREATION_INTERNAL_
