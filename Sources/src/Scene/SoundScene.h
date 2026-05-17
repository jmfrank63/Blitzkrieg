#ifndef __SOUNDSCENE_H__
#define __SOUNDSCENE_H__

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\SFX\SFX.h"
#include "..\Misc\2DArray.h"
#include "..\Formats\fmtMap.h"
#include "..\Misc\TypeConvertor.h"
#include "..\Misc\FreeIDs.h"

#include "CellsConglomerateContainer.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSoundSceneConsts
{
	static int SS_SOUND_CELL_SIZE;									// ����� ������� � ������
	static NTimer::STime SS_MIX_DELTA;							// ������������ ������� �� ������� 
																									// ��������� ������ ��� SFX_MIX_IF_TIME_EQUALS
	static NTimer::STime SS_UPDATE_PERIOD;					// � ������������
	static NTimer::STime SS_SOUND_DIM_TIME;								// ����� ��������� ����� ��� ��������
	static NTimer::STime SS_COMBAT_MUSIC_PLAY_WO_NOTIFY;	// ������� ������ ������� ��������� ����� ��������� ��������� ������
	static NTimer::STime SS_COMBAT_MUSIC_FADE;						//����� ��������� ��������� ������ � ������������.
	
	static NTimer::STime SS_STREAMING_SILENT_PAUSE;				// ��� ���������� ������� ������ ����� Combat ����� �������� 
	static NTimer::STime SS_STREAMING_SILENT_PAUSE_RND;		// IDLE
	static NTimer::STime SS_IDLE_PAUSE; 

	static NTimer::STime SS_AMBIENT_SOUND_CHANGE_RANDOM;//for changing looped sounds from time to time
	static NTimer::STime SS_AMBIENT_SOUND_CHANGE;			//for changing not looped sounds from time to time
	static int SS_AMBIENT_TERRAIN_SOUNDS;						//number of playing terrain sounds simualteniously 
	static float TERRAIN_SOUND_RADIUS_MIN;					//in percent of screen size
	static float TERRAIN_SOUND_RADIUS_MAX;
	static float TERRAIN_CRITICAL_WEIGHT;
	static float DEFAULT_SCREEN_WIDTH;
	static float DEFAULT_SCREEN_HEIGHT;
	static int MAP_SOUND_CELL;

	static int TERRAIN_NONCYCLED_SOUNDS_MIN_RADIUS;
	static int TERRAIN_NONCYCLED_SOUNDS_MAX_RADIUS;
	
	static NTimer::STime MAP_SOUNDS_UPDATE;

	static float COMBAT_MUSIC_VOLUME;
	static float IDLE_MUSIC_VOLUME;
	static float COMBAT_SOUNDR_FEAR_RADIUS;
	static float COMBAT_FEAR_TIME;

	static NTimer::STime SS_MAP_SOUND_PERIOND;
	static NTimer::STime SS_MAP_SOUND_PERIOND_RANDOM;

	static int MIN_SOUND_COUNT_TO_PLAY_LOOPED;
};
	// ��� ������, ����������� � ����� �����
class CMapSounds
{
	DECLARE_SERIALIZE;

	typedef CTypeConvertor<std::string, WORD> RegisteredSounds;

	//
  class CMapSoundCell
	{
		DECLARE_SERIALIZE;

		// for similar map sounds
		struct SMapSounds
		{
			DECLARE_SERIALIZE;
		public:
			std::unordered_map<WORD,CVec2> instanceIDs;
			int nCount;
			SMapSounds() : nCount( 0 ) {  }
		};
		struct SMaxCountPredicate
		{
			bool operator()( const std::pair<WORD,SMapSounds> &s1, const std::pair<WORD,SMapSounds> &s2 ) const
			{ return s1.second.nCount > s2.second.nCount; }
		};

		struct SPlaying
		{
			WORD wSoundTypeID;								// type of sound
			WORD wInstanceID;									// instance of sound
			WORD wSceneID;										// if added to scene, then scene ID
			//
			SPlaying() { Clear(); }
			void Clear() { wInstanceID = 0; wSceneID = 0; wSoundTypeID = 0; }
		};

		SPlaying playingLoopedSound;							// ������� �������� ��� ����� (�����������)
		SPlaying playingSound;										// ������� ������������� ����

