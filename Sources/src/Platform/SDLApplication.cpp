#include "SDLApplication.h"
#include "Clock.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <cstdint>

#include "Debug.h"

namespace
{
bool fail_initialization_for_tests = false;

static_assert( sizeof( void * ) <= sizeof( BkPlatformWindowHandle ), "window identity cannot represent a native pointer" );

BkPlatformWindowHandle EncodeWindowIdentity( void *window )
{
	return static_cast<BkPlatformWindowHandle>( reinterpret_cast<std::uintptr_t>( window ) );
}
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
	if ( !SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD ) ) { SetError( "SDL_Init" ); return false; }
	// No SDL_WINDOW_HIGH_PIXEL_DENSITY: on a 2x Retina display it makes the
	// drawable twice the window size in points, but the engine sizes its
	// render target from the video mode and receives mouse positions in
	// points. The mismatch left most of the swapchain undrawn and put the
	// in-game cursor at half the pointer's position. A 1:1 drawable keeps the
	// legacy coordinate space consistent.
	window_ = SDL_CreateWindow( title ? title : "Blitzkrieg", width, height,
		SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE );
	if ( !window_ )
	{
		SetError( "SDL_CreateWindow" );
		SDL_Quit();
		return false;
	}
	// SDL3 keeps text input off until it is asked for, and only then does it
	// emit SDL_EVENT_TEXT_INPUT. Without this no keystroke ever carried a
	// character, so every edit box in the game - the save game name above all -
	// took arrows and Enter but could not be typed into. Windows delivered
	// WM_CHAR unconditionally; CInputAPI already discards text in
	// INPUT_TEXT_MODE_NOTEXT, so leaving it on matches that.
	SDL_StartTextInput( static_cast<SDL_Window *>( window_ ) );
	initialized_ = true;
	visible_ = false;
	last_error_.clear();
	return true;
}

