// WindowMultiBkg.cpp: implementation of the CWindowMultiBkg class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "WindowMultiBkg.h"
#include "Background.h"
#include "UIScreen.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// SWindowSubState
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
SWindowSubState::SWindowSubState( int TEST )
{
	pBackground = new CBackgroundPlainTexture;
}
//////////////////////////////////////////////////////////////////////
void SWindowSubState::Visit( interface ISceneVisitor * pVisitor )
{
	if ( pBackground )
		pBackground->Visit( pVisitor );
}
//////////////////////////////////////////////////////////////////////
int SWindowSubState::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Background", &pBackground );
	return 0;
}
//////////////////////////////////////////////////////////////////////
int SWindowSubState::operator&( IStructureSaver &ss )
{
		//CRAP{ TO DO
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
	//CRAP}
}
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// SWindowState
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int SWindowState::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Normal", &substates[EST_NORMAL] );
	saver.Add( "Disabled", &substates[EST_DISABLED] );
	saver.Add( "Highlighted", &substates[EST_HIGHLIGHTED] );
	saver.Add( "Pushed", &substates[EST_PUSHED] );
	return 0;
}
//////////////////////////////////////////////////////////////////////
int SWindowState::operator&( IStructureSaver &ss )
{
		//CRAP{ TO DO
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
	//CRAP}
}
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// CWindowMultiBkg
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
int CWindowMultiBkg::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CWindow*>(this) );
	saver.Add( "States", &states );
	saver.Add( "State", &nState );
	return 0;
}
//////////////////////////////////////////////////////////////////////
int CWindowMultiBkg::operator&( IStructureSaver &ss )
{
		//CRAP{ TO DO
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
	//CRAP}

}
//////////////////////////////////////////////////////////////////////
CWindowMultiBkg::CWindowMultiBkg()
{

}
//////////////////////////////////////////////////////////////////////
void CWindowMultiBkg::SetNextState()
{
	++nState;
	nState %= states.size();
}
//////////////////////////////////////////////////////////////////////
void CWindowMultiBkg::SwitchSubState( const ESubState substate )
{
	SetBackground( states[nState].substates[substate].pBackground );
	states[nState].nSubState = substate;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// CWindowMSButton
//////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
IMPLEMENT_CLONABLE( CWindowMSButton );
////////////////////////////////////////////////////////////////////
int CWindowMSButton::SButtonState::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "OnPush", &szOnPush );
	saver.Add( "OnEnter", &szOnEnter );
	saver.Add( "OnStateChange", &szOnStateChange );
	saver.Add( "OnStateChangeForward", &bOnStateChangeForward );
	saver.Add( "PushMessage", &szPushMessage );
	return 0;
}
////////////////////////////////////////////////////////////////////
int CWindowMSButton::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CWindowMultiBkg*>(this) );
	saver.Add( "ButtonStates", &buttonStates );
	
	if ( saver.IsReading() )
	{
		NI_ASSERT_T( GetNStates() == buttonStates.size(), NStr::Format("button \"%s\" has %i logical and %i graphical states", GetName().c_str(), buttonStates.size(), GetNStates() ) );
		buttonStates.resize( GetNStates() );
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////
int CWindowMSButton::operator&( IStructureSaver &ss )
{
		//CRAP{ TO DO
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
	//CRAP}
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::OnEnter()
{
	if ( bPressed )
	{
		SwitchSubState( EST_PUSHED );
	}
	else
	{
		SwitchSubState( EST_HIGHLIGHTED );
		if ( !bMouseEntered )
		{
			bMouseEntered = true;
			szEntered = buttonStates[GetState()].szOnEnter;
			GetScreen()->RunStateCommandSequience( szEntered, this, true );
		}
	}
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::OnLeave()
{
	if ( bMouseEntered )
	{
		SwitchSubState( EST_NORMAL );
		GetScreen()->RunStateCommandSequience( szEntered, this, false );
		bMouseEntered = false;
	}
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::OnPush()
{
	if ( !bPressed )
	{
		SwitchSubState( EST_PUSHED );
		szPushed = buttonStates[GetState()].szOnPush;
		GetScreen()->RunStateCommandSequience( szPushed, this, true );
		bPressed = true;
	}
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::OnRelease()
{
	if ( bPressed )
	{
		SwitchSubState( EST_NORMAL );
		bPressed = false;
		
		GetScreen()->RunStateCommandSequience( buttonStates[GetState()].szOnStateChange, this, buttonStates[GetState()].bOnStateChangeForward );
		GetScreen()->RunStateCommandSequience( szPushed, this, false );
	}
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::NotifyStateSequenceFinished()
{
}
//////////////////////////////////////////////////////////////////////
CWindowMSButton::~CWindowMSButton()
{
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::Init()
{
	SwitchSubState( EST_NORMAL );
	bPressed = false;
	bMouseEntered = false;

	// register message sinks
	for ( int i = 0; i < buttonStates.size(); ++i )
		if ( !buttonStates[i].szPushMessage.empty() )
			buttonStates[i].push.Init( GetScreen(), this, &CWindowMSButton::OnMessagePush, buttonStates[i].szPushMessage );
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::OnButtonDown( const CVec2 &vPos, const int nButton )
{
	if ( nButton == MSTATE_BUTTON1 )
		OnPush();
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::OnButtonUp( const CVec2 &vPos, const int nButton )
{
	if ( nButton == MSTATE_BUTTON1 )
	{
		if ( GetSubState() == EST_PUSHED )
			OnRelease();
		OnMouseMove( vPos, nButton & ~MSTATE_BUTTON1 );
		SetNextState();
	}
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::OnMessagePush( const struct SGameMessage &msg )
{
	if ( 0 == msg.nParam )
		OnPush();
	else
	{
		OnRelease();
		SetNextState();
	}
}
//////////////////////////////////////////////////////////////////////
void CWindowMSButton::OnMouseMove( const CVec2 &vPos, const int nButton )
{
	if ( IsInside( vPos ) )
		OnEnter();
	else
		OnLeave();
}