		// �� ����� ����� ������
		typedef std::unordered_map<WORD, SMapSounds> CellSounds;
		CellSounds cellSounds;
		CellSounds cellLoopedSounds;
		NTimer::STime timeNextRun;			// ����� ���������� ��������� �����

		void RemoveSound( CellSounds *pCellSounds, const WORD wInstanceID );
	public:
		CMapSoundCell() : timeNextRun( 0 ) { }

		void AddSound( const WORD wSoundID, const CVec2 &vPos, const RegisteredSounds &registeredSounds, const WORD wInstanceID, const bool bLooped );
		void RemoveSound( const WORD wInstanceID, class CSoundScene * pScene );
		void Update( class CSoundScene * pScene, const RegisteredSounds &registeredSounds );
	};

	CFreeIds soundIDs;									// ��� ����������� ������
	CFreeIds instanceIDs;								// ������ ���� ����� ����� ���������� ID
	RegisteredSounds registeredSounds;	// ������ �������� ������, ������� ���� � �����

	// 2d map of sound cells
	CArray2D< CMapSoundCell > mapCells;
	// cell - sound instance id
	std::unordered_map<WORD, SIntPair > cells;

	CSoundScene * pSoundScene; 
	NTimer::STime timeNextUpdate;

public:
	CMapSounds() : pSoundScene( 0 ), timeNextUpdate( 0 ) {  }
	void SetSoundScene( class CSoundScene *pSoundScene );
	void Update( interface ICamera *pCamera );
	void Clear();

	void InitSizes( const int nSizeX, const int nSizeY );
	WORD AddSound( const CVec2 &vPos, const char *szName );
	void RemoveSound( const WORD wInstanceID );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoundScene : public IRefCount
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS( CSoundScene );
public:

	static const NTimer::STime &GetCurTime();		// ����� �� ���������� �����
	// ��� ����������� ����� �� ���� � �������� �����
	static bool IsInBounds( int x, int y );
	static IObjectsDB * GetObjectDB();
	
	// ��� �������� ������ ������ � ������ �� �� �������
	class CPlayList : public IRefCount
	{
		OBJECT_COMPLETE_METHODS( CPlayList );
		DECLARE_SERIALIZE;
		std::vector< std::string > melodies;
		int nIter;
	public:
		CPlayList(): nIter ( 0 ) {  }
		void Reset();
		void Clear();
		void Shuffle();
		void AddMelody( const std::string &pszFileName );
		const char* GetNextMelody();
		const bool IsEmpty() const { return melodies.empty(); }
	};

		// ��� ������, ��������� �� Terrain � �� ����������� ��������.
	class CTerrainSounds
	{
		DECLARE_SERIALIZE;
	public:
		// ��� �������� ����� � ��� ���������
		class CTerrainSound 
		{
			DECLARE_SERIALIZE;

			struct SSoundInfo
			{
				DECLARE_SERIALIZE;
			public:
				CPtr<ISound> pSound;
				bool bPeaceful;
				SSoundInfo() {  }
				SSoundInfo( CPtr<ISound> pSound, bool bPeaceful )
					: pSound( pSound ), bPeaceful( bPeaceful )
				{
				}
			};
			typedef std::list<SSoundInfo> CCycledSounds;
			CVec3 vSoundPos;									// placement of nonCycleSound

			CCycledSounds cycledSounds;				//cycle sounds from that terrain

			float fVolume;
			float fPan;
			CVec2 vOffset;										//offset of this sound from camera
			NTimer::STime timeRestart;				//time when sound must be restarted
			bool bMustPlay;
			bool bNeedUpdate;
			WORD wSound;
		public:
			CTerrainSound() : fVolume ( 0.0f ), fPan ( 0.0f ), 
												bMustPlay( false ),
												timeRestart ( 0 ), vOffset( VNULL2 ),
												bNeedUpdate ( false ), vSoundPos( VNULL3 ), wSound( 0 ) {  }
			//returns true if update of sounds is needed.
			void SetSound( const char *pszName, NTimer::STime timeWhenRestart );
			bool HasCycleSound() const { return !cycledSounds.empty(); }
			void AddCycledSound( const char * szName, interface ISoundManager *pSoundManager );
			void StartCycledSounds( ISFX *pSFX, bool bNonPeacefulOnly );

