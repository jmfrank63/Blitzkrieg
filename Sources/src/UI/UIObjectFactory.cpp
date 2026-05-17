#include "StdAfx.h"

#include "UIObjectFactory.h"
#include "UIMask.h"
#include "UIScreen.h"
#include "UIButton.h"
#include "UIConsole.h"
#include "UIStatusBar.h"
#include "UIDialog.h"
#include "UISlider.h"
#include "UIList.h"
#include "UIListSorter.h"
#include "UIEdit.h"
#include "UIScrollText.h"
#include "UIMiniMap.h"
#include "UIObjectiveScreen.h"
#include "UINumberIndicator.h"
#include "UITimeCounter.h"
#include "UIObjMap.h"
#include "UIMedals.h"
#include "UITree.h"
#include "UIShortcutBar.h"
#include "UIComboBox.h"
#include "UIVideoButton.h"
#include "UICreditsScroller.h"
#include "MaskManager.h"
#include "UIColorTextScroll.h"
#include "UIComplexScroll.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CUIObjectFactory theUIObjectFactory;
CUIObjectFactory::CUIObjectFactory()
{
	REGISTER_CLASS( this, UI_SCREEN, CUIScreenBridge );
	REGISTER_CLASS( this, UI_STATIC, CUIStaticBridge );
	REGISTER_CLASS( this, UI_BUTTON, CUIButtonBridge );
	REGISTER_CLASS( this, UI_CONSOLE, CUIConsoleBridge );
	REGISTER_CLASS( this, UI_STATUS_BAR, CUIStatusBarBridge );
	REGISTER_CLASS( this, UI_DIALOG, CUIDialogBridge );
	REGISTER_CLASS( this, UI_SLIDER, CUISliderBridge );
	REGISTER_CLASS( this, UI_SCROLLBAR, CUIScrollBarBridge );
	REGISTER_CLASS( this, UI_LIST, CUIListBridge );
	REGISTER_CLASS( this, UI_EDIT_BOX, CUIEditBoxBridge );
	REGISTER_CLASS( this, UI_MESSAGE_BOX, CUIMessageBoxBridge );
	REGISTER_CLASS( this, UI_SCROLL_TEXT, CUIScrollTextBoxBridge );
	REGISTER_CLASS( this, UI_MINIMAP, CUIMiniMapBridge );
	REGISTER_CLASS( this, UI_OBJECTIVE_SCREEN, CUIObjectiveScreenBridge );
	REGISTER_CLASS( this, UI_NUMBER_INDICATOR, CUINumberIndicatorBridge );
	REGISTER_CLASS( this, MASK_MANAGER, CMaskManager );
	REGISTER_CLASS( this, UI_MASK, CUIMask );
	REGISTER_CLASS( this, UI_TIME_COUNTER, CUITimeCounterBridge );
	REGISTER_CLASS( this, UI_OBJ_MAP, CUIObjMapBridge );
	REGISTER_CLASS( this, UI_MEDALS, CUIMedalsBridge );
	REGISTER_CLASS( this, UI_SHORTCUT_BAR, CUIShortcutBarBridge );
	REGISTER_CLASS( this, UI_COMBOBOX, CUIComboBoxBridge );
	REGISTER_CLASS( this, UI_VIDEO_BUTTON, CUIVideoButtonBridge );
	REGISTER_CLASS( this, UI_CREDITS_SCROLL, CUICreditsScrollerBridge );
	REGISTER_CLASS( this, UI_COLOR_TEXT_SCROLL, CUIColorTextScrollBridge );
	REGISTER_CLASS( this, UI_COMPLEX_TEXT_SCROLL, CUIComplexScrollBridge );
	
	//��� �������������� sort functors
	REGISTER_CLASS( this, UI_TEXT_SORTER,		CUIListTextSorter );
	REGISTER_CLASS( this, UI_NUMBER_SORTER, CUIListNumberSorter );
	REGISTER_CLASS( this, UI_USER_DATA_SORTER, CUIListUserDataSorter );

	//����������� ��������������� ��������
	REGISTER_CLASS( this, UI_LIST_ROW, SUIListRow );
	REGISTER_CLASS( this, UI_LIST_HEADER, SUIListHeader );
	REGISTER_CLASS( this, UI_COLOR_TEXT_ENTRY, CUIColorTextScroll::CColorTextEntry );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static SModuleDescriptor theModuleDescriptor( "UI", UI_BASE_VALUE, 0x0100, &theUIObjectFactory, 0 );
const SModuleDescriptor* STDCALL GetModuleDescriptor()
{
	return &theModuleDescriptor;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
