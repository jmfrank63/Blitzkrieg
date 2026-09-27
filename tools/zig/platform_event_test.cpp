#include "../../Sources/src/Platform/Clock.h"
#include "../../Sources/src/Platform/SDLApplication.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

#define CHECK(condition) \
	do { \
		if ( !(condition) ) { \
			std::fprintf( stderr, "platform event check failed: %s\\n", #condition ); \
			return 1; \
		} \
	} while ( false )

static bool Push(SDL_Event event)
{
	return SDL_PushEvent( &event ) == 1;
}

static bool NextWheel(NPlatform::SDLApplication &app, NPlatform::PlatformEvent &event)
{
	for ( int attempt = 0; attempt < 4; ++attempt )
		while ( app.PollEvent( event ) )
			if ( event.type == NPlatform::EventType::mouseWheel ) return true;
	return false;
}

int main()
{
	NPlatform::SDLApplication app;
	CHECK( app.Initialize( "Blitzkrieg event test", 320, 200 ) );
	NPlatform::PlatformEvent ignored;
	while ( app.PollEvent( ignored ) ) {}
	// The stamps below have to fall between that drain and the poll further
	// down, where a real event's stamp always falls.
	SDL_Delay( 100 );
	const Uint64 t0 = SDL_GetTicksNS() - 70 * 1000000;

	SDL_Event event{};
	event.type = SDL_EVENT_WINDOW_RESIZED; event.window.timestamp = t0 + 10 * 1000000; event.window.data1 = 640; event.window.data2 = 480; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_KEY_DOWN; event.key.timestamp = t0 + 20 * 1000000; event.key.key = SDLK_RETURN; event.key.mod = SDL_KMOD_ALT; event.key.repeat = true; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_TEXT_INPUT; event.text.timestamp = t0 + 30 * 1000000; event.text.text = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-long"; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_MOUSE_MOTION; event.motion.timestamp = t0 + 40 * 1000000; event.motion.x = 12.0f; event.motion.y = 34.0f; event.motion.xrel = 2.0f; event.motion.yrel = -3.0f; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.timestamp = t0 + 50 * 1000000; event.wheel.x = 1.0f; event.wheel.y = -2.0f; event.wheel.mouse_x = 12.0f; event.wheel.mouse_y = 34.0f; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_QUIT; event.quit.timestamp = t0 + 60 * 1000000; CHECK( Push( event ) );
	event = {}; event.type = static_cast<SDL_EventType>( 0x7fff0000 ); CHECK( Push( event ) );

	NPlatform::PlatformEvent translated[6]{};
	int count = 0;
	while ( count < 6 && app.PollEvent( translated[count] ) ) ++count;
	CHECK( count == 6 );
	CHECK( translated[0].type == NPlatform::EventType::windowResized );
	// SDL3 stamps events in nanoseconds; PollEvent converts to the milliseconds
	// the input layer measures double clicks and key repeats in.
	CHECK( translated[0].x == 640 && translated[0].y == 480 );
	// Timestamps come out in milliseconds on the engine's monotonic clock, not
	// SDL's tick epoch: CInputSlider integrates a held key from its activation
	// stamp to CInputAPI's clock, and mixing the two epochs made a single tap of
	// an arrow key read as days of holding it. Only the offset is shared, so the
	// spacing between events has to survive and the absolute value has to sit
	// alongside MonotonicMilliseconds.
	const std::uint32_t engine_now = NPlatform::MonotonicMilliseconds();
	CHECK( translated[0].timestamp + 60000u > engine_now && translated[0].timestamp < engine_now + 60000u );
	CHECK( translated[1].type == NPlatform::EventType::keyDown && translated[1].repeat );
	CHECK( translated[1].modifiers == SDL_KMOD_ALT );
	CHECK( translated[2].type == NPlatform::EventType::textInput );
	CHECK( translated[2].text[sizeof( translated[2].text ) - 1] == '\0' );
	CHECK( std::strlen( translated[2].text ) == sizeof( translated[2].text ) - 1 );
	CHECK( translated[3].type == NPlatform::EventType::mouseMotion && translated[3].x == 12 && translated[3].data2 == -3 );
	CHECK( translated[4].type == NPlatform::EventType::mouseWheel && translated[4].x == 120 && translated[4].y == -240 );
	CHECK( translated[5].type == NPlatform::EventType::quit );
	// 10ms and 60ms in, so 50ms apart whatever the offset is.
	CHECK( translated[5].timestamp - translated[0].timestamp == 50 );

	// A stamp can still be off the engine clock by more than the epoch. SDL's
	// Cocoa backend pins NSEvent stamps, which stop while the Mac sleeps, to its
	// tick clock, which does not, once at the first event, and afterwards only
	// pulls back stamps that land in the future. Every sleep with the game open
	// left each later event that much further in the past, so an arrow key
	// pressed after waking was integrated from hours ago and the camera jumped
	// to the edge of the map until the game was restarted. No event polled now
	// can have happened before the previous drain found the queue empty, nor
	// after the poll that returns it.
	while ( app.PollEvent( ignored ) ) {}
	const std::uint32_t drained = NPlatform::MonotonicMilliseconds();
	event = {}; event.type = SDL_EVENT_KEY_DOWN; event.key.timestamp = 1; event.key.key = SDLK_LEFT; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_KEY_UP; event.key.timestamp = SDL_GetTicksNS() + 10 * SDL_NS_PER_SECOND; event.key.key = SDLK_LEFT; CHECK( Push( event ) );
	NPlatform::PlatformEvent stale{}, early{};
	CHECK( app.PollEvent( stale ) && app.PollEvent( early ) );
	const std::uint32_t polled = NPlatform::MonotonicMilliseconds();
	CHECK( stale.type == NPlatform::EventType::keyDown && early.type == NPlatform::EventType::keyUp );
	CHECK( stale.timestamp + 50u > drained && stale.timestamp < drained + 50u );
	CHECK( early.timestamp <= polled && polled - early.timestamp < 50u );

	// Task 7.3: a two-finger swipe as SDL 3 delivers it on macOS - many small
	// fractional wheel events, both axes - through the real translation.
	while ( app.PollEvent( ignored ) ) {}
	// A slow swipe: 0.004 of a notch per event used to be int(0.48) = 0 each
	// time. The fraction is carried now, so 250 of them are one notch.
	for ( int i = 0; i < 250; ++i )
	{
		event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.x = -0.002f; event.wheel.y = 0.004f; CHECK( Push( event ) );
	}
	int nSumX = 0, nSumY = 0, nWheels = 0;
	bool bMonotonic = true, bTrackpad = false;
	while ( app.PollEvent( ignored ) )
	{
		if ( ignored.type != NPlatform::EventType::mouseWheel ) continue;
		++nWheels;
		nSumX += ignored.x;
		nSumY += ignored.y;
		if ( ignored.x > 0 || ignored.y < 0 ) bMonotonic = false;
		bTrackpad = bTrackpad || ignored.trackpad;
	}
	CHECK( nWheels == 250 );
	CHECK( nSumY == 120 && nSumX == -60 );
	CHECK( bMonotonic );
	CHECK( !bTrackpad );			// no finger down: a wheel, as far as anyone can tell
	// Natural scrolling: a FLIPPED event keeps the sign SDL delivered.
	event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.x = 0.0f; event.wheel.y = -1.0f; event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED; CHECK( Push( event ) );
	NPlatform::PlatformEvent flipped{};
	CHECK( app.PollEvent( flipped ) && flipped.type == NPlatform::EventType::mouseWheel && flipped.y == -120 );
	// A finger on the trackpad (SDL_HINT_TRACKPAD_IS_TOUCH_ONLY makes SDL
	// send them) marks the wheel events as the trackpad's; the finger events
	// themselves are not the engine's business.
	CHECK( std::strcmp( SDL_GetHint( SDL_HINT_TRACKPAD_IS_TOUCH_ONLY ) ? SDL_GetHint( SDL_HINT_TRACKPAD_IS_TOUCH_ONLY ) : "", "1" ) == 0 );
	event = {}; event.type = SDL_EVENT_FINGER_DOWN; event.tfinger.touchID = 77; event.tfinger.fingerID = 1; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_FINGER_DOWN; event.tfinger.touchID = 77; event.tfinger.fingerID = 2; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.x = 0.3f; event.wheel.y = 0.2f; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_FINGER_UP; event.tfinger.touchID = 77; event.tfinger.fingerID = 1; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_FINGER_UP; event.tfinger.touchID = 77; event.tfinger.fingerID = 2; CHECK( Push( event ) );
	// Momentum, right after the lift: still the trackpad's.
	event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.x = 0.1f; event.wheel.y = 0.1f; CHECK( Push( event ) );
	NPlatform::PlatformEvent swipe{}, momentum{};
	// PollEvent swallows the finger events; a poll that meets SDL's end-of-pump
	// sentinel after them ends early, so poll again rather than count polls.
	CHECK( NextWheel( app, swipe ) && NextWheel( app, momentum ) );
	CHECK( swipe.type == NPlatform::EventType::mouseWheel && swipe.trackpad && swipe.x == 36 && swipe.y == 24 );
	CHECK( momentum.type == NPlatform::EventType::mouseWheel && momentum.trackpad );
	CHECK( !app.PollEvent( ignored ) );
	// Long after the momentum: a notch is a wheel's again.
	SDL_Delay( 300 );
	event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.y = 1.0f; CHECK( Push( event ) );
	NPlatform::PlatformEvent notch{};
	CHECK( app.PollEvent( notch ) && notch.type == NPlatform::EventType::mouseWheel && !notch.trackpad && notch.y == 120 );
	// A finger whose FINGER_UP went missing with the focus: the focus loss
	// forgets it, so a wheel afterwards is a wheel.
	SDL_Delay( 300 );
	event = {}; event.type = SDL_EVENT_FINGER_DOWN; event.tfinger.touchID = 78; event.tfinger.fingerID = 5; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_WINDOW_FOCUS_LOST; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.y = 1.0f; CHECK( Push( event ) );
	NPlatform::PlatformEvent afterLoss{};
	CHECK( NextWheel( app, afterLoss ) && !afterLoss.trackpad );
	return 0;
}
