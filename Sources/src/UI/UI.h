#ifndef __USER_INTERFACE_H__
#define __USER_INTERFACE_H__
#pragma ONCE
interface IGFXTexture;
enum
{
	UI_BASE_VALUE				= 0x10000000,
	UI_UI								= UI_BASE_VALUE + 0x00001100 + 1,
	UI_SCREEN						= UI_BASE_VALUE + 0x00001100 + 2,
	UI_BUTTON						= UI_BASE_VALUE + 0x00001100 + 3,
	UI_CONSOLE					= UI_BASE_VALUE + 0x00001100 + 4,
	UI_STATIC						= UI_BASE_VALUE + 0x00001100 + 5,
	UI_STATUS_BAR				= UI_BASE_VALUE + 0x00001100 + 6,
	UI_DIALOG						= UI_BASE_VALUE + 0x00001100 + 7,
	UI_SLIDER						= UI_BASE_VALUE + 0x00001100 + 8,
	UI_SCROLLBAR				= UI_BASE_VALUE + 0x00001100 + 9,
	UI_LIST							= UI_BASE_VALUE + 0x00001100 + 10,
	UI_EDIT_BOX					= UI_BASE_VALUE + 0x00001100 + 11,
	UI_MESSAGE_BOX			= UI_BASE_VALUE + 0x00001100 + 12,
	UI_SCROLL_TEXT			= UI_BASE_VALUE + 0x00001100 + 13,
	UI_MINIMAP    			= UI_BASE_VALUE + 0x00001100 + 14,
	UI_OBJECTIVE_SCREEN = UI_BASE_VALUE + 0x00001100 + 15,
	UI_OBJECTIVE   			= UI_BASE_VALUE + 0x00001100 + 16,
	UI_NUMBER_INDICATOR	= UI_BASE_VALUE + 0x00001100 + 17,
	UI_TIME_COUNTER			= UI_BASE_VALUE + 0x00001100 + 18,
	UI_OBJ_MAP   				= UI_BASE_VALUE + 0x00001100 + 19,
	UI_MEDALS		   			= UI_BASE_VALUE + 0x00001100 + 20,
	UI_TREE							= UI_BASE_VALUE + 0x00001100 + 21,
	UI_SHORTCUT_BAR			= UI_BASE_VALUE + 0x00001100 + 22,
	UI_COMBOBOX					= UI_BASE_VALUE + 0x00001100 + 23,
	UI_VIDEO_BUTTON			= UI_BASE_VALUE + 0x00001100 + 24,
	UI_CREDITS_SCROLL		= UI_BASE_VALUE + 0x00001100 + 25,
	UI_COLOR_TEXT_SCROLL	= UI_BASE_VALUE + 0x00001100 + 26,
	UI_COLOR_TEXT_ENTRY		= UI_BASE_VALUE + 0x00001100 + 27,	// CANNOT BE IN xml
	UI_COMPLEX_TEXT_SCROLL = UI_BASE_VALUE + 0x00001100 + 28,

