#ifndef __UISCREEN_H__
#define __UISCREEN_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Input\Input.h"
#include "UIBasic.h"
#include "UIConsole.h"
#include "UIMessageBox.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIScreen : public CMultipleWindow
{
	
public:
	//��� chat/acks system
	struct SAcknowledgment
	{
		DECLARE_SERIALIZE;
	public:
		std::wstring szString;
		DWORD dwColor;
		NTimer::STime time;
	};

private:
	//
	std::string szResourceName;
	DWORD m_mouseState;
	DWORD m_keyboardState;
	CVec2 m_prevLButtonPos;				//����������� ����������, ��� �������� ����� ������ �����
	CVec2 m_prevPrevLButtonPos;		//���������� ����������� ���������� ������ ����� �����
	
	std::list< SUIMessage > uiMessageList;

	//��� mouse wheel
	CPtr<IInputSlider> pMouseWheelSlider;

	typedef std::list<SAcknowledgment> CListOfAcks;
	CListOfAcks listOfAcks;

	//chat message
	std::wstring szChatMessage;
	std::wstring szLastChatMessage;
	int nNumChatDublicates;
	bool bChatMode;
	bool bMessagesToEveryone;
	int  nCursorPos;

	//Message Box
	CPtr<CUIMessageBoxBridge> pMessageBox;

public:
	CUIScreen();

	virtual int STDCALL operator&( interface IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );

	//
	virtual int STDCALL Load( const char *pszResourceName, bool bRelative = true );
	virtual bool STDCALL Update( const NTimer::STime &currTime );
	virtual void STDCALL Draw( interface IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	
	virtual bool STDCALL IsInside( const CVec2 &_vPos ) { return IsInsideChild( _vPos ); }
	virtual bool STDCALL IsEmpty() { return CMultipleWindow::IsEmpty(); }
	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	
	//���������� ��������� �������
	virtual bool STDCALL OnLButtonDblClk( const CVec2 &vPos );
	virtual bool STDCALL OnMouseMove( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnLButtonUp( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnRButtonDown( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnRButtonUp( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );
	virtual void STDCALL ProcessGameMessage( const SGameMessage &msg );
	virtual bool STDCALL GetMessage( SGameMessage *pMsg );

	virtual int STDCALL MessageBox( const WORD *pszText, int nType );
	
	virtual bool STDCALL OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState );
	virtual void STDCALL ClearStrings() { listOfAcks.clear(); }
	void UpdateChatString( int nAsciiCode, int nVirtualKey, bool bPressed );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIScreenBridge : public IUIScreen, public CUIScreen
{
	OBJECT_NORMAL_METHODS( CUIScreenBridge );
	DECLARE_SUPER( CUIScreen );
	DEFINE_UISCREEN_BRIDGE;
	virtual bool STDCALL IsEmpty() { return CSuper::IsEmpty(); }
	virtual int STDCALL MessageBox( const WORD *pszText, int nType ) { return CSuper::MessageBox( pszText, nType ); }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UISCREEN_H__
