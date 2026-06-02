#ifndef __ROTATING_FIREPLACES_OBJECT_H__
#define __ROTATING_FIREPLACES_OBJECT_H__

#pragma ONCE
class CRotatingFireplacesObject
{
	DECLARE_SERIALIZE;

	struct SUnitInfo
	{
		DECLARE_SERIALIZE;
	public:
		class CSoldier *pSoldier;
		int nLastFireplace;
		NTimer::STime lastFireplaceChange;

		SUnitInfo() : pSoldier( 0 ), nLastFireplace( 0 ), lastFireplaceChange( 0 ) { }
	};

	std::list<SUnitInfo> units;

	bool IsBetterToGoToFireplace( class CSoldier *pSoldier, const int nFireplace ) const;
public:
	CRotatingFireplacesObject() { }

	void AddUnit( class CSoldier *pSoldier, const int nFireplace );
	void DeleteUnit( class CSoldier *pSoldier );

	virtual void Segment();

	virtual bool CanRotateSoldier( class CSoldier *pSoldier ) const = 0;
	virtual void ExchangeUnitToFireplace( class CSoldier *pSoldier, int nFirePlace ) = 0;
	virtual const int GetNFirePlaces() const = 0;
	virtual class CSoldier* GetSoldierInFireplace( const int nFireplace) const = 0;
};
#endif // __ROTATING_FIREPLACES_OBJECT_H__