	UI_FORCE_DWORD = 0x7fffffff
};
enum EMouseState
{
	E_MOUSE_FREE		= 0,
	E_LBUTTONDOWN		= 1,
	E_RBUTTONDOWN		= 2,
	E_CBUTTONDOWN		= 4
};
enum EKeyboardState
{
	E_KEYBOARD_FREE		= 0,
	E_SHIFT_KEY_DOWN	= 1,
	E_ALT_KEY_DOWN		= 2,
	E_CTRL_KEY_DOWN		= 4
};
enum
{
	UIPLACE_LEFT	= 0x0001,
	UIPLACE_HMID	= 0x0002,
	UIPLACE_RIGHT	= 0x0003,
	UIPLACE_TOP		= 0x0010,
	UIPLACE_VMID	= 0x0020,
	UIPLACE_BOTTOM= 0x0030
};
enum
{
	UI_SW_HIDE = 0,
	UI_SW_SHOW = 1,			//show window and move it to the end of drawing order (the window will be topmost)
	UI_SW_LAST = 2,			//show window and move it to the begin of drawing order (the other windows will be up to it)
	UI_SW_MINIMIZE = 0x10,			//minimize window
	UI_SW_MAXIMIZE = 0x11,			//maximize window
	UI_SW_SHOW_DONT_MOVE_UP = 0x100,
	UI_SW_SHOW_MODAL				= 0x200,
	UI_SW_HIDE_MODAL				= 0x300,

};
struct SUIMessage
{
	int nMessageCode;			//message code, see file UIMessages.h
	int nFirst;						//first parameter, depends upon message code
	int nSecond;					//second parameter, depends upon message code
};
const int NOTIFY_PARENT_FLAG = 0x10000000;
inline bool IsNotifyParentMessage( int nMessageCode ) { return nMessageCode & NOTIFY_PARENT_FLAG; }
inline bool IsNotifyParentMessage( const SUIMessage &msg ) { return msg.nMessageCode & NOTIFY_PARENT_FLAG; }
const int NOTIFY_FLAG = 0x20000000;
inline bool IsNotifyMessage( int nMessageCode ) { return nMessageCode & NOTIFY_FLAG; }
inline bool IsNotifyMessage( const SUIMessage &msg ) { return msg.nMessageCode & NOTIFY_FLAG; }
const int PROCESSED_FLAG = 0x40000000;
inline bool IsProcessedMessage( int nMessageCode ) { return nMessageCode & PROCESSED_FLAG; }
inline bool IsProcessedMessage( const SUIMessage &msg ) { return msg.nMessageCode & PROCESSED_FLAG; }
interface IUIElement : public IRefCount
{
	virtual int STDCALL operator&( IDataTree &ss ) = 0;

	virtual bool STDCALL Update( const NTimer::STime &currTime ) = 0;
	virtual void STDCALL Reposition( const CTRect<float> &rcParent ) = 0;
	
	virtual void STDCALL Draw( interface IGFX *pGFX ) = 0;
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor ) = 0;
	
	virtual void STDCALL SetWindowTexture( IGFXTexture *pTexture ) = 0;
	virtual IGFXTexture* STDCALL GetWindowTexture() = 0;
	virtual void STDCALL SetWindowMap( const CTRect<float> &maps ) = 0;
	virtual void STDCALL SetWindowPlacement( const CVec2 *vPos, const CVec2 *vSize ) = 0;
	virtual void STDCALL SetWindowID( int nID ) = 0;
	virtual void STDCALL SetBoundRect( const CTRect<float> &rc ) = 0;
	
	virtual bool STDCALL OnLButtonDblClk( const CVec2 &vPos ) = 0;
	virtual bool STDCALL OnMouseMove( const CVec2 &vPos, EMouseState mouseState ) = 0;
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState ) = 0;
	virtual bool STDCALL OnLButtonUp( const CVec2 &vPos, EMouseState mouseState ) = 0;
	virtual bool STDCALL OnRButtonDown( const CVec2 &vPos, EMouseState mouseState ) = 0;
	virtual bool STDCALL OnRButtonUp( const CVec2 &vPos, EMouseState mouseState ) = 0;
	virtual bool STDCALL OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta ) = 0;
	virtual bool STDCALL IsInside( const CVec2 &vPos ) = 0;
	virtual bool STDCALL OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState ) = 0;
	virtual void STDCALL SetParent( interface IUIContainer *pParent ) = 0;
	virtual IUIContainer* STDCALL GetParent() = 0;

	virtual void STDCALL SetWindowText( int nState, const WORD *pszText ) = 0;
#ifdef _NATIVE_WCHAR_T_DEFINED
	// bridge for native wchar_t; under /Zc:wchar_t- this would redefine the WORD* overload
	inline void STDCALL SetWindowText( int nState, const wchar_t *pszText )
	{
		SetWindowText( nState, reinterpret_cast<const WORD*>( pszText ) );
	}
