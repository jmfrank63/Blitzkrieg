#ifndef BLITZKRIEG_PLATFORM_WHEEL_SCROLL_H
#define BLITZKRIEG_PLATFORM_WHEEL_SCROLL_H

// The mouse wheel and the trackpad swipe, from SDL's float deltas to what the
// engine consumes: std-only, so tools/zig/wheel_scroll_test.cpp tests every
// piece without SDL or the engine.
//
// SDL 3 on macOS sends a two-finger swipe as a stream of SDL_EVENT_MOUSE_WHEEL
// events, one per NSEvent, through the momentum phase after the fingers lift:
// x = -scrollingDeltaX and y = scrollingDeltaY, both times 0.1 when the device
// has precise deltas (SDL_cocoamouse.m), so a slow swipe is a run of 0.1-0.5s.
// A physical wheel sends about +-1 per notch. The values already carry the
// user's natural-scrolling setting; `direction` only reports it, and nothing
// here applies it a second time.

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace NPlatform
{
// One notch in the legacy WHEEL_DELTA units every consumer is tuned against.
constexpr int kWheelDelta = 120;

// SDL's float deltas as legacy WHEEL_DELTA integers, the fraction carried to
// the next event rather than truncated away: 0.004 of a notch per event used to
// give int(0.48) = 0 every time. A whole notch still gives exactly 120.
class CWheelResidual
{
	double fX = 0;
	double fY = 0;
	static int Take( double &fAcc, float fDelta )
	{
		fAcc += double( fDelta ) * kWheelDelta;
		// A float's rounding error left over from earlier fractions must not
		// turn the next notch's -120 into -119.99999 and truncate it to -119.
		const double fNearest = std::round( fAcc );
		const double fWhole = std::fabs( fAcc - fNearest ) < 1e-3 ? fNearest : std::trunc( fAcc );
		fAcc -= fWhole;
		return int( fWhole );
	}
public:
	void Feed( float x, float y, int *pnX, int *pnY )
	{
		*pnX = Take( fX, x );
		*pnY = Take( fY, y );
	}
};

// The wheel as the absolute axis CControlAxis reads (InputAPI.cpp): it
// emits new-minus-last, like the DirectInput and Win32 sources it was written
// for (WinFrame.cpp's WM_MOUSEWHEEL keeps a running absZ). Fed a per-event
// delta instead, the binder's running sum of those differences telescoped to
// 40 x the last event's delta: a swipe went forth while it sped up, back
// while it slowed down, and ended where it began; a second notch in the same
// direction repeated the last value and was ignored.
class CWheelAxis
{
	std::uint32_t nPos = 0;
public:
	int Feed( int nDelta )
	{
		nPos += std::uint32_t( nDelta );			// wraps instead of overflowing
		return int( std::int32_t( nPos ) );
	}
};

// Tells a trackpad swipe from a physical wheel. SDL has no field for it:
// `which` is the global mouse for both, and an accelerated wheel gives
// fractions too. With SDL_HINT_TRACKPAD_IS_TOUCH_ONLY set, the fingers on a
// trackpad arrive as SDL_EVENT_FINGER_* events (without it SDL drops them), so
// a wheel event while a finger is down is the trackpad's. The momentum after
// the fingers lift keeps arriving every frame or so; an event within
// kMomentumGapMs of the last trackpad event is still the trackpad's.
class CWheelSource
{
	std::vector< std::pair<std::uint64_t, std::uint64_t> > fingers;
	std::uint64_t nLastTrackpadMs = 0;
	bool bHaveTrackpad = false;
public:
	static constexpr std::uint64_t kMomentumGapMs = 150;
	void FingerDown( std::uint64_t nTouch, std::uint64_t nFinger )
	{
		FingerUp( nTouch, nFinger );
		fingers.push_back( std::make_pair( nTouch, nFinger ) );
	}
	void FingerUp( std::uint64_t nTouch, std::uint64_t nFinger )
	{
		for ( std::size_t i = 0; i < fingers.size(); ++i )
			if ( fingers[i].first == nTouch && fingers[i].second == nFinger )
			{
				fingers.erase( fingers.begin() + i );
				return;
			}
	}
	int FingersDown() const { return int( fingers.size() ); }
	bool IsTrackpad( std::uint64_t nTimestampMs )
	{
		const bool bMomentum = bHaveTrackpad && ( nTimestampMs < nLastTrackpadMs || nTimestampMs - nLastTrackpadMs <= kMomentumGapMs );
		const bool bTrackpad = !fingers.empty() || bMomentum;
		if ( bTrackpad )
		{
			nLastTrackpadMs = nTimestampMs;
			bHaveTrackpad = true;
		}
		return bTrackpad;
	}
};

// Screen pixels a trackpad swipe pans the game's map per legacy unit: one SDL
// unit is 10 points of finger travel, 120 legacy units, so the map follows
// the fingers one to one. Positive x pans right (SDL: scroll right); positive
// y pans up, which is negative screen y.
constexpr float kTrackpadPixelsPerWheelDelta = 10.0f / kWheelDelta;
inline void TrackpadPanPixels( int nX, int nY, float *pfX, float *pfY )
{
	*pfX = float( nX ) * kTrackpadPixelsPerWheelDelta;
	*pfY = -float( nY ) * kTrackpadPixelsPerWheelDelta;
}

// The player's trackpad scroll sensitivity, the GamePlay.TrackpadScroll
// option's slider position 0-100, as a multiplier: 50 (the default) is 1x -
// the map following the fingers one to one - and each 50 either side is a
// factor of four, so the slider spans 0.25x to 4x evenly by ear.
constexpr int kTrackpadSensitivityDefault = 50;
inline float TrackpadSensitivityFromOption( int nSlider )
{
	if ( nSlider < 0 ) nSlider = 0;
	if ( nSlider > 100 ) nSlider = 100;
	if ( nSlider == kTrackpadSensitivityDefault )
		return 1.0f;
	return float( std::pow( 4.0, double( nSlider - kTrackpadSensitivityDefault ) / 50.0 ) );
}

// A trackpad event's WHEEL_DELTA integers times the sensitivity, the fraction
// carried like CWheelResidual's. At 1x the integers pass through untouched.
class CTrackpadScale
{
	double fX = 0;
	double fY = 0;
	static int Take( double &fAcc, int nDelta, float fScale )
	{
		fAcc += double( nDelta ) * double( fScale );
		const double fNearest = std::round( fAcc );
		const double fWhole = std::fabs( fAcc - fNearest ) < 1e-3 ? fNearest : std::trunc( fAcc );
		fAcc -= fWhole;
		return int( fWhole );
	}
public:
	void Scale( int nX, int nY, float fScale, int *pnX, int *pnY )
	{
		*pnX = Take( fX, nX, fScale );
		*pnY = Take( fY, nY, fScale );
	}
};

// A list's wheel step, for the UI controls whose scroll position is an int.
// A step of a pixel or more (a notch is 21) goes through unchanged, the way it
// always did - Sub returns false and the caller keeps its own expression. A
// smaller one - a trackpad frame - accumulates until it makes a whole pixel,
// instead of being truncated to nothing every frame; a change of direction
// drops what was carried, so jitter nets out without stepping back and forth.
class CScrollStepper
{
	float fResidual = 0;
public:
	bool Sub( float fAmount, int *pnStep )
	{
		*pnStep = 0;
		if ( std::fabs( fAmount ) >= 1.0f )
		{
			fResidual = 0;
			return false;
		}
		if ( fAmount == 0 )
			return true;
		if ( fResidual != 0 && ( fAmount > 0 ) != ( fResidual > 0 ) )
			fResidual = 0;
		fResidual += fAmount;
		*pnStep = int( fResidual );
		fResidual -= float( *pnStep );
		return true;
	}
};
}

#endif
