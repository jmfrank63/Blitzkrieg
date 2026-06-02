#ifndef __HITS_STORE_H__
#define __HITS_STORE_H__

#pragma ONCE
class CHitsStore
{
	DECLARE_SERIALIZE;
	
	CArray2D<BYTE> hits[2];
	BYTE curIndex;
	NTimer::STime timeOfIndexBegin;
public:
	enum EHitTypes { EHT_OPEN_SIGHT = 0x1, EHT_OVER_SIGHT = 0x10, EHT_ANY = 0x11 };

	void Init( const int nMapSizeX, const int nMapSizeY );
	void Clear();
	void Segment();

	void AddHit( const CVec2 &center, const EHitTypes eHitType );
	bool WasHit( const CVec2 &center, const float fR, const EHitTypes eHitType ) const;
};
#endif // __HITS_STORE_H__
