#ifndef __UNITS_H__
#define __UNITS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// for profiling
#include "..\Misc\FreeIDs.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIUnit;
class CFormation;
class CAviation;

class CCircleIter;
class CRectIter;
class CLineIter;

template<class T> class CIter;
class CGlobalIter;
class CPlanesIter;
class CDeadPlanesIter;

template<BYTE cOnlyVisible, int nSize> class CUnitsIter;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnits : public IRefCount
{
	OBJECT_NORMAL_METHODS( CUnits );
	DECLARE_SERIALIZE;
	
	// ��� �����, ������������� �� ����. ��������
	CListsSet< CObj<CAIUnit> > units;
	std::set< CObj<CFormation> > formations;
	std::vector<int> sizes;
	// �������
	std::list< CObj<CAviation> > planes;
	// ��� �������� ���������� � �������� ��������

	// ���������� ������ � ������
	CArray2D<WORD> nUnitsCell;
	// ����� ������
	CArray2D<WORD> nCell;
	// ������ ������ ��� ������ �� �����, 0 - not visible for enemy, 1 - visible for enemy
	std::vector< CListsSet<WORD> > unitsInCells;
	// ������� ����� � �������
	struct SUnitPosition
	{
		int nCellID; int nUnitPos; SVector cell;
		SUnitPosition() : nCellID(0), nUnitPos(0), cell( 0, 0 ) { }
	};
	std::vector<SUnitPosition> posUnitInCell;

	enum { N_CELLS_LEVELS = 3 };
	CArray2D<WORD> numUnits[2][N_CELLS_LEVELS][3][2];
	
	// ��� ��������� �����
	CFreeIds cellsIds;
	// ��� ������������
	std::unordered_map< int, SVector > cellIdToCoord;


	// CRAP{ for debug
	std::unordered_set<int> unitsInCellsSet;
	// CRAP}
	
	std::vector< std::unordered_map<int, int> > nUnitsOfType;
	
	//
	void AddUnitToConcreteCell( class CAIUnit* pUnit, const SVector &cell, bool bWithLeveledCelles );
	void AddUnitToCell( class CAIUnit *pUnit, const CVec2 &newPos, bool bWithLeveledCelles );

	void AddUnitToLeveledCells( CAIUnit *pUnit, const SVector &bigCell, const int nVis );
	void DelUnitFromLeveledCells( CAIUnit *pUnit, const SVector &bigCell, const int nVis );
	const bool IsUnitInCell( const int nUnitID ) const;
public:
	CUnits() { }
	
	void Init();
	void Clear() { DestroyContents(); }
	
	int AddFormation( class CFormation *pFormation );
	void DelFormation( class CFormation *pFormation );
	
	// �������� ���� � ������ ������ � ������ ��� �����
	int AddUnitToUnits( class CAIUnit *pUnit, const int nPlayer, const int nUnitType );
	// �������� ���� �� �����
	void AddUnitToMap( class CAIUnit *pUnit );
	
	// ������� ���� ��������, �� �� ����� ��� id
	void DeleteUnitFromMap( class CAIUnit *pUnit );
	// ������������ ������� ����
	void FullUnitDelete( class CAIUnit *pUnit );

	void AddUnitToCell( class CAIUnit *pUnit, bool bWithLeveledCelles );
	void DelUnitFromCell( class CAIUnit *pUnit, bool bWithLeveledCelles );
	
	void UnitChangedPosition( class CAIUnit *pUnit, const CVec2 &newPos );
	void ChangePlayer( class CAIUnit *pUnit, const BYTE cNewPlayer );
	
	CAIUnit* operator[]( const int id );
	const int Size( const int nParty ) const;
	
	// ���������� ������ � ����� ������� vCenter � �������� fRadius � ������� nParty
	// ������ ���������� - ������ ������������ ������ �����
	const int GetNSoldiers( const CVec2 &vCenter, const float fRadius, const int nParty );
	const int GetNUnits( const CVec2 &vCenter, const float fRadius, const int nParty );

	const int GetNUnitsOfType( const int nParty, const int nType )
	{
		NI_ASSERT_T( nParty >= 0 && nParty < 3, NStr::Format( "Wrong number of party (%d)", nParty ) );
		return nUnitsOfType[nParty][nType];
	}

	void UpdateUnitVis4Enemy( CAIUnit *pUnit );

	const int GetVisIndex( CAIUnit *pUnit );

	// for debug
	void CheckCorrectness( const SVector &tile );
	void CheckUnitCell();
	
	friend class CIter<CCircleIter>;
	friend class CIter<CRectIter>;
	friend class CIter<CLineIter>;
  friend class CGlobalIter;
	friend class CPlanesIter;
	friend class CDeadPlanesIter;

	friend class CUnitsIter<0,3>;
	friend class CUnitsIter<0,2>;
	friend class CUnitsIter<0,1>;
	friend class CUnitsIter<0,0>;
	friend class CUnitsIter<1,3>;
	friend class CUnitsIter<1,2>;
	friend class CUnitsIter<1,1>;
	friend class CUnitsIter<1,0>;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UNITS_H__
