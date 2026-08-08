#ifndef BLITZKRIEG_GAME_FRAME_H
#define BLITZKRIEG_GAME_FRAME_H

#include <deque>
#include <cstdint>

#include "../Platform/SDLApplication.h"
#include "SysKeys.h"

namespace NGame
{
struct FramePlan
{
	bool render = false;
	std::uint32_t sleepMilliseconds = 0;
};

class FramePacingPolicy
{
public:
	void Apply( const NPlatform::PlatformEvent &event );
	void Reset();
	FramePlan Next() const;
	bool IsActive() const { return active_; }
	bool IsMinimized() const { return minimized_; }
	bool IsQuitRequested() const { return quit_requested_; }

private:
	bool active_ = true;
	bool minimized_ = false;
	bool quit_requested_ = false;
};

class GameFrame
{
public:
	GameFrame() = default;
	~GameFrame();

	GameFrame( const GameFrame & ) = delete;
	GameFrame &operator=( const GameFrame & ) = delete;

	bool Initialize( const char *title, int width, int height );
	void Shutdown();
	void Show();
	void Hide();
	bool Resize( int width, int height );
	bool SetFullscreen( bool enabled );
	void CaptureMouse();
	void ReleaseMouse();
	bool SetCursorVisible( bool visible );
	void PumpMessages();
	bool PollEvent( NPlatform::PlatformEvent &event );

	bool IsActive() const { return active_; }
	bool IsFullscreen() const { return fullscreen_; }
	bool IsExit() const { return exit_; }
	void ResetExit() { exit_ = false; }
	NPlatform::WindowBorrow BorrowWindow() const { return application_.BorrowWindow(); }
	NPlatform::WindowSize LogicalSize() const { return application_.LogicalSize(); }
	NPlatform::WindowSize PixelSize() const { return application_.PixelSize(); }
	const std::string &LastError() const { return application_.LastError(); }

private:
	NPlatform::SDLApplication application_;
	std::deque<NPlatform::PlatformEvent> events_;
	bool active_ = false;
	bool exit_ = false;
	bool fullscreen_ = false;
};
}

#endif
