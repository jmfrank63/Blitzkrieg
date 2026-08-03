#include "Thread.h"
#include "../Platform/Clock.h"

using namespace NWin32Helper;

void CThread::TheThreadProc( CThread *pThread )
{
	pThread->StartThread();
	const int nDelay = pThread->GetDelay();
	while ( pThread->CanWork() )
	{
		NPlatform::SleepMilliseconds( static_cast<std::uint32_t>( nDelay > 0 ? nDelay : 0 ) );
		pThread->Step();
	}
	pThread->FinishThread();
}

void CThread::StartThread() { hFinishReport.Reset(); }
bool CThread::CanWork() { return !hStopCommand.IsSet(); }
void CThread::FinishThread() { hStopCommand.Set(); hFinishReport.Set(); }

CThread::CThread( const int _nDelay )
	: nDelay( _nDelay ), bRun( false )
{
}

void CThread::StopThread()
{
	std::thread threadToJoin;
	{
		CCriticalSectionLock criticalSectionLock( criticalSection );
		if ( !bRun ) return;
		hStopCommand.Set();
		threadToJoin = std::move( hThread );
		bRun = false;
	}
	if ( threadToJoin.joinable() ) threadToJoin.join();
	hStopCommand.Reset();
	hFinishReport.Reset();
}

CThread::~CThread() { StopThread(); }

void CThread::RunThread()
{
	CCriticalSectionLock criticalSectionLock( criticalSection );
	if ( bRun ) return;
	hStopCommand.Reset();
	hFinishReport.Reset();
	bRun = true;
	hThread = std::thread( &CThread::TheThreadProc, this );
}
