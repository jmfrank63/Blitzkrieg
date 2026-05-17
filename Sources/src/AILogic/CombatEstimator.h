#ifndef __COMBATESTIMATOR_H__
#define __COMBATESTIMATOR_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\HashFuncs.h"
#include "AIHashFuncs.h"
class CAIUnit;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ��� ����������� ��������� �������
class CCombatEstimator
{
	DECLARE_SERIALIZE;
	float fDamage;
	struct SShellInfo
	{
		NTimer::STime time;
		float fDamage;
		//
		SShellInfo() {}
		SShellInfo( NTimer::STime time, float fDamage )
			:time( time ), fDamage( fDamage ) { }
	};

	typedef std::unordered_set<int> CRegisteredUnits;
	CRegisteredUnits registeredMechUnits;			// ��������� ����� (�� ������)� ��������� ������� ���������
	CRegisteredUnits registeredInfantry;			// ��������� ����� (������)� ��������� ������� ���������

	typedef std::list<SShellInfo> CShellTimes;
	CShellTimes shellTimes;								// ����� ��������

public:
	CCombatEstimator();

	void Clear();
	void Segment();

	bool IsCombatSituation() const ;

	void AddShell( NTimer::STime time, float fDamage );
	
	void AddUnit( CAIUnit *pUnit );
	void DelUnit( CAIUnit *pUnit );
	                                                                                                                          
};
#endif // __COMBATESTIMATOR_H__

