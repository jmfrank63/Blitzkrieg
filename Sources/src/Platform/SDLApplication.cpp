#include "SDLApplication.h"

#include <SDL3/SDL.h>

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
