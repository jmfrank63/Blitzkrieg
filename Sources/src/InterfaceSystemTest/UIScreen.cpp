
#include "stdafx.h"
#include "UIScreen.h"

#include "..\GFX\GFX.h"
#include "MessageReactions.h"
#include "WindowConsole.h"
IWindow * B2UITest()
{
	CScreen *pScreen = new CScreen;
	pScreen->Load( "test_screen" );
	pScreen->Reposition( GetSingleton<IGFX>()->GetScreenRect() );
	pScreen->Init();
	return (IWindow*)pScreen;
}
CScreen::CStates::CStates( const SUICommandSequence &seq, const std::string &_szCmdName, const bool _bReversable ) 
	: szCmdName( _szCmdName ), nCurIndex( 0 ), bForward( true ), bReversable( _bReversable ), bEnd( true ) 
{
	Reserve( seq.cmds.size() );
	for ( SUICommandSequence::CCmds::const_reverse_iterator it	= seq.cmds.rbegin(); it != seq.cmds.rend(); ++it )
		Add( *it );
	CheckEnd();
}
int CScreen::CStates::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
void CScreen::CStates::NotifyParent()
{
	if ( pNotifySink )
		pNotifySink->NotifyStateSequenceFinished();
}
void CScreen::CStates::CheckEnd()
{
	if ( bForward && nCurIndex >= states.size() )
	{
		nCurIndex = states.size() -1;
		bEnd = true;
	}
	else if ( !bForward && nCurIndex < 0 )
	{
		nCurIndex = 0;
		bEnd = true;
	}
	else if ( !states.empty() )
		bEnd = false;
}
void CScreen::CStates::Add( const SUIStateCommand &cmd )
{
	states.push_back( SUIState( cmd ) );
	CheckEnd();
}
void CScreen::CStates::Advance() 
{ 
	NI_ASSERT_T( states.size() != 0, "no states" );
	bForward ? ++nCurIndex : --nCurIndex;
	CheckEnd();
}
const bool CScreen::CStates::IsToBeDeleted() const 
{ 
	return IsEnd() &&	(!bForward || !bReversable );
}
void CScreen::CStates::Reverse() 
{
	bForward = !bForward;
	CheckEnd();
	if ( !IsEnd() )													// notify every effect about reverce
	{
		for ( int i = 0; i < states.size(); ++i )
			if ( states[i].pEffect )
				states[i].pEffect->Reverse();
	}
}
void CScreen::CStates::Segment( const NTimer::STime timeDiff, class CScreen *pScreen )
{
	NI_ASSERT_T( !IsEnd(), "ended states, but segment called" );
	
	while( !IsEnd() )
	{
		if ( GetCur().pEffect == 0 ) // command isn't launched yet
		{
			IUIEffector *pEff = CreateEffect( GetCur().cmd, pScreen );
			if ( 0 == pEff )
				Advance();
			else
				GetCur().pEffect = pEff;
		}
		else if ( GetCur().pEffect->IsFinished() ) // effect is finished, advance to next command
			Advance();
		else
		{
			GetCur().pEffect->Segment( timeDiff, pScreen );
			break;
		}
	}
}
IUIEffector *CScreen::CStates::CreateEffect( const SUIStateCommand &cmd, class CScreen *pScreen )
{
	CPtr<IEffectorCommand> pCmd;
	if ( cmd.nCommandID != 0 ) 
	{
		pCmd = CreateObject<IEffectorCommand>( cmd.nCommandID );
		IUIEffector * pEff = pCmd->Configure( cmd, pScreen );
		NI_ASSERT_T( pEff != 0, NStr::Format( "cannot create effector for command id = %i", cmd.nCommandID ) );
		if ( pEff->NeedElement() )
			pEff->SetElement( pScreen->GetChild( cmd.szParam1 ) );
		
		if ( pEff->IsFinished() )
		{
			delete pEff;
			return 0;
		}
		return pEff;
	}
		
	return 0;
}

