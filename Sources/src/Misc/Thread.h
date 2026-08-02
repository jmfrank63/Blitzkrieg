#ifndef __THREAD_H__
#define __THREAD_H__
#pragma ONCE
#include "Win32Helper.h"
#include <thread>
class CThread
{
	std::thread hThread;
	NWin32Helper::CEvent hFinishReport;
	NWin32Helper::CEvent hStopCommand;

	NWin32Helper::CCriticalSection criticalSection;

	const int nDelay;
	bool bRun;
protected:
	virtual void Step() = 0;
public:
	explicit CThread( const int nDelay );
	~CThread();

	const int GetDelay() const { return nDelay; }

	void RunThread();
	void StopThread();

	static void TheThreadProc( CThread *pThread );

	void StartThread();
	bool CanWork();
	void FinishThread();
};
#endif // __THREAD_H__
