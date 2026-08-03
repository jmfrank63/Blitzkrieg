
#include "StdAfx.h"
#include "editor.h"
#include "PictureOptions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CPictureOptions::CPictureOptions(CWnd* pParent /*=NULL*/)
	: CDialog(CPictureOptions::IDD, pParent)
{
	m_CurrentProjectCheck = FALSE;
}


void CPictureOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RIGHT_IMAGE, m_rightImage);
	DDX_Control(pDX, IDC_LEFT_IMAGE, m_leftImage);
	DDX_Control(pDX, IDC_CONTRAST, m_contrastSlider);
	DDX_Control(pDX, IDC_GAMMA, m_gammaSlider);
	DDX_Control(pDX, IDC_BRIGHTNESS, m_brightnessSlider);
	DDX_Control(pDX, IDC_EDIT_GAMMA, m_editGamma);
	DDX_Control(pDX, IDC_EDIT_CONTRAST, m_editContrast);
	DDX_Control(pDX, IDC_EDIT_BRIGHTNESS, m_editBrightness);
	DDX_Check(pDX, IDC_CONFIG_FOR_CURRENT_PROJECT_ONLY, m_CurrentProjectCheck);
}


BEGIN_MESSAGE_MAP(CPictureOptions, CDialog)
ON_BN_CLICKED(IDAPPLY, OnApply)
ON_EN_CHANGE(IDC_EDIT_CONTRAST, OnChangeEditContrast)
	ON_EN_CHANGE(IDC_EDIT_BRIGHTNESS, OnChangeEditBrightness)
	ON_EN_CHANGE(IDC_EDIT_GAMMA, OnChangeEditGamma)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_CONTRAST, OnReleasedcaptureContrast)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_BRIGHTNESS, OnReleasedcaptureBrightness)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_GAMMA, OnReleasedcaptureGamma)
END_MESSAGE_MAP()


void CPictureOptions::OnApply() 
{
	BeginWaitCursor();
	m_rightImage.ApplyGammaCorrection( fBrightness, fContrast, fGamma );
	EndWaitCursor();
}

BOOL CPictureOptions::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_editBrightness.SetWindowText( NStr::Format("%g", fBrightness) );
	m_editContrast.SetWindowText( NStr::Format("%g", fContrast) );
	m_editGamma.SetWindowText( NStr::Format("%g", fGamma) );
	m_brightnessSlider.SetRange( 0, 2000 );
	m_brightnessSlider.SetPos( 1 );
	m_contrastSlider.SetRange( 0, 2000 );
	m_contrastSlider.SetPos( 1 );
	m_gammaSlider.SetRange( 0, 2000 );
	m_gammaSlider.SetPos( 1 );
	
	BeginWaitCursor();
	RECT rc;
	m_leftImage.GetWindowRect( &rc );
	m_leftImage.SetImageSize( rc.right - rc.left, rc.bottom - rc.top );
	m_rightImage.GetWindowRect( &rc );
	m_rightImage.SetImageSize( rc.right - rc.left, rc.bottom - rc.top );
	std::string szName = "editor\\bright.tga";
	m_leftImage.LoadBitmap( szName.c_str(), "" );
	m_rightImage.ApplyGammaCorrection( fBrightness, fContrast, fGamma );
	m_rightImage.LoadBitmap( szName.c_str(), "" );
	EndWaitCursor();

	m_brightnessSlider.SetPos( fBrightness*1000 + 1000 );
	m_contrastSlider.SetPos( fContrast*1000 + 1000 );
	m_gammaSlider.SetPos( fGamma*1000 + 1000 );
	
	return TRUE;  // return TRUE unless you set the focus to a control
}

