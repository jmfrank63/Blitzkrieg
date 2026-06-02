#include "StdAfx.h"

#include "Resource.h"
#include "ChooseFontsDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CChooseFontsDialog::CChooseFontsDialog( CWnd* pParent )
	: CResizeDialog( CChooseFontsDialog::IDD, pParent ), bCreateControls( true ), strFontName( CFontGen::FONT_NAME ), dwNormalFontSize( CFontGen::FONTS_SIZE[2] ), dwLargeFontSize( CFontGen::FONTS_SIZE[3] ), nCodePage( GetACP() )
{

	SetControlStyle( IDC_CHFS_FONT_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CHFS_FONT_COMBO_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_CHFS_SAMPLE_FRAME, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	SetControlStyle( IDC_CHFS_SAMPLE_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	
	SetControlStyle( IDC_CHFS_SIZE_FRAME, ANCHORE_LEFT_BOTTOM | RESIZE_HOR );
	
	SetControlStyle( IDC_CHFS_SIZE_NORMAL_LABEL_LEFT, ANCHORE_LEFT_BOTTOM );
	SetControlStyle( IDC_CHFS_SIZE_NORMAL_COMBO_BOX, ANCHORE_LEFT_BOTTOM );
	SetControlStyle( IDC_CHFS_SIZE_NORMAL_LABEL_RIGHT, ANCHORE_LEFT_BOTTOM | RESIZE_HOR );

	SetControlStyle( IDC_CHFS_SIZE_LARGE_LABEL_LEFT, ANCHORE_LEFT_BOTTOM );
	SetControlStyle( IDC_CHFS_SIZE_LARGE_COMBO_BOX, ANCHORE_LEFT_BOTTOM );
	SetControlStyle( IDC_CHFS_SIZE_LARGE_LABEL_RIGHT, ANCHORE_LEFT_BOTTOM | RESIZE_HOR );

	SetControlStyle( IDOK, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDCANCEL, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDC_CHFS_SET_DEFAULT_BUTTON, ANCHORE_RIGHT_BOTTOM );
}

std::string CChooseFontsDialog::GetRegistryKey()
{
	CString strPath;
	CString strProgramKey;
	CString strKey;
	strPath.LoadString( IDS_REGISTRY_PATH );
	strProgramKey.LoadString( AFX_IDS_APP_TITLE );
	strKey.LoadString( IDS_CHFS_REGISTRY_KEY );
	std::string szRegistryKey = NStr::Format( _T( "Software\\%s\\%s\\%s" ), LPCTSTR( strPath ), LPCTSTR( strProgramKey ), LPCTSTR( strKey ) );
	return szRegistryKey;
}

void CChooseFontsDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CHFS_SIZE_NORMAL_COMBO_BOX, wndNormalFontSizeComboBox);
	DDX_Control(pDX, IDC_CHFS_SIZE_LARGE_COMBO_BOX, wndLargeFontSizeComboBox);
	DDX_Control(pDX, IDC_CHFS_SAMPLE_LABEL, wndSample);
	DDX_Control(pDX, IDC_CHFS_FONT_COMBO_BOX, wndFontComboBox);
}

BEGIN_MESSAGE_MAP(CChooseFontsDialog, CResizeDialog)
	ON_CBN_SELCHANGE(IDC_CHFS_FONT_COMBO_BOX, OnSelchangeFontComboBox)
	ON_CBN_SELCHANGE(IDC_CHFS_SIZE_LARGE_COMBO_BOX, OnSelchangeSizeLargeComboBox)
	ON_CBN_SELCHANGE(IDC_CHFS_SIZE_NORMAL_COMBO_BOX, OnSelchangeSizeNormalComboBox)
	ON_BN_CLICKED(IDC_CHFS_SET_DEFAULT_BUTTON, OnDefaultButton)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CChooseFontsDialog::OnInitDialog()
{
  CResizeDialog::OnInitDialog();

	CreateControls();
	UpdateControls();
	
	return true;
}

