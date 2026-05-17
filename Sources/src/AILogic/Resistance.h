#ifndef __RESISTANCE_H__
#define __RESISTANCE_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ����� �������������
struct SResistance
{
private:
	int nCellNumber;
	int nWeight;
public:
	SResistance() { Clear(); }
	SResistance( const int _nCellNumber, const float fWeight ) 
		: nCellNumber( _nCellNumber ), nWeight( fWeight * 100 ) { }

	void Clear(){ nCellNumber = -1; nWeight = -1.0f; }
	const float GetWeight() const { return (float)nWeight / 100.0f; }
	const bool IsInitted() const { return nWeight != -1; }
	const int GetCellNumber() const { return nCellNumber; }
	const CVec2 GetResistanceCellCenter() const { return GetResistanceCellCenter( nCellNumber ); }

	static const CVec2 GetResistanceCellCenter( const int nCell );

	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;

		saver.Add( 1, &nCellNumber );
		saver.Add( 2, &nWeight );

		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SResistanceCmp
{
	bool operator()( const SResistance &r1, const SResistance &r2 ) const
	{
		return r1.GetWeight() > r2.GetWeight();
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::set<SResistance, SResistanceCmp> CResistance;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CResistancesContainer
{
	DECLARE_SERIALIZE;
	
	CResistance resistances;

	struct SSellInfo
	{
		float fCellWeight;									// net weight for this cell
		bool bInUse;												// this cell is being attacked
		bool bAllowShoot;										// shooting to the cell is allowed

		SSellInfo() : fCellWeight( 0.0f ), bInUse( false ), bAllowShoot( true ) { }
		SSellInfo( const float _fCellWeight, const bool _bInUse, const bool _bAllowShoot ) : fCellWeight( _fCellWeight ), bInUse( _bInUse ), bAllowShoot( _bAllowShoot ) { }
	};

	typedef std::unordered_map<int, SSellInfo> CCellsWeights;
	CCellsWeights cellsWeights;
	std::list<CCircle> excluded;				// general will not shoot to these circles

	class CIter
	{
		CResistance::iterator iter;
		CResistancesContainer *pContainter;

		CIter() { }
		void IterateToNotInUse();
	public:
		CIter( CResistancesContainer *_pContainter, CResistance::iterator _iter )
			: pContainter( _pContainter ), iter( _iter ) { IterateToNotInUse(); }

		void Iterate();
		bool IsFinished() const { return iter == pContainter->resistances.end(); }
		const SResistance& operator*() const { NI_ASSERT_T( !IsFinished(), "Can't call operator *" ); return *iter;	}
	};

	const int GetResistanceCellNumber( const CVec2 &vPos );
	bool IsCellExcluded( const CVec2 &vCellCenter );

	void AddCell( const int nCell, const SSellInfo &cell );
public:
	CResistancesContainer() { }

	void Clear() { resistances.clear(); cellsWeights.clear(); }

	void UpdateEnemyUnitInfo( class CAIUnitInfoForGeneral *pInfo,
			const NTimer::STime lastVisibleTimeDelta, const CVec2 &vLastVisiblePos,
			const NTimer::STime lastAntiArtTimeDelta, const CVec2 &vLastVisibleAntiArtCenter, const float fDistToLastVisibleAntiArt );			

	void UnitDied( CAIUnitInfoForGeneral *pInfo );
	void UnitChangedParty( CAIUnitInfoForGeneral *pInfo );

	bool IsEmpty() const { return resistances.empty(); }

	void SetCellInUse( const int nResistanceCellNumber, bool bInUse );
	bool IsInUse( const int nResistanceCellNumber );

	void RemoveExcluded( const CVec2 &vCenter );
	void AddExcluded( const CVec2 &vCenter, const float fRadius );
	bool IsInResistanceCircle( const CVec2 &vCenter ) const;

	typedef CIter iterator;
	friend class CIter;

	iterator begin() { return CIter( this, resistances.begin() ); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RESISTANCE_H__
