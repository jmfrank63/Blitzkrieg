#ifndef _interfaceBase_h_included_
#define _interfaceBase_h_included_


#include "DeepCPtrCopy.h"

//CRAP{ FOR OBSERVERS TO WORK
// basic interface for GameMessage observer
interface IGMObserver : public IRefCount
{
	virtual void Execute( const SGameMessage &msg ) = 0;
};
// member-function game message observer
template <typename TObj, typename TMemFun>
class CGMMemFunObserver : public IGMObserver
{
	OBJECT_MINIMAL_METHODS( CGMMemFunObserver );
	CPtr<TObj> pObj;											// object to call member function with
	TMemFun pfnMemFun;										// member function to call
public:
	CGMMemFunObserver( TObj *_pObj, TMemFun _pfnMemFun ) : pObj( _pObj ), pfnMemFun( _pfnMemFun ) {}
	//
	void Execute( const SGameMessage &msg ) { ((*pObj).*pfnMemFun)( msg ); }
};
// raw function game message observer
template <typename TFun>
class CGMFunObserver : public IGMObserver
{
	OBJECT_MINIMAL_METHODS( CGMFunObserver );
	TFun fun;															// function to call
public:
	CGMFunObserver( const TFun &_fun ) : fun( _fun ) {}
	//
	void Execute( const SGameMessage &msg ) { fun( msg ); }
};
// helper functions to create GameMessage observers
template <typename TFun>
IGMObserver* MakeGMObserver( const TFun &fun )
{
	return new CGMFunObserver<TFun>( fun );
}
template <typename TObj, typename TMemFun>
IGMObserver* MakeGMObserver( TObj *_pObj, TMemFun _pfnMemFun )
{
	return new CGMMemFunObserver<TObj, TMemFun>( _pObj, _pfnMemFun );
}


