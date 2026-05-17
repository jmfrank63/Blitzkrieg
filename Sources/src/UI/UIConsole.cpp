#include "StdAfx.h"

#include "..\Main\iMainCommands.h"
#include "..\Input\Input.h"
#include "..\GameTT\iMission.h"
#include "UIConsole.h"
#include "UIMessages.h"

#include "..\Net\NetDriver.h"
#include "..\Main\Transceiver.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const int OPEN_TIME = 200;						//����� �������� � �������� ��������� � �������������
static const int CONSOLE_HEIGHT = 240;			//������ ������� � ��������
static const int TEXT_LEFT_SPACE = 20;			//������ �� ������ ���� ������ �� ������ � �������
static const int TEXT_VERTICAL_SIZE = 20;		//������ ������ �� ���������
static const int MINUS_PAGE_SIZE = 5;				//����������� ��������� ������� ��� PgUp PgDown,
static const int CURSOR_ANIMATION_TIME = 400;		//������ ������������ �������
static const WCHAR szPrefix[] = L">>";

/*
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SetIGlobalVar( struct lua_State *state )
int SetFGlobalVar( struct lua_State *state )
int SetSGlobalVar( struct lua_State *state )
int GetIGlobalVar( struct lua_State *state )
int GetFGlobalVar( struct lua_State *state )
int GetSGlobalVar( struct lua_State *state )
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIConsole::SColorString::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "String", &szString );
	saver.Add( "Color", &dwColor );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIConsole::SColorString::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szString );
	saver.Add( 2, &dwColor );
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIConsole::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CSimpleWindow*>(this) );
	saver.Add( 2, &vectorOfStrings );
	saver.Add( 3, &vectorOfCommands );
	saver.Add( 4, &nCursorPos );
	saver.Add( 5, &nBeginString );
	saver.Add( 6, &nBeginCommand );
	saver.Add( 7, &commandsChain );
	saver.Add( 8, &dwLastOpenTime );
	saver.Add( 9, &dwLastCloseTime );
	saver.Add( 10, &szEditString );
	saver.Add( 11, &bShowCursor );
	saver.Add( 12, &dwLastCursorAnimatedTime );

	if ( saver.IsReading() )
		InitConsoleScript();

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIConsole::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CSimpleWindow*>(this) );
	saver.Add( "ConsoleStrings", &vectorOfStrings );
	saver.Add( "ConsoleCommands", &vectorOfCommands );
	saver.Add( "CursorPosition", &nCursorPos );
	saver.Add( "BeginString", &nBeginString );
	saver.Add( "BeginCommand", &nBeginCommand );
	saver.Add( "ShowCursor", &bShowCursor );
	saver.Add( "LastAnim", &dwLastCursorAnimatedTime );
	
	if ( saver.IsReading() )
		InitConsoleScript();
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIConsole::IsVisible()
{
	if ( bAnimation && dwLastCloseTime > 0 )
		return false;					//��������� �����������

	return CSimpleWindow::IsVisible();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIConsole::ShowWindow( int _nCmdShow )
{
	if ( _nCmdShow != UI_SW_HIDE && _nCmdShow != UI_SW_MINIMIZE )
	{
		//���������� ���������
		DWORD dwCurrentTime = GetSingleton<IGameTimer>()->GetAbsTime();
		if ( dwCurrentTime - dwLastCloseTime < OPEN_TIME )
			dwLastOpenTime = dwCurrentTime + ( dwCurrentTime - dwLastCloseTime - OPEN_TIME );
		else
			dwLastOpenTime = dwCurrentTime;
		dwLastCloseTime = 0;
		bAnimation = true;

		//�������� ������ ��������� ����� ������������ TEXT_MODE
		SUIMessage msg;
		msg.nMessageCode = MC_SET_TEXT_MODE;
		msg.nFirst = GetWindowID();
		GetParent()->ProcessMessage( msg );
		SetFocus( true );

		CSimpleWindow::ShowWindow( _nCmdShow );
	}
	else
	{
		SetFocus( false );
		if ( !IsVisible() )
		{
			CSimpleWindow::ShowWindow( _nCmdShow );
			return;
		}

		//�������� ���������
		DWORD dwCurrentTime = GetSingleton<IGameTimer>()->GetAbsTime();
		if ( dwCurrentTime - dwLastOpenTime < OPEN_TIME )
			dwLastCloseTime = dwCurrentTime + ( dwCurrentTime - dwLastOpenTime - OPEN_TIME );
		else
			dwLastCloseTime = dwCurrentTime;
		dwLastOpenTime = 0;
		bAnimation = true;

		//�������� ������ ��������� ����� ��������� TEXT_MODE
		SUIMessage msg;
		msg.nMessageCode = MC_CANCEL_TEXT_MODE;
		msg.nFirst = GetWindowID();
		GetParent()->ProcessMessage( msg );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIConsole::Reposition( const CTRect<float> &rcParent )
{
	CTRect<float> rc = GetScreenRect();
	//������, ��� ������� �������� child ��� screen
	rc.x1 = rcParent.x1;
	rc.x2 = rcParent.x2;
	SetScreenRect( rc );
	UpdateSubRects();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIConsole::Update( const NTimer::STime &currTime )
{
	if ( currTime - dwLastCursorAnimatedTime > CURSOR_ANIMATION_TIME )
	{
		dwLastCursorAnimatedTime = currTime;
		bShowCursor = !bShowCursor;
	}
	
	// retrieve and parse commands from console buffer
	{
		IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
		DWORD color = 0;
		// read console log
		while ( const wchar_t *pszString = pBuffer->Read(CONSOLE_STREAM_CONSOLE, &color) ) 
			vectorOfStrings.push_back( SColorString(pszString, color) );
		// read commands
		while ( const wchar_t *pszString = pBuffer->Read(CONSOLE_STREAM_COMMAND, &color) ) 
		{
			std::wstring szString = pszString;
			vectorOfCommands.push_back( szString );
			vectorOfStrings.push_back( SColorString(szString, color) );
			ParseCommand( szString );

			// read console log
			while ( const wchar_t *pszString = pBuffer->Read(CONSOLE_STREAM_CONSOLE, &color) ) 
				vectorOfStrings.push_back( SColorString(pszString, color) );
		}
	}
	//
	if ( !bAnimation )
		return false;

	CTRect<float> rc = GetScreenRect();
	if ( currTime - dwLastOpenTime < OPEN_TIME )
	{
		//������� � �������� ��������
		//��������� ���������� �������
		rc.bottom = CONSOLE_HEIGHT * ( currTime - dwLastOpenTime ) / OPEN_TIME;
		rc.top = rc.bottom - CONSOLE_HEIGHT;
		SetScreenRect( rc );
		UpdateSubRects();
		return true;
	}
	else if ( dwLastCloseTime == 0 )
	{
		//���� ������� ��������� ���� ��������
		rc.top = 0;
		rc.bottom = CONSOLE_HEIGHT;
		bAnimation = false;
		SetScreenRect( rc );
		UpdateSubRects();
		return true;
	}
	
	if ( currTime - dwLastCloseTime < OPEN_TIME )
	{
		//������� � �������� ��������
		//��������� ���������� �������
		rc.bottom = CONSOLE_HEIGHT - CONSOLE_HEIGHT * ( currTime - dwLastCloseTime ) / OPEN_TIME;
		rc.top = rc.bottom - CONSOLE_HEIGHT;
		SetScreenRect( rc );
		UpdateSubRects();
		return true;
	}
	else if ( dwLastOpenTime == 0 )
	{
		//���� ������� ��������� ���� ��������
		rc.top = -CONSOLE_HEIGHT;
		rc.bottom = 0;
		bAnimation = false;
		SetScreenRect( rc );
		UpdateSubRects();
//CSimpleWindow::ShowWindow( UI_SW_MINIMIZE );
		CSimpleWindow::ShowWindow( UI_SW_HIDE );

		SUIMessage msg;
		msg.nMessageCode = UI_SHOW_WINDOW;
		msg.nFirst = GetWindowID();
		msg.nSecond = UI_SW_MINIMIZE;
		CMultipleWindow *pScreen = dynamic_cast<CMultipleWindow *> ( GetParent() );
		if ( pScreen )
			pScreen->messageList.push_back( msg );
		GetSingleton<IInput>()->AddMessage( SGameMessage( 0 ) );
		
		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIConsole::Visit( interface ISceneVisitor *pVisitor )
{
	CSimpleWindow::Visit( pVisitor );
	pVisitor->VisitUICustom( dynamic_cast<IUIElement*>(this) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIConsole::Draw( interface IGFX *pGFX )
{
	if ( IsVisible() || bAnimation )
	{
		pGFX->SetShadingEffect( 3 );
		// ��������� ������������� �������
		int nCurrentY = wndRect.y2 - 2 * TEXT_VERTICAL_SIZE;
		pGFX->DrawString( szPrefix, TEXT_LEFT_SPACE, nCurrentY );
		pGFX->DrawString( szEditString.c_str(), TEXT_LEFT_SPACE + TEXT_VERTICAL_SIZE, nCurrentY );
		if ( nBeginString != 0 )
		{
			nCurrentY -= TEXT_VERTICAL_SIZE;
			pGFX->DrawString( L"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^", TEXT_LEFT_SPACE, nCurrentY, 0xFFFF0000 );
		}
		nCurrentY -= TEXT_VERTICAL_SIZE;

		// ��������� ������� � �������
		int nSize = vectorOfStrings.size();
		for ( int i = nBeginString; i < nSize; ++i )
		{
			pGFX->DrawString( vectorOfStrings[nSize - i - 1].szString.c_str(), TEXT_LEFT_SPACE, nCurrentY, vectorOfStrings[nSize - i - 1].dwColor );
			nCurrentY -= TEXT_VERTICAL_SIZE;
			if ( nCurrentY < 0 )
				break;
		}

		// ������ ������
		if ( bShowCursor )
		{
			IText *pText = states[nCurrentState].pGfxText->GetText();
			pText->SetText( szEditString.c_str() );
			int nWidth = states[nCurrentState].pGfxText->GetWidth( nCursorPos );
			pText->SetText( L"" );

			SGFXRect2 rc;
			rc.rect.left = wndRect.left + nWidth + vTextPos.x - 1 + TEXT_LEFT_SPACE + TEXT_VERTICAL_SIZE;
			rc.rect.right = rc.rect.left + 2;
			if ( rc.rect.left < wndRect.right - 1 )
			{
				//������ �� ������� �� ���� ������
				int nH = states[nCurrentState].pGfxText->GetLineSpace();
				rc.rect.top = wndRect.bottom - 2 * TEXT_VERTICAL_SIZE;
				rc.rect.bottom = rc.rect.top + nH;
				rc.maps.x1 = rc.maps.y1 = rc.maps.x2 = rc.maps.y2 = 0;
				
				if ( bBounded )
				{
					// ��������, ����� ����� ������ ����� �������
					float fTemp;
					fTemp = rcBound.y1 - rc.rect.y1;
					if ( fTemp > 0 )
					{
						rc.rect.y1 = rcBound.y1;
					}
					
					fTemp = rc.rect.y2 - rcBound.y2;
					if ( fTemp > 0 )
					{
						rc.rect.y2 = rcBound.y2;
					}
				}
				
				rc.color = dwTextColor;
				rc.fZ = 0;
				pGFX->SetTexture( 0, 0 );
				pGFX->DrawRects( &rc, 1 );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIConsole::OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState )
{
	if ( !IsVisible() )
		return false;

	if ( !bPressed )
		return false;
	
	//���� �������� ������, �� ������ ������� ���
//	if ( isprint( nAsciiCode ) )
	if ( nAsciiCode >= 32 && ( keyState == E_KEYBOARD_FREE || keyState == E_SHIFT_KEY_DOWN ) )
	{
		szEditString.insert( nCursorPos, 1, nAsciiCode );
		nCursorPos++;

		return true;
	}

	if ( nVirtualKey == VK_TAB && ( keyState == E_KEYBOARD_FREE || keyState == E_SHIFT_KEY_DOWN ) )
	{
		szEditString.insert( nCursorPos, 4, VK_SPACE );
		nCursorPos += 4;
		return true;
	}

	//���� �� �������� ������, �� ������������ �������������� ����������
	switch( nVirtualKey )
	{
	case VK_RETURN:
		nBeginCommand = -1;
		GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_COMMAND, szEditString.c_str(), m_dwColor );
		szEditString = L"";
		nCursorPos = 0;
		if ( nBeginString != 0 )
			nBeginString++;
		break;

	case VK_BACK:
		if ( nCursorPos > 0 )
		{
			szEditString.erase( nCursorPos-1, 1 );
			nCursorPos--;
		}
		break;

	case VK_DELETE:
		if ( nCursorPos < szEditString.size() )
			szEditString.erase( nCursorPos, 1 );
		break;

	case VK_LEFT:
		if ( nCursorPos == 0 )
			break;
		if ( keyState == E_KEYBOARD_FREE )
		{
			//�� ���� ������� �����
			nCursorPos--;
		}
		if ( keyState & E_CTRL_KEY_DOWN )
		{
			//���� ������ crtl � ������� �����, �� ���������� ����� �� ���� �����
			while( nCursorPos > 0 && isspace(szEditString[nCursorPos-1]) )
				nCursorPos--;
			if ( nCursorPos > 0 )
			{
				if ( isalpha(szEditString[nCursorPos-1]) )
					while( nCursorPos > 0 && isalpha(szEditString[nCursorPos-1]) )
						nCursorPos--;
					else
						while( nCursorPos > 0 && !isalpha(szEditString[nCursorPos-1]) )
							nCursorPos--;
			}
		}
		break;

	case VK_RIGHT:
		if ( nCursorPos == szEditString.size() )
			break;
		if ( keyState == E_KEYBOARD_FREE )
		{
			//�� ���� ������� ������
			nCursorPos++;
		}
		else if ( keyState & E_CTRL_KEY_DOWN )
		{
			//���� ������ crtl � ������� ������, �� ���������� ������ �� ���� �����
			if ( nCursorPos < szEditString.size() )
			{
				if ( isalpha(szEditString[nCursorPos]) )
					while( nCursorPos < szEditString.size() && isalpha(szEditString[nCursorPos]) )
						nCursorPos++;
					else
						while( nCursorPos < szEditString.size() && !isalpha(szEditString[nCursorPos]) )
							nCursorPos++;
			}
			
			while( nCursorPos < szEditString.size() && isspace(szEditString[nCursorPos]) )
				nCursorPos++;
		}
		break;

	case VK_UP:
		if ( keyState == E_KEYBOARD_FREE )
		{
			if ( nBeginCommand == -1 && !vectorOfCommands.empty() )
			{
				nBeginCommand = vectorOfCommands.size() - 1;
				szEditString = vectorOfCommands[ nBeginCommand ];
				nCursorPos = szEditString.size();
				break;
			}

			if ( nBeginCommand > 0 )
			{
				//������� ������� �� ������� ����
				nBeginCommand--;
				szEditString = vectorOfCommands[ nBeginCommand ];
				nCursorPos = szEditString.size();
			}
		}
		break;

	case VK_DOWN:
		if ( nBeginCommand == vectorOfCommands.size() || nBeginCommand == -1 )
			break;
		
		if ( keyState == E_KEYBOARD_FREE )
		{
			//��������� ���������� �������
			nBeginCommand++;
			if ( nBeginCommand == vectorOfCommands.size() )
			{
				nBeginCommand = -1;
				szEditString = L"";
				nCursorPos = szEditString.size();
			}
			else
			{
				szEditString = vectorOfCommands[ nBeginCommand ];
				nCursorPos = szEditString.size();
			}
		}
		break;
		
	case VK_HOME:
		if ( keyState == E_KEYBOARD_FREE )
		{
			//�� ������ ������
			nCursorPos = 0;
		}
		else if ( keyState == E_CTRL_KEY_DOWN )
		{
			//���������� ������ ����������� ������ ( ����� ������ )
			if ( vectorOfStrings.size() > CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE )
				nBeginString = vectorOfStrings.size() - CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE + MINUS_PAGE_SIZE;
		}
		break;

	case VK_END:
		if ( keyState == E_KEYBOARD_FREE )
		{
			//�� ����� ������
			nCursorPos = szEditString.size();
		}
		else if ( keyState == E_CTRL_KEY_DOWN )
		{
			//���������� ����� ����������� ������ ( ����� ����� )
			nBeginString = 0;
		}
		break;

	case VK_ESCAPE:
		szEditString = L"";
		nBeginCommand = -1;
		nCursorPos = 0;
		break;

	case VK_PRIOR:							//PAGE UP
		if ( nBeginString + CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE < vectorOfStrings.size() )
			nBeginString += CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE - MINUS_PAGE_SIZE;
		break;

	case VK_NEXT:								//PAGE DOWN
		if ( nBeginString - CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE > 0 )
			nBeginString -= CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE - MINUS_PAGE_SIZE;
		else
			nBeginString = 0;
		break;
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIConsole::RegisterCommand( IConsoleCommandHandler *pHandler )
{
	NI_ASSERT_T( pHandler != 0, "NULL POINTER, WTF??" );
	commandsChain.push_back( pHandler );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIConsole::ParseCommand( const std::wstring &szExtCommand )
{
	std::string szCommandString;
	NStr::ToAscii( &szCommandString, szExtCommand );
	NStr::TrimLeft( szCommandString );
	if ( szCommandString.empty() )
		return;
	// check for special commands
	if ( szCommandString[0] == '@' )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_SCRIPT, szCommandString.c_str() + 1 );
		return;
	}
	else if ( szCommandString[0] == '#' )
	{
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_WORLD, szCommandString.c_str() + 1 );
		return;
	}
// CRAP{ for debug
	else if ( szCommandString == "PauseNet()" )
	{
		if ( GetSingleton<ITransceiver>()->GetInGameNetDriver() )
			GetSingleton<ITransceiver>()->GetInGameNetDriver()->PauseNet();

		return;
	}
	else if ( szCommandString == "UnpauseNet()" )
	{
		if ( GetSingleton<ITransceiver>()->GetInGameNetDriver() )
			GetSingleton<ITransceiver>()->GetInGameNetDriver()->UnpauseNet();

		return;
	}
	else if ( szCommandString.substr( 0, 7 ) == "SetLag(" )
	{
		if ( GetSingleton<ITransceiver>()->GetInGameNetDriver() )
		{
			std::string szNumber = szCommandString.substr( 7 );
			NStr::TrimBoth( szNumber );

			int i = 0;
			int nLag = 0;
			while ( i < szNumber.size() && szNumber[i] >= '0' && szNumber[i] <= '9' )
			{
				nLag = nLag * 10 + szNumber[i] - '0';
				++i;
			}

			GetSingleton<ITransceiver>()->GetInGameNetDriver()->SetLag( nLag );
		}

		return;
	}
	else if ( szCommandString == "CallAssert()" )
	{
		throw 1;
	}
// CRAP}
	else
	{
		for ( CCommandsList::iterator it = commandsChain.begin(); it != commandsChain.end(); ++it )
		{
			if ( (*it)->HandleCommand( szCommandString.c_str() ) == true )
				return;
		}
	}

	//��������, ����� ��� ������� ���������������� ��� ���������� � ������� �������, ����� �������� ��
	int nPos = szCommandString.find( '(' );
	std::string szFunctionName;
	if ( nPos > 0 )
	{
		while ( isspace(szCommandString[nPos]) )
			nPos--;
		szFunctionName = szCommandString.substr( 0, nPos );
	}
	CConsoleFunctions::iterator findIt = consoleFunctions.find( szFunctionName );
	if ( findIt != consoleFunctions.end() )
	{
		const int oldtop = consoleScript.GetTop();
		consoleScript.DoString( ("return " + szCommandString).c_str() );
		
		const int nReturns = consoleScript.GetTop();
		std::string szAnswer("");
		
		for ( int i = 1; i <= nReturns; ++i )
		{
			Script::Object obj = consoleScript.GetObject( i );
			
			if ( obj.IsNumber() )
				szAnswer += std::string( NStr::Format( "%d", int(obj) ) );
			else if ( const char *pszAnswer = obj.GetString() )
				szAnswer += pszAnswer;
			
			if ( i < nReturns )
				szAnswer += ", ";
		}
		
		consoleScript.SetTop(oldtop);
		if ( !szAnswer.empty() )
			GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, szAnswer.c_str(), 0xff00ff00 );
	}
	else
	{
		// unknown command - report it
		const std::string szError = std::string( "Unknown command: " ) + szCommandString;
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, szError.c_str(), 0xffff0000 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//������ ���������� �������
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SetIGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetIGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "SetIGlobalVar: the second parameter is not a number", return 0 );
	
	SetGlobalVar( script.GetObject( 1 ), int(script.GetObject( 2 )) );
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SetFGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetFGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "SetFGlobalVar: the second parameter is not a number", return 0 );
	
	SetGlobalVar( script.GetObject( 1 ), float(script.GetObject( 2 )) );
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SetSGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "SetSGlobalVar: the first parameter is not a string", return 0 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "SetSGlobalVar: the second parameter is not a number", return 0 );
	
	SetGlobalVar( script.GetObject( 1 ), (const char *)(script.GetObject( 2 )) );
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetIGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetIGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsNumber( 2 ), "GetIGlobalVar: the second parameter is not a number", return 1 );
	
	script.PushNumber( GetGlobalVar( script.GetObject(1), int(script.GetObject(2)) ) );
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetFGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetFGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "GetFGlobalVar: the second parameter is not a number", return 1 );
	
	script.PushNumber( GetGlobalVar( script.GetObject(1), float(script.GetObject(2)) ) );
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int GetSGlobalVar( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetSGlobalVar: the first parameter is not a string", return 1 );
	NI_ASSERT_SLOW_TF( script.IsString( 2 ), "GetSGlobalVar: the second parameter is not a string", return 1 );
	
	script.PushString( GetGlobalVar( script.GetObject(1), (const char*)(script.GetObject(2)) ) );
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int Exec( struct lua_State *state )
{
	Script script( state );
	
	NI_ASSERT_SLOW_TF( script.IsString( 1 ), "GetSGlobalVar: the first parameter is not a string", return 1 );
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	std::string szFileName = pStorage->GetName();
	int nPos = szFileName.rfind( '\\' );
	if ( nPos != std::string::npos )
		szFileName = szFileName.substr( 0, nPos );
	nPos = szFileName.rfind( '\\' );
	if ( nPos != std::string::npos )
		szFileName = szFileName.substr( 0, nPos + 1 );
	szFileName += script.GetObject( 1 );
//	IStream *pStream = OpenFileStream( szFileName, STREAM_ACCESS_READ );
	
	FILE *pFile = nullptr;
	if ( fopen_s( &pFile, szFileName.c_str(), "r" ) != 0 || !pFile )
	{
		szFileName = "Error opening file: " + szFileName;
		script.PushString( szFileName.c_str() );
		return 1;
	}

	const int STRING_SIZE = 1024;
	char szCur[STRING_SIZE];
	while ( fgets( szCur, STRING_SIZE, pFile ) != 0 )
	{
		szCur[ strlen( szCur ) - 1 ] = '\0';
		GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_COMMAND, szCur, 0xff0000ff );
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Script::SRegFunction reglist[] =
{
	{ "SetIGlobalVar"	,	SetIGlobalVar },
	{ "SetFGlobalVar"	,	SetFGlobalVar },
	{ "SetSGlobalVar"	,	SetSGlobalVar },
	{ "GetIGlobalVar"	,	GetIGlobalVar },
	{ "GetFGlobalVar"	,	GetFGlobalVar },
	{ "GetSGlobalVar"	,	GetSGlobalVar },
	{ "Exec"					, Exec					},
	{ 0, 0 },
};
const int nRegListSize = sizeof( reglist ) / sizeof( reglist[0] ) - 1;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIConsole::InitConsoleScript()
{
	// register global script functions
	consoleScript.Clear();
	consoleScript.Init();
	consoleScript.Register( reglist );
}
////////////////////////////////////////////////////////////////////////////////
bool CUIConsole::RunScriptFile( const std::string &szScriptFileName )
{
	// read and execute script
	if ( (szScriptFileName != "") && !szScriptFileName.empty() )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (szScriptFileName + ".lua").c_str(), STREAM_ACCESS_READ );
		if ( pStream )
		{
			const int nSize = pStream->GetSize();
			// +10 �� ������ ������
			std::vector<char> buffer( nSize + 10 );
			pStream->Read( &(buffer[0]), nSize );
			return !( consoleScript.DoBuffer( &(buffer[0]), nSize, "Script" ) );
		}
		else
			return false;
	}
	else
		return false;
}
////////////////////////////////////////////////////////////////////////////////
CUIConsole::CUIConsole() : dwLastOpenTime( 0 ), dwLastCloseTime( 0 ), nCursorPos( 0 ),
	dwLastCursorAnimatedTime( 0 ), bAnimation( false ), nBeginString( 0 ), nBeginCommand( -1 ),
	m_dwColor( 0xFFFFFFFF ), bShowCursor( true )
{
	CSimpleWindow::ShowWindow( UI_SW_HIDE );
	
	for ( int i=0; i<nRegListSize; i++ )
	{
		consoleFunctions[ reglist[i].name ] = 1;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
