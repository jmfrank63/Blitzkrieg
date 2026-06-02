
#if !defined(AFX_WINDOWMULTIBKG_H__12210016_22A9_4349_BFA0_D81ED89414D9__INCLUDED_)
#define AFX_WINDOWMULTIBKG_H__12210016_22A9_4349_BFA0_D81ED89414D9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Window.h"
enum ESubState
{
	EST_NORMAL				= 0,
	EST_HIGHLIGHTED		= 1,
	EST_PUSHED				= 2,
	EST_DISABLED			= 3,
	
	_EST_COUNT				= 4,
};
struct SWindowSubState
{	
	DECLARE_SERIALIZE;
public:
	CDCPtr<IBackground> pBackground;									// background for this substats
	SWindowSubState() {  }
	SWindowSubState( int TEST );
	
	virtual int STDCALL operator&( IDataTree &ss );
	void Visit( interface ISceneVisitor * pVisitor );
};
struct SWindowState
{
	DECLARE_SERIALIZE;
public:
	std::vector<SWindowSubState> substates;
	int nSubState;																	// current active substate


	SWindowState( int TEST )
	{
		nSubState = 0;
		substates.push_back( TEST );
		substates.push_back( TEST );
		substates.push_back( TEST );
		substates.push_back( TEST ) ;
	}

	SWindowState() : substates( _EST_COUNT ), nSubState ( -1 ) {  }
	
	virtual int STDCALL operator&( IDataTree &ss );
	void Visit( interface ISceneVisitor * pVisitor )
	{
		if ( !substates.empty() )
			substates[nSubState].Visit( pVisitor );
	}
};
class CWindowMultiBkg : public CWindow  
{
	DECLARE_SERIALIZE;
	std::vector<SWindowState> states;				// all possible states here
	int nState;															// current state
	
protected:
	int GetNStates() const { return states.size(); }
	void SwitchSubState( const ESubState substate );
	void SetNextState();
	int GetState()const { return nState; }
	int GetSubState() const { return states[nState].nSubState; } 
public:

	virtual int STDCALL operator&( IDataTree &ss );
	CWindowMultiBkg();

	CWindowMultiBkg ( int TEST ) 
	{
		CWindow::Init( 0 );
		
		states.push_back( 0 );
		nState = 0;
	}
};
class CWindowMSButton : public CWindowMultiBkg, public IButton
{
	DECLARE_SERIALIZE;
	OBJECT_NORMAL_METHODS( CWindowMSButton );
	DECLARE_CLONABLE_CLASS;

	struct SButtonState
	{
		std::string szOnEnter;								// on mouse enter animation
		std::string szOnPush;									// on push animation
		std::string szOnStateChange;					// button reaction
		bool bOnStateChangeForward;

		std::string szPushMessage;							// message to activate/deactivate push state
		CObserverRegistrator<CWindowMSButton, void (CWindowMSButton::*)( const struct SGameMessage &)>  push;						// observer
		int operator&( IDataTree &ss );
	};

	bool bPressed;
	bool bMouseEntered;

	std::string szEntered;									// effect, that runned upon enter. to enable UNdo
	std::string szPushed;

	std::vector<SButtonState> buttonStates;
	void OnEnter();
	void OnLeave();

	void OnPush();
	void OnRelease();
public:

	virtual ~CWindowMSButton();
	void OnMessagePush( const struct SGameMessage &msg );
	
	virtual void STDCALL OnMouseMove( const CVec2 &vPos, const int nButton );
	virtual void STDCALL OnButtonDown( const CVec2 &vPos, const int nButton );
	virtual void STDCALL OnButtonUp( const CVec2 &vPos, const int nButton );

	virtual int STDCALL operator&( IDataTree &ss );
	virtual void NotifyStateSequenceFinished();
	virtual void STDCALL Init();
};
#endif // !defined(AFX_WINDOWMULTIBKG_H__12210016_22A9_4349_BFA0_D81ED89414D9__INCLUDED_)