			NTimer::STime GetRestartTime() { return timeRestart; }
			// ���� ���������� ��������� ������ IsNeedUpdate ������ true
			void Update(	const SSoundTerrainInfo& info, 
										const CVec3 &vCameraAnchor, const CVec2 &vScreenSize, const float fRelativeVolume );
			void SetMustPlay( bool _bMustPlay ) ;
			bool IsMustPlay() const { return bMustPlay; }

			bool IsNeedUpdate() const { return bNeedUpdate; }
			void DoUpdate( ISFX * pSFX );
			
			void StopSounds( ISFX * pSFX, bool bOnlyPeaceful );
		};

	private:
		CPtr<ISoundManager> pSoundManager ;
		CPtr<ISFX> pSFX;
		CVec3 vCameraAncor;									//to determine weather coordinates changed
		// for every terrain there will be separate sound
		typedef std::map< BYTE, CTerrainSound > CSounds;
		CSounds terrainSounds;
		CPtr<ITerrain> pTerrain;						// to get terrain sounds
		NTimer::STime lastUpdateTime;
		CVec2 vScreen;												// screen sizes
		bool bMuteAll;												// mute terrain sounds
	public:
		CTerrainSounds() : vCameraAncor( CVec3(-1,-1,-1) ), lastUpdateTime( 0 ), bMuteAll( false ) {  }
		void Init( interface ITerrain *pTerrain );
		void Update( interface ICamera *pCamera, const bool bCombat  );
		void Mute( const bool bMute );
		void Clear();
	};

private:
	// ������������� ������
	class CStreamingSounds
	{
		//������ ��������� �� ��������, ��������� �� ���������.
		// �� ��������� ���������� ��� ����� - ������.
		DECLARE_SERIALIZE;

		// for load
		typedef std::vector<std::string> CMusicFiles;
		struct SMusicSettings
		{
			CMusicFiles combat;
			CMusicFiles explore;
			
			int operator&( IDataTree &ss )
			{
				CTreeAccessor saver = &ss;
				saver.Add( "Combat", &combat );
				saver.Add( "Explore", &explore );
				return 0;
			}
		};
		typedef std::unordered_map< std::string/*Party Name*/, SMusicSettings > CMusicSettingsList;

		enum EStreamingSoundsState
		{
			ESSS_IDLE,
			ESSS_FADE_IDLE,
			ESSS_IDLE_PAUSE,
			ESSS_START_COMBAT,
			ESSS_COMBAT,
			ESSS_COMBAT_FADE,
			ESSS_COMBAT_RESTORE_VOLUME,
			ESSS_START_COMBAT_AFTER_LOAD,
			ESSS_PAUSE,
		};
		CPtr<ISFX> pSFX;
		EStreamingSoundsState eState;
		CPtr<IGameTimer> pGameTimer;

		bool bCombatNotify;									// �������� ������
		NTimer::STime timeLastCombatNotify;	// ����� ���������� �������� � ����
		NTimer::STime timeLastUpdate;				//
		NTimer::STime timeDesiredPause;			// ����������� ����� ����� Combat � Idle

		CPtr<CPlayList> pIdle;							// 
		CPtr<CPlayList> pCombat;						// 

		void StartIdleMusic();
		void StartCombatMusic();
		void Init( class CPlayList *pIdle, class CPlayList *pCombat );
		
	public:
		CStreamingSounds();
		
		void Clear();
		// 
		void Init( const std::string &szPartyName );
		// ������ (��� �����������) ��������� ������
		void CombatNotify();
		// 
		void Update();
	};

	// ��� Hash �������
	struct SIntPairHash
	{
		size_t operator() ( const SIntPair &v ) const
		{
			return v.x * RAND_MAX +  v.y;
		}
	};

	struct SUpdatedCell
	{
		int nFormerRadius;
		int nNewRadius;
		SIntPair vCell;
		SUpdatedCell( const SIntPair &vCell, const int nFormerRadius, const int nNewRadius )
			: vCell( vCell ), nFormerRadius( nFormerRadius ), nNewRadius( nNewRadius ) {  }
	};
	
	typedef std::list< SUpdatedCell > CUpdatedCells; 

public:

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ����� ������������� ����� ����������� ��� ��������. ��� �����������
	// ������ - ��� ������ ������������� ������ ����� �� ����� �� ������
	class CSubstSound : public IRefCount
	{
		OBJECT_NORMAL_METHODS(CSubstSound);
		DECLARE_SERIALIZE;
		CPtr<ISFX> pSFX;
		CPtr<ISound> pSample;
	public:
		CSubstSound() {  }
		CSubstSound( ISound *pSound, ISFX *pSFX ): pSample( pSound ), pSFX( pSFX ) {  }
		virtual ~CSubstSound()
		{
			if ( pSFX )
				pSFX->StopSample( pSample );
		}
		ISound * GetSound()
		{
			return pSample;
		}
	};

