#ifndef BLITZKRIEG_GAME_FRAME_H
#define BLITZKRIEG_GAME_FRAME_H

#include <deque>
#include <cstdint>

#include "../Platform/SDLApplication.h"
#include "MouseCapture.h"
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
	bool SetAppIcon( const char *path ) { return application_.SetAppIcon( path ); }
	bool ShowSplash( const char *bmpPath, int width, int height ) { return application_.ShowSplash( bmpPath, width, height ); }
	void HideSplash() { application_.HideSplash(); }
	void Shutdown();
	void Show();
	void Hide();
	bool Resize( int width, int height );
	bool SetFullscreen( bool enabled );
	void CaptureMouse();
	void ReleaseMouse();
	// Brings the confinement in line with focus, the shift gesture and any
	// standing ctrl+escape release. Idempotent, and driven from the message
	// pump rather than from focus events alone: a window that comes up already
	// focused never produces a focus-gained event to hang the first grab on.
	void ReconcileMouseCapture();
	void HoldPointerInsideWindow();
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
	// Set by ctrl+escape and cleared when the window next takes focus, so the
	// hotkey frees the pointer for as long as the player stays outside instead
	// of being undone by the next reconcile.
	bool release_requested_ = false;
	// Whether we asked for the pointer. Kept here rather than read back from
	// SDL, which reports a grab as off whenever the window is unfocused.
	bool grabbed_ = false;
};
}

#endif
