#if !defined(__EDITOR_WINDOW_SINGLETON__)
#define __EDITOR_WINDOW_SINGLETON__

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CEditorWindowSingletonBase
{
protected:
  bool SendCommand( HWND hWndDst, HWND hWndSrc, DWORD dwCommand, DWORD dwDataLength, const void* pData ) const;

  static const char MAP_FILE_NAME[];
  static const DWORD MAP_FILE_MAX_SIZE;

public:
	enum COMMANDS
  {
    OPEN_FILE = 0x10,
  };
};

class CEditorWindowSingletonApp : public CEditorWindowSingletonBase
{
private:
  HANDLE hMapFileHandle;
  void *pMapFileData;

public:
  CEditorWindowSingletonApp();
  ~CEditorWindowSingletonApp();
  
	bool CreateMapFile( HWND hWndApp );
  void RemoveMapFile();
};

class CEditorWindowSingletonChecker : public CEditorWindowSingletonBase
{
private:
  HWND GetAppHwnd() const;

public:
  bool BringAppOnTop() const;
	bool OpenFileOnApp( const std::string &rszFilePath ) const;
};
#endif // !defined(__EDITOR_WINDOW_SINGLETON__)