	//����
	class CSound : public IRefCount
	{
		OBJECT_NORMAL_METHODS( CSound );
		DECLARE_SERIALIZE;
		WORD wID;														// 
		CPtr<ISound> pSample;								// ����, ������� ����� ������
		CPtr<CSubstSound> pSubstitute;						// ����, ������� �������� ������ pSample
		
		ESoundCombatType eCombatType;				// ��������� ����� �� ����� ���
		NTimer::STime timeBegin;						// ������ ��������� �����. 
		NTimer::STime timeToPlay;						// ����� ������ �����
		NTimer::STime timeBeginDim;

		std::string szName;
		ESoundMixType eMixType;
		CVec3 vPos;
		bool bLooped;

		int nMaxRadius, nMinRadius;

		bool bStartedMark;											// ���� ��������� ���
		bool bFinishedMark;											// ���� ��� �������
		bool bDimMark;												  // ������ ��������� �����
	public:
		CSound() {  }
		CSound(	const WORD wID, 
															const std::string &szName,
															interface ISound *pSound , 
															const enum ESoundMixType eMixType,
															const CVec3 &vPos,
															const bool bLooped,
															const ESoundCombatType eCombatType,
															float fMinRadius,
															float fMaxRadius);
		virtual ~CSound();
	
		// �������� ������� ��� ��������
		unsigned int GetSamplesPassed();
		bool IsTimeToFinish();
		
		//����� ��������� ����� �����
		NTimer::STime GetPlayTime() const { return timeToPlay; }
		
		// ������ �����
		bool IsSubstituted() const { return pSubstitute!=0; }
		CSubstSound * GetSubst();
		void Substitute( CSubstSound *_pSubstitute, NTimer::STime nStartTime );
		void UnSubstitute();

		bool IsLooped() const { return bLooped; }

		//������ ��������� ����� �����
		void MarkStarted();
		bool IsMarkedStarted() const { return bStartedMark; }
		//����� ��������� �����
		bool IsMarkedFinished() const { return bFinishedMark; }
		void MarkFinished( bool bFinished =true ) { bFinishedMark=bFinished; }
		// ��� ��������� ����� ��� ��������
		void MarkToDim( const NTimer::STime time );
		bool IsMarkedForDim() const { return bDimMark; }
		// ��-�� ��������� (�� �������) ��� ��-�� ���������� ��������� ����� ���� �� ������.
		float GetVolume( const NTimer::STime time, const float fDist  ) const;


		void SetBeginTime( const NTimer::STime time );
		const NTimer::STime & GetBeginTime() const {return timeBegin;}


		int GetRadiusMax() const;								// ��������� (� �������) �� ������ ���� ���� ������
		ISound * GetSound();
		ESoundMixType GetMixType() const { return eMixType; }
		const WORD GetID() { return wID; }
		const std::string &GetName() const;

		void SetPos( const class CVec3 & vPos );
		const CVec3& GetPos() const { return vPos; }

		ESoundCombatType GetCombatType() const { return eCombatType; }
	};

	//������, ���������� ����
	class CSoundCell : public IRefCount
	{
		DECLARE_SERIALIZE;
		OBJECT_COMPLETE_METHODS( CSoundCell );
		int nRadius;												// ������ �������� ���� ������(� �������)
		//std::list< SIntPair > hearableCells;	// ������ ������, ����� ������� ������ �� ���� ������
		typedef std::list< CPtr<CSound> > CSounds;
		CSounds sounds;												// ����� ���� ������
		void RecountForDelete();
		NTimer::STime timeLastCombatHear;
		bool IsSoundHearable( const CSound *pSound, const int nRadius ) const;
	public:
		CSoundCell();
		void Clear();

		int GetRadius() const { return nRadius; }
		void SetRadius( int nRad ) { nRadius = nRad; }
		void AddSound( class CSound *pSound );	// ��������� ���� � ������������� ������ ��������
		void RemoveSound( const WORD wID, ISFX * pSFX =0 );					// ������� ���� � ������������� ������ ��������
		CSound * GetSound( const WORD wID );

