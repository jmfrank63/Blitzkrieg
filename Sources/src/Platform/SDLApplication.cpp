#include "SDLApplication.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>

namespace
{
bool fail_initialization_for_tests = false;
}

namespace NPlatform
{
SDLApplication::SDLApplication() : main_thread_( std::this_thread::get_id() ) {}

SDLApplication::~SDLApplication()
{
	Shutdown();
}

bool SDLApplication::Initialize(const char *title, int width, int height)
{
	if ( !OnMainThread() ) { SetError( "Initialize called off main thread" ); return false; }
	if ( initialized_ ) return true;
	if ( fail_initialization_for_tests ) { SetError( "SDL initialization failure injected" ); return false; }
	if ( !SDL_SetAppMetadata( "Blitzkrieg", "2.0.0", "org.blitzkrieg.game" ) ) { SetError( "SDL_SetAppMetadata" ); return false; }
	if ( !SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS ) ) { SetError( "SDL_Init" ); return false; }
	window_ = SDL_CreateWindow( title ? title : "Blitzkrieg", width, height,
		SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY );
	if ( !window_ )
	{
		SetError( "SDL_CreateWindow" );
		SDL_Quit();
		return false;
	}
	initialized_ = true;
	visible_ = false;
	last_error_.clear();
	return true;
}

void SDLApplication::Shutdown()
{
	if ( !OnMainThread() ) { SetError( "Shutdown called off main thread" ); return; }
	if ( window_ ) SDL_DestroyWindow( static_cast<SDL_Window *>( window_ ) );
	window_ = nullptr;
	visible_ = false;
	if ( initialized_ ) SDL_Quit();
	initialized_ = false;
}

void SDLApplication::Show()
{
	if ( window_ && OnMainThread() ) { SDL_ShowWindow( static_cast<SDL_Window *>( window_ ) ); visible_ = true; }
}

void SDLApplication::Hide()
{
	if ( window_ && OnMainThread() ) { SDL_HideWindow( static_cast<SDL_Window *>( window_ ) ); visible_ = false; }
}

bool SDLApplication::IsVisible() const { return visible_; }

WindowBorrow SDLApplication::BorrowWindow() const { return { window_ }; }

WindowSize SDLApplication::LogicalSize() const
{
	WindowSize result;
	if ( window_ ) SDL_GetWindowSize( static_cast<SDL_Window *>( window_ ), &result.width, &result.height );
	return result;
}

WindowSize SDLApplication::PixelSize() const
{
	WindowSize result;
	if ( window_ ) SDL_GetWindowSizeInPixels( static_cast<SDL_Window *>( window_ ), &result.width, &result.height );
	return result;
}

bool SDLApplication::PollEvent(PlatformEvent &event)
{
	if ( !OnMainThread() ) { SetError( "PollEvent called off main thread" ); return false; }
	SDL_Event raw{};
	while ( SDL_PollEvent( &raw ) )
	{
		event = {};
		switch ( raw.type )
		{
			case SDL_EVENT_QUIT: event.type = EventType::quit; event.timestamp = raw.quit.timestamp; break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED: event.type = EventType::focusGained; event.timestamp = raw.window.timestamp; event.windowId = raw.window.windowID; break;
			case SDL_EVENT_WINDOW_FOCUS_LOST: event.type = EventType::focusLost; event.timestamp = raw.window.timestamp; event.windowId = raw.window.windowID; break;
			case SDL_EVENT_WINDOW_MOVED: event.type = EventType::windowMoved; event.timestamp = raw.window.timestamp; event.windowId = raw.window.windowID; event.x = raw.window.data1; event.y = raw.window.data2; break;
			case SDL_EVENT_WINDOW_RESIZED: event.type = EventType::windowResized; event.timestamp = raw.window.timestamp; event.windowId = raw.window.windowID; event.x = raw.window.data1; event.y = raw.window.data2; break;
			case SDL_EVENT_WINDOW_MINIMIZED: event.type = EventType::windowMinimized; event.timestamp = raw.window.timestamp; event.windowId = raw.window.windowID; break;
			case SDL_EVENT_WINDOW_RESTORED: event.type = EventType::windowRestored; event.timestamp = raw.window.timestamp; event.windowId = raw.window.windowID; break;
			case SDL_EVENT_KEY_DOWN: event.type = EventType::keyDown; event.timestamp = raw.key.timestamp; event.windowId = raw.key.windowID; event.key = static_cast<int>( raw.key.key ); event.modifiers = static_cast<int>( raw.key.mod ); event.repeat = raw.key.repeat; break;
			case SDL_EVENT_KEY_UP: event.type = EventType::keyUp; event.timestamp = raw.key.timestamp; event.windowId = raw.key.windowID; event.key = static_cast<int>( raw.key.key ); event.modifiers = static_cast<int>( raw.key.mod ); break;
			case SDL_EVENT_TEXT_INPUT:
				event.type = EventType::textInput; event.timestamp = raw.text.timestamp; event.windowId = raw.text.windowID;
				if ( raw.text.text ) std::strncpy( event.text, raw.text.text, sizeof( event.text ) - 1 );
				break;
			case SDL_EVENT_MOUSE_MOTION: event.type = EventType::mouseMotion; event.timestamp = raw.motion.timestamp; event.windowId = raw.motion.windowID; event.x = static_cast<int>( raw.motion.x ); event.y = static_cast<int>( raw.motion.y ); event.data1 = static_cast<int>( raw.motion.xrel ); event.data2 = static_cast<int>( raw.motion.yrel ); break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN: event.type = EventType::mouseButtonDown; event.timestamp = raw.button.timestamp; event.windowId = raw.button.windowID; event.button = raw.button.button; event.x = static_cast<int>( raw.button.x ); event.y = static_cast<int>( raw.button.y ); break;
			case SDL_EVENT_MOUSE_BUTTON_UP: event.type = EventType::mouseButtonUp; event.timestamp = raw.button.timestamp; event.windowId = raw.button.windowID; event.button = raw.button.button; event.x = static_cast<int>( raw.button.x ); event.y = static_cast<int>( raw.button.y ); break;
			case SDL_EVENT_MOUSE_WHEEL: event.type = EventType::mouseWheel; event.timestamp = raw.wheel.timestamp; event.windowId = raw.wheel.windowID; event.x = static_cast<int>( raw.wheel.x ); event.y = static_cast<int>( raw.wheel.y ); event.data1 = static_cast<int>( raw.wheel.mouse_x ); event.data2 = static_cast<int>( raw.wheel.mouse_y ); break;
			default: continue;
		}
		return true;
	}
	return false;
}

const std::string &SDLApplication::LastError() const { return last_error_; }

void SDLApplication::SetInitializationFailureForTests(bool enabled) { fail_initialization_for_tests = enabled; }

bool SDLApplication::OnMainThread() const { return std::this_thread::get_id() == main_thread_; }

void SDLApplication::SetError(const char *operation)
{
	last_error_ = operation ? operation : "SDL operation failed";
	const char *detail = SDL_GetError();
	if ( detail && *detail ) { last_error_ += ": "; last_error_ += detail; }
}
}
