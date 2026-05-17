#ifndef __GENERAL_AIR_FORCE__
#define __GENERAL_AIR_FORCE__

#include "GeneralInternalInterfaces.h"
#include "AIHashFuncs.h"
#include "..\Misc\FreeIDs.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnit;
class CEnemyRememberer;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ��������� ���������� � �x ������� ��� ���� �������
class CGeneralAirForce : public IRefCount, public IEnemyEnumerator
{
	friend class CGeneralAirForceLaunchFighters;
	OBJECT_COMPLETE_METHODS(CGeneralAirForce);
	DECLARE_SERIALIZE;
	
	struct SSameEnemyPointPredicate
	{
		bool operator()( const CVec2 &v1, const CVec2 &v2 ) {	return fabs2(v1-v2) < sqr(SConsts::PLANE_GUARD_STATE_RADIUS/2); }
	};

	int nParty;
	std::vector<int> players;							// ������ �������, ������� ��������� ��� ����������� 
	CFreeIds requestsID;
	interface IEnemyContainer *pEnemyContainer;

	bool bReservedByFighters;							// ����, ���� ����������� �������, ����� ������� �����������

	typedef std::unordered_map< int, CPtr<CEnemyRememberer> > AntiAviation;
	AntiAviation antiAviation;

	struct SSupportInfo
	{
		CVec2 vPoint;												// ���� �������
		int nResistanceCellNumber;					// ���� ������� ���������� ����� �������������, �� > 0

		SSupportInfo() : vPoint( VNULL2 ), nResistanceCellNumber( -1 ) { }
	};

	// �����, � �������� ������� ������ ���� � �������������� ���������
	std::vector<NTimer::STime> reservedTimes;		

	
	std::list<CVec2> vFighterPoints;
	NTimer::STime timeLastCheck, timeLastFighterCheck ;
	NTimer::STime checkPeriod;						// ��������� �������� ��� ���� �������, ����� ������������
	NTimer::STime fighterCheckPeriod;			// ��������� �������� ��� ������������
public:
	typedef std::unordered_map< int /*request ID*/, SSupportInfo > Requests;
	typedef std::vector<Requests> RequestsByForceType;
private:

	RequestsByForceType requests;

	void LaunchScoutFree( const int nPlayer );
	void PrepeareFighters( const int nPlayer  );
	void LaunchFighters( const int nPlayer );
	void LaunchByRequest( const int nPlayer, const int nAvia, Requests *pRequest );

	void LaunchPlane( const int /*SUCAviation::AIRCRAFT_TYPE*/ nType, const std::list<CVec2> &vPoints, const int nPlayer );

	// returns 0 if line is safe to fly.
	// otherwize returns severty( how many planes will die while flying by this line )
	float CheckLineForSafety( const CVec2 &vStart, const CVec2 &vFinish, const float fFlyHeight );
	bool IsTimePossible( const int nPlayer, const NTimer::STime timeToLaunch ) const; // ����������� ��������� �������� � ��� �����.

	void InitCheckPeriod();
	void InitFighterCheckPeriod();
public:
	CGeneralAirForce() {  }
	CGeneralAirForce( const int nPlayer, IEnemyContainer *pEnemyContainer ) ;

	void Segment();

	int /*request ID*/RequestForSupport( const CVec2 &vSupportCenter, enum EForceType eType, int nResistanceCellNumber = -1 );
	void CancelRequest( int nRequestID, enum EForceType eType );

	void SetEnemyContainer( IEnemyContainer * _pEnemyConatainer )
	{
		pEnemyContainer = _pEnemyConatainer;
	}

	//IEnemyEnumerator
	virtual bool EnumEnemy( class CAIUnit *pEnemy );

	void SetAAVisible( class CAIUnit *pUnit, const bool bVisible );
	void DeleteAA( class CAIUnit * pUnit );
	void ReserveAviationForTimes( const std::vector<NTimer::STime> &times );

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGeneralAirForceLaunchFighters : public IGeneralDelayedTask 
{
	OBJECT_COMPLETE_METHODS( CGeneralAirForceLaunchFighters );
	DECLARE_SERIALIZE;

	CPtr<CGeneralAirForce>  pAirForce;
	NTimer::STime timeToRun;
	int nPlayer;
public:
	CGeneralAirForceLaunchFighters() {  }
	CGeneralAirForceLaunchFighters( class CGeneralAirForce *pAirForce, const NTimer::STime timeToRun, const int nPlayer ); 
	virtual bool IsTimeToRun() const ;
	virtual void Run() ;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GENERAL_AIR_FORCE__