void SDLApplication::Shutdown()
{
	if ( !OnMainThread() ) { SetError( "Shutdown called off main thread" ); return; }
	for ( std::vector<GamepadRecord>::iterator it = gamepads_.begin(); it != gamepads_.end(); ++it )
		if ( it->handle ) SDL_CloseGamepad( static_cast<SDL_Gamepad *>( it->handle ) );
	gamepads_.clear();
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

bool SDLApplication::Resize(int width, int height)
{
	if ( !window_ || !OnMainThread() || width <= 0 || height <= 0 ) return false;
	return SDL_SetWindowSize( static_cast<SDL_Window *>( window_ ), width, height );
}

bool SDLApplication::SetFullscreen(bool enabled)
{
	if ( !window_ || !OnMainThread() ) return false;
	return SDL_SetWindowFullscreen( static_cast<SDL_Window *>( window_ ), enabled );
}

bool SDLApplication::SetMouseGrab(bool enabled)
{
	if ( !window_ || !OnMainThread() ) return false;
	return SDL_SetWindowMouseGrab( static_cast<SDL_Window *>( window_ ), enabled );
}

bool SDLApplication::SetRelativeMouseMode(bool enabled)
{
	if ( !window_ || !OnMainThread() ) return false;
	return SDL_SetWindowRelativeMouseMode( static_cast<SDL_Window *>( window_ ), enabled );
}

bool SDLApplication::SetCursorVisible(bool visible)
{
	if ( !OnMainThread() ) return false;
	return visible ? SDL_ShowCursor() : SDL_HideCursor();
}

bool SDLApplication::SetClipboardText(const char *text)
{
	if ( !OnMainThread() ) return false;
	return SDL_SetClipboardText( text ? text : "" );
}

std::string SDLApplication::GetClipboardText() const
{
	if ( !OnMainThread() ) return std::string();
	char *text = SDL_GetClipboardText();
	std::string result = text ? text : "";
	if ( text ) SDL_free( text );
	return result;
}

bool SDLApplication::GetControllerName(int deviceId, char *destination, std::size_t capacity) const
{
	if ( destination == nullptr || capacity == 0 ) return false;
	destination[0] = 0;
	for ( std::vector<GamepadRecord>::const_iterator it = gamepads_.begin(); it != gamepads_.end(); ++it )
	{
		if ( it->device_id != deviceId ) continue;
		const char *name = it->name.c_str();
		if ( it->handle )
		{
			if ( const char *sdlName = SDL_GetGamepadName( static_cast<SDL_Gamepad *>( it->handle ) ) ) name = sdlName;
		}
		const std::size_t length = std::strlen( name );
		const std::size_t copied = length < capacity - 1 ? length : capacity - 1;
		std::memcpy( destination, name, copied );
		destination[copied] = 0;
		return copied == length;
	}
	return false;
}

bool SDLApplication::AddVirtualControllerForTests(int deviceId, const char *name)
{
	if ( deviceId < 0 || name == nullptr || *name == 0 ) return false;
	for ( std::vector<GamepadRecord>::const_iterator it = gamepads_.begin(); it != gamepads_.end(); ++it )
		if ( it->device_id == deviceId ) return false;
	GamepadRecord record;
	record.device_id = deviceId;
	record.name = name;
	gamepads_.push_back( record );
	return true;
}

bool SDLApplication::RemoveVirtualControllerForTests(int deviceId)
{
	for ( std::vector<GamepadRecord>::iterator it = gamepads_.begin(); it != gamepads_.end(); ++it )
	{
		if ( it->device_id != deviceId || it->handle != nullptr ) continue;
		gamepads_.erase( it );
		return true;
	}
	return false;
}

bool SDLApplication::IsMinimized() const
{
	return window_ && (SDL_GetWindowFlags( static_cast<SDL_Window *>( window_ ) ) & SDL_WINDOW_MINIMIZED) != 0;
}

bool SDLApplication::IsVisible() const { return visible_; }

WindowBorrow SDLApplication::BorrowWindow() const { return { EncodeWindowIdentity( window_ ) }; }

void *SDLApplication::GetWindowsNativeHandle() const
{
#if defined(_WIN32)
	if ( !window_ || !OnMainThread() ) return nullptr;
	SDL_PropertiesID properties = SDL_GetWindowProperties( static_cast<SDL_Window *>( window_ ) );
	return properties ? SDL_GetPointerProperty( properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr ) : nullptr;
#else
	return nullptr;
#endif
}

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
			case SDL_EVENT_KEY_DOWN: event.type = EventType::keyDown; event.timestamp = raw.key.timestamp; event.windowId = raw.key.windowID; event.key = static_cast<int>( raw.key.key ); event.scancode = static_cast<int>( raw.key.scancode ); event.modifiers = static_cast<int>( raw.key.mod ); event.repeat = raw.key.repeat; break;
			case SDL_EVENT_KEY_UP: event.type = EventType::keyUp; event.timestamp = raw.key.timestamp; event.windowId = raw.key.windowID; event.key = static_cast<int>( raw.key.key ); event.scancode = static_cast<int>( raw.key.scancode ); event.modifiers = static_cast<int>( raw.key.mod ); break;
		case SDL_EVENT_TEXT_INPUT:
				event.type = EventType::textInput; event.timestamp = raw.text.timestamp; event.windowId = raw.text.windowID;
				if ( raw.text.text )
				{
					const std::size_t length = std::strlen( raw.text.text );
					if ( length >= sizeof( event.text ) )
					{
						if ( !event_overflow_episode_ ) NPlatform::DebugWrite( "SDL normalized text event truncated (overflow episode)\n" );
						event_overflow_episode_ = true;
					}
					else
						event_overflow_episode_ = false;
					std::strncpy( event.text, raw.text.text, sizeof( event.text ) - 1 );
				}
				else
					event_overflow_episode_ = false;
				break;
			case SDL_EVENT_MOUSE_MOTION: event.type = EventType::mouseMotion; event.timestamp = raw.motion.timestamp; event.windowId = raw.motion.windowID; event.x = static_cast<int>( raw.motion.x ); event.y = static_cast<int>( raw.motion.y ); event.data1 = static_cast<int>( raw.motion.xrel ); event.data2 = static_cast<int>( raw.motion.yrel ); break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN: event.type = EventType::mouseButtonDown; event.timestamp = raw.button.timestamp; event.windowId = raw.button.windowID; event.button = raw.button.button; event.x = static_cast<int>( raw.button.x ); event.y = static_cast<int>( raw.button.y ); break;
			case SDL_EVENT_MOUSE_BUTTON_UP: event.type = EventType::mouseButtonUp; event.timestamp = raw.button.timestamp; event.windowId = raw.button.windowID; event.button = raw.button.button; event.x = static_cast<int>( raw.button.x ); event.y = static_cast<int>( raw.button.y ); break;
			case SDL_EVENT_MOUSE_WHEEL: event.type = EventType::mouseWheel; event.timestamp = raw.wheel.timestamp; event.windowId = raw.wheel.windowID; event.x = static_cast<int>( raw.wheel.x ); event.y = static_cast<int>( raw.wheel.y ); event.data1 = static_cast<int>( raw.wheel.mouse_x ); event.data2 = static_cast<int>( raw.wheel.mouse_y ); break;
			case SDL_EVENT_GAMEPAD_ADDED:
				event.type = EventType::controllerAdded; event.timestamp = raw.gdevice.timestamp; event.deviceId = static_cast<int>( raw.gdevice.which );
				if ( SDL_Gamepad *gamepad = SDL_OpenGamepad( raw.gdevice.which ) )
				{
					GamepadRecord record;
					record.device_id = event.deviceId;
					record.handle = gamepad;
					if ( const char *name = SDL_GetGamepadName( gamepad ) ) record.name = name;
					gamepads_.push_back( record );
				}
				break;
			case SDL_EVENT_GAMEPAD_REMOVED:
				event.type = EventType::controllerRemoved; event.timestamp = raw.gdevice.timestamp; event.deviceId = static_cast<int>( raw.gdevice.which );
				for ( std::vector<GamepadRecord>::iterator it = gamepads_.begin(); it != gamepads_.end(); ++it ) if ( it->device_id == event.deviceId ) { if ( it->handle ) SDL_CloseGamepad( static_cast<SDL_Gamepad *>( it->handle ) ); gamepads_.erase( it ); break; }
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN: event.type = EventType::controllerButtonDown; event.timestamp = raw.gbutton.timestamp; event.deviceId = static_cast<int>( raw.gbutton.which ); event.control = raw.gbutton.button; event.value = 0x80; break;
			case SDL_EVENT_GAMEPAD_BUTTON_UP: event.type = EventType::controllerButtonUp; event.timestamp = raw.gbutton.timestamp; event.deviceId = static_cast<int>( raw.gbutton.which ); event.control = raw.gbutton.button; event.value = 0; break;
			case SDL_EVENT_GAMEPAD_AXIS_MOTION: event.type = EventType::controllerAxis; event.timestamp = raw.gaxis.timestamp; event.deviceId = static_cast<int>( raw.gaxis.which ); event.control = raw.gaxis.axis; event.value = raw.gaxis.value; break;
			default: continue;
		}
		// SDL3 stamps events in nanoseconds. Everything downstream measures time
		// in milliseconds -- CControlKey::ChangeState compares against
		// TIME_DIFF_DBL_CLK, GenerateRepeats against a delay and period, and
		// CInputAPI seeds its clock from NPlatform::MonotonicMilliseconds. Handing
		// those nanoseconds straight through put two clicks a third of a second
		// apart 300 million units apart, so no double click ever registered, and
		// truncating to 32 bits wrapped the value every 4.3 seconds on top.
		// ...and on the same epoch. SDL counts from its own initialisation while
		// MonotonicMilliseconds counts from the steady clock's, about a month
		// apart on a machine that has been up a while. CInputSlider integrates a
		// held key from its activation stamp to CInputAPI's clock, so mixing the
		// two made one tap of an arrow key look like days of holding it and the
		// camera jumped straight to the edge of the map.
		event.timestamp = static_cast<std::uint32_t>( SDL_NS_TO_MS( event.timestamp ) + TimestampBase() );
		return true;
	}
	return false;
}

// Offset from SDL's tick epoch to the engine's monotonic clock, sampled once.
// Both are monotonic and tick in milliseconds, so a single difference aligns
// them for the life of the process.
std::uint64_t SDLApplication::TimestampBase()
{
	static const std::uint64_t base = NPlatform::MonotonicMilliseconds64() - SDL_GetTicks();
	return base;
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
