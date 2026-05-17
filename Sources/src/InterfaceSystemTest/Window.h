// Window.h: interface for the CWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WINDOW_H__54783510_EE35_420B_A2EC_19C1C30EA449__INCLUDED_)
#define AFX_WINDOW_H__54783510_EE35_420B_A2EC_19C1C30EA449__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Interface.h"
#include "Heap.h"
interface IBackground;

#include "DeepCPtrCopy.h"
#include "WindowMessageHandle.h"
//////////////////////////////////////////////////////////////////////
struct SWindowCompare
{
	bool operator()( const CDCPtr<CWindow> &o1, const CDCPtr<CWindow> &o2 ) const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EWindowPlacementFlags
{
	EWPF_POS_X					= 1,
	EWPF_POS_Y					= 2,
	EWPF_SIZE_X					= 4,
	EWPF_SIZE_Y					= 8,
	
	EWPF_ALL						= 0xffff,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum EPositionAllign
{
	EPA_LOW_END,							// LEFT OR TOP
	ERA_CENTER,								
	EPA_HIGH_END,							// BOTTOM OR RIGHT
};

enum EMouseStateB2
{
	MSTATE_FREE				= 0,
	MSTATE_BUTTON1		= 1,
	MSTATE_BUTTON2		= 2,
	MSTATE_BUTTON3		= 4,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// base class to all UI windows;
// single background window.
class CWindow : public IWindow
{
	DECLARE_SERIALIZE;
	DECLARE_CLONE_PROHIBITED;
	
	// dynamic data, set during execution
	CDCPtr<IBackground> pBackground;					// may be 0
	
	CNCPtr<CWindow> pParent;									// parent window.
	CNCPtr<CWindow> pFocused;									// child that has keyboard focus
	CNCPtr<CWindow> pModal;										// modal child
	CNCPtr<CWindow> pHighlighted;							// window currently under mouse cursor
	std::vector< CNCPtr<CWindow> > pressed;		// pressed with each mouse button
	CVec2 vScreenPos;
	// end dynamic data

	// BEGIN these loads from data
	typedef CHeap< CDCPtr<CWindow>, SWindowCompare > CDrawOrder;
	CDrawOrder drawOrder;

	typedef std::unordered_set<std::string> CChildren;
	CChildren children;
	

	// message handler
	DECLARE_HANDLE_MAP;
	DECLARE_MESSAGE_HANDLER(ShowWindow);
	DECLARE_MESSAGE_HANDLER(SwitchTextMode);

	std::string szTooltip;									// tooltip keyname
	std::string szName;								// window ID
	bool bVisible;
	int nPriority;
	CVec2 vChildPos;													// coordinates relative to parent & alingnment
	CVec2 vSize;												// size
	EPositionAllign nVerAllign;				//������ ����� �������� (vertical)
	EPositionAllign nHorAllign;									//������ ����� �������� (horisontal)
	// END loads from data
protected:

	CWindow() {  }
	//CRAP{ FOR TEST
	void Init( int TEST );
	//CRAP}

	void SetBackground( IBackground *_pBackground );

	void SetModal( CWindow *pChild );
	bool IsVisible() const;
	void ShowWindow( const bool bShow ) { bVisible = bShow; }
	int GetPriority() const;
	// is point (in screen coordinates) inside control
	bool IsInside( const CVec2 &vPos ) const;
	CWindow* PickInternal( const CVec2 &vPos );
	// fills rect with on-screen coordinates
	void FillWindowRect( CTRect<float> *pRect ) const;
	void RepositionChildren();

	const char* GetPressedName( const int nButton )
	{
		if ( pressed.size() > nButton && pressed[nButton] )
			return pressed[nButton]->GetName().c_str();
		return 0;
	}
public:
	void InitStatic();

	// serializing...
	virtual int STDCALL operator&( IDataTree &ss );

	virtual void STDCALL Reposition( const CTRect<float> &parentRect );
	virtual void STDCALL Init();
	// window may want to be notified about finish state sequience, that it launched
	virtual void NotifyStateSequenceFinished() { }

	// placement flags = OR of number EWindowPlacementFlags
	void GetPlacement( int *pX, int *pY, int *pSizeX, int *pSizeY ) const;
	void SetPlacement( int x, int y, int sizeX, int sizeY, const DWORD flags );
	class CScreen * GetScreen();
	void SetFocused( CWindow *pChild, const bool bFocus );
	// window is notified about removing focus from it
	virtual void RemoveFocus();

		// children/parent work
	void AddChild( CWindow *pWnd );
	// immidiate window children
	void RemoveChild( const std::string &_szName );
	CWindow* GetChild( const std::string &_szName );
	// deep children
	CWindow* GetDeepChild( const std::string &_szName );
	void SetParent( CWindow *_pParent );
	CWindow* GetParent();

	const std::string &STDCALL  GetName() const;
	void SetName( const std::string &_szName );
	
	// broadcast UI message processing
	// return true if message is processed and don't need to process it anymore
	// generally, if Screen returns false, that means something wriong with this message
	// (no message sing exists on the screen - wrong situation or wrong message ID)
	bool ProcessMessage( const struct SBUIMessage &msg );

	// IWindow implementation
	// input work
	virtual void STDCALL OnButtonDown( const CVec2 &vPos, const int nButton );
	virtual void STDCALL OnButtonUp( const CVec2 &vPos, const int nButton ); 
	virtual void STDCALL OnButtonDblClk( const CVec2 &vPos, const int nButton );
	virtual void STDCALL OnChar( const wchar_t chr );
	virtual void STDCALL OnMouseMove( const CVec2 &vPos, const int nButton );
	virtual IWindow* STDCALL Pick( const CVec2 &vPos );
	//get manipulator for editor functionality
	virtual IManipulator* STDCALL GetManipulator();
	// help context
	virtual interface IText* STDCALL GetHelpContext();
	// DRAWING
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );

	// friends
	friend struct SWindowCompare;
	friend class CUIMessageHandler;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // !defined(AFX_WINDOW_H__54783510_EE35_420B_A2EC_19C1C30EA449__INCLUDED_)





















