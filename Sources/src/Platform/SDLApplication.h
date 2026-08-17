#ifndef BLITZKRIEG_PLATFORM_SDL_APPLICATION_H
#define BLITZKRIEG_PLATFORM_SDL_APPLICATION_H

#include <cstdint>
#include <string>
#include <cstddef>
#include <thread>
#include <vector>

#include "Event.h"
#include "../PlatformABI/platform_c.h"

namespace NPlatform
{
struct WindowBorrow
{
	// The storage crossing the facade is always eight bytes. The conversion
	// operators are a transitional compatibility bridge for existing private
	// renderer callers; SDL types do not appear in this header or ABI.
	struct Value
	{
		BkPlatformWindowHandle bits = 0;

		Value() = default;
		Value( BkPlatformWindowHandle handle ) : bits( handle ) {}

		template <typename T>
		operator T *() const
		{
			return reinterpret_cast<T *>( static_cast<std::uintptr_t>( bits ) );
		}

		bool operator==( std::nullptr_t ) const { return bits == 0; }
		bool operator!=( std::nullptr_t ) const { return bits != 0; }
	};

	Value value;
};

static_assert( sizeof( BkPlatformWindowHandle ) == 8, "window ABI handles must be 64-bit" );
static_assert( sizeof( WindowBorrow::Value ) == sizeof( BkPlatformWindowHandle ), "borrowed window identity must stay fixed-width" );
static_assert( sizeof( WindowBorrow ) == sizeof( BkPlatformWindowHandle ), "borrowed window bridge must stay one word" );

struct WindowSize
{
	int width = 0;
	int height = 0;
};

class SDLApplication
{
public:
	SDLApplication();
	~SDLApplication();

	SDLApplication(const SDLApplication &) = delete;
	SDLApplication &operator=(const SDLApplication &) = delete;

	bool Initialize(const char *title, int width, int height);
	// Gives the bare executable an application icon (Dock / cmd-tab on
	// macOS, where a binary without an .app bundle shows the generic
	// executable icon). No-op on other platforms. Accepts a PNG path.
	bool SetAppIcon(const char *path);
	void Shutdown();
	void Show();
	void Hide();
	bool Resize(int width, int height);
	bool SetFullscreen(bool enabled);
	bool SetMouseGrab(bool enabled);
	bool SetRelativeMouseMode(bool enabled);
	bool SetCursorVisible(bool visible);
	bool SetClipboardText(const char *text);
	// Aligns SDL's tick epoch with NPlatform::MonotonicMilliseconds.
	static std::uint64_t TimestampBase();
	std::string GetClipboardText() const;
	bool GetControllerName(int deviceId, char *destination, std::size_t capacity) const;
	bool AddVirtualControllerForTests(int deviceId, const char *name);
	bool RemoveVirtualControllerForTests(int deviceId);
	bool IsMinimized() const;
	bool IsVisible() const;
	bool HasInputFocus() const;
	// The pointer is over our window. Distinct from input focus: the two part
	// company whenever the pointer wanders off a still-focused window.
	bool HasMouseFocus() const;
	// Pointer position in desktop coordinates: unlike a window-relative one it
	// stays meaningful after the pointer has left the window, which is exactly
	// the case the confinement has to detect and undo.
	bool GetGlobalMousePosition( float *x, float *y ) const;
	bool GetWindowPosition( int *x, int *y ) const;
	bool WarpMousePosition( float x, float y );
	WindowBorrow BorrowWindow() const;
	void *GetWindowsNativeHandle() const;
	WindowSize LogicalSize() const;
	WindowSize PixelSize() const;
	bool PollEvent(PlatformEvent &event);
	const std::string &LastError() const;

	static void SetInitializationFailureForTests(bool enabled);

private:
	bool OnMainThread() const;
	void SetError(const char *operation);

	void *window_ = nullptr;
	std::thread::id main_thread_;
	std::string last_error_;
	bool initialized_ = false;
	bool visible_ = false;
	bool event_overflow_episode_ = false;
	struct GamepadRecord
	{
		int device_id = 0;
		void *handle = nullptr;
		std::string name;
	};
	std::vector<GamepadRecord> gamepads_;
};
}

#endif
