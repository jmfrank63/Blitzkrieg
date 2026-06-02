#ifndef __UI_MESSAGE_BOX_H__
#define __UI_MESSAGE_BOX_H__

#include "UIBasic.h"
#include "UIButton.h"

class CUIMessageBox : public CMultipleWindow
{
	DECLARE_SERIALIZE;

	CUIButton *pOK;				//инициализируется во время загрузки и используется для ускорения доступа к компонентам
	CUIButton *pCancel;
	CUIStatic *pText;
	int m_nResult;
	int m_nType;
	
public:
	CUIMessageBox() : pOK( 0 ), pCancel( 0 ), pText( 0 ), m_nResult( 0 ), m_nType( 0 ) {}
	virtual ~CUIMessageBox() {}
	
	virtual void STDCALL ShowWindow( int _nCmdShow );
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );
	
	virtual int STDCALL operator&( IDataTree &ss );
	
	void SetMessageBoxType( int nType );
	virtual void STDCALL SetWindowText( int nState, const WORD *pszText );
	
	int GetResult() { return m_nResult; }
};

class CUIMessageBoxBridge : public IUIContainer, public CUIMessageBox
{
	OBJECT_NORMAL_METHODS( CUIMessageBoxBridge );
	DECLARE_SUPER( CUIMessageBox );

public:
	DEFINE_UICONTAINER_BRIDGE;
	virtual void STDCALL SetMessageBoxType( int nType ) { CSuper::SetMessageBoxType( nType ); }
	virtual int STDCALL GetResult() { return CSuper::GetResult(); }
};

#endif		//__UI_MESSAGE_BOX_H__
