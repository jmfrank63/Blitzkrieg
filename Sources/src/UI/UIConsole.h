#ifndef __UI_CONSOLE_H__
#define __UI_CONSOLE_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "UIBasic.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIConsole : public CSimpleWindow
{
	DECLARE_SERIALIZE;
public:
	struct SColorString
	{
		DECLARE_SERIALIZE;
	public:
		std::wstring szString;
		DWORD dwColor;
		
		SColorString() : dwColor( 0xffffffff ) {  }
		SColorString( const wchar_t *pszStr, DWORD col ) : szString( pszStr ), dwColor( col ) {  }
		SColorString( const std::wstring &szStr, DWORD col ) : szString( szStr ), dwColor( col ) {  }
		int operator&( IDataTree &ss );
	};
private:
	typedef std::vector<std::wstring> CVectorOfStrings;
	typedef std::vector<SColorString> CVectorOfColorStrings;
	CVectorOfColorStrings vectorOfStrings;		//��� ������� � �������
	CVectorOfStrings vectorOfCommands;				//����������� ������� � �������, ��� ������ ���������� ������ �� ���������� �����/����
	
	DWORD dwLastOpenTime;				//����� ����� �������� �������� �������� �������
	DWORD dwLastCloseTime;			//����� ����� �������� �������� �������� �������
	bool bAnimation;						//���� ����, ��� ���������� ��������, ������� ��� ��������
	int nCursorPos;							//������� ������� � ������� ������������� ������
	int nBeginString;						//��������� ������������ ������ �� ������ �����
															//0 ��������� ����� ������ ��������
	int nBeginCommand;					//������� ������� �� ���� ������
	DWORD m_dwColor;
	bool bShowCursor;						//������� �� ������ � ������� ������
	DWORD dwLastCursorAnimatedTime;	//��� �������� �������
	
	std::wstring szEditString;	//������� ������������� ������
	//��� ��������� ������
	typedef std::list< CPtr<IConsoleCommandHandler> > CCommandsList;
	CCommandsList commandsChain;
	Script consoleScript;				// console script

	//��� ���� ���������� ����� ���������� ����� �������� �� ������
	void ParseCommand( const std::wstring &szCommand );
	void InitConsoleScript();
	bool RunScriptFile( const std::string &szScriptFileName );
	typedef std::unordered_map< std::string, int > CConsoleFunctions;
	CConsoleFunctions consoleFunctions;

public:
	CUIConsole();
	~CUIConsole() {}

	// serializing...
	virtual int STDCALL operator&( IDataTree &ss );

	virtual bool STDCALL IsVisible();
	virtual void STDCALL ShowWindow( int _nCmdShow );
	virtual bool STDCALL IsAnimationStage() { return bAnimation; }
	
	//������� ������ �������� ��� ������ ������
	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	//��� ����� ��� �������� �������, ����������� � �������������
	virtual bool STDCALL Update( const NTimer::STime &currTime );
	//����� ��������� ������, ����� ������ ������������ �������
	virtual void STDCALL Draw( interface IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );

	virtual bool STDCALL OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState );
	
	virtual void STDCALL RegisterCommand( IConsoleCommandHandler *pHandler );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIConsoleBridge : public IUIConsole, public CUIConsole
{
	OBJECT_NORMAL_METHODS( CUIConsoleBridge );
	DECLARE_SUPER( CUIConsole );
	DEFINE_UIELEMENT_BRIDGE;
	
	virtual void STDCALL RegisterCommand( IConsoleCommandHandler *pHandler ) { CSuper::RegisterCommand( pHandler ); }
	virtual bool STDCALL IsAnimationStage() { return CSuper::IsAnimationStage(); }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif		//__UI_CONSOLE_H__
