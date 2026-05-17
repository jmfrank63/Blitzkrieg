#include "StdAfx.h"
#include "UIMessageBox.h"
#include "UIMessages.h"

void CUIMessageBox::ShowWindow( int _nCmdShow )
{
	if ( _nCmdShow == UI_SW_SHOW )
	{
		CMultipleWindow *pPapa = dynamic_cast<CMultipleWindow *> ( GetParent() );
		NI_ASSERT_T( pPapa != 0, "Window has no parent!" );
		if ( pPapa )
			pPapa->SetModalFlag( true );
	}
	
	CMultipleWindow::ShowWindow( _nCmdShow );
}

bool CUIMessageBox::ProcessMessage( const SUIMessage &msg )
{
	switch ( msg.nMessageCode )
	{
		case UI_NOTIFY_WINDOW_CLICKED:
			SUIMessage newMsg;
			newMsg.nMessageCode = UI_BREAK_MESSAGE_BOX;
			newMsg.nFirst = msg.nFirst;
			newMsg.nSecond = msg.nSecond;
			GetParent()->ProcessMessage( newMsg );

			switch ( m_nType )
			{
			case E_MESSAGE_BOX_TYPE_OK:
				m_nResult = E_MESSAGE_BOX_RETURN_OK;
				break;

			case E_MESSAGE_BOX_TYPE_OKCANCEL:
				if ( msg.nFirst == 1 )
					m_nResult = E_MESSAGE_BOX_RETURN_OK;
				else
					m_nResult = E_MESSAGE_BOX_RETURN_CANCEL;
				break;

			case E_MESSAGE_BOX_TYPE_YESNO:
				if ( msg.nFirst == 1 )
					m_nResult = E_MESSAGE_BOX_RETURN_YES;
				else
					m_nResult = E_MESSAGE_BOX_RETURN_NO;
				break;
			}
/*
			CMultipleWindow *pPapa = dynamic_cast<CMultipleWindow *> ( GetParent() );
			NI_ASSERT( pPapa != 0 );
			if ( pPapa )
				pPapa->SetModalFlag( false );
			ShowWindow( UI_SW_HIDE );
*/
			return true;
	}

	return CMultipleWindow::ProcessMessage( msg );
}

int CUIMessageBox::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMultipleWindow*>(this) );
	
	if ( saver.IsReading() )
	{
		//�������������� ���������� ����������
		pOK = dynamic_cast<CUIButton *>( GetChildByID(1) );
		pCancel = dynamic_cast<CUIButton *>( GetChildByID(2) );
		pText = dynamic_cast<CUIStatic *>( GetChildByID(3) );
		NI_ASSERT_T( pOK && pCancel && pText, "Invalid data for MessageBox, can not create elements" );
	}
	return 0;
}

int CUIMessageBox::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>(this) );
	
	if ( saver.IsReading() )
	{
		//�������������� ���������� ����������
		pOK = dynamic_cast<CUIButton *>( GetChildByID(1) );
		pCancel = dynamic_cast<CUIButton *>( GetChildByID(2) );
		pText = dynamic_cast<CUIStatic *>( GetChildByID(3) );
		NI_ASSERT_T( pOK && pCancel && pText, "Invalid data for MessageBox, can not create elements" );
	}
	return 0;
}

void CUIMessageBox::SetMessageBoxType( int nType )
{
	m_nType = nType;
	switch ( nType )
	{
		case E_MESSAGE_BOX_TYPE_OK:
			pOK->SetWindowText( 0, L"OK" );
			break;

		case E_MESSAGE_BOX_TYPE_OKCANCEL:
			pOK->SetWindowText( 0, L"OK" );
			pOK->SetWindowText( 0, L"Cancel" );
			break;

		case E_MESSAGE_BOX_TYPE_YESNO:
			pOK->SetWindowText( 0, L"Yes" );
			pOK->SetWindowText( 0, L"No" );
			break;

		default:
			NI_ASSERT_T( 0, "Unknown message box type" );
	}
}

void CUIMessageBox::SetWindowText( int nState, const WORD *pszText )
{
	pText->CSimpleWindow::SetWindowText( nState, pszText );
}
