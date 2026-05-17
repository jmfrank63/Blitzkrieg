// UIScreen.h: interface for the CScreen class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_UISCREEN_H__B9599715_34A7_477E_9D09_8DE9B2953C08__INCLUDED_)
#define AFX_UISCREEN_H__B9599715_34A7_477E_9D09_8DE9B2953C08__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Window.h"
#include "IUIInternal.h"
#include "MessageReactions.h"
//////////////////////////////////////////////////////////////////////
class CScreen : public CWindow, public IScreen  
{
	DECLARE_SERIALIZE;
	OBJECT_COMPLETE_METHODS(CScreen);
	
	// UIScreen recieve command sequience, produce states sequience
	// and track execution of states. when Effect is finished or
	// no effect was produced by command, state sequience moves to next state
	struct SUIState
	{
		SUIStateCommand cmd;										// cmd that creates this state
		CPtr<IUIEffector> pEffect;							// effect that was created by the command
																						// null if no effect.
		CPtr<CWindow> pCommandParent;						// window that must be notified after 
		SUIState( const SUIStateCommand &_cmd ) : cmd( _cmd ) {  }
		int operator&( IStructureSaver &ss )
		{
			//CRAP{ TO DO
			NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
			return 0;
			//CRAP}
		}
	};
	class CStates 
	{
		DECLARE_SERIALIZE;
		std::vector<SUIState> states;
		int nCurIndex;												// currently running effect
		bool bForward;												// effect direction 
		bool bEnd;														// all effects are finished
		CPtr<CWindow> pNotifySink;						// window that must be notified after
		bool bReversable;												// effect can be undone
		std::string szCmdName;
		
		void CheckEnd();
		IUIEffector *CreateEffect( const SUIStateCommand &cmd, class CScreen *pScreen );
		SUIState &GetCur() { return states[nCurIndex]; }
		void Advance();
		void NotifyParent();
		void Reserve( const int nSize ) { states.reserve( nSize ); }
		void Add( const SUIStateCommand &cmd );
	public:
		CStates() : nCurIndex( 0 ), bForward( true ), bEnd( true ), bReversable( false ) {  }
		CStates( const SUICommandSequence &seq, const std::string &_szCmdName, const bool _bReversable );
		const std::string &GetName() const { return szCmdName; }
		void SetNotifySink( class CWindow *pWindow ) { pNotifySink = pWindow; }
		// effect can be deleted already, all work is done
		const bool IsToBeDeleted() const;
		const bool IsEnd() const { return bEnd; }
		// run effects in reverse direction
		void Reverse();
		void Segment( const NTimer::STime timeDiff, class CScreen *pScreen );
		bool IsReversable() const { return bReversable; }
		const bool IsForward() const { return bForward; }
	};
	typedef std::list<CStates> CStateSequiences;
	CStateSequiences stateSequiences;

	// segment calling
	typedef std::list< CPtr<IWindow> > CSegmentObjs;
	CSegmentObjs segmentObjs;

	// message reactions
	CMessageReactions messageReactions;

	//CRAP{ for observers to start work
	typedef std::list< CPtr<IGMObserver> > CObserversList;
	typedef std::unordered_map<std::string, CObserversList> CObservers;
	CObservers observers;
	//CRAP}
	
	// all possible command sequiences possible on this screen
	// when some window generates command sequience it only 
	// sends it's id
	typedef std::unordered_map<std::string, SUICommandSequence> CCommandSequiences;
	CCommandSequiences commandSequiences;
	
	void ProcessStateSequiences( const NTimer::STime timeDiff );
	IUIEffector *RunStateCommand( const SUIStateCommand &cmd );

public:
	CScreen() {  }

	//CRAP{ FOR TEST
	CScreen( int TEST );
	//CRAP}
	virtual int STDCALL operator&( IDataTree &ss );
	void RegisterToSegment( interface IWindow *pWnd, const bool bRegister );
	
	// run state (animation) sequience.
	virtual void STDCALL RunStateCommandSequience( const std::string &szCmdSeq, CWindow *pNotifySink, const bool bForward );
	virtual void STDCALL RunReaction( const std::string &szReactionName );
	virtual void STDCALL Load( const std::string &szResourceName );
	virtual void STDCALL Segment( const NTimer::STime timeDiff );
	virtual void STDCALL SetWindowText( const std::string &szWindowName, const std::wstring &szText );
	virtual void STDCALL RegisterEffect( const std::string &szEffect, const SUICommandSequence &cmds );
	virtual void STDCALL RegisterReaction( const std::string &szReactionKey, interface IMessageReactionB2 *pReaction );

	virtual void STDCALL RegisterObserver( interface IGMObserver *pObserver, const std::string &szMessage )
	{
		observers[szMessage].push_back( pObserver );
	}
	virtual void STDCALL UnregisterObserver( interface IGMObserver *pObserver, const std::string &szMessage )
	{
		CObservers::iterator it = observers.find( szMessage );
		if ( it != observers.end() )
			it->second.remove( pObserver );
	}

	
};
//////////////////////////////////////////////////////////////////////
#endif // !defined(AFX_UISCREEN_H__B9599715_34A7_477E_9D09_8DE9B2953C08__INCLUDED_)
