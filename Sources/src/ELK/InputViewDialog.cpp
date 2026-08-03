#include "StdAfx.h"
#include "resource.h"
#include "InputViewDialog.h"
#include "Messages.h"
#include "ELK_Types.h"
#include "../Image/Image.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CInputViewDialog::vID[] = 
{
	IDC_IV_ORIGINAL_LABEL,										//0
	IDC_IV_ORIGINAL_EDIT,											//1
	IDC_IV_ORIGINAL_DESCRIPTION_LABEL,				//2
	IDC_IV_ORIGINAL_DESCRIPTION_EDIT,					//3
	IDC_IV_TRANSLATE_LABEL,										//4
	IDC_IV_TRANSLATE_EDIT,										//5
	IDC_IV_STATE_LABEL,												//6
	IDC_IV_STATE_FRAME_LABEL,									//7
	IDC_IV_NOT_TRANSLATED_RADIO_BUTTON,				//8
	IDC_IV_OUTDATED_RADIO_BUTTON,							//9
	IDC_IV_TRANSLATED_RADIO_BUTTON,						//10
	IDC_IV_APPROVED_RADIO_BUTTON,							//11
	IDC_IV_NOT_TRANSLATED_RADIO_BUTTON_LABEL,	//12
	IDC_IV_OUTDATED_RADIO_BUTTON_LABEL,				//13
	IDC_IV_TRANSLATED_RADIO_BUTTON_LABEL,			//14
	IDC_IV_APPROVED_RADIO_BUTTON_LABEL,				//15
	IDC_IV_IMAGE_BORDER,											//16
	IDC_IV_NOT_TRANSLATED_RADIO_BUTTON_BITMAP,//17
	IDC_IV_OUTDATED_RADIO_BUTTON_BITMAP,			//18
	IDC_IV_TRANSLATED_RADIO_BUTTON_BITMAP,		//19
	IDC_IV_APPROVED_RADIO_BUTTON_BITMAP,			//20
};

CInputViewDialog::CInputViewDialog( CWnd* pParent )
: CResizeDialog( CInputViewDialog::IDD, pParent ), hNextIcon( 0 ), pwndMainFrame( 0 ), bTranslatedTextChanged( false ), bManualState( false ), nInitialState( SELKTextProperty::STATE_NOT_TRANSLATED )