		// ������� ����� � ID == 0 , ������� �����������
		// ��� ���������� ����� �������� ��� ����������
		void Update( ISFX * pSFX );
		
		// ������ � ������ �������� ������.
		//void AddHearCell( const SIntPair &cell);
		//void RemoveHearCell( const SIntPair &cell);
		// replace cell registration with new
		//void ReplaceHearCell( const SIntPair &vFormerCell, const SIntPair &vNewCell );


		bool HasSounds() const { return sounds.begin() != sounds.end(); }
//		bool HearSounds() const { return hearableCells.begin() != hearableCells.end(); } 

		void SetLastHearCombat( const NTimer::STime hearTime );
		bool IsCombat() const;

		
		// ��� ���� ������, ������� ������ �� ���������� ������ nRadius � ��� �� ��������
		template <class TEnumFunc> 
			void EnumHearableSounds( int nRadius, TEnumFunc func )
		{
			for ( CSounds::iterator it = sounds.begin(); it != sounds.end(); ++it )
			{
				if ( IsSoundHearable( *it, nRadius )	)
						func( *it );
			}
		}
		// ��� �������� ���� ������.
		template <class TEnumFunc>
				void EnumAllSounds( TEnumFunc func, int nRadius )
		{
			if ( nRadius <= GetRadius() )
			{
				for ( CSounds::iterator it = sounds.begin(); it != sounds.end(); ++it )
				{
					if ( IsSoundHearable( *it, nRadius ) )
						func( *it );
					else
						func( *it, false );
				}
			}
			else
			{
				// ������ �� ���� �� ������ ���� ������ �� ������
				for ( CSounds::iterator it = sounds.begin(); it != sounds.end(); ++it )
					func( *it, false );
			}
		}
	};
	
	typedef std::list< CPtr<ISound> > CSamplesList;
	typedef std::list< CPtr<CSound> > CSoundsList;
	typedef std::unordered_map< std::string/*subst name*/, CSoundsList > CHearableSounds;
	typedef std::unordered_map< std::string/*sound name*/, std::string/*subst name*/ > CSoundSubstTable;
	
	//typedef std::unordered_map< SIntPair, CSoundCell, SIntPairHash > CSoundCells;
	typedef CArray2D< CPtr<CSoundCell> > CSoundCellsInBounds;
	typedef std::unordered_map< SIntPair, CPtr<CSoundCell>, SIntPairHash > CSoundCellsOutOfBounds;
	typedef std::unordered_map< SIntPair, CPtr<CSoundCell>, SIntPairHash > CSoundCellsWithSound;

	// ��� ����� ������, ������� ������ � ������ � ���������� �� ��
	// ������ �� ����������
	class CSoundsCollector
	{
		CSoundSubstTable	&substTable;
		CHearableSounds		&sounds;
		CSoundCellsWithSound &cellsWSound;
		CSoundsList				&muteSounds;
	public:
		CSoundsCollector(	CSoundSubstTable &substTable, CHearableSounds &sounds, CSoundCellsWithSound & cellsWSound, CSoundsList & muteSounds )
			:substTable( substTable ), sounds( sounds ), cellsWSound( cellsWSound ), muteSounds( muteSounds ) {  }
		void operator()( int nRadius, const SIntPair & vCell );
		void operator()( class CSound * sound, bool bHearable = true );
		
		void CalcMuteSounds();
	};

private:
	enum ESoundSceneMode eSoundSceneMode;
	CFreeIds freeIDs;											// ������� ID ������
	std::unordered_map< WORD, SIntPair >				soundIDs;			// � ����� ������ ��������� ����.

	ISFX * pSFX;
	ISoundManager * pSoundManager;
	IGameTimer * pGameTimer;
	static IObjectsDB * pObjectsDB;

	CUpdatedCells updatedCells;						// ��� �������������� ��������
	
	NTimer::STime timeLastUpdate;
	SIntPair vFormerCameraCell;								// ��� ������������ ����������� ������
	static SIntPair vLimit;											// ������������ ������� �����

	CSoundSubstTable substTable;					// ������� ������ ������

	CStreamingSounds streamingSounds;
	
	CHearableSounds interfaceSounds;					// ����� �� ����������
	std::unordered_set<int> finishedInterfaceSounds;
	std::unordered_set<int> deletedInterfaceSounds;

