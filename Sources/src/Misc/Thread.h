#ifndef __THREAD_H__
#define __THREAD_H__
#pragma ONCE
#include "..\Misc\Win32Helper.h"
class CThread
{
	HANDLE hThread;
	HANDLE hFinishReport;
	HANDLE hStopCommand;

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

	static DWORD WINAPI TheThreadProc( LPVOID lpParameter );

	void StartThread();
	bool CanWork();
	void FinishThread();
};
#endif // __THREAD_H__
