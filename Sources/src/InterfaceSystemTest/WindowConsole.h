// WindowConsole.h: interface for the CWindowConsole class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WINDOWCONSOLE_H__3222DEC4_B4EB_4831_9CE4_417F561454C6__INCLUDED_)
#define AFX_WINDOWCONSOLE_H__3222DEC4_B4EB_4831_9CE4_417F561454C6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Window.h"
class CWindowEditLine;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CWindowConsole : public CWindow  
{
	OBJECT_COMPLETE_METHODS(CWindowConsole)
	DECLARE_SERIALIZE
	DECLARE_CLONABLE_CLASS 

	struct SColorString
	{
		DECLARE_SERIALIZE;
	public:
		std::wstring szString;
		DWORD dwColor;
		
		SColorString() : dwColor( 0xffffffff ) {  }
		SColorString( const wchar_t *pszStr, DWORD col ) : szString( pszStr ), dwColor( col ) {  }
		SColorString( const std::wstring &szStr, DWORD col ) : szString( szStr ), dwColor( col ) {  }
	};

	typedef std::vector<std::wstring> CVectorOfStrings;
	typedef std::vector<SColorString> CVectorOfColorStrings;
	CVectorOfColorStrings vectorOfStrings;		//��� ������� � �������
	CVectorOfStrings vectorOfCommands;				//����������� ������� � �������, ��� ������ ���������� ������ �� ���������� �����/����

	typedef std::unordered_set<std::string> CConsoleFunctions;
	CConsoleFunctions consoleFunctions;

	CNCPtr<CWindowEditLine> pEditLine;

	NTimer::STime currTime;
	int nCursorPos;							//������� ������� � ������� ������������� ������
	int nBeginString;						//��������� ������������ ������ �� ������ �����
															//0 ��������� ����� ������ ��������
	int nBeginCommand;					//������� ������� �� ���� ������
	bool bCanLaunchEffect;									// console is notified about open effect finish.
	DWORD dwColor;

	// message sink registration
	void RegisteMessageSinks();
	void UnRegisteMessageSinks();

	//��� ���� ���������� ����� ���������� ����� �������� �� ������
	void ParseCommand( const std::wstring &szCommand );
public:
	CWindowConsole() : currTime( 0 ), bCanLaunchEffect( true ), nBeginCommand( 0 ),
										nBeginString( 0 ) {  }
	void RegisterEffects( interface IScreen *pScreen );
	virtual void NotifyStateSequenceFinished();
	
	void OnKeyUp( const struct SGameMessage &msg );
	void OnKeyDown( const struct SGameMessage &msg );
	void OnCtrlHome( const struct SGameMessage &msg );
	void OnCtrlEnd( const struct SGameMessage &msg );
	void OnPgUp( const struct SGameMessage &msg );
	void OnPgDn( const struct SGameMessage &msg );
	void OnShowConsole( const struct SGameMessage &msg );
	void OnReturn( const struct SGameMessage &msg );

	virtual void STDCALL Reposition( const CTRect<float> &rcParent );

	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	virtual void STDCALL Segment( const NTimer::STime timeDiff );

	virtual int STDCALL operator&( IDataTree &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX_WINDOWCONSOLE_H__3222DEC4_B4EB_4831_9CE4_417F561454C6__INCLUDED_)
