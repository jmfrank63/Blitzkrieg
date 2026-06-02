#ifndef __WINFRAME_H__
#define __WINFRAME_H__
namespace NWinFrame
{
	struct SWindowsMsg
	{
		enum EMsg
		{
			MOUSE_MOVE,
			RB_DOWN,
			LB_DOWN,
			RB_UP,
			LB_UP,
			KEY_DOWN,
			KEY_UP,
			TIME,
		};
		DWORD time;
		EMsg msg;
		union
		{
			struct { int x, y; };         // mouse
			struct { int nKey, nRep; };  // key
		};
		DWORD dwFlags;
	};
	bool GetMessage( SWindowsMsg *pRes );
	HWND GetHWnd();
	HINSTANCE GetHInstance();
	bool InitApplication( HINSTANCE hInstance, const char *pszAppName, const char *pszWndName, int nWidth, int nHeight );
	void ShowAppWindow( bool bShow );
	void ShowSplashScreen( HINSTANCE hInstance, bool bShow );
	void PumpMessages();
	void SetActive( bool bActive );
	void Exit( int nExitCode );
	bool IsActive();
	bool IsExit();
	void ResetExit();
};
#endif // __WINFRAME_H__
