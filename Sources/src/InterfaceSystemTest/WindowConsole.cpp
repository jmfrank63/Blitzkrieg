
#include "stdafx.h"
#include "WindowConsole.h"
#include "IUIInternal.h"
#include "UIScreen.h"
#include "..\GFX\GFX.h"
#include "WindowEditLine.h"
#include "MessageReaction.h"

static const int CONSOLE_HEIGHT = 240;			//Высота консоли в пикселах
static const int TEXT_LEFT_SPACE = 20;			//Отступ от левого края экрана до текста в консоли
static const int TEXT_VERTICAL_SIZE = 20;		//Размер шрифта по вертикали
static const int MINUS_PAGE_SIZE = 5;				//Специальная константа отступа для PgUp PgDown,
static const int CURSOR_ANIMATION_TIME = 400;		//период переключения курсора
static const WCHAR szPrefix[] = L">>";
IMPLEMENT_CLONABLE(CWindowConsole);
int CWindowConsole::SColorString::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szString );
	saver.Add( 2, &dwColor );
	return 0;
}

int CWindowConsole::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
int CWindowConsole::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CWindow*>( this ) );
	saver.Add( "TextColor", &dwColor );
	return 0;
}
void CWindowConsole::RegisterEffects( interface IScreen *pScreen )
{
	SUICommandSequence cmdShow;
	cmdShow.cmds.push_back( SUIStateCommand(EUISM_PS_MOVETO_COMMAND, "Console", "", CVec2( 0, CONSOLE_HEIGHT ), MAKELONG(1000,0)) );
	cmdShow.cmds.push_back( SUIStateCommand(EUISM_RUN_REACTION_COMMAND, "MakeConsoleVisible", "MakeConsoleInvisible", VNULL2, 0) );
	pScreen->RegisterEffect( "ShowConsole", cmdShow );

	CMessageReactionB2 * pReaction = new CMessageReactionB2;
	pReaction->AddCommonBefore( new CARSendMessage("UI_SHOW_WINDOW",GetName(), true) );
	pScreen->RegisterReaction( "MakeConsoleVisible", pReaction );

	pReaction = new CMessageReactionB2;
	pReaction->AddCommonBefore( new CARSendMessage("UI_SHOW_WINDOW",GetName(), false) );
	pScreen->RegisterReaction( "MakeConsoleInvisible", pReaction );
}
void CWindowConsole::OnShowConsole( const struct SGameMessage &msg )
{
	if ( bCanLaunchEffect )
	{
		if ( !IsVisible() )
			GetScreen()->RunStateCommandSequience( "ShowConsole", this, true );
		else 
			GetScreen()->RunStateCommandSequience( "ShowConsole", this, false );
	}
}
void CWindowConsole::NotifyStateSequenceFinished()
{
	bCanLaunchEffect = true;
}
void CWindowConsole::Reposition( const CTRect<float> &rcParent )
{

	SetPlacement( 0, -CONSOLE_HEIGHT, rcParent.Width(), CONSOLE_HEIGHT, EWPF_ALL );

	pEditLine = dynamic_cast<CWindowEditLine*>( GetChild( "ConsoleEditLine" ) );
	NI_ASSERT_T( pEditLine != 0, "console must have edit line" );

	if ( pEditLine )
		pEditLine->SetPlacement( 0, 0, rcParent.Width(), 0, EWPF_SIZE_X );

	CWindow::Reposition( rcParent );
}
void CWindowConsole::Segment( const NTimer::STime timeDiff )
{
	IConsoleBuffer *pBuffer = GetSingleton<IConsoleBuffer>();
	DWORD color = 0;
	while ( const wchar_t *pszString = pBuffer->Read(CONSOLE_STREAM_CONSOLE, &color) ) 
		vectorOfStrings.push_back( SColorString(pszString, color) );
	while ( const wchar_t *pszString = pBuffer->Read(CONSOLE_STREAM_COMMAND, &color) ) 
	{
		std::wstring szString = pszString;
		vectorOfCommands.push_back( szString );
		vectorOfStrings.push_back( SColorString(szString, color) );
		ParseCommand( szString );

		while ( const wchar_t *pszString = pBuffer->Read(CONSOLE_STREAM_CONSOLE, &color) ) 
			vectorOfStrings.push_back( SColorString(pszString, color) );
	}
}
void CWindowConsole::Visit( interface ISceneVisitor *pVisitor )
{
	CWindow::Visit( pVisitor );

	if ( IsVisible() )
	{
		CTRect<float> wndRect;
		FillWindowRect( &wndRect );
		
		int nCurrentY = wndRect.y2 - 2 * TEXT_VERTICAL_SIZE;
		if ( nBeginString != 0 )
		{
			nCurrentY -= TEXT_VERTICAL_SIZE;
			GetSingleton<IGFX>()->DrawString( L"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^", 
				TEXT_LEFT_SPACE, nCurrentY, 0xFFFF0000 );
		}
		nCurrentY -= TEXT_VERTICAL_SIZE;

		int nSize = vectorOfStrings.size();
		for ( int i = nBeginString; i < nSize; ++i )
		{
			GetSingleton<IGFX>()->DrawString( vectorOfStrings[nSize - i - 1].szString.c_str(), 
																				TEXT_LEFT_SPACE, nCurrentY, vectorOfStrings[nSize - i - 1].dwColor );
			nCurrentY -= TEXT_VERTICAL_SIZE;
			if ( nCurrentY < 0 )
				break;
		}
	}
}
void CWindowConsole::RegisteMessageSinks()
{
}
void CWindowConsole::UnRegisteMessageSinks()
{
}
void CWindowConsole::OnKeyUp( const struct SGameMessage &msg )
{
	if ( nBeginCommand == -1 && !vectorOfCommands.empty() )
	{
		nBeginCommand = vectorOfCommands.size() - 1;
		pEditLine->SetText( vectorOfCommands[nBeginCommand].c_str() );
	}

	if ( nBeginCommand > 0 )
	{
		nBeginCommand--;
		pEditLine->SetText( vectorOfCommands[nBeginCommand].c_str() );
	}
}
void CWindowConsole::OnKeyDown( const struct SGameMessage &msg )
{
	if ( nBeginCommand == vectorOfCommands.size() || nBeginCommand == -1 )
			return;
		
	nBeginCommand++;
	if ( nBeginCommand == vectorOfCommands.size() )
	{
		nBeginCommand = -1;
		pEditLine->SetText( L"" );
	}
	else
	{
		pEditLine->SetText( vectorOfCommands[nBeginCommand].c_str() );
	}
}
void CWindowConsole::OnCtrlHome( const struct SGameMessage &msg )
{
	if ( vectorOfStrings.size() > CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE )
		nBeginString = vectorOfStrings.size() - CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE + MINUS_PAGE_SIZE;
}
void CWindowConsole::OnCtrlEnd( const struct SGameMessage &msg )
{
	nBeginString = 0;
}
void CWindowConsole::OnPgUp( const struct SGameMessage &msg )
{
	if ( nBeginString + CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE < vectorOfStrings.size() )
		nBeginString += CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE - MINUS_PAGE_SIZE;
}
void CWindowConsole::OnPgDn( const struct SGameMessage &msg )
{
	if ( nBeginString - CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE > 0 )
		nBeginString -= CONSOLE_HEIGHT / TEXT_VERTICAL_SIZE - MINUS_PAGE_SIZE;
	else
		nBeginString = 0;
}
void CWindowConsole::OnReturn( const struct SGameMessage &msg )
{
	nBeginCommand = -1;
	GetSingleton<IConsoleBuffer>()->Write( CONSOLE_STREAM_COMMAND, pEditLine->GetText(), dwColor );
	if ( nBeginString != 0 )
		nBeginString++;

}
void CWindowConsole::ParseCommand( const std::wstring &szExtCommand )
{
	std::string szCommandString;
	NStr::ToAscii( &szCommandString, szExtCommand );
	NStr::TrimLeft( szCommandString );
	if ( szCommandString.empty() )
		return;
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

	const std::string szError = std::string( "Unknown command: " ) + szCommandString;
	GetSingleton<IConsoleBuffer>()->WriteASCII( CONSOLE_STREAM_CONSOLE, szError.c_str(), 0xffff0000 );
}