BASIC_REGISTER_CLASS(CScreen)
int CScreen::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CWindow*>(this) );
	saver.Add( "Commands", &commandSequiences );
	saver.Add( "Reactions", &messageReactions );
	
	if ( saver.IsReading() )
	{
		stateSequiences.clear();
	}
	return 0;
}
CScreen::CScreen( int TEST)
: messageReactions( TEST )
{
	CWindow::Init( TEST );
	SUICommandSequence seq;
	seq.cmds.push_back( 1 );
	commandSequiences["on_enter_child1"] = seq;
}
int CScreen::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
void CScreen::Load( const std::string &szResourceName )
{
	CWindow::InitStatic();

	/*
	{
		CPtr<IDataStream> pStream = CreateFileStream( "c:\\a7\\Data\\test_screen.xml" , STREAM_ACCESS_WRITE );
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
		this->operator&( *pDT );
	}
*/

	{
		CPtr<IDataStream> pStream;
		pStream = GetSingleton<IDataStorage>()->OpenStream( (szResourceName + ".xml").c_str(), STREAM_ACCESS_READ );
		
		NI_ASSERT_T( pStream != 0, NStr::Format( "cannot open stream \"%s\"", szResourceName.c_str() ));
		if ( pStream )
		{
			CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
			
			this->operator&( *pDT );
		}
	}
	

	{
		CPtr<IDataStream> pStream;
		pStream = GetSingleton<IDataStorage>()->OpenStream( "console.xml", STREAM_ACCESS_READ );
		NI_ASSERT_T( pStream != 0, NStr::Format( "cannot open stream \"%s\"", "console.xml" ));
		
		CWindowConsole *pC = new CWindowConsole;

		if ( pStream )
		{
			CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
			pC->operator&( *pDT );
		}

		pC->RegisterEffects( this );
		AddChild( pC );
	}
}
void CScreen::RegisterEffect( const std::string &szEffect, const SUICommandSequence &cmds )
{
	commandSequiences[szEffect] = cmds;
}
void CScreen::RegisterReaction( const std::string &szReactionKey, interface IMessageReactionB2 *pReaction )
{
	messageReactions.Register( szReactionKey, pReaction );
}
void CScreen::Segment( const NTimer::STime timeDiff )
{
	ProcessStateSequiences( timeDiff );
	/*
	{
	SGameMessage msg;
	msg.nParam = 0;
	const std::string szTestMessage = "USER_ACTION_ATTACK";
	

	CObservers::iterator it = observers.find( szTestMessage );
	if ( it != observers.end() )
	{
		for ( CObserversList::iterator obs = it->second.begin(); obs != it->second.end(); ++obs )
			(*obs)->Execute( msg );
	}
	}
	*/
	/*
	{
	SBUIMessage msg;
	msg.szMessageID = "UI_SHOW_WINDOW";
	ProcessMessage( msg );
	}*/
	for ( CSegmentObjs::iterator it = segmentObjs.begin(); it != segmentObjs.end(); ++it )
		(*it)->Segment( timeDiff );
}
void CScreen::ProcessStateSequiences( const NTimer::STime timeDiff )
{
	for ( CStateSequiences::iterator ss = stateSequiences.begin(); ss != stateSequiences.end(); )
	{
		CStates &states = *ss;
		if ( states.IsToBeDeleted() )
		{
			ss = stateSequiences.erase( ss );
		}
		else 
		{
			if ( !states.IsEnd() )
				states.Segment( timeDiff, this );
			++ss;
		}
	}
}
void CScreen::RegisterToSegment( interface IWindow *pWnd, const bool bRegister )
{
	if ( bRegister )
		segmentObjs.push_back( pWnd );
	else
		segmentObjs.remove( pWnd );
}
void CScreen::RunStateCommandSequience( const std::string &szCmdSeq, CWindow *pNotifySink, const bool bForward )
{
	if ( szCmdSeq.empty() ) return;

	NStr::DebugTrace( NStr::Format( "RunStateCommandSequience: %s\n", szCmdSeq.c_str() ) );

	CCommandSequiences::const_iterator it = commandSequiences.find( szCmdSeq );
	NI_ASSERT_T( it != commandSequiences.end(), NStr::Format( "unknown commad sequience number %s", szCmdSeq.c_str() ) );
	
	if ( bForward )
	{
		bool bFound = false;
		for ( CStateSequiences::iterator it = stateSequiences.begin(); it != stateSequiences.end(); ++it )
		{
			if ( szCmdSeq == it->GetName() ) 
			{
				bFound = true;
				if ( !it->IsForward() )							// reversed one found
				{
					it->Reverse();
				}
			}
		}
		if ( !bFound && it != commandSequiences.end() )
		{
			const SUICommandSequence &seq = it->second;
			stateSequiences.push_front( CStates( seq, szCmdSeq, seq.bReversable ) );
			CStateSequiences::iterator ss = stateSequiences.begin();
			ss->SetNotifySink( pNotifySink );
		}
	}
	else
	{
		for ( CStateSequiences::iterator it = stateSequiences.begin(); it != stateSequiences.end(); ++it )
		{
			if ( szCmdSeq == it->GetName() ) 
				it->Reverse();
		}
	}
}
void CScreen::SetWindowText( const std::string &szWindowName, const std::wstring &szText )
{
	CWindow *pChild = GetDeepChild( szWindowName );
	NI_ASSERT_T( pChild != 0, NStr::Format( "cannot find deeper child \"%s\"", szWindowName.c_str() ) );
	if ( pChild )
	{
		ITextView *pText = dynamic_cast<ITextView*>( pChild );
		NI_ASSERT_T( pText != 0, NStr::Format( "attemt to set text \"%s\" to window that doesn't have text \"%d\"", NStr::ToAscii(szText).c_str(), szWindowName.c_str() ) );
		if ( pText )
			pText->SetText( szText );
	}
}
void CScreen::RunReaction( const std::string &szReactionName )
{
	messageReactions.Execute( szReactionName, this );

}
