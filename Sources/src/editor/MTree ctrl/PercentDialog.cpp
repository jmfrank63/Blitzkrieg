
#include "StdAfx.h"
#include "PercentDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CPercentDialog::CPercentDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CPercentDialog::IDD, pParent)
{
	m_value = 0;
	m_variable = 0;
}


void CPercentDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SLIDER1, m_slider);
	DDX_Slider(pDX, IDC_SLIDER1, m_value);
	DDX_Text(pDX, IDC_EDIT1, m_variable);
}


BEGIN_MESSAGE_MAP(CPercentDialog, CDialog)
	ON_WM_HSCROLL()
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDER1, OnReleasedcaptureSlider1)
END_MESSAGE_MAP()


void CPercentDialog::OnOK() 
{
	GetParent()->SendMessage( WM_USER + 1);	
}

void CPercentDialog::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	UpdateData(true);
	m_variable = m_value;
	UpdateData(  false );
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CPercentDialog::OnReleasedcaptureSlider1(NMHDR* pNMHDR, LRESULT* pResult) 
{
	GetParent()->SendMessage( WM_USER + 1);	
	*pResult = 0;
}