{

	SetControlStyle( IDC_IV_ORIGINAL_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_IV_ORIGINAL_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR_VER, 0.5f, 0.5f, 0.5f, 1.0f );
	
	SetControlStyle( IDC_IV_ORIGINAL_DESCRIPTION_LABEL, ANCHORE_LEFT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_IV_ORIGINAL_DESCRIPTION_EDIT, ANCHORE_LEFT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	
	SetControlStyle( IDC_IV_TRANSLATE_LABEL, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_IV_TRANSLATE_EDIT, ANCHORE_RIGHT_TOP | RESIZE_HOR_VER, 0.5f, 0.5f, 0.5f, 1.0f );
	
	SetControlStyle( IDC_IV_STATE_LABEL, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_IV_STATE_FRAME_LABEL, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	
	SetControlStyle( IDC_IV_NOT_TRANSLATED_RADIO_BUTTON, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_IV_OUTDATED_RADIO_BUTTON, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_IV_TRANSLATED_RADIO_BUTTON, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_IV_APPROVED_RADIO_BUTTON, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	
	SetControlStyle( IDC_IV_IMAGE_BORDER, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_IV_NOT_TRANSLATED_RADIO_BUTTON_BITMAP, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_IV_OUTDATED_RADIO_BUTTON_BITMAP, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_IV_TRANSLATED_RADIO_BUTTON_BITMAP, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_IV_APPROVED_RADIO_BUTTON_BITMAP, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );

	SetControlStyle( IDC_IV_NOT_TRANSLATED_RADIO_BUTTON_LABEL, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_IV_OUTDATED_RADIO_BUTTON_LABEL, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_IV_TRANSLATED_RADIO_BUTTON_LABEL, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_IV_APPROVED_RADIO_BUTTON_LABEL, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_IV_IMAGE_BORDER, ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

void CInputViewDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_IV_NOT_TRANSLATED_RADIO_BUTTON, m_NotTranslatedButton);
	DDX_Control(pDX, IDC_IV_OUTDATED_RADIO_BUTTON, m_OutdatedButton);
	DDX_Control(pDX, IDC_IV_TRANSLATED_RADIO_BUTTON, m_TranslatedButton);
	DDX_Control(pDX, IDC_IV_APPROVED_RADIO_BUTTON, m_ApprovedButton);
	DDX_Control(pDX, IDC_IV_TRANSLATE_EDIT, m_TranslateEdit);
	DDX_Control(pDX, IDC_IV_ORIGINAL_EDIT, m_OriginalEdit);
	DDX_Control(pDX, IDC_IV_ORIGINAL_DESCRIPTION_EDIT, m_DescriptionEdit);
	DDX_Control(pDX, IDC_IV_NEXT_BUTTON, m_NextButton);
	DDX_Control(pDX, IDC_IV_BACK_BUTTON, m_BackButton);
}

BEGIN_MESSAGE_MAP(CInputViewDialog, CResizeDialog)
	ON_BN_CLICKED(IDC_IV_NOT_TRANSLATED_RADIO_BUTTON, OnNotTranslatedRadioButton)
	ON_BN_CLICKED(IDC_IV_TRANSLATED_RADIO_BUTTON, OnTranslatedRadioButton)
	ON_BN_CLICKED(IDC_IV_APPROVED_RADIO_BUTTON, OnApprovedRadioButton)
	ON_EN_CHANGE(IDC_IV_TRANSLATE_EDIT, OnChangeTranslateEdit)
	ON_BN_CLICKED(IDC_IV_BACK_BUTTON, OnBackButton)
	ON_BN_CLICKED(IDC_IV_NEXT_BUTTON, OnNextButton)
	ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL CInputViewDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	RecreateUnicodeEditControls();
	
	hBackIcon = AfxGetApp()->LoadIcon( IDI_IV_BACK );
	m_BackButton.SetIcon( hBackIcon );
	
	hNextIcon = AfxGetApp()->LoadIcon( IDI_IV_NEXT );
	m_NextButton.SetIcon( hNextIcon );

	return true;
}

void CInputViewDialog::RecreateUnicodeEditControl( int nControlID, CEdit *pEdit )
{
	NI_ASSERT_T( pEdit != 0, NStr::Format( _T( "CInputViewDialog::RecreateUnicodeEditControl() wrong parameter: pEdit %x" ), pEdit ) );
	if ( pEdit == 0 )
	{
		return;
	}

	HWND hOldWnd = pEdit->GetSafeHwnd();
	if ( hOldWnd == 0 )
	{
		return;
	}

	CRect rect;
	::GetWindowRect( hOldWnd, &rect );
	ScreenToClient( &rect );

	const DWORD dwStyle = ::GetWindowLong( hOldWnd, GWL_STYLE );
	const DWORD dwExStyle = ::GetWindowLong( hOldWnd, GWL_EXSTYLE );
	HFONT hFont = reinterpret_cast<HFONT>( ::SendMessage( hOldWnd, WM_GETFONT, 0, 0 ) );

	pEdit->UnsubclassWindow();
	::DestroyWindow( hOldWnd );

	HWND hNewWnd = ::CreateWindowExW( dwExStyle,
																		L"EDIT",
																		L"",
																		dwStyle,
																		rect.left,
																		rect.top,
																		rect.Width(),
																		rect.Height(),
																		GetSafeHwnd(),
																		reinterpret_cast<HMENU>( nControlID ),
																		AfxGetInstanceHandle(),
																		0 );
	if ( hNewWnd != 0 )
	{
		if ( hFont != 0 )
		{
			::SendMessage( hNewWnd, WM_SETFONT, reinterpret_cast<WPARAM>( hFont ), TRUE );
		}
	}
}

void CInputViewDialog::RecreateUnicodeEditControls()
{
	RecreateUnicodeEditControl( IDC_IV_ORIGINAL_EDIT, &m_OriginalEdit );
	RecreateUnicodeEditControl( IDC_IV_TRANSLATE_EDIT, &m_TranslateEdit );
	RecreateUnicodeEditControl( IDC_IV_ORIGINAL_DESCRIPTION_EDIT, &m_DescriptionEdit );
}

void CInputViewDialog::OnNotTranslatedRadioButton() 
{
	bManualState = true;
	if ( pwndMainFrame && ( pwndMainFrame->GetSafeHwnd() != 0 ) )
	{
		CheckRadioButton( IDC_IV_NOT_TRANSLATED_RADIO_BUTTON, IDC_IV_APPROVED_RADIO_BUTTON, IDC_IV_NOT_TRANSLATED_RADIO_BUTTON );
		pwndMainFrame->SendMessage( WM_INPUT_FORM_NOTIFY, IFN_STATE_CHANGED, SELKTextProperty::STATE_NOT_TRANSLATED );
	}
}

void CInputViewDialog::OnTranslatedRadioButton() 
{
	bManualState = true;
	if ( pwndMainFrame && ( pwndMainFrame->GetSafeHwnd() != 0 ) )
	{
		CheckRadioButton( IDC_IV_NOT_TRANSLATED_RADIO_BUTTON, IDC_IV_APPROVED_RADIO_BUTTON, IDC_IV_TRANSLATED_RADIO_BUTTON );
		pwndMainFrame->SendMessage( WM_INPUT_FORM_NOTIFY, IFN_STATE_CHANGED, SELKTextProperty::STATE_TRANSLATED );
	}
}

void CInputViewDialog::OnApprovedRadioButton() 
{
	bManualState = true;
	if ( pwndMainFrame && ( pwndMainFrame->GetSafeHwnd() != 0 ) )
	{
		CheckRadioButton( IDC_IV_NOT_TRANSLATED_RADIO_BUTTON, IDC_IV_APPROVED_RADIO_BUTTON, IDC_IV_APPROVED_RADIO_BUTTON );
		pwndMainFrame->SendMessage( WM_INPUT_FORM_NOTIFY, IFN_STATE_CHANGED, SELKTextProperty::STATE_APPROVED );
	}
}

void CInputViewDialog::OnChangeTranslateEdit() 
{
	if ( pwndMainFrame && ( pwndMainFrame->GetSafeHwnd() != 0 ) )
	{
		pwndMainFrame->SendMessage( WM_INPUT_FORM_NOTIFY, IFN_TRANSLATION_CHANGED, 0 );
	}

	bool bPreviousTranslatedTextChanged = bTranslatedTextChanged;
	std::wstring strActualText;
	if ( CWnd *pWnd = GetDlgItem( IDC_IV_TRANSLATE_EDIT ) )
	{
		const int nTextLength = ::GetWindowTextLengthW( pWnd->GetSafeHwnd() );
		strActualText.resize( nTextLength + 1 );
		if ( nTextLength > 0 )
		{
			::GetWindowTextW( pWnd->GetSafeHwnd(), &( strActualText[0] ), nTextLength + 1 );
		}
		strActualText.resize( nTextLength );
	}
	bTranslatedTextChanged = ( strInitialTranslatedWideText != strActualText );

	if ( ( bTranslatedTextChanged != bPreviousTranslatedTextChanged ) && ( !bManualState ) )
	{
		if ( bTranslatedTextChanged )
		{
			CheckRadioButton( IDC_IV_NOT_TRANSLATED_RADIO_BUTTON, IDC_IV_APPROVED_RADIO_BUTTON, IDC_IV_NOT_TRANSLATED_RADIO_BUTTON + SELKTextProperty::STATE_TRANSLATED );
			if ( pwndMainFrame && ( pwndMainFrame->GetSafeHwnd() != 0 ) )
			{
				pwndMainFrame->SendMessage( WM_INPUT_FORM_NOTIFY, IFN_STATE_CHANGED, SELKTextProperty::STATE_TRANSLATED );
			}
		}
		else
		{
			CheckRadioButton( IDC_IV_NOT_TRANSLATED_RADIO_BUTTON, IDC_IV_APPROVED_RADIO_BUTTON, IDC_IV_NOT_TRANSLATED_RADIO_BUTTON + nInitialState );
			if ( pwndMainFrame && ( pwndMainFrame->GetSafeHwnd() != 0 ) )
			{
				pwndMainFrame->SendMessage( WM_INPUT_FORM_NOTIFY, IFN_STATE_CHANGED, nInitialState );
			}
		}
	}
}

void CInputViewDialog::OnNextButton() 
{
	if ( pwndMainFrame && ( pwndMainFrame->GetSafeHwnd() != 0 ) )
	{
		pwndMainFrame->SendMessage( WM_INPUT_FORM_NOTIFY, IFN_NEXT_BUTTON_PRESED, 0 );
	}
}

void CInputViewDialog::OnBackButton() 
{
	if ( pwndMainFrame && ( pwndMainFrame->GetSafeHwnd() != 0 ) )
	{
		pwndMainFrame->SendMessage( WM_INPUT_FORM_NOTIFY, IFN_BACK_BUTTON_PRESED, 0 );
	}
}

void CInputViewDialog::LoadGameImage( const std::string &rszGameImagePath )
{
	szGameImagePath = rszGameImagePath;
	if ( !szGameImagePath.empty() )
	{
		if ( CPtr<IDataStream> pDataStream = CreateFileStream( szGameImagePath, STREAM_ACCESS_READ ) )
		{
			if ( CPtr<IImageProcessor> pImageProseccor = GetImageProcessor() )
			{
				if ( CPtr<IImage> pImage = pImageProseccor->LoadImage( pDataStream ) ) 
				{
					gameImageBitmap.DeleteObject();
					BITMAPINFO bmi;
					bmi.bmiHeader.biSize = sizeof( bmi.bmiHeader );
					bmi.bmiHeader.biWidth = pImage->GetSizeX();
					bmi.bmiHeader.biHeight = -pImage->GetSizeY();
					bmi.bmiHeader.biPlanes = 1;
					bmi.bmiHeader.biBitCount = 32;
					bmi.bmiHeader.biCompression = BI_RGB;
					bmi.bmiHeader.biSizeImage = 0;
					bmi.bmiHeader.biClrUsed = 0;
					
					CDC *pDC = GetDC();
					HBITMAP hbm = CreateCompatibleBitmap( pDC->m_hDC, pImage->GetSizeX(), pImage->GetSizeY() );
					SetDIBits( pDC->m_hDC, hbm, 0, pImage->GetSizeY(), pImage->GetLFB(), &bmi, DIB_RGB_COLORS );
					
					ReleaseDC( pDC );
					gameImageBitmap.Attach( hbm );
					gameImageSize.x = pImage->GetSizeX();
					gameImageSize.y = pImage->GetSizeY();
				}
			}
		}
	}
	
	if ( CWnd *pWnd = GetDlgItem( IDC_IV_IMAGE_BORDER ) )
	{
		CRect rect;
		pWnd->GetWindowRect( &rect );
		ScreenToClient( &rect );
		InvalidateRect( rect, false );
	}
}

void CInputViewDialog::OnPaint() 
{
	CPaintDC dc(this);
	
	if ( CWnd *pWnd = GetDlgItem( IDC_IV_IMAGE_BORDER ) )
	{
		CRect rect;
		pWnd->GetWindowRect( &rect );
		ScreenToClient( &rect );
		rect.left += 2;
		rect.right -= 3;
		rect.top += 2;
		rect.bottom -= 3;

		if ( ( rect.Width() > 0 ) && ( rect.Height() > 0 ) )
		{
			dc.FillSolidRect( rect, RGB( 0x92, 0x92, 0x92 ) );
			if ( !szGameImagePath.empty() )
			{
				CDC memDC;
				int nRes = memDC.CreateCompatibleDC( &dc );
				CBitmap *pOldBitmap = memDC.SelectObject( &gameImageBitmap );
				dc.StretchBlt( rect.left + ( rect.Width() - gameImageSize.x ) / 2,
											 rect.top + ( rect.Height() - gameImageSize.y ) / 2,
											 gameImageSize.x,
											 gameImageSize.y,
											 &memDC,
											 0, 0, gameImageSize.x, gameImageSize.y,
											 SRCCOPY );
				memDC.SelectObject( pOldBitmap );
			}
		}
	}
}

