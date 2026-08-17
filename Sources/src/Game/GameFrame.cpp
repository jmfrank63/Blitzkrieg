#include "GameFrame.h"

namespace NGame
{
void FramePacingPolicy::Apply( const NPlatform::PlatformEvent &event )
{
	switch ( event.type )
	{
		case NPlatform::EventType::focusGained: active_ = true; break;
		case NPlatform::EventType::focusLost: active_ = false; break;
		case NPlatform::EventType::windowMinimized: minimized_ = true; active_ = false; break;
		case NPlatform::EventType::windowRestored: minimized_ = false; active_ = true; break;
		case NPlatform::EventType::quit: quit_requested_ = true; break;
		default: break;
	}
}

void FramePacingPolicy::Reset()
{
	active_ = true;
	minimized_ = false;
	quit_requested_ = false;
}

FramePlan FramePacingPolicy::Next() const
{
	if ( quit_requested_ || !active_ || minimized_ ) return FramePlan{ false, 40 };
	return FramePlan{ true, 0 };
}

GameFrame::~GameFrame()
{
	Shutdown();
}

bool GameFrame::Initialize( const char *title, int width, int height )
{
	if ( !application_.Initialize( title, width, height ) ) return false;
	active_ = true;
	exit_ = false;
	fullscreen_ = false;
	release_requested_ = false;
	grabbed_ = false;
	events_.clear();
	return true;
}

void GameFrame::Shutdown()
{
	events_.clear();
	active_ = false;
	exit_ = false;
	fullscreen_ = false;
	release_requested_ = false;
	grabbed_ = false;
	application_.Shutdown();
}

bool GameFrame::SetCursorVisible( bool visible ) { return application_.SetCursorVisible( visible ); }
void GameFrame::Show() { application_.Show(); }
void GameFrame::Hide() { application_.Hide(); }
bool GameFrame::Resize( int width, int height ) { return application_.Resize( width, height ); }
bool GameFrame::SetFullscreen( bool enabled )
{
	if ( !application_.SetFullscreen( enabled ) ) return false;
	fullscreen_ = enabled;
	return true;
}
void GameFrame::CaptureMouse()
{
	grabbed_ = true;
	application_.SetMouseGrab( true );
	// The engine draws its own cursor, so the system pointer stays down while
	// the game owns the mouse - otherwise both are on screen at once.
	application_.SetCursorVisible( false );
}
void GameFrame::ReleaseMouse()
{
	grabbed_ = false;
	application_.SetMouseGrab( false );
	// Hand the arrow back, or the pointer the player just walked out of the
	// window is invisible everywhere it lands.
	application_.SetCursorVisible( true );
}

void GameFrame::ReconcileMouseCapture()
{
	NMouseCapture::SInputs inputs;
	inputs.bWindowFocused = application_.HasInputFocus();
	inputs.bPointerOverWindow = application_.HasMouseFocus();
	inputs.bReleaseRequested = release_requested_;
	// Our own intent, not SDL_GetWindowMouseGrab: SDL reports a grab as off the
	// moment the window loses focus, so asking it would make the release below
	// look already done and leave the system cursor hidden while tabbed out.
	inputs.bGrabbed = grabbed_;

	const bool want = NMouseCapture::WantGrab( inputs );
	if ( want != grabbed_ )
	{
		if ( want )
			CaptureMouse();
		else
			ReleaseMouse();
	}
	HoldPointerInsideWindow();
}

// SDL's grab alone was not keeping the pointer in: it hands AppKit a
// mouseConfinementRect and the pointer still reached the desktop at the bottom
// corners. Checking it against the window ourselves every pump closes that
// path, and does so identically on every platform.
void GameFrame::HoldPointerInsideWindow()
{
	if ( !grabbed_ ) return;
	float fGlobalX = 0.0f, fGlobalY = 0.0f;
	int nWindowX = 0, nWindowY = 0;
	if ( !application_.GetGlobalMousePosition( &fGlobalX, &fGlobalY ) ) return;
	if ( !application_.GetWindowPosition( &nWindowX, &nWindowY ) ) return;
	const NPlatform::WindowSize size = application_.LogicalSize();
	float fInsideX = 0.0f, fInsideY = 0.0f;
	if ( NMouseCapture::ClampIntoWindow( fGlobalX - float( nWindowX ), fGlobalY - float( nWindowY ),
																	 size.width, size.height, &fInsideX, &fInsideY ) )
		application_.WarpMousePosition( fInsideX, fInsideY );
}

void GameFrame::PumpMessages()
{
	ReconcileMouseCapture();
	NPlatform::PlatformEvent event{};
	while ( application_.PollEvent( event ) )
	{
		const NSysKeys::Action action = NSysKeys::Process( event );
		if ( action == NSysKeys::Action::toggleFullscreen )
		{
			SetFullscreen( !fullscreen_ );
			continue;
		}
		if ( action == NSysKeys::Action::releaseMouse )
		{
			release_requested_ = true;
			ReleaseMouse();
			continue;
		}
		if ( action == NSysKeys::Action::consume ) continue;
		if ( event.type == NPlatform::EventType::quit ) exit_ = true;
		else if ( event.type == NPlatform::EventType::focusGained ) { active_ = true; release_requested_ = false; }
		else if ( event.type == NPlatform::EventType::focusLost ) active_ = false;
		events_.push_back( event );
	}
}

bool GameFrame::PollEvent( NPlatform::PlatformEvent &event )
{
	if ( events_.empty() ) return false;
	event = events_.front();
	events_.pop_front();
	return true;
}
}
