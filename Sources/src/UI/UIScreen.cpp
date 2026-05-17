#include "StdAfx.h"

#include "..\Main\iMainCommands.h"
#include "..\GameTT\iMission.h"
#include "..\Scene\Scene.h"
#include "UIScreen.h"
#include "UIMessages.h"
#include "..\GameTT\CommonID.h"

static const int GLOBAL_CONSOLE_ID = 0xAC07A918;

static const int ACKS_VERTICAL_POSITION = 30;				// ������� ��������� ���������, ������ ������ ������
static const int TEXT_VERTICAL_SIZE = 20;						// ������ ������ �� ���������
static const int TEXT_TWO = 20;											// ������ >> �� �����������
static const int TEXT_LEFT_SPACE = 0;								// ������ �� ������ ���� ������ �� ������ � acknowledgement
static const int CHAT_MESSAGE_LEFT = 0;							// ������ �� ������ ���� ������ �� ������ � chat message
static const int CHAT_MESSAGE_TOP = 0;							// ������ �� �������� ���� ������ �� ������ � chat message
// ���������, ������������ �� �����
static int TEXT_ANIMATION_TIME = 5000;							// ����� ��� ����������� ���������� ���������, ����� ��� ���������
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScreen::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	//CRAP{ FOR SAVES COMPATIBILITY
	CMultipleWindow::operator&( ss );
	
	saver.Add( 102, &szResourceName );
	saver.Add( 103, &m_mouseState );
	saver.Add( 104, &m_keyboardState );
	saver.Add( 105, &m_prevLButtonPos );
	saver.Add( 106, &m_prevPrevLButtonPos );
	saver.Add( 107, &uiMessageList );
	saver.Add( 108, &listOfAcks );
	saver.Add( 109, &szChatMessage );
	saver.Add( 100, &szLastChatMessage );
	saver.Add( 101, &nNumChatDublicates );
	saver.Add( 102, &bChatMode );
	saver.Add( 103, &bMessagesToEveryone );
	saver.Add( 104, &nCursorPos );
	saver.Add( 105, &pMessageBox );

	//CRAP}

	if ( saver.IsReading() )
	{
		pMouseWheelSlider = GetSingleton<IInput>()->CreateSlider( "mouse_wheel" );
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScreen::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>( this ) );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScreen::SAcknowledgment::operator&( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szString );
	saver.Add( 2, &dwColor );
	saver.Add( 3, &time );
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIScreen::CUIScreen() : m_mouseState( E_MOUSE_FREE ), m_keyboardState( E_KEYBOARD_FREE ), bChatMode( false ), nCursorPos( 0 )
{
	SetShowBackgroundFlag( false );
	szLastChatMessage = L"";
	nNumChatDublicates = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScreen::Load( const char *pszResourceName, bool bRelative )
{
	TEXT_ANIMATION_TIME = GetGlobalVar( "UI.TextAnimTime", 5000 );
	//
	CPtr<IDataStream> pStream;
	if ( bRelative )
	{
		szResourceName = pszResourceName;
		pStream = GetSingleton<IDataStorage>()->OpenStream( (szResourceName + ".xml").c_str(), STREAM_ACCESS_READ );
	}
	else
		pStream = OpenFileStream( pszResourceName, STREAM_ACCESS_READ );
	if ( pStream )
	{
		//mouse wheel slider
		pMouseWheelSlider = GetSingleton<IInput>()->CreateSlider( "mouse_wheel" );
		NI_ASSERT_T( pMouseWheelSlider != 0, "Can not create mouse_wheel slider" );

		CPtr<IDataTree> pDT = bRelative ? CreateDataTreeSaver( pStream, IDataTree::READ ) : CreateDataTreeSaver( pStream, IDataTree::READ, "GUI_Composer_Project" );
		this->operator&( *pDT );
		IUIElement *pElement = GetChildByIndex( 0 );
		return pElement == 0 ? 0 : pElement->GetWindowID();
	}
	else
	{
		std::string szErr = "UIScreen can not be loaded: ";
		szErr += pszResourceName;
		NI_ASSERT_T( 0, szErr.c_str() );
		return 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScreen::Reposition( const CTRect<float> &rcParent )
{
	SetScreenRect( rcParent );
	SetPos( CVec2(0, 0) );
	SetSize( CVec2(rcParent.Width(), rcParent.Height()) );
	CMultipleWindow::Reposition( rcParent );
//	UpdateSubRects();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::OnLButtonDblClk( const CVec2 &vPos )
{
	//���� ������� �������� � Input
	return CMultipleWindow::OnLButtonDblClk( vPos );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::OnMouseMove( const CVec2 &vPos, EMouseState mouseState )
{
	return CMultipleWindow::OnMouseMove( vPos, (EMouseState) m_mouseState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	m_prevPrevLButtonPos.x = m_prevLButtonPos.x;
	m_prevPrevLButtonPos.y = m_prevLButtonPos.y;
	m_prevLButtonPos.x = vPos.x;
	m_prevLButtonPos.y = vPos.y;

	//��� ���� ��������� state
	m_mouseState |= E_LBUTTONDOWN;
	return CMultipleWindow::OnLButtonDown( vPos, (EMouseState) m_mouseState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::OnLButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	//��� ���� ��������� state
	m_mouseState &= ~E_LBUTTONDOWN;
	return CMultipleWindow::OnLButtonUp( vPos, (EMouseState) m_mouseState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::OnRButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	//��� ���� ��������� state
	m_mouseState |= E_RBUTTONDOWN;
	return CMultipleWindow::OnRButtonDown( vPos, (EMouseState) m_mouseState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::OnRButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	//��� ���� ��������� state
	m_mouseState &= ~E_RBUTTONDOWN;
	return CMultipleWindow::OnRButtonUp( vPos, (EMouseState) m_mouseState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState )
{
	if ( nVirtualKey == VK_SHIFT )
	{
		if ( bPressed )
			m_keyboardState |= E_SHIFT_KEY_DOWN;
		else if ( m_keyboardState & E_SHIFT_KEY_DOWN )
			m_keyboardState ^= E_SHIFT_KEY_DOWN;
	}
	else if ( nVirtualKey == VK_MENU )
	{
		if ( bPressed )
			m_keyboardState |= E_ALT_KEY_DOWN;
		else if ( m_keyboardState & E_ALT_KEY_DOWN )
			m_keyboardState ^= E_ALT_KEY_DOWN;
	}
	else if ( nVirtualKey == VK_CONTROL )
	{
		if ( bPressed )
			m_keyboardState |= E_CTRL_KEY_DOWN;
		else if ( m_keyboardState & E_CTRL_KEY_DOWN )
			m_keyboardState ^= E_CTRL_KEY_DOWN;
	}



	//���� ������ ~ ��� ������ �� ������� ���������
	//192 ��� ����������� ��� ~
	if ( /* !GetModalFlag() && */ nVirtualKey == 192 && bChatMode == false && bPressed == true && m_keyboardState == E_KEYBOARD_FREE )
	{
		/*
		IUIElement *pConsole = GetChildByID( GLOBAL_CONSOLE_ID );
		if ( pConsole )
		{
			// DISABLE CONSOLE IN MULTIPLAYER (BUG 6309)
			pConsole->ShowWindow( UI_SW_HIDE );
		}
		*/
		return true;
	}

	if ( bChatMode && bPressed )
		UpdateChatString( nAsciiCode, nVirtualKey, bPressed );
	
	return CMultipleWindow::OnChar( nAsciiCode, nVirtualKey, bPressed, m_keyboardState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::ProcessMessage( const SUIMessage &msg )
{
	if ( msg.nMessageCode == UI_CLEAR_KEYBOARD_STATE )
	{
		m_keyboardState = E_KEYBOARD_FREE;
		return true;
	}

	if ( /* !GetModalFlag() && */ msg.nMessageCode == MC_SHOW_CONSOLE )
	{
		IUIElement *pConsole = GetChildByID( GLOBAL_CONSOLE_ID );
		if ( pConsole )
		{
#if defined(_FINALRELEASE) && !defined(_DEVVERSION)
	if ( !GetGlobalVar( "MultiplayerGame", 0 ) )
#endif // defined(_FINALRELEASE) && !defined(_DEVVERSION)
			if ( !pConsole->IsVisible() ) 
			{
				MoveWindowUp( pConsole );
				pConsole->ShowWindow( UI_SW_SHOW );
			}
			else
				pConsole->ShowWindow( UI_SW_HIDE );
		}
//		pConsole->ShowWindow( !pConsole->IsVisible() );
		return true;
	}

/*
	if ( msg.nMessageCode == MC_ENTER_CHAT_MODE )
	{
		if ( CMultipleWindow::ProcessMessage( msg ) )
			return true;

		//������ � ����� ������ ������
		bChatMode = true;
		SUIMessage msg;
		msg.nMessageCode = MC_SET_TEXT_MODE;
		msg.nFirst = GetWindowID();
		uiMessageList.push_back( msg );
		return true;
	}
*/

	if ( msg.nMessageCode == UI_BREAK_MESSAGE_BOX )
	{
		SetModalFlag( false );
		int nRes = pMessageBox->GetResult();
		pMessageBox = 0;
		return true;
	}
	
	if ( IsProcessedMessage( msg ) )
	{
		if ( msg.nMessageCode == (MC_ENTER_CHAT_MODE | PROCESSED_FLAG) )
		{
			//������ � ����� ������ ������
			bChatMode = true;
			bMessagesToEveryone = true;
			SetGlobalVar( "bMessagesToEveryone", 1 );
			SUIMessage msg;
			msg.nMessageCode = MC_SET_TEXT_MODE;
			msg.nFirst = GetWindowID();
			uiMessageList.push_back( msg );
//			NStr::DebugTrace("All\n");
			return true;
		}
		if ( msg.nMessageCode == (MC_ENTER_CHAT_MODE_FRIENDS | PROCESSED_FLAG) )
		{
			//������ � ����� ������ ������
			bChatMode = true;
			bMessagesToEveryone = false;
			SetGlobalVar( "bMessagesToEveryone", 0 );
			SUIMessage msg;
			msg.nMessageCode = MC_SET_TEXT_MODE;
			msg.nFirst = GetWindowID();
			uiMessageList.push_back( msg );
//			NStr::DebugTrace("Allies\n");
			return true;
		}
		else
		{
			uiMessageList.push_back( msg );
			return true;
		}
	}
	else
	{
		if ( !CMultipleWindow::ProcessMessage( msg ) )
		{
			if ( IsNotifyMessage( msg ) )
				return false;
			uiMessageList.push_back( msg );
		}
		return true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScreen::ProcessGameMessage( const SGameMessage &msg )
{
/*
	if ( msg.nEventID == MC_BEGIN_ACTION )
	{
		if ( OnLButtonDown( pCursor->GetPos(), E_LBUTTONDOWN ) == false )

	}
*/

	SUIMessage uiMsg;
	uiMsg.nMessageCode = msg.nEventID;
	uiMsg.nFirst = msg.nParam;
	uiMsg.nSecond = 0;
	ProcessMessage( uiMsg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::GetMessage( SGameMessage *pMsg )
{
	if ( !uiMessageList.empty() )
	{
		int nMessageCode = uiMessageList.front().nMessageCode;
		pMsg->nEventID = nMessageCode & (~PROCESSED_FLAG);
		pMsg->nParam = uiMessageList.front().nFirst;
		uiMessageList.pop_front();
		return true;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScreen::Visit( interface ISceneVisitor *pVisitor )
{
	CMultipleWindow::Visit( pVisitor );
	// 
	IUIConsole *pConsole = checked_cast<IUIConsole*>( GetChildByID(GLOBAL_CONSOLE_ID) );
	if ( !pConsole || ( !pConsole->IsVisible() && !pConsole->IsAnimationStage() ) )
		pVisitor->VisitUICustom( dynamic_cast<IUIElement*>(this) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScreen::Draw( interface IGFX *pGFX )
{
	IUIConsole *pConsole = checked_cast<IUIConsole*>( GetChildByID(GLOBAL_CONSOLE_ID) );
	if ( !pConsole || ( !pConsole->IsVisible() && !pConsole->IsAnimationStage() ) )
	{
		pGFX->SetShadingEffect( 3 );	
		//���� ���� ���������, ��� ���� �������, �� ��� ���������
		const CTRect<float> &rc = GetScreenRect();
		int nCurrentY = rc.y1 + ACKS_VERTICAL_POSITION;
		for ( CListOfAcks::iterator it = listOfAcks.begin(); it != listOfAcks.end(); ++it )
		{
			pGFX->DrawString( it->szString.c_str(), TEXT_LEFT_SPACE, nCurrentY, it->dwColor );
			nCurrentY += TEXT_VERTICAL_SIZE;
		}

		if ( bChatMode )
		{
			const bool bMessagesToEveryone = GetGlobalVar( "bMessagesToEveryone", 0 );
			IText * pToWho = GetSingleton<ITextManager>()->GetDialog( bMessagesToEveryone ? "Textes\\UI\\Mission\\toAll" : "Textes\\UI\\Mission\\toAllies" );
			std::wstring szResult = L"";
			if ( pToWho )
				szResult = reinterpret_cast<const wchar_t*>(pToWho->GetString());

			szResult += szChatMessage;
			//pGFX->DrawString( L">>", CHAT_MESSAGE_LEFT, rc.y1 + CHAT_MESSAGE_TOP );
			pGFX->DrawString( szResult.c_str(), CHAT_MESSAGE_LEFT, rc.y1 + CHAT_MESSAGE_TOP );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScreen::Update( const NTimer::STime &currTime )
{
	// pump chat strings
	IConsoleBuffer *pConsoleBuffer = GetSingleton<IConsoleBuffer>();
	DWORD color = GetGlobalVar( "Scene.Colors.Summer.Text.Chat.Color", int(0xffd8bd3e) );
	while ( const wchar_t *pszString = pConsoleBuffer->Read(CONSOLE_STREAM_CHAT, &color) ) 
	{
		SAcknowledgment ack;
		ack.szString = pszString;
		ack.dwColor = color;
		ack.time = currTime;
		// bug   #6815 
 		//listOfAcks.push_front( ack );
		listOfAcks.push_back( ack );
	}
	//
	for ( CListOfAcks::iterator it = listOfAcks.begin(); it != listOfAcks.end(); )
	{
		if ( it->time + TEXT_ANIMATION_TIME < currTime )
			it = listOfAcks.erase( it );
		else
			++it;
	}

	//mouse wheel support
	IScene *pScene = GetSingleton<IScene>();
	if ( pMouseWheelSlider && pScene->GetUIScreen() == static_cast<CUIScreenBridge*>(this) )
	{
		const float fVal = pMouseWheelSlider->GetDelta();
		if ( fVal )
		{
			CMultipleWindow::OnMouseWheel( GetSingleton<ICursor>()->GetPos(), (EMouseState) m_mouseState, fVal );
		}
	}

	return CMultipleWindow::Update( currTime );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScreen::UpdateChatString( int nAsciiCode, int nVirtualKey, bool bPressed )
{
	//���� �������� ������, �� ������ ������� ���
//	if ( isprint( nAsciiCode ) )
	if ( nAsciiCode >= 32 )
	{
		szChatMessage.insert( nCursorPos, 1, nAsciiCode );
		nCursorPos++;

		return;
	}

	if ( nVirtualKey == VK_TAB )
	{
		szChatMessage.insert( nCursorPos, 4, VK_SPACE );
		nCursorPos += 4;
		return;
	}
	

	//���� �� �������� ������, �� ������������ �������������� ����������
	switch( nVirtualKey )
	{
	case VK_RETURN:
		{
			if ( nNumChatDublicates < 2 || szChatMessage != szLastChatMessage )
			{
				if ( szChatMessage != szLastChatMessage )
				{
					nNumChatDublicates = 1;
					szLastChatMessage = szChatMessage;
				}
				else
					++nNumChatDublicates;
				if ( bMessagesToEveryone )
				{
					const DWORD dwChatColor = GetGlobalVar( "Scene.Colors.Summer.Text.Chat.Color", int(0xffd8bd3e) );
//					GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_CHAT, szChatMessage.c_str(), dwChatColor );
					GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_NET_CHAT, L"All", dwChatColor );
					GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_NET_CHAT, szChatMessage.c_str(), dwChatColor );
				}
				else
				{
					const DWORD dwChatColor = GetGlobalVar( "Scene.Colors.Summer.Text.ChatAllies.Color", int(0xff00ff00) );
//					GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_CHAT, szChatMessage.c_str(), dwChatColor );
					GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_NET_CHAT, L"Allies", dwChatColor );
					GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_NET_CHAT, szChatMessage.c_str(), dwChatColor );
				}
			}
			/*
			SAcknowledgment ack;
			ack.szString = szChatMessage;
			ack.dwColor = 0xffffffff;
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			ack.time = pTimer->GetAbsTime();
			listOfAcks.push_front( ack );
			*/
			szChatMessage = L"";
			nCursorPos = 0;
			bChatMode = false;
			bMessagesToEveryone = true;

			//������� text enter mode
			SUIMessage msg;
			msg.nMessageCode = MC_CANCEL_TEXT_MODE;
			msg.nFirst = GetWindowID();
			uiMessageList.push_back( msg );
		}
		break;
		
	case VK_BACK:
		if ( nCursorPos > 0 )
		{
			szChatMessage.erase( nCursorPos-1, 1 );
			nCursorPos--;
		}
		break;

	case VK_DELETE:
		if ( nCursorPos < szChatMessage.size() )
			szChatMessage.erase( nCursorPos, 1 );
		break;

	case VK_LEFT:
		if ( nCursorPos == 0 )
			break;
		if ( m_keyboardState == E_KEYBOARD_FREE )
		{
			//�� ���� ������� �����
			nCursorPos--;
		}
		if ( m_keyboardState & E_CTRL_KEY_DOWN )
		{
			//���� ������ crtl � ������� �����, �� ���������� ����� �� ���� �����
			while( nCursorPos > 0 && isspace(szChatMessage[nCursorPos-1]) )
				nCursorPos--;
			if ( nCursorPos > 0 )
			{
				if ( isalpha(szChatMessage[nCursorPos-1]) )
					while( nCursorPos > 0 && isalpha(szChatMessage[nCursorPos-1]) )
						nCursorPos--;
					else
						while( nCursorPos > 0 && !isalpha(szChatMessage[nCursorPos-1]) )
							nCursorPos--;
			}
		}
		break;

	case VK_RIGHT:
		if ( nCursorPos == szChatMessage.size() )
			break;
		if ( m_keyboardState == E_KEYBOARD_FREE )
		{
			//�� ���� ������� ������
			nCursorPos++;
		}
		else if ( m_keyboardState & E_CTRL_KEY_DOWN )
		{
			//���� ������ crtl � ������� ������, �� ���������� ������ �� ���� �����
			if ( nCursorPos < szChatMessage.size() )
			{
				if ( isalpha(szChatMessage[nCursorPos]) )
					while( nCursorPos < szChatMessage.size() && isalpha(szChatMessage[nCursorPos]) )
						nCursorPos++;
					else
						while( nCursorPos < szChatMessage.size() && !isalpha(szChatMessage[nCursorPos]) )
							nCursorPos++;
			}
			
			while( nCursorPos < szChatMessage.size() && isspace(szChatMessage[nCursorPos]) )
				nCursorPos++;
		}
		break;

	case VK_HOME:
		if ( m_keyboardState == E_KEYBOARD_FREE )
		{
			//�� ������ ������
			nCursorPos = 0;
		}
		break;

	case VK_END:
		if ( m_keyboardState == E_KEYBOARD_FREE )
		{
			//�� ����� ������
			nCursorPos = szChatMessage.size();
		}
		break;

	case VK_ESCAPE:
		szChatMessage = L"";
		nCursorPos = 0;
		bChatMode = false;
		
		//������� text enter mode
		SUIMessage msg;
		msg.nMessageCode = MC_CANCEL_TEXT_MODE;
		msg.nFirst = GetWindowID();
		uiMessageList.push_back( msg );
		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScreen::MessageBox( const WORD *pszText, int nType )
{
	IObjectFactory *pFactory = GetCommonFactory();
	IRefCount *pObj = pFactory->CreateObject( UI_MESSAGE_BOX );
	NI_ASSERT_T( pObj != 0, "Can't create message box" );

	pMessageBox = dynamic_cast<CUIMessageBoxBridge*> ( pObj );
	std::string szFile;
	switch( nType )
	{
		case E_MESSAGE_BOX_TYPE_OK:
			szFile = "UI\\OkMessageBox.xml";
			break;
		default:
			return -1;
	}

	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	CPtr<IDataStream> pStream = pStorage->OpenStream( szFile.c_str(), STREAM_ACCESS_READ );
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
	pMessageBox->operator&( *pDT );
	pMessageBox->SetMessageBoxType( nType );
	pMessageBox->SetWindowText( 0, pszText );

	IUIElement *pElement = dynamic_cast_ptr<IUIElement *>( pMessageBox );
	AddChild( pElement );
	SetModalFlag( true );
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
