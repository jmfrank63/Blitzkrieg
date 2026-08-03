#ifndef BLITZKRIEG_PLATFORM_SDL_APPLICATION_H
#define BLITZKRIEG_PLATFORM_SDL_APPLICATION_H

#include <string>
#include <thread>

#include "Event.h"

namespace NPlatform
{
struct WindowBorrow
{
	void *value = nullptr;
};

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
	void Shutdown();
	void Show();
	void Hide();
	bool IsVisible() const;
	WindowBorrow BorrowWindow() const;
	WindowSize LogicalSize() const;
	WindowSize PixelSize() const;
	bool PollEvent(Event &event);
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
};
}

#endif
