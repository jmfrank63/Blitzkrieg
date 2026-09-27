// Task 7.3: the wheel and the trackpad swipe, from SDL's float deltas to a
// list's pixels and the map's pan (Sources/src/Platform/WheelScroll.h).
//
// EngineChain replays what the engine does with MOUSE_AXIS_Z so the old and
// the new feeding can be compared: CControlAxis::ChangeState (InputAPI.cpp)
// emits new-minus-last of an absolute position, CCombo's forced notify and
// CAxisAccumulator::Add (InputBinder.cpp, InputTypes.h) sum those offsets
// times the control's Power (40, defconf.cfg) into the "mouse_wheel" slider
// minus, and CInputSlider::GetDelta (InputSlider.cpp) hands a frame the
// change times 0.001.
#include "../../Sources/src/Platform/WheelScroll.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int failures = 0;

#define CHECK( condition ) \
	do { \
		if ( !( condition ) ) { \
			std::fprintf( stderr, "wheel scroll check failed (line %d): %s\n", __LINE__, #condition ); \
			++failures; \
		} \
	} while ( false )

static bool Near( float a, float b, float eps = 1e-4f ) { return std::fabs( a - b ) <= eps; }

struct EngineChain
{
	int nAbsPos = 0;
	float fAcc = 0;
	float fLast = 0;
	void Axis( int nNewState )
	{
		if ( nAbsPos == nNewState )
			return;
		fAcc += -float( nNewState - nAbsPos ) * 40.0f;			// slider minus, Power 40
		nAbsPos = nNewState;
	}
	float Frame()
	{
		const float fDelta = ( fAcc - fLast ) * 0.001f;
		fLast = fAcc;
		return fDelta;
	}
};

// The old feeding: SDLApplication's int(y*120) handed to the axis as if it
// were a position.
static std::vector<float> OldFrames( const std::vector<float> &ys )
{
	EngineChain chain;
	std::vector<float> frames;
	for ( float y : ys )
	{
		chain.Axis( int( y * 120.0f ) );
		frames.push_back( chain.Frame() );
	}
	return frames;
}

// The new feeding: the residual carries the fraction, the axis gets a
// running absolute.
static std::vector<float> NewFrames( const std::vector<float> &ys )
{
	NPlatform::CWheelResidual residual;
	NPlatform::CWheelAxis axis;
	EngineChain chain;
	std::vector<float> frames;
	for ( float y : ys )
	{
		int nX = 0, nY = 0;
		residual.Feed( 0.0f, y, &nX, &nY );
		chain.Axis( axis.Feed( nY ) );
		frames.push_back( chain.Frame() );
	}
	return frames;
}

static float Sum( const std::vector<float> &v )
{
	float f = 0;
	for ( float x : v ) f += x;
	return f;
}

static void TestResidual()
{
	NPlatform::CWheelResidual residual;
	int nX = 0, nY = 0;
	residual.Feed( 1.0f, -2.0f, &nX, &nY );
	CHECK( nX == 120 && nY == -240 );			// a notch: exactly what int(y*120) gave
	int nSum = 0;
	for ( int i = 0; i < 250; ++i )
	{
		residual.Feed( 0.0f, 0.004f, &nX, &nY );
		nSum += nY;
		CHECK( nY >= 0 );
	}
	CHECK( nSum == 120 );				// 250 x 0.004 = one notch, nothing lost
	CHECK( int( 0.004f * 120.0f ) == 0 );		// what the old cast made of each of them
	nSum = 0;
	for ( int i = 0; i < 10; ++i )
	{
		residual.Feed( -0.1f, 0.0f, &nX, &nY );
		nSum += nX;
	}
	CHECK( nSum == -120 );
	// After all those fractions, a notch either way is still exactly 120.
	residual.Feed( 1.0f, -1.0f, &nX, &nY );
	CHECK( nX == 120 && nY == -120 );
	for ( int i = 0; i < 7; ++i )
		residual.Feed( 0.013f, 0.0f, &nX, &nY );
	residual.Feed( -1.0f, 0.0f, &nX, &nY );
	CHECK( nX == -119 );		// 7 x 1.56 = 10.92: the 0.92 carried is real travel, not a rounding error
}

static void TestSwipeNoLongerOscillates()
{
	// A swipe that speeds up and slows down, one event per frame.
	const std::vector<float> swipe = { 0.1f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.1f };
	const std::vector<float> old_frames = OldFrames( swipe );
	bool bOldForth = false, bOldBack = false;
	for ( float f : old_frames )
	{
		if ( f < 0 ) bOldForth = true;
		if ( f > 0 ) bOldBack = true;
	}
	CHECK( bOldForth && bOldBack );				// the bug: forth and back
	CHECK( Near( Sum( old_frames ), -12 * 40 * 0.001f ) );	// and it nets 40 x the last delta only
	const std::vector<float> new_frames = NewFrames( swipe );
	for ( float f : new_frames )
		CHECK( f < 0 );						// every frame the same way
	float fSwipe = 0;
	for ( float y : swipe ) fSwipe += y;
	CHECK( Near( Sum( new_frames ), -fSwipe * 4.8f, 1e-3f ) );	// and the whole swipe arrives
}

static void TestMixedSignJitterNetsOut()
{
	const std::vector<float> jitter = { 0.03f, -0.03f, 0.02f, -0.02f, 0.05f, -0.05f };
	const std::vector<float> frames = NewFrames( jitter );
	CHECK( Near( Sum( frames ), 0.0f, 1e-3f ) );
	for ( std::size_t i = 0; i < frames.size(); ++i )
		CHECK( std::fabs( frames[i] ) <= std::fabs( jitter[i] ) * 4.8f + 0.05f );	// no step bigger than the jitter
}

static void TestNotchesAsTheFirstAlwaysWas()
{
	// Three notches up, then one down.
	const std::vector<float> notches = { 1.0f, 1.0f, 1.0f, -1.0f };
	const std::vector<float> old_frames = OldFrames( notches );
	CHECK( Near( old_frames[0], -4.8f ) );
	CHECK( Near( old_frames[1], 0.0f ) && Near( old_frames[2], 0.0f ) );	// the bug: repeats ignored
	CHECK( Near( old_frames[3], 9.6f ) );					// and a reversal doubled
	const std::vector<float> new_frames = NewFrames( notches );
	CHECK( Near( new_frames[0], old_frames[0] ) );	// the first notch is exactly what it was
	CHECK( Near( new_frames[1], -4.8f ) && Near( new_frames[2], -4.8f ) );
	CHECK( Near( new_frames[3], 4.8f ) );
}

static void TestHorizontalLeavesTheVerticalAxisAlone()
{
	// A horizontal swipe's events carry y = 0. Fed as a position, that 0 was
	// a jump back from the last vertical delta.
	const std::vector<float> ys = { 0.5f, 0.0f, 0.0f };
	CHECK( OldFrames( ys )[1] > 0 );
	const std::vector<float> new_frames = NewFrames( ys );
	CHECK( Near( new_frames[1], 0.0f ) && Near( new_frames[2], 0.0f ) );
}

static void TestStepper()
{
	NPlatform::CScrollStepper stepper;
	int nStep = 0;
	CHECK( !stepper.Sub( 21.0f, &nStep ) && nStep == 0 );		// a notch: the caller's own expression
	CHECK( !stepper.Sub( -21.0f, &nStep ) );
	int nTotal = 0;
	for ( int i = 0; i < 10; ++i )
	{
		CHECK( stepper.Sub( 0.3f, &nStep ) );
		CHECK( nStep >= 0 );
		nTotal += nStep;
	}
	CHECK( nTotal == 3 );
	NPlatform::CScrollStepper jittery;
	for ( int i = 0; i < 20; ++i )
	{
		CHECK( jittery.Sub( ( i % 2 ) ? -0.6f : 0.6f, &nStep ) );
		CHECK( nStep == 0 );					// never a step back and forth
	}
	NPlatform::CScrollStepper turning;
	turning.Sub( 0.9f, &nStep );
	CHECK( nStep == 0 );
	turning.Sub( -0.2f, &nStep );
	CHECK( nStep == 0 );						// the 0.9 is dropped, not paid out
	turning.Sub( -0.9f, &nStep );
	CHECK( nStep == -1 );
	CHECK( turning.Sub( 0.0f, &nStep ) && nStep == 0 );
}

static void TestSlowSwipeScrollsAList()
{
	// CUIList moves fDelta * fStep, fStep at least 4.375. A slow swipe, 0.01
	// of a notch a frame: the old int() made nothing of every frame.
	const std::vector<float> slow( 40, 0.01f );
	const std::vector<float> frames = NewFrames( slow );
	NPlatform::CScrollStepper stepper;
	int nPixels = 0, nOldPixels = 0;
	for ( float fDelta : frames )
	{
		const float fAmount = fDelta * 4.375f;
		int nStep = 0;
		CHECK( stepper.Sub( fAmount, &nStep ) );
		CHECK( nStep <= 0 );
		nPixels += nStep;
		nOldPixels += int( fAmount );
	}
	CHECK( nOldPixels == 0 );
	CHECK( nPixels == int( 40 * -0.01f * 4.8f * 4.375f ) );	// -8: the swipe's worth
}

static void TestSource()
{
	NPlatform::CWheelSource source;
	CHECK( !source.IsTrackpad( 1000 ) );			// no finger: a wheel
	source.FingerDown( 7, 1 );
	source.FingerDown( 7, 2 );
	CHECK( source.FingersDown() == 2 );
	CHECK( source.IsTrackpad( 1010 ) );
	source.FingerUp( 7, 1 );
	source.FingerUp( 7, 2 );
	source.FingerUp( 7, 2 );				// a second up is harmless
	CHECK( source.FingersDown() == 0 );
	// Momentum: the stream goes on after the lift, every 16 ms.
	for ( std::uint64_t t = 1026; t < 1600; t += 16 )
		CHECK( source.IsTrackpad( t ) );
	// A notch long after the momentum ended is a wheel again.
	CHECK( !source.IsTrackpad( 3000 ) );
	CHECK( !source.IsTrackpad( 3016 ) );			// and a wheel does not start a momentum chain
	source.FingerDown( 9, 4 );
	source.FingerDown( 9, 4 );				// a repeated down counts once
	CHECK( source.FingersDown() == 1 );
	CHECK( source.IsTrackpad( 5000 ) );
	source.FingerUp( 9, 4 );
	CHECK( source.IsTrackpad( 4990 ) );			// a stamp behind the last is not a gap
}

static void TestPan()
{
	float fX = 0, fY = 0;
	NPlatform::TrackpadPanPixels( 120, 0, &fX, &fY );
	CHECK( Near( fX, 10.0f ) && Near( fY, 0.0f ) );		// one SDL unit, ten points of finger travel
	NPlatform::TrackpadPanPixels( 0, 120, &fX, &fY );
	CHECK( Near( fX, 0.0f ) && Near( fY, -10.0f ) );	// scroll up: the view goes up the screen
	// A slow swipe's fractions pan exactly what they add up to.
	NPlatform::CWheelResidual residual;
	float fTotal = 0;
	for ( int i = 0; i < 100; ++i )
	{
		int nX = 0, nY = 0;
		residual.Feed( 0.03f, 0.0f, &nX, &nY );
		NPlatform::TrackpadPanPixels( nX, nY, &fX, &fY );
		CHECK( fX >= 0 );
		fTotal += fX;
	}
	CHECK( Near( fTotal, 30.0f, 0.1f ) );
}

static void TestSensitivity()
{
	CHECK( NPlatform::TrackpadSensitivityFromOption( NPlatform::kTrackpadSensitivityDefault ) == 1.0f );	// the default is today's behaviour
	CHECK( Near( NPlatform::TrackpadSensitivityFromOption( 0 ), 0.25f ) );
	CHECK( Near( NPlatform::TrackpadSensitivityFromOption( 100 ), 4.0f ) );
	CHECK( Near( NPlatform::TrackpadSensitivityFromOption( -5 ), 0.25f ) && Near( NPlatform::TrackpadSensitivityFromOption( 250 ), 4.0f ) );
	for ( int n = 0; n < 100; ++n )
		CHECK( NPlatform::TrackpadSensitivityFromOption( n ) < NPlatform::TrackpadSensitivityFromOption( n + 1 ) );
	// 1x passes a swipe's integers through untouched.
	NPlatform::CTrackpadScale same;
	int nX = 0, nY = 0;
	for ( int i = 0; i < 20; ++i )
	{
		same.Scale( 7, -13, 1.0f, &nX, &nY );
		CHECK( nX == 7 && nY == -13 );
	}
	// Other settings scale what a swipe pans and scrolls, monotonically in
	// the setting, with the fraction carried.
	int nPrevious = -1;
	for ( int nSlider = 0; nSlider <= 100; nSlider += 10 )
	{
		NPlatform::CTrackpadScale scale;
		const float fScale = NPlatform::TrackpadSensitivityFromOption( nSlider );
		int nTotal = 0;
		for ( int i = 0; i < 100; ++i )
		{
			scale.Scale( 5, 0, fScale, &nX, &nY );
			CHECK( nX >= 0 );
			nTotal += nX;
		}
		CHECK( std::abs( nTotal - int( 500 * fScale ) ) <= 1 );
		CHECK( nTotal > nPrevious );
		nPrevious = nTotal;
	}
}

int main()
{
	TestSensitivity();
	TestResidual();
	TestSwipeNoLongerOscillates();
	TestMixedSignJitterNetsOut();
	TestNotchesAsTheFirstAlwaysWas();
	TestHorizontalLeavesTheVerticalAxisAlone();
	TestStepper();
	TestSlowSwipeScrollsAList();
	TestSource();
	TestPan();
	if ( failures != 0 )
	{
		std::fprintf( stderr, "wheel scroll: %d check(s) failed\n", failures );
		return 1;
	}
	std::puts( "wheel scroll: PASS" );
	return 0;
}
