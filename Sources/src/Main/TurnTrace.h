#ifndef __TURN_TRACE_H__
#define __TURN_TRACE_H__
#pragma ONCE
// BK_TURN_TRACE=1: where the frame of a Remastered camera turn
// (NCameraTurn) spends its time, by phase, and the frames around it
// (docs/superpowers/specs/2026-10-26-camera-turn-design.md, "the turn
// preload"). Each module adds its phase's milliseconds to a global var
// `Perf.Turn.<phase>`, as BK_PERF's Perf.FormVisMs crosses from Scene to
// Common; CInterfaceScreenBase::Step prints them and sets them back to 0
// every frame. Off, every call is a test of one static bool. BK_TURN_TRACE=2\n// also times the fine phases (sprite and texture loads, storage probes), which\n// are called thousands of times a turn: the trace's own cost shows in the\n// frame's total then.
//
// The header reads global vars through GetGlobalVar, which each module's
// StdAfx.h brings in (StreamIO/Globals.h).
namespace NTurnTrace
{
	inline int Level()
	{
		static const int nLevel = getenv( "BK_TURN_TRACE" ) == 0 ? 0 : atoi( getenv( "BK_TURN_TRACE" ) ) > 1 ? 2 : 1;
		return nLevel;
	}
	inline bool On()
	{
		return Level() > 0;
	}
	// The phases a turn's frame is broken down into, printed in this order.
	const char * const pszPhases[] = { "world", "anim", "tex", "probe", "terrain", "fog", "minimap", "draw", "preload" };
	inline std::string VarName( const char *pszPhase )
	{
		return std::string( "Perf.Turn." ) + pszPhase;
	}
	inline void Add( const char *pszPhase, const double fMs )
	{
		const std::string szVar = VarName( pszPhase );
		SetGlobalVar( szVar.c_str(), float( GetGlobalVar( szVar.c_str(), 0.0f ) + fMs ) );
	}
	// Times its own scope into a phase while the trace is on (a fine phase at
	// level 2 only).
	class CScope
	{
		const char *pszPhase;
		const bool bActive;
		LARGE_INTEGER start;
	public:
		explicit CScope( const char *_pszPhase, const bool bFine = false ) : pszPhase( _pszPhase ), bActive( Level() > ( bFine ? 1 : 0 ) )
		{
			if ( bActive )
				QueryPerformanceCounter( &start );
		}
		~CScope()
		{
			if ( !bActive )
				return;
			LARGE_INTEGER end, freq;
			QueryPerformanceCounter( &end );
			QueryPerformanceFrequency( &freq );
			Add( pszPhase, ( end.QuadPart - start.QuadPart ) * 1000.0 / double( freq.QuadPart ) );
		}
	};
};
#endif // __TURN_TRACE_H__
