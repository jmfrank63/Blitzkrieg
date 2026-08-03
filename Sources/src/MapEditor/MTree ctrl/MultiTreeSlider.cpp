
#include "StdAfx.h"
#include "MultiTreeSlider.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CMultiTreeSlider::CMultiTreeSlider()
{
}

CMultiTreeSlider::~CMultiTreeSlider()
{
}


BEGIN_MESSAGE_MAP(CMultiTreeSlider, CSliderCtrl)
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()


void CMultiTreeSlider::OnKillFocus(CWnd* pNewWnd) 
{
		GetParent()->SendMessage( WM_USER + 1);

	
}
