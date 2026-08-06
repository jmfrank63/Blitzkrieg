#include "GameFrame.h"

namespace NGame
{
GameFrame::~GameFrame()
{
	Shutdown();
}

bool GameFrame::Initialize( const char *title, int width, int height )
{
	if ( !application_.Initialize( title, width, height ) ) return false;
	active_ = true;
	exit_ = false;
	events_.clear();
	return true;
}

void GameFrame::Shutdown()
{
	events_.clear();
	active_ = false;
	exit_ = false;
	application_.Shutdown();
}

void GameFrame::Show() { application_.Show(); }
void GameFrame::Hide() { application_.Hide(); }
bool GameFrame::Resize( int width, int height ) { return application_.Resize( width, height ); }
bool GameFrame::SetFullscreen( bool enabled ) { return application_.SetFullscreen( enabled ); }
void GameFrame::CaptureMouse() { application_.SetMouseGrab( true ); }
void GameFrame::ReleaseMouse() { application_.SetMouseGrab( false ); }

void GameFrame::PumpMessages()
{
	NPlatform::PlatformEvent event{};
	while ( application_.PollEvent( event ) )
	{
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