/*
BOOL CPictureOptions::PreTranslateMessage(MSG* pMsg) 
{
	if ( ( pMsg->message == WM_KEYDOWN ) && ( pMsg->wParam == VK_RETURN ) )
	{
		CWnd *pWnd = GetFocus();
		if ( pWnd == &m_editGamma )
		{
			CString szVal;
			m_editGamma.GetWindowText( szVal );
			fGamma = atof( szVal );
			m_gammaSlider.SetPos( fGamma * 1000 );
			UpdateData( TRUE );
			return TRUE;       // запрет дальнейшей обработки
		}

		if ( pWnd == &m_editContrast )
		{
			CString szVal;
			m_editContrast.GetWindowText( szVal );
			fContrast = atof( szVal );
			m_contrastSlider.SetPos( fContrast * 1000 );
			UpdateData( TRUE );
			return TRUE;       // запрет дальнейшей обработки
		}
		
		if ( pWnd == &m_editBrightness )
		{
			CString szVal;
			m_editBrightness.GetWindowText( szVal );
			fBrightness = atof( szVal );
			m_brightnessSlider.SetPos( fBrightness * 1000 );
			UpdateData( TRUE );
			return TRUE;       // запрет дальнейшей обработки
		}
	}
	
	return CDialog::PreTranslateMessage(pMsg);
}
*/

void CPictureOptions::OnChangeEditContrast() 
{
	UpdateData( TRUE );
	CString szVal;
	m_editContrast.GetWindowText( szVal );
	fContrast = atof( szVal );
	if ( fContrast > 1.0f )
	{
		fContrast = 1.0f;
		m_editContrast.SetWindowText( "1" );
	}
	if ( fContrast < -1.0f )
	{
		fContrast = -1.0f;
		m_editContrast.SetWindowText( "-1" );
	}
	m_contrastSlider.SetPos( fContrast * 1000 + 1000 );

	UpdateData( FALSE );
}


void CPictureOptions::OnChangeEditBrightness() 
{
	UpdateData( TRUE );
	CString szVal;
	m_editBrightness.GetWindowText( szVal );
	fBrightness = atof( szVal );
	if ( fBrightness > 1.0f )
	{
		fBrightness = 1.0f;
		m_editBrightness.SetWindowText( "1" );
	}
	else if ( fBrightness < -1.0f )
	{
		fBrightness = -1.0f;
		m_editBrightness.SetWindowText( "-1" );
	}
	m_brightnessSlider.SetPos( fBrightness * 1000 + 1000 );
	UpdateData( FALSE );
}

void CPictureOptions::OnChangeEditGamma() 
{
	UpdateData( TRUE );
	CString szVal;
	m_editGamma.GetWindowText( szVal );
	fGamma = atof( szVal );
	if ( fGamma > 1.0f )
	{
		fGamma = 1.0f;
		m_editGamma.SetWindowText( "1" );
	}
	if ( fGamma < -1.0f )
	{
		fGamma = -1.0f;
		m_editGamma.SetWindowText( "-1" );
	}
	m_gammaSlider.SetPos( fGamma * 1000 + 1000 );
	UpdateData( FALSE );
}

void CPictureOptions::OnReleasedcaptureContrast(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UpdateData( TRUE );
	fContrast = (float) ( m_contrastSlider.GetPos() - 1000 ) / 1000;
	m_editContrast.SetWindowText( NStr::Format("%g", fContrast) );
	UpdateData( FALSE );
	
	*pResult = 0;
}

void CPictureOptions::OnReleasedcaptureBrightness(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UpdateData( TRUE );
	fBrightness = (float) ( m_brightnessSlider.GetPos() - 1000 ) / 1000;
	m_editBrightness.SetWindowText( NStr::Format("%g", fBrightness) );
	UpdateData( FALSE );
	
	*pResult = 0;
}

void CPictureOptions::OnReleasedcaptureGamma(NMHDR* pNMHDR, LRESULT* pResult) 
{
	UpdateData( TRUE );
	fGamma = (float) ( m_gammaSlider.GetPos() - 1000 ) / 1000;
	m_editGamma.SetWindowText( NStr::Format("%g", fGamma) );
	UpdateData( FALSE );
	
	*pResult = 0;
}