void CChooseFontsDialog::CreateControls()
{
	bCreateControls = true;

	std::set<CString> fonts;
	CFontGen::GetFonts( nCodePage, &fonts );
	if ( !fonts.empty() )
	{
		if ( fonts.find( strFontName ) == fonts.end() )
		{
			strFontName = *( fonts.begin() );
		}
	}

	wndFontComboBox.ResetContent();
	for ( std::set<CString>::const_iterator it = fonts.begin(); it != fonts.end(); ++it )
	{
		wndFontComboBox.AddString( *it );
	}
	if ( wndFontComboBox.SelectString( CB_ERR, strFontName ) == CB_ERR )
	{
		if ( wndFontComboBox.GetCount() > 0 )
		{
			wndFontComboBox.SetCurSel( 0 );
		}
	}
	wndNormalFontSizeComboBox.ResetContent();
	for ( int i = CFontGen::FONTS_SIZE[1]; i <= CFontGen::FONTS_SIZE[2]; ++i )
	{
		const int nItemIndex = wndNormalFontSizeComboBox.AddString( NStr::Format( "%d", i ) );
		if ( nItemIndex != CB_ERR )
		{
			wndNormalFontSizeComboBox.SetItemData( nItemIndex, i );
		}
	}
	wndNormalFontSizeComboBox.SelectString( CB_ERR, NStr::Format( "%d", dwNormalFontSize ) );
	{
		if ( wndFontComboBox.GetCount() > 0 )
		{
			wndNormalFontSizeComboBox.SelectString( CB_ERR, NStr::Format( "%d", CFontGen::FONTS_SIZE[2] ) );
		}
	}
	wndLargeFontSizeComboBox.ResetContent();
	for ( int i = CFontGen::FONTS_SIZE[1]; i <= CFontGen::FONTS_SIZE[3]; ++i )
	{
		const int nItemIndex = wndLargeFontSizeComboBox.AddString( NStr::Format( "%d", i ) );
		if ( nItemIndex != CB_ERR )
		{
			wndLargeFontSizeComboBox.SetItemData( nItemIndex, i );
		}
	}
	if ( wndLargeFontSizeComboBox.SelectString( CB_ERR, NStr::Format( "%d", dwLargeFontSize ) ) == CB_ERR )
	{
		if ( wndFontComboBox.GetCount() > 0 )
		{
			wndLargeFontSizeComboBox.SelectString( CB_ERR, NStr::Format( "%d", CFontGen::FONTS_SIZE[3] ) );
		}
	}
	CString strFontSizeDimensionsFormat;
	strFontSizeDimensionsFormat.LoadString( IDS_FONT_SIZE_DIMENSIONS_FORMAT );
	SetDlgItemText( IDC_CHFS_SIZE_NORMAL_LABEL_RIGHT, NStr::Format( strFontSizeDimensionsFormat,
																																	CFontGen::FONTS_SIZE[1],
																																	CFontGen::FONTS_SIZE[2],
																																	CFontGen::FONTS_SIZE[2] ) );
	SetDlgItemText( IDC_CHFS_SIZE_LARGE_LABEL_RIGHT, NStr::Format( strFontSizeDimensionsFormat,
																																	CFontGen::FONTS_SIZE[1],
																																	CFontGen::FONTS_SIZE[3],
																																	CFontGen::FONTS_SIZE[3] ) );

	CPINFOEX cpInfoEx;
	GetCPInfoEx( nCodePage, 0, &cpInfoEx );

	CString strBuffer;
	for ( TCHAR nCharIndex = 'A'; nCharIndex <= 'Z'; ++nCharIndex ) 
	{
		strBuffer += nCharIndex;
	}
	for ( TCHAR nCharIndex = 'a'; nCharIndex <= 'z'; ++nCharIndex ) 
	{
		strBuffer += nCharIndex;
	}
	for ( TCHAR nCharIndex = '0'; nCharIndex <= '9'; ++nCharIndex ) 
	{
		strBuffer += nCharIndex;
	}

	wndSample.SetWindowText( strBuffer );

	{
		LOGFONT logFont;
		{
			CFont *pFont = wndSample.GetFont();
			pFont->GetLogFont( &logFont );
		}
		sampleFont.CreateFontIndirect( &logFont );
	}

	bCreateControls = false;
}

void CChooseFontsDialog::UpdateControls()
{
	LOGFONT logFont;
	{
		CFont *pFont = wndSample.GetFont();
		pFont->GetLogFont( &logFont );
	}
	strcpy( logFont.lfFaceName, strFontName );
	CDC *pDC = GetDC();
	logFont.lfHeight = CFontGen::FONTS_SIZE[2];
	ReleaseDC( pDC );
	sampleFont.DeleteObject();
	sampleFont.CreateFontIndirect( &logFont );
	wndSample.SetFont( &sampleFont, true );
}

void CChooseFontsDialog::OnSelchangeFontComboBox() 
{
	if ( !bCreateControls )
	{
		CString strBuffer;
		wndFontComboBox.GetWindowText( strBuffer );
		if ( !strBuffer.IsEmpty() )
		{
			strFontName = strBuffer;
		}
		UpdateControls();
	}
}

void CChooseFontsDialog::OnSelchangeSizeNormalComboBox() 
{
	if ( !bCreateControls )
	{
		const int nItemIndex = wndNormalFontSizeComboBox.GetCurSel();
		if ( nItemIndex != CB_ERR )
		{
			dwNormalFontSize = wndNormalFontSizeComboBox.GetItemData( nItemIndex );
		}
		UpdateControls();
	}
}

void CChooseFontsDialog::OnSelchangeSizeLargeComboBox() 
{
	if ( !bCreateControls )
	{
		const int nItemIndex = wndLargeFontSizeComboBox.GetCurSel();
		if ( nItemIndex != CB_ERR )
		{
			dwLargeFontSize = wndLargeFontSizeComboBox.GetItemData( nItemIndex );
		}
		UpdateControls();
	}
}

void CChooseFontsDialog::OnDefaultButton() 
{
	strFontName = CFontGen::FONT_NAME;
	dwNormalFontSize = CFontGen::FONTS_SIZE[2];
	dwLargeFontSize = CFontGen::FONTS_SIZE[3];
	CreateControls();
	UpdateControls();
}

void CChooseFontsDialog::OnDestroy() 
{
	CResizeDialog::OnDestroy();
	
	sampleFont.DeleteObject();
}
