
#if !defined(__AUTO_RUN_TYPES__)
#define __AUTO_RUN_TYPES__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DataStorage.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SARMainSection;
struct SARMenu;
struct SARButton
{
	friend struct SARMainSection;
	friend struct SARMenu;

private:
	bool Load( const std::string &rszName, class CIniFile& rIniFile, std::unordered_map<std::string, SARMenu> *pMenus, const SARMainSection &rMainSection );

public:	
	static DWORD GAME_INSTALLED;
	static DWORD GAME_NOT_INSTALLED;

	enum EAction
	{
		ACTION_OPEN				= 0,
		ACTION_SHELL_OPEN	= 1,
		ACTION_SHOW_MENU	= 2,
		ACTION_RETURN			= 3,	
		ACTION_CLOSE			= 4,
		ACTION_COUNT			= 5,
	};

	enum EActionBehaviour
	{
		AB_LOCK						= 0,
		AB_KEEP						= 1,
		AB_CLOSE					= 2,
		AB_COUNT					= 3,
	};
	
	static const char* ACTION_NAMES[ACTION_COUNT];
	static const char* ACTION_BEHAVIOUR_NAMES[AB_COUNT];

	DWORD dwPresentState;						//� ����� ������ ������ ������������ �� ������
	DWORD dwEnableState;						//� ���� ������ ������ ����� ���������� �� ������
	bool bSelectable;								//������ �����
	std::string szLabelFileName;		//���������
	std::string szTooltipFileName;	//������
	std::string szFocusedSoundFileName;	//���� ��� ���������
	std::string szActionSoundFileName;	//���� ��� ����������
	std::vector<DWORD> dwColors;		//����� �� ���������� ( ���� ������� ���� ������� �� ������ �� ������������ � ���� ��������� )
	DWORD dwShadowColor;						//���� ����
	DWORD dwAlign;									//������������ ��������� � ��������������
	CRect position;									//������������ �� ������
	int nFontSize;									//������ ������
	EAction	action;
	std::string szActionTarget;
	std::string szActionParameter;
	std::string szActionFolderName;
	EActionBehaviour actionBehaviour;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SARMenu
{
	friend struct SARButton;
	friend struct SARMainSection;

private:
	bool Load( const std::string &rszName, class CIniFile& rIniFile, std::unordered_map<std::string, SARMenu> *pMenus, const SARMainSection &rMainSection );

public:	
	std::vector<SARButton> buttons;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CARMenuSelector;
struct SARMainSection
{
	friend struct SARButton;
	friend class CARMenuSelector;
	friend struct SLogo;

	enum ERegistryType
	{
		RT_HKLM						= 0,
		RT_HKCU						= 1,
		RT_COUNT					= 2,
	};

	static const char* RT_NAMES[RT_COUNT];

private:
	static DWORD ALIGN_LEFT;
	static DWORD ALIGN_RIGHT;
	static DWORD ALIGN_HOR_CENTER;
	static DWORD ALIGN_TOP;
	static DWORD ALIGN_BOTTOM;
	static DWORD ALIGN_VER_CENTER;
	static DWORD ALIGN_CENTER;
	
	enum EStates
	{
		STATE_NORMAL		= 0,
		STATE_FOCUSED		= 1,
		STATE_SELECTED	= 2,
		STATE_DISABLED	= 3,
		STATE_COUNT			= 3,
	};
	
	void Clear()
	{
		logos.clear();
		menus.clear();
	}
	
	bool Load( const std::string &rszName, class CIniFile& rIniFile );

	struct SLogo
	{
		std::string szImageFileName;
		CPoint position;
	};

	struct SRegistryKey
	{
		ERegistryType type;
		std::string szKey;
	
		bool Load( const std::string &rszName, const std::string &rszPrefix, class CIniFile& rIniFile );
	};

public:
	std::string szWrongDiskTitleFileName;
	std::string szWrongDiskMessageFileName;
	std::string szRuningGameTitle;
	std::string szRuningInstallTitle;
	std::string szTitleFileName;
	std::string szSoundFileName;
	std::string szBackgroundImageFileName;
	std::vector<SLogo> logos;
	CPoint shadowPoint;
	std::vector<DWORD> dwColors;		//����� �� ���������� ( ���� ������� ���� ������� �� ������ �� ������������ � ���� ��������� )
	DWORD dwShadowColor;						//���� ����
	DWORD dwAlign;									//������������ ��������� � ��������������
	int nCodePage;									//��������� ������������ ��� ���� ���������
	bool bShowToolTips;							//���������� �� �������

	std::string szFontName;					//��� ������
	int nFontWeight;								//������� ������

	SRegistryKey rkInstallFolder;		//Install Folder
	SRegistryKey rkUninstallFolder;	//Uninstall Folder
	std::string szGameInstalledFileName;
	
	std::string szMenuName;
	std::unordered_map<std::string, SARMenu> menus;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CARMenuSelector
{
	static const char* CONFIGURATION_FILE_NAME;

	inline COLORREF GetRGBColorFromARGBB( DWORD dwColor ){ return RGB( 0xFF & ( ( dwColor & 0xFF0000 ) >> 16 ), 0xFF & ( ( dwColor & 0xFF00 ) >> 8 ), dwColor & 0xFF ); }
	inline DWORD GetAlphaFromARGBB( DWORD dwColor ) { return ( 0xFF & ( ( dwColor & 0xFF000000 ) >> 24 ) ); }

	enum EActionFolder
	{
		AF_INSTALL				= 0,
		AF_CURRENT				= 1,
		AF_UNINSTALL			= 2,
		AF_COUNT					= 3,
	};
	static const char* ACTION_FOLDER_NAMES[AF_COUNT];

public:
	static const char* DATA_FILE_NAME;

	SARMainSection mainSection;
	CDataStorage dataStorage;

	std::string szActionFolders[AF_COUNT];
	
	bool bGameInstalled;
	std::list<std::string> menuStack;
	int nCurrentFocusedButton;

	bool Load( class CWnd *pWnd );
	void PlayStartSound();
	void DrawBackgroundAndLogos( CDC *pDC );
	void DrawMenu( CDC *pDC, const CPoint &rMousePoint, int nMouseFlags );
	int HitTest( const CPoint &rMousePoint );
	bool Action( class CAutoRunDialog *pWnd, const CPoint &rMousePoint );
	bool ActionShortcut( int nShortcut );
	bool ReturnMenu();
	void FillToolTips( CToolTipCtrl *pToolTipCtrl, const CPoint &rMousePoint );
	CString GetTitle();
	CString GetRunningGameTitle();
	CString GetRunningInstallTitle();
	bool ExecuteTarget( class CAutoRunDialog *pWnd, const std::string &szTarget, const std::string &szParameters, const std::string &szDirectory, bool bWait, bool bCurrent );
	bool ShellExecuteTarget( class CAutoRunDialog *pWnd, const std::string &szTarget, const std::string &szParameters, const std::string &szDirectory, bool bWait, bool bCurrent );
	std::string ParseFolder( const std::string &rszFolder, bool *pbCurrent );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // !defined(__AUTO_RUN_TYPES__)

