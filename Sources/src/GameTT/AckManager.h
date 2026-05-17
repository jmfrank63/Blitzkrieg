#ifndef __CLIENTACKMANAGERINTERNAL_H__
#define __CLIENTACKMANAGERINTERNAL_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMission.h"
#include "..\Misc\HashFuncs.h"
#include "..\Common\Actions.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface ITextManager;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CClientAckManager : public IClientAckManager
{
	DECLARE_SERIALIZE;
	OBJECT_NORMAL_METHODS(CClientAckManager);

public:
	static int MIN_UNITS_TO_RUSH_ACK;
	static int MIN_UNITS_TO_TRAVEL_ACK;
private:
	
	enum EAcknowledgementColor
	{
		ACOL_INFORMATION,
	};
	enum ESoundPosition
	{
		ESP_FROM_INTERFACE,
		ESP_FROM_MAP,
	};
	//
	enum EAcknowledgementAdditionalSound
	{
		AAS_NONE,
		AAS_INFORMATION,
		AAS_TAKING_OFF,
	};
	//
	enum EAcknowledgementType
	{
		ACKT_POSITIVE,
		ACKT_NEGATIVE,
		ACKT_SELECTION,
		ACKT_NOTIFY,
		ACKT_BORED,
		ACKT_VOID,									// �� �������� ������
	};
	//
	class CBoredUnitsContainer
	{
		DECLARE_SERIALIZE;
		typedef std::unordered_map<CPtr<IMOUnit>, bool, SPtrHash> CBoredUnits;
		CBoredUnits boredUnits;							
		int nCounter;												// ��� ����������� - ������ boredUnits
		NTimer::STime timeLastBored;				// time for last bored sound
		void Copy( const CBoredUnitsContainer &cp );
	public:
		CBoredUnitsContainer();
		CBoredUnitsContainer operator=( const CBoredUnitsContainer &cp ) { Copy(cp); return *this; }
		CBoredUnitsContainer( const CBoredUnitsContainer&cp ) { Copy(cp); }

		int GetCount() const { return nCounter; }

		void AddUnit( interface IMOUnit *pUnit );
		void DelUnit( interface IMOUnit *pUnit );
		// ���� ����� �������, �� ���������� ���������� ����� ������� Ack. 
		//���� ack �������, �� true;
		bool SendAck( const NTimer::STime curTime, 
									const EUnitAckType eBored, 
									IClientAckManager *pAckManager,
									const NTimer::STime timeInterval );
		void Clear();
	};

	// 
	struct SUnitAckInfo
	{
		EAcknowledgementType eType;
		std::string szTextKey;
		EAcknowledgementColor eColor;
		EAcknowledgementAdditionalSound eSound;
		ESoundPosition ePosition;
		int nTimeAfterPrevious;
		
		SUnitAckInfo() {  }
		SUnitAckInfo( EAcknowledgementType _eType,
									const char *_szKey,
									EAcknowledgementColor _eColor,
									EAcknowledgementAdditionalSound _eSound,
									ESoundPosition _ePosition = ESP_FROM_INTERFACE,
									int _nTimeAfterPrevious = 0 )
			: szTextKey( _szKey ), eType( _eType ), eColor( _eColor ), eSound( _eSound ), ePosition( _ePosition ), nTimeAfterPrevious( _nTimeAfterPrevious )
		{
		}
	};
	//
	struct SUnitAckInfoForLoad
	{
		std::string szAckName;
		std::string szTextKey;
		
		EAcknowledgementType eType;
		EAcknowledgementColor eColor;
		EAcknowledgementAdditionalSound eSound;
		ESoundPosition ePosition;
		int nTimeAfterPrevious;
		//
		SUnitAckInfoForLoad() {  }
		SUnitAckInfoForLoad( const std::string &_szAckName,
									EAcknowledgementType _eType,
									const char *_szKey,
									EAcknowledgementColor _eColor,
									EAcknowledgementAdditionalSound _eSound,
									int _nTimeAfterPrevious )
			: szTextKey( _szKey ), szAckName( _szAckName ), eColor( _eColor ), eSound( _eSound ), eType( _eType ), nTimeAfterPrevious( _nTimeAfterPrevious )
		{
		}
		//
		int operator&( IDataTree &ss )
		{
			CTreeAccessor tree = &ss;
			tree.Add( "AckName", &szAckName );
			tree.Add( "AckType", &eType );
			tree.Add( "TextKey", &szTextKey );
			tree.Add( "ColorType", &eColor );
			tree.Add( "SoundType", &eSound );
			tree.Add( "Position", &ePosition );
			tree.Add( "TimeAfterPrevious", &nTimeAfterPrevious );
			return 0;
		}

	};
	//
	struct SAck
	{
		DECLARE_SERIALIZE;
	public:
		int /*EUnitAckType*/ eAck;
		std::string sound;
		int /*ESoundmixType*/ eMixType;
		bool operator==( const SAck & ack ) const { return eAck == ack.eAck; }
	};
	typedef std::list< SAck > CAcks;
	//	
	struct SUnitAck
	{
		DECLARE_SERIALIZE;
	public:
		CAcks acks;													// ������� Ack, ������� ��� �����������
		WORD wSoundID;											// ���� ���� ������
		int /*EUnitAckType*/ eCurrentAck;		// ����  Ack ������ ������
		NTimer::STime timeRun;							// ����� ����� ����� ��������� AckPisitive
		SUnitAck()
			:wSoundID( 0 ), eCurrentAck( -1 ), timeRun( 0 ) { }
	};
	// acknowledgement of dead unit
	struct SDeathAck
	{
		DECLARE_SERIALIZE;
	public:
		std::string szSoundName;
		CVec3 vPos;
		NTimer::STime timeSinceStart; 
		SDeathAck() {  }
		SDeathAck( const CVec3 &_vPos, const std::string &_szSoundName, const unsigned int nTimeSinceStart )
			: vPos( _vPos ), szSoundName( _szSoundName ), timeSinceStart( nTimeSinceStart ) {  }
	};
	//
	typedef std::unordered_map< int, SUnitAckInfo > CUnitAcksInfo;
	typedef std::unordered_map< int, NTimer::STime > CUnitAcksPresence;
	typedef std::unordered_map< CPtr<IMOUnit>, SUnitAck, SDefaultPtrHash > CUnitsAcks;
	typedef std::list< SDeathAck > CDeathAcks;

	//��� ������ ��������� ���� �����
	class CAckPredicate
	{
		const EAcknowledgementType eType;
		CUnitAcksInfo &info;
	public:
		CAckPredicate( CUnitAcksInfo &info, EAcknowledgementType eType ) : info( info ), eType( eType ) {  }
		bool operator()( const SAck & a ) 
			{ return eType == info[a.eAck].eType; }
	};

	//NTimer::STime timeNextBored;
	
	ITextManager *pTextManager;
	IConsoleBuffer *pConsoleBuffer;
	IGameTimer *pGameTimer;
	
	CPtr<IMOUnit> pLastSelected;
	int nSelectionCounter;
	
	// ��� ���� � ������, ������������������ � bored ����������
	typedef std::unordered_map<int, CBoredUnitsContainer> BoredUnits;
	BoredUnits boredUnits;	

	CUnitAcksPresence acksPresence;				// ������� � ����� ������ � �������� ������� Ack'� 
	CUnitsAcks				unitAcks;						// ��� �������� ���� ������� Ack'��
	CDeathAcks				deathAcks;					// for death acknowledgements;
	NTimer::STime timeLastDeath;
	
	// �� �������������.
	CUnitAcksInfo			acksInfo;						// ������ �� Ack'��
	std::unordered_map<std::string,int> loadHelper;
	// ���������
	int MIN_ACK_RADIUS;
	int MAX_ACK_RADIUS;
	int TIME_ACK_WAIT;
	int NUM_SELECTIONS_BEFORE_F_OFF;
	int ACK_BORED_INTERVAL;
	int ACK_BORED_INTERVAL_RANDOM;
	
	void InitConsts();

	const char * GetAdditionalSound( EAcknowledgementAdditionalSound eSound );
	DWORD GetMessageColor( enum CClientAckManager::EAcknowledgementColor eColor );
	void RegisterAck( SUnitAck *ack, const NTimer::STime curTime  );
	void UnregisterAck( SUnitAck *ack );


public:
	CClientAckManager();
	virtual void STDCALL Init();
	virtual void STDCALL Clear();
	virtual bool STDCALL IsNegative( const enum EUnitAckType eAck );

	virtual void STDCALL AddDeathAcknowledgement( const CVec3 &vPos, const std::string &sound, const unsigned int nTimeSinceStart );
	virtual void STDCALL AddAcknowledgement( interface IMOUnit *pUnit, const enum EUnitAckType eAck, const std::string &sound, const int nSet, const unsigned int nTimeSinceStart = 0);
	virtual void STDCALL UnitDead( struct SMapObject*pUnit, interface IScene * pScene);
	virtual void STDCALL Update( interface IScene * pScene );
	virtual void STDCALL RegisterAsBored( EUnitAckType eBored, interface IMOUnit *pObject );
	virtual void STDCALL UnRegisterAsBored( EUnitAckType eBored, interface IMOUnit *pObject );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __CLIENTACKMANAGERINTERNAL_H__