#endif
	virtual const WORD* STDCALL GetWindowText( int nState ) = 0;
	virtual void STDCALL SetTextColor( DWORD dwColor ) = 0;

	virtual interface IText* STDCALL GetHelpContext( const CVec2 &vPos, CTRect<float> *pRect ) = 0;
	virtual void STDCALL SetHelpContext( int nState, const WORD *pszToolTipText ) = 0;

	virtual void STDCALL SetFocus( bool bFocus ) = 0;
	virtual void STDCALL EnableWindow( bool bEnable ) = 0;
	virtual bool STDCALL IsWindowEnabled() = 0;
	virtual bool STDCALL IsModal() const = 0;
	virtual void STDCALL SetState( int nState, bool bNotify = true ) = 0;
	virtual int  STDCALL GetState() = 0;
	virtual bool STDCALL IsVisible() = 0;
	virtual int  STDCALL GetVisibleState() = 0;		//see enum UI_SW_SHOW .. UI_SW_MAXIMIZE
	virtual void STDCALL ShowWindow( int nCmdShow ) = 0;
	virtual int STDCALL  GetWindowID() = 0;

	virtual int STDCALL GetWindowPlacement( CVec2 *pPos, CVec2 *pSize, CTRect<float> *pScreenRect ) = 0;
	virtual int STDCALL GetPositionFlag() = 0;

	virtual bool STDCALL ProcessMessage( const SUIMessage &msg ) = 0;
	virtual IUIElement* STDCALL PickElement( const CVec2 &vPos, int nRecursion ) = 0;
	virtual IManipulator* STDCALL GetManipulator() = 0;

	virtual void STDCALL GetTextSize( const int nState, int *pSizeX, int *pSizeY ) const = 0;

	virtual IUIElement * STDCALL Duplicate() { NI_ASSERT_T( false, "wrong call"); return 0; }
};
interface IUIContainer : public IUIElement
{
	virtual void STDCALL AddChild( IUIElement *pWnd ) = 0;
	virtual void STDCALL RemoveChild( IUIElement *pWnd ) = 0;
	virtual void STDCALL RemoveAllChildren() = 0;
	virtual IUIElement* STDCALL GetChildByID( int nChildID ) = 0;
	virtual void STDCALL MoveWindowUp( IUIElement *pWnd ) = 0;
	virtual void STDCALL MoveWindowDown( IUIElement *pWnd ) = 0;
	virtual void STDCALL SetModalFlag( bool bFlag ) = 0;
	virtual void STDCALL SetFocusedWindow( IUIElement *pNewFocusWindow ) = 0;
};
enum EMessageBoxType
{
	E_MESSAGE_BOX_TYPE_OK,
	E_MESSAGE_BOX_TYPE_OKCANCEL,
	E_MESSAGE_BOX_TYPE_YESNO,
};
enum EMessageBoxReturnValues
{
	E_MESSAGE_BOX_RETURN_OK,
	E_MESSAGE_BOX_RETURN_CANCEL,
	E_MESSAGE_BOX_RETURN_YES,
	E_MESSAGE_BOX_RETURN_NO,
};
interface IUIScreen : public IUIContainer
{
	virtual bool STDCALL IsEmpty() = 0;
	virtual int STDCALL Load( const char *pszResourceName, bool bRelative = true ) = 0;
	virtual void STDCALL ProcessGameMessage( const SGameMessage &msg ) = 0;
	virtual bool STDCALL GetMessage( SGameMessage *pMsg ) = 0;				//return false when no messages available
	virtual void STDCALL ClearStrings() = 0;
	virtual int STDCALL MessageBox( const WORD *pszText, int nType ) = 0;
};
interface IConsoleCommandHandler : public IRefCount
{
	virtual bool STDCALL HandleCommand( const char *pszCommandString ) = 0;
};
interface IUIConsole : public IUIElement
{
	virtual bool STDCALL IsAnimationStage() = 0;

