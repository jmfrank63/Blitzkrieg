#ifndef __RANDOMGENINTERNAL_H__
#define __RANDOMGENINTERNAL_H__
#pragma ONCE
#include "RandomGen.h"
const int RANDSIZL = 8;
const int RANDSIZ = 1 << RANDSIZL;
struct SRandData
{
	unsigned _int32 randcnt;
	unsigned _int32 randrsl[RANDSIZ];
	unsigned _int32 randmem[RANDSIZ];
	unsigned _int32 randa;
	unsigned _int32 randb;
	unsigned _int32 randc;
};
class CRandomGenSeed : public CTRefCount<IRandomGenSeed>
{
	OBJECT_SERVICE_METHODS( CRandomGenSeed );
	DECLARE_SERIALIZE;
	SRandData rnd;
	bool RecFindFile( LPSTR pszFindedName, LPCSTR pszBaseMask, int nToFind, int* pnTotFinded );
	void FillRandRsl();
	void InitVariables();
public:
	virtual void STDCALL Init();
	virtual void STDCALL InitByZeroSeed();
	
	const SRandData& GetRandData() const { return rnd; }
	void SetRandData( const SRandData &_rnd ) { rnd = _rnd; }
	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL Store( IDataStream *pStream );
	virtual void STDCALL Restore( IDataStream *pStream );
};
void Isaac( SRandData *pRnd );
class CRandomGenerator : public CTRefCount<IRandomGen>
{
	OBJECT_SERVICE_METHODS( CRandomGenerator );
	DECLARE_SERIALIZE;
	BOOL bIsReady;
	SRandData rnd;
public:
	CRandomGenerator() { bIsReady = FALSE; }
	virtual void STDCALL Init();
	virtual void STDCALL SetSeed( IRandomGenSeed *pSeed );
	virtual IRandomGenSeed* STDCALL GetSeed();
	virtual unsigned int STDCALL Get()
	{
		if ( rnd.randcnt-- == 0 )
		{
			Isaac( &rnd );
			rnd.randcnt = RANDSIZ - 1;
		}
		return rnd.randrsl[rnd.randcnt];
	}
	virtual void STDCALL Store( IDataStream *pStream );
	virtual void STDCALL Restore( IDataStream *pStream );

	unsigned _int32 Get( unsigned _int32 nMax ) { return Get() % ( nMax + 1 ); }
	unsigned _int32 Get( unsigned _int32 nMin, unsigned _int32 nMax )	{ return Get( nMax - nMin ) + nMin; }
	float GetFloat( float fMin, float fMax ) { return fMin + float( double( Get() ) * double( fMax - fMin ) / double( 0xFFFFFFFF ) ); }
	unsigned _int32 Dice( unsigned _int32 nNum, unsigned _int32 nDice )
	{
		unsigned _int32 nRes = 0;
		for ( unsigned _int32 i = 0; i < nNum; i++ )
			nRes += Get() % nDice + 1;
		return nRes;
	}
};
#endif // __RANDOMGENINTERNAL_H__
