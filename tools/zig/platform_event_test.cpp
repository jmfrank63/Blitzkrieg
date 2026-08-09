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

int main()
{
	NPlatform::SDLApplication app;
	CHECK( app.Initialize( "Blitzkrieg event test", 320, 200 ) );
	NPlatform::PlatformEvent ignored;
	while ( app.PollEvent( ignored ) ) {}

	SDL_Event event{};
	event.type = SDL_EVENT_WINDOW_RESIZED; event.window.timestamp = 10 * 1000000; event.window.data1 = 640; event.window.data2 = 480; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_KEY_DOWN; event.key.timestamp = 20 * 1000000; event.key.key = SDLK_RETURN; event.key.mod = SDL_KMOD_ALT; event.key.repeat = true; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_TEXT_INPUT; event.text.timestamp = 30 * 1000000; event.text.text = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-long"; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_MOUSE_MOTION; event.motion.timestamp = 40 * 1000000; event.motion.x = 12.0f; event.motion.y = 34.0f; event.motion.xrel = 2.0f; event.motion.yrel = -3.0f; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.timestamp = 50 * 1000000; event.wheel.x = 1.0f; event.wheel.y = -2.0f; event.wheel.mouse_x = 12.0f; event.wheel.mouse_y = 34.0f; CHECK( Push( event ) );
	event = {}; event.type = SDL_EVENT_QUIT; event.quit.timestamp = 60 * 1000000; CHECK( Push( event ) );
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
	CHECK( translated[4].type == NPlatform::EventType::mouseWheel && translated[4].x == 1 && translated[4].y == -2 );
	CHECK( translated[5].type == NPlatform::EventType::quit );
	// 10ms and 60ms in, so 50ms apart whatever the offset is.
	CHECK( translated[5].timestamp - translated[0].timestamp == 50 );
	return 0;
}