	virtual void STDCALL RegisterCommand( IConsoleCommandHandler *pHandler ) = 0;
};
interface IUIDialog : public IUIContainer
{
};
interface IUIStatic : public IUIElement
{
};
interface IUIButton : public IUIElement
{
};
interface IUICreditsScroller : public IUIElement
{
};
interface IUIEditBox : public IUIElement
{
	virtual void STDCALL SetCursor( int nPos ) = 0;
	virtual int  STDCALL GetCursor() = 0;
	virtual void STDCALL SetSel( int nBegin, int nEnd ) = 0;
	virtual void STDCALL GetSel( int *nBegin, int *nEnd ) = 0;		// -1 if no selection
	virtual void STDCALL SetMaxLength( const int nLength ) = 0;
};
interface IUINumberIndicator : public IUIElement
{
	virtual void STDCALL SetValue( float fVal ) = 0;
	virtual void STDCALL ClearColors() = 0;
	virtual void STDCALL SetColor( float fVal, DWORD dwColor ) = 0;
};
interface IUIStatusBar : public IUIContainer
{
	virtual void STDCALL OutputString( int nControl, const WORD *pszText ) = 0;
	virtual void STDCALL OutputValue( int nControl, float fVal ) = 0;
	virtual void STDCALL SetUnitProperty( int nPropType, int nPropValue, const WORD *pszToolText ) = 0;
	virtual void STDCALL SetUnitIcons( DWORD dwIcons ) = 0;
};
interface IUISlider : public IUIElement
{
	virtual void STDCALL SetMinValue( int nVal ) = 0;
	virtual void STDCALL SetMaxValue( int nVal ) = 0;
	virtual void STDCALL SetStep( int nVal ) = 0;
	virtual void STDCALL SetPosition( int nPos ) = 0;
	virtual int STDCALL GetPosition() = 0;
};
interface IUIScrollBar : public IUIContainer
{
	virtual void STDCALL SetMinValue( int nVal ) = 0;
	virtual void STDCALL SetMaxValue( int nVal ) = 0;
	virtual void STDCALL SetStep( int nVal ) = 0;
	virtual void STDCALL SetButtonStep( int nVal ) = 0;		//how the position is changed if we press min or max button
	virtual void STDCALL SetPosition( int nPos ) = 0;
	virtual int STDCALL GetPosition() = 0;
};
interface IUIListRow : public IRefCount
{
	virtual int STDCALL GetNumberOfElements() const = 0;
	virtual IUIElement* STDCALL GetElement( int nIndex ) const = 0;
	virtual void STDCALL SetUserData( int nData ) = 0;
	virtual int  STDCALL GetUserData() const = 0;
};
interface IUIListSorter : public IRefCount
{
public:
	virtual bool STDCALL operator() ( int nSortColumn, const IUIListRow *pRow1, const IUIListRow *pRow2, const bool bForward ) const = 0;
};
interface IUIListControl : public IUIContainer
{
	virtual int STDCALL GetNumberOfItems() = 0;
	virtual void STDCALL AddItem( int nData = 0 ) = 0;
	virtual void STDCALL RemoveItem( int nIndex ) = 0;
	virtual IUIListRow* STDCALL GetItem( int nIndex ) = 0;
	virtual int STDCALL GetItemByID( int nID ) = 0;
	virtual void STDCALL SetSelectionItem( int nSel ) = 0;
	virtual int STDCALL GetSelectionItem() = 0;
	virtual void STDCALL InitialUpdate() = 0;
	virtual void STDCALL SetSortFunctor( int nColumn, IUIListSorter *pSorter ) = 0;
	virtual bool STDCALL Sort( int nColumn, const int nSortType = 0 ) = 0;
	virtual bool STDCALL ReSort() = 0;
};
interface IUIComplexScroll : public IUIContainer
{
	virtual void STDCALL AddItem( IUIElement *pElement, const bool bResizeToFitText ) = 0;
	virtual void STDCALL Clear() = 0;
};
interface IUIComboBox : public IUIContainer
{
	virtual void STDCALL AddItem( IUIElement *pElement ) = 0;
	virtual int STDCALL GetSelectionItem() = 0;
	virtual void STDCALL SetSelectionItem( int nItem ) = 0;
	virtual int STDCALL GetNumberOfItems() = 0;
	virtual IUIElement* STDCALL GetItem( int nItem ) = 0;
	virtual void STDCALL Clear() = 0;
};
interface IUIShortcutBar : public IUIContainer
{
	virtual IUIElement* STDCALL AddBar() = 0;
	virtual IUIElement* STDCALL AddItem() = 0;
	virtual void STDCALL AddMultyItems( int nNum ) = 0;
	virtual IUIElement* STDCALL AddTextItem( const WORD *pszText ) = 0;
	virtual void STDCALL InitialUpdate() = 0;
	virtual void STDCALL GetSelectionItem( int *pBar, int *pItem ) = 0;
	virtual void STDCALL SetSelectionItem( int nBar, int nItem ) = 0;
	virtual bool STDCALL GetBarExpandState( int nBar ) = 0;
	virtual void STDCALL SetBarExpandState( int nBar, bool bExpand, const bool bNotify = false ) = 0;
	virtual IUIElement* STDCALL GetBar( int nBar ) = 0;
	virtual int STDCALL GetNumberOfBars() = 0;
	virtual int STDCALL GetNumberOfItems( int nBar ) = 0;
	virtual IUIElement* STDCALL GetItem( int nBar, int nItem ) = 0;
	virtual void STDCALL Clear() = 0;
};