	CTerrainSounds terrainSounds;
	CMapSounds mapSounds;

	CSoundCellsInBounds	soundCellsInBounds;								// ����� �� �����
	CSoundCellsOutOfBounds soundCellsOutOfBounds;		// ����� �� ������ (��.������� ��������)
	CSoundCellsWithSound soundCellsWithSound;				// ������ ���� ������ �� �������
	CVec2 vScreenResize;

	CCellsConglomerateContainer cellsPHS;
	static NTimer::STime curTime;					// ����� �� ���������� �����

	// helper functions
	CSoundCell * GetSoundCell( const CSoundScene::SIntPair &vCell );

	void AddSound( const SIntPair &vCell, CSound *s );
	void To2DSoundPos( const CVec3 &vPos, CVec3 *pv2DPos );
	// ordinary update
	void UpdatePHSMap( const SIntPair &vCell, const int nFormerRadius, const int nNewRadius );

	void UpdateCombatMap( const CSoundScene::SIntPair &vCell, CSoundScene::CSound *pSound );
	void CalcVolNPan( float *fVolume, float *fPan, const CVec3 &vSound, const float fMaxHear );
	void MuteSounds( CSoundsList	* muteSounds );

	void MixInterfaceSounds();
	void MixMixedAlways( CSoundScene::CHearableSounds & sounds, const CVec3 & vCameraPos );
	void MixSingle( CSoundScene::CHearableSounds & sounds, const CVec3 & vCameraPos );
	void MixMixedWithDelta( CSoundScene::CHearableSounds & sounds, const CVec3 & vCameraPos );
	void Mix(	CSoundsList & curSounds,
					const CSoundsList::iterator begin_iter,
					const CSoundsList::iterator end_iter,
					const std::string &substName,
					const CVec3 &vCameraPos,
					const ESoundMixType eMixType,
					const int nMixMinimum,
					bool bDelete = true );


	// ��� ���������� ������ �� ������� ������
	class CSoundStartTimePredicate
	{
	public:
		bool operator()( CSound* one, CSound *two ) 
		{ return one->GetBeginTime() < two->GetBeginTime(); }
	};
	//��� ������ ������, ������ ������� ����� �� ������, ��� timeDelta
	// �� timeBase
	class CSoundsWithinDeltaPredicate
	{
		NTimer::STime timeToCompare;
	public:
		CSoundsWithinDeltaPredicate( NTimer::STime timeBase, NTimer::STime timeDelta)
			: timeToCompare( timeBase+timeDelta ) {  }
		bool operator()( const CSound * sound )
		{
			return sound->GetBeginTime() >= timeToCompare;
		}
	};
	
	void InitConsts();
public:

	CSoundScene();
	void Clear();

	void Init( const int nMaxX, const int nMaxY );
	void InitScreenResolutionConsts();		// must be called after all screen resolution changes

	void InitTerrain( interface ITerrain *pTerrain );
	void InitMap( const struct CMapSoundInfo *pSound, int nElements );
	/*void InitMusic(	const char **ppszCombat, const int nCombatSize,
									const char **ppszExplore, const int nExploreSize );*/
	void InitMusic( const std::string &szPartyName );

	void SetTerrain( interface ITerrain * pTerrain ) { terrainSounds.Init( pTerrain ); }
	// ���� ������� ��� ���� ��� - �� �������� �� �������
	void CombatNotify();

	void MuteTerrain( const bool bMute );
	
	WORD AddSound( 	const char *pszName,
												const CVec3 &vPos,
												const enum ESoundMixType eMixMode,
												const enum ESoundAddMode eAddMode,
												const enum ESoundCombatType eCombatType,
												const int nMinRadius,
												const int nMaxRadius,
												const unsigned int nTimeAfterStart = 0 );

	//������� ���� �� �����. ID ���������� ����������
	void RemoveSound( const WORD wID );
	// ������ ����� ������� �����.
	void SetSoundPos( const WORD wID, const class CVec3 &vPos );

	bool IsFinished( const WORD wID );

	void Update( interface ICamera *pCamera );
	
	void SetMode( const enum ESoundSceneMode eSoundSceneMode );
	enum ESoundSceneMode GetMode() { return eSoundSceneMode; };

	WORD AddSoundToMap( const char *pszName, const CVec3 &vPos );
	void RemoveSoundFromMap( const WORD	wInstanceID );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SOUNDSCENE_H__
