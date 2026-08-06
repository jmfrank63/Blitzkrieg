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
	events_.clear();
	return true;
}

void GameFrame::Shutdown()
{
	events_.clear();
	active_ = false;
	exit_ = false;
	fullscreen_ = false;
	application_.Shutdown();
}

void GameFrame::Show() { application_.Show(); }
void GameFrame::Hide() { application_.Hide(); }
bool GameFrame::Resize( int width, int height ) { return application_.Resize( width, height ); }
bool GameFrame::SetFullscreen( bool enabled )
{
	if ( !application_.SetFullscreen( enabled ) ) return false;
	fullscreen_ = enabled;
	return true;
}
void GameFrame::CaptureMouse() { application_.SetMouseGrab( true ); }
void GameFrame::ReleaseMouse() { application_.SetMouseGrab( false ); }

void GameFrame::PumpMessages()
{
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
			ReleaseMouse();
			continue;
		}
		if ( action == NSysKeys::Action::consume ) continue;
		if ( event.type == NPlatform::EventType::quit ) exit_ = true;
		else if ( event.type == NPlatform::EventType::focusGained ) active_ = true;
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