interface IUIMessageBox : public IUIContainer
{
	virtual void STDCALL SetMessageBoxType( int nType ) = 0;
	virtual void STDCALL SetMessageBoxText( const char *pszText ) = 0;
	virtual int STDCALL GetResult() = 0;
};
interface IUIScrollTextBox : public IUIContainer
{
	virtual void STDCALL AppendText( const WORD *pszText ) = 0;
};
interface IUIColorTextScroll : public IUIContainer
{
	enum EColorEntrys
	{
		E_COLOR_DEFAULT											= 0,
		E_COLOR_IMPORTANT										= 1,
	};
	virtual void STDCALL AppendMessage( const WORD *pszCaption, const WORD *pszMessage, 
																			const enum IUIColorTextScroll::EColorEntrys color = IUIColorTextScroll::E_COLOR_DEFAULT )= 0;

};
interface IUITimeCounter : public IUIElement
{
};
interface IUIObjMap : public IUIContainer
{
	virtual void STDCALL Init() = 0;
	virtual void STDCALL SetMapTexture( IGFXTexture *p ) = 0;
};

enum MINIMAP_CIRCLE_STYLE
{
	MMC_STYLE_DIVERGENT						= 0,	//circle
	MMC_STYLE_CONVERGENT					= 1,	//
	MMC_STYLE_MIXED								= 2,	//
	MMC_STYLE_LPOLYGON_DIVERGENT	= 3,	//left rotating polygon ( counterclockwise )
	MMC_STYLE_LPOLYGON_CONVERGENT	= 4,
	MMC_STYLE_LPOLYGON_MIXED			= 5,
	MMC_STYLE_RPOLYGON_DIVERGENT	= 6,	//right rotating polygon ( clockwise )
	MMC_STYLE_RPOLYGON_CONVERGENT	= 7,
	MMC_STYLE_RPOLYGON_MIXED			= 8,
};

interface IUIMiniMap : public IUIElement
{
  virtual void STDCALL SetTerrainSize( int nXTerrainSize, int nYTerrainSize, int _nPlayersCount ) = 0;
	virtual void STDCALL SetBackgroundTexture( IGFXTexture *_pBackgroundTexture ) = 0;
	virtual bool STDCALL AddWarFogData( const BYTE *pVizBuffer, int nLength ) = 0;
	virtual void STDCALL AddUnitsData( const struct SMiniMapUnitInfo *pUnitsBuffer, int nUnitsCount ) = 0;
	virtual void STDCALL AddFireRangeAreas( const struct SShootAreas *pShootAreasBuffer, int nShootAreasCount ) = 0;
	virtual void STDCALL AddCircle( const CVec2 &vCenter, const float fRadius, int nStyle, WORD wColor, const NTimer::STime &rStart, const NTimer::STime &rDuration, bool bRelative, LPARAM lParam ) = 0;
	virtual int STDCALL AddMarker( const std::string &rszName, const CVec2 &vPos, bool _bActive, int _nID, const NTimer::STime &rStart, const NTimer::STime &rDuration, bool bRelative ) = 0;
	virtual void STDCALL ActivateMarker( int _nID, bool _bActive ) = 0;
	virtual void STDCALL ActivateMarker( const std::string &rszName, bool _bActive ) = 0;
	virtual void STDCALL RemoveMarker( int _nID ) = 0;
	virtual void STDCALL RemoveMarker( const std::string &rszName ) = 0;
};
interface IUIVideoButton : public IUIElement
{
	virtual int STDCALL GetCurrentFrame() = 0;
	virtual bool STDCALL SetCurrentFrame( int nFrame ) = 0;
};
#endif // __USER_INTERFACE_H__
