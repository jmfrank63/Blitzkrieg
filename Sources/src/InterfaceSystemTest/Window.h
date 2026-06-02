
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
struct SWindowCompare
{
	bool operator()( const CDCPtr<CWindow> &o1, const CDCPtr<CWindow> &o2 ) const;
};
enum EWindowPlacementFlags
{
	EWPF_POS_X					= 1,
	EWPF_POS_Y					= 2,
	EWPF_SIZE_X					= 4,
	EWPF_SIZE_Y					= 8,
	
	EWPF_ALL						= 0xffff,
};
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
class CWindow : public IWindow
{
	DECLARE_SERIALIZE;
	DECLARE_CLONE_PROHIBITED;
	
	CDCPtr<IBackground> pBackground;					// may be 0
	
	CNCPtr<CWindow> pParent;									// parent window.
	CNCPtr<CWindow> pFocused;									// child that has keyboard focus
	CNCPtr<CWindow> pModal;										// modal child
	CNCPtr<CWindow> pHighlighted;							// window currently under mouse cursor
	std::vector< CNCPtr<CWindow> > pressed;		// pressed with each mouse button
	CVec2 vScreenPos;

	typedef CHeap< CDCPtr<CWindow>, SWindowCompare > CDrawOrder;
	CDrawOrder drawOrder;

	typedef std::unordered_set<std::string> CChildren;
	CChildren children;
	

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
protected:

	CWindow() {  }
	void Init( int TEST );

	void SetBackground( IBackground *_pBackground );

	void SetModal( CWindow *pChild );
	bool IsVisible() const;
	void ShowWindow( const bool bShow ) { bVisible = bShow; }
	int GetPriority() const;
	bool IsInside( const CVec2 &vPos ) const;
	CWindow* PickInternal( const CVec2 &vPos );
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

	virtual int STDCALL operator&( IDataTree &ss );

	virtual void STDCALL Reposition( const CTRect<float> &parentRect );
	virtual void STDCALL Init();
	virtual void NotifyStateSequenceFinished() { }

	void GetPlacement( int *pX, int *pY, int *pSizeX, int *pSizeY ) const;
	void SetPlacement( int x, int y, int sizeX, int sizeY, const DWORD flags );
	class CScreen * GetScreen();
	void SetFocused( CWindow *pChild, const bool bFocus );
	virtual void RemoveFocus();

	void AddChild( CWindow *pWnd );
	void RemoveChild( const std::string &_szName );
	CWindow* GetChild( const std::string &_szName );
	CWindow* GetDeepChild( const std::string &_szName );
	void SetParent( CWindow *_pParent );
	CWindow* GetParent();

	const std::string &STDCALL  GetName() const;
	void SetName( const std::string &_szName );
	
	bool ProcessMessage( const struct SBUIMessage &msg );

	virtual void STDCALL OnButtonDown( const CVec2 &vPos, const int nButton );
	virtual void STDCALL OnButtonUp( const CVec2 &vPos, const int nButton ); 
	virtual void STDCALL OnButtonDblClk( const CVec2 &vPos, const int nButton );
	virtual void STDCALL OnChar( const wchar_t chr );
	virtual void STDCALL OnMouseMove( const CVec2 &vPos, const int nButton );
	virtual IWindow* STDCALL Pick( const CVec2 &vPos );
	virtual IManipulator* STDCALL GetManipulator();
	virtual interface IText* STDCALL GetHelpContext();
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );

	friend struct SWindowCompare;
	friend class CUIMessageHandler;
};
#endif // !defined(AFX_WINDOW_H__54783510_EE35_420B_A2EC_19C1C30EA449__INCLUDED_)





