interface IGameEvent : public IRefCount
{
	virtual void STDCALL RaiseEvent( const SGameMessage &msg ) = 0;
};
class CGameEvent : public IGameEvent
{
	OBJECT_COMPLETE_METHODS( CGameEvent );
	//
	typedef std::list< CPtr<IGMObserver> > CObserversList;
	CObserversList observers;							// main observers pool
	//
public:
	void AddObserver( IGMObserver *pObserver )
	{
		observers.push_back( pObserver );
	}
	virtual void STDCALL RaiseEvent( const SGameMessage &msg )
	{
		for ( CObserversList::iterator it = observers.begin(); it != observers.end(); ++it )
			(*it)->Execute( msg );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMessageQueue : public IRefCount
{
	OBJECT_COMPETE_METHODS( CMessageQueue );
	//
	std::list<SGameMessage> messages;
	typedef std::unordered_map<std::string, CObj<CGameEvent> > CEventsMap;
	CEventsMap events;
public:
	void STDCALL AddMessage( const SGameMessage &msg )
	{
		messages.push_back( msg );
	}
	// get next message from queue. returns false if queue are empty
	bool STDCALL GetMessage( SGameMessage *pMsg )
	{
		if ( messages.empty() ) 
			return false;
		*pMsg = messages.front();
		messages.pop_front();
		return true;
	}
	// clear all messages
	void STDCALL Clear()
	{
		messages.clear();
	}
	void AddObserver( const std::string &szEventName, IGMObserver *pObserver )
	{
		CEventsMap::iterator it = events.find( szEventName );
		if ( events.end() == it )
		{
			events[szEventName] = new CGameEvent;
			it = events.find( szEventName );
		}
		it->second->AddObserver( pObserver ); 
	}
	//
	IGameEvent* STDCALL GetEvent( const std::string &szName )
	{
		CEventsMap::iterator pos = events.find( szName );
		if ( pos == events.end() ) 
		{
			CGameEvent *pEvent = new CGameEvent();
			events[szName] = pEvent;
			return pEvent;
		}
		else
			return pos->second;
	}
};
//CRAP}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// generic window functionality
interface IWindow : public IRefCount
{
	// input work
	virtual void STDCALL OnButtonDown( const CVec2 &vPos, const int nButton ) = 0;
	virtual void STDCALL OnButtonUp( const CVec2 &vPos, const int nButton ) = 0; 
	virtual void STDCALL OnButtonDblClk( const CVec2 &vPos, const int nButton ) = 0;
	virtual void STDCALL OnChar( const wchar_t chr ) = 0;
	virtual void STDCALL OnMouseMove( const CVec2 &vPos, const int mouseState ) = 0;
	// pick
	virtual IWindow* STDCALL Pick( const CVec2 &vPos ) = 0;
	//get manipulator for editor functionality
	virtual IManipulator* STDCALL GetManipulator() = 0;
	// help context
	virtual interface IText* STDCALL GetHelpContext() = 0;
	// DRAWING
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor ) = 0;
	// dynamic behaviour
	virtual void STDCALL Segment( const NTimer::STime timeDiff ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// specific to button functionality
interface IButton 
{
};
// edit line
interface IEditLine
{
	virtual void STDCALL SetText( const wchar_t *pszText ) = 0;
	virtual void STDCALL SetCursor( const int nPos ) = 0;
	virtual void STDCALL SetSelection( const int nBegin, const int nEnd ) = 0;
	virtual const wchar_t * STDCALL GetText() const = 0;
};
// console
interface IConsole
{
};
// window that has text must support this
interface ITextView
{
	// return true if height of window is updated
	virtual bool STDCALL SetText( const std::wstring &szText ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ISlider's owner extend this interface to be notified about slider events
interface ISliderNotify
{
	// fPosition 0..1
	virtual void STDCALL SliderPosition( const float fPosition ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface ISlider
{
	virtual void STDCALL SetRange( const float fMin, const float fMax, const float fPageSize ) = 0;
	virtual void STDCALL GetRange( int *pMax, int *pMin ) const = 0;
	virtual void STDCALL SetPos( const int nCur ) = 0;
	virtual int STDCALL GetPos() const = 0;
	virtual void STDCALL SetNotifySink( interface ISliderNotify *pNotify ) = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// specific screen funcitonality
interface IScreen
{
	virtual void STDCALL Load( const std::string &szResourceName ) = 0;
	virtual void STDCALL RegisterObserver( interface IGMObserver *pObserver, const std::string &szMessage ) = 0;
	virtual void STDCALL UnregisterObserver( interface IGMObserver *pObserver, const std::string &szMessage ) = 0;
	// set child window text
	virtual void STDCALL SetWindowText( const std::string &szWindowName, const std::wstring &szText ) = 0;
	virtual void STDCALL RunReaction( const std::string &szReactionName ) = 0;
	virtual void STDCALL RunStateCommandSequience( const std::string &szCmdSeq, class CWindow *pNotifySink, const bool bForward ) = 0;
	// 
	virtual void STDCALL RegisterEffect( const std::string &szEffect, const struct SUICommandSequence &cmds ) = 0;
	virtual void STDCALL RegisterReaction( const std::string &szReactionKey, interface IMessageReactionB2 *pReaction ) = 0;
};

//CRAP{ FOR OBSERVERS TO START WORK
class CScreen;
template <typename TObj, typename TMemFun>
class CObserverRegistrator
{
	IScreen *pScreen; 
	CPtr<IGMObserver> pObserver;
	std::string szMessage;
public:
	CObserverRegistrator() : pScreen ( 0 ) {  }
	void Init( IScreen *_pScreen, TObj *_pObj, TMemFun _pfnMemFun, const std::string &_szMessage )
	{
		//CRAP{ MUST ALLOW MULTIPLE init without registering second observer
		szMessage = _szMessage;
		pScreen = _pScreen;
		pObserver = MakeGMObserver( _pObj, _pfnMemFun );
		pScreen->RegisterObserver( pObserver, szMessage );
		//CRAP}
	}
	~CObserverRegistrator()
	{
		if ( pScreen && pObserver )
			pScreen->UnregisterObserver( pObserver, szMessage );
	}
};
//CRAP}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// effect on interface
interface IUIEffector : public IRefCount
{
	// effect may want to calculate something 
	virtual void STDCALL Segment( const NTimer::STime timeDiff, interface IScreen *pScreen ) = 0;
	// effect may want to draw somwthing
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor ) = 0;
	// does this effect need UI element
	virtual bool STDCALL NeedElement() const = 0;
	// if need element, then supplied
	virtual void STDCALL SetElement( class CWindow *pElement ) = 0;
	// effect is finished
	virtual bool STDCALL IsFinished() const = 0;
	// configure effect with commad
	virtual void STDCALL Configure( const struct SUIStateCommand &cmd, interface IScreen *pScreen ) = 0;
	// reversed effect ( that effect + effect->Reverse() = NULL );
	virtual void STDCALL Reverse() = 0;
};
/////////////////////////////////////////////////////////////////////////////
// all backgrounds must instance this interface
interface IBackground : public IRefCount
{
	DECLARE_CLONABLE_INTERFACE;
	virtual void STDCALL Visit( interface ISceneVisitor * pVisitor ) = 0;
	virtual int STDCALL operator&( interface IDataTree &ss ) = 0;
	virtual int STDCALL operator&( interface IStructureSaver &ss ) = 0;
	
	// notify background about window position and size change
	virtual void STDCALL SetPos( const CVec2 &vPos, const CVec2 &vSize ) = 0;
};
#endif //_interfaceBase_h_included_




















