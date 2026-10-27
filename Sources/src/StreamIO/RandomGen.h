#ifndef __RANDOM_GEN_H__
#define __RANDOM_GEN_H__
#pragma ONCE
interface IRandomGenSeed : public IRefCount
{
	virtual void STDCALL Init() = 0;
	virtual void STDCALL InitByZeroSeed() = 0;
	virtual int STDCALL operator&( IDataTree &ss ) = 0;
	virtual void STDCALL Store( IDataStream *pStream ) = 0;
	virtual void STDCALL Restore( IDataStream *pStream ) = 0;
};
interface IRandomGen : public IRefCount
{
	enum { tidTypeID = -3 };
	virtual void STDCALL Init() = 0;
	virtual void STDCALL SetSeed( IRandomGenSeed *pSeed ) = 0;
	virtual IRandomGenSeed* STDCALL GetSeed() = 0;
	virtual unsigned int STDCALL Get() = 0;
	virtual void STDCALL Store( IDataStream *pStream ) = 0;
	virtual void STDCALL Restore( IDataStream *pStream ) = 0;
	// The replay-rng-audit trap (StreamIOZig/legacy_bridge.cpp, debug builds with
	// BK_RNG_AUDIT=1): a replay holds commands, not draws, so a draw that the
	// simulation did not make itself shifts every draw after it on playback. The
	// transceivers bracket IAILogic::Segment() and command execution, the mission
	// the AI's Init; between ArmAudit( true ) at the mission start and
	// ArmAudit( false ) at its end every draw outside a bracket is reported.
	virtual void STDCALL EnterSimulation() = 0;
	virtual void STDCALL LeaveSimulation() = 0;
	virtual void STDCALL ArmAudit( const bool bArm ) = 0;
	// BK_RNG_TRACE=<from>-<to> (non-final builds): every draw the AI makes in
	// those segments is printed with its caller, so two runs that part at one
	// segment can be compared draw by draw. The single-player transceiver tells
	// the generator which segment runs.
	virtual void STDCALL SetTraceSegment( const int nSegment ) = 0;
};
extern IRandomGen *g_pGlobalRandomGen;
// One bracket of the simulation for the trap above, for the scope it lives in.
class CSimulationRandomScope
{
	IRandomGen *pGen;
public:
	explicit CSimulationRandomScope( IRandomGen *_pGen ) : pGen( _pGen ) { if ( pGen ) pGen->EnterSimulation(); }
	~CSimulationRandomScope() { if ( pGen ) pGen->LeaveSimulation(); }
};
inline unsigned int Random() 
{ 
#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	if ( GetGlobalVar( "lograndom", 0 ) != 0 )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( 10,
			NStr::Format( "r" ),
			0, true
		);
	}
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

	return g_pGlobalRandomGen->Get();
}
inline unsigned int Random( unsigned int uMax ) 
{
	const unsigned int nResult = Random() % uMax;

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	if ( GetGlobalVar( "lograndom", 0 ) != 0 )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( 10,
			NStr::Format( "r %d = %d", uMax, nResult ),
			0, true
		);
	}
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

	return nResult;
}

inline int Random( int nMin, int nMax ) 
{
	const int nResult = nMin + (int)Random( (unsigned int)(nMax - nMin + 1) );

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	if ( GetGlobalVar( "lograndom", 0 ) != 0 )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( 10,
			NStr::Format( "int r %d, %d = %d", nMin, nMax, nResult ),
			0, true
		);
	}
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)

	return nResult;
}

inline float Random( float fMin, float fMax )
{
	const float fResult = fMin + float( double(Random()) / double(0xffffffffUL) * double(fMax - fMin) );

#if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	if ( GetGlobalVar( "lograndom", 0 ) != 0 )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( 10,
			NStr::Format( "f r %g, %g = %g", fMin, fMax, fResult ),
			0, true
		);
	}
#endif // #if !defined(_FINALRELEASE) && !defined(_BETARELEASE)
	
	return fResult;
}

inline int Dice( int nNum, int nDice )
{
	unsigned int uRes = 0;
	for ( int i = 0; i < nNum; ++i )
		uRes += Random( nDice )  + 1;
	return uRes;
}
inline unsigned int RandomCheck( unsigned int uMax ) { return uMax == 0 ? 0 : Random( uMax ); }
inline int RandomCheck( int nMin, int nMax ) { return nMax < nMin ? nMin : Random( nMin, nMax ); }
#endif // __RANDOM_GEN_H__
