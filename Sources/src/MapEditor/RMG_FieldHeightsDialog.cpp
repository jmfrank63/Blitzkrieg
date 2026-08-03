#include "StdAfx.h"

#include "RMG_CreateFieldDialog.h"
#include "RMG_FieldHeightsDialog.h"

#include "ValuesCollector.h"

#include "../RandomMapGen/RMG_Types.h"
#include "../RandomMapGen/MapInfo_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CRMGFieldHeightsDialog::CRMGFieldHeightsDialog( CWnd* pParent )
	: CResizeDialog( CRMGFieldHeightsDialog::IDD, pParent ), pRMGCreateFieldDialog( 0 ), bCreateControls( true ), pRMFieldSet( 0 )
{

	SetControlStyle( IDC_CF_H_PROFILE_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_PROFILE_EDIT, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_CF_H_PROFILE_BROWSE_BUTTON, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDC_CF_H_HEIGHT_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_HEIGHT_EDIT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_HEIGHT_LABEL_RIGHT, ANCHORE_LEFT_TOP );
	
	SetControlStyle( IDC_CF_H_SIZE_MIN_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_SIZE_MIN_EDIT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_SIZE_MIN_LABEL_RIGHT, ANCHORE_LEFT_TOP );
	
	SetControlStyle( IDC_CF_H_SIZE_MAX_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_SIZE_MAX_EDIT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_SIZE_MAX_LABEL_RIGHT, ANCHORE_LEFT_TOP );
	
	SetControlStyle( IDC_CF_H_POSITIVE_RATIO_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_POSITIVE_RATIO_EDIT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_POSITIVE_RATIO_SLIDER, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_CF_H_POSITIVE_RATIO_SLIDER_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CF_H_POSITIVE_RATIO_SLIDER_LABEL_RIGHT, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDOK, ANCHORE_LEFT_TOP, ANCHORE_LEFT_TOP );
	SetControlStyle( IDCANCEL, ANCHORE_LEFT_TOP, ANCHORE_LEFT_TOP );
}

void CRMGFieldHeightsDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CF_H_SIZE_MIN_EDIT, m_SizeMinEdit);
	DDX_Control(pDX, IDC_CF_H_SIZE_MAX_EDIT, m_SizeMaxEdit);
	DDX_Control(pDX, IDC_CF_H_PROFILE_EDIT, m_ProfileEdit);
	DDX_Control(pDX, IDC_CF_H_POSITIVE_RATIO_EDIT, m_PositiveRatioEdit);
	DDX_Control(pDX, IDC_CF_H_HEIGHT_EDIT, m_HeightEdit);
	DDX_Control(pDX, IDC_CF_H_POSITIVE_RATIO_SLIDER, m_PositiveRatioSlider);
}

BEGIN_MESSAGE_MAP(CRMGFieldHeightsDialog, CResizeDialog)
	ON_EN_CHANGE(IDC_CF_H_HEIGHT_EDIT, OnChangeHeightEdit)
	ON_EN_CHANGE(IDC_CF_H_POSITIVE_RATIO_EDIT, OnChangePositiveRatioEdit)
	ON_EN_CHANGE(IDC_CF_H_PROFILE_EDIT, OnChangeProfileEdit)
	ON_EN_CHANGE(IDC_CF_H_SIZE_MAX_EDIT, OnChangeSizeMaxEdit)
	ON_EN_CHANGE(IDC_CF_H_SIZE_MIN_EDIT, OnChangeSizeMinEdit)
	ON_WM_HSCROLL()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_CF_H_PROFILE_BROWSE_BUTTON, OnProfileBrowseButton)
END_MESSAGE_MAP()

BOOL CRMGFieldHeightsDialog::OnInitDialog()
{
  CResizeDialog::OnInitDialog();
	
	CreateControls();
	return true;
}

void CRMGFieldHeightsDialog::CreateControls()
{
	bCreateControls = true;
	m_PositiveRatioSlider.SetRange( 0, 100 );
	m_PositiveRatioSlider.SetPos( 0 );
	bCreateControls = false;
}

void CRMGFieldHeightsDialog::UpdateControls()
{
	m_SizeMinEdit.EnableWindow( ( pRMFieldSet != 0 ) || ( fieldSets.size() > 0 ) );
	m_SizeMaxEdit.EnableWindow( ( pRMFieldSet != 0 ) || ( fieldSets.size() > 0 ) );
	m_ProfileEdit.EnableWindow( ( pRMFieldSet != 0 ) || ( fieldSets.size() > 0 ) );
	m_PositiveRatioEdit.EnableWindow( ( pRMFieldSet != 0 ) || ( fieldSets.size() > 0 ) );
	m_HeightEdit.EnableWindow( ( pRMFieldSet != 0 ) || ( fieldSets.size() > 0 ) );
	m_PositiveRatioSlider.EnableWindow( ( pRMFieldSet != 0 ) || ( fieldSets.size() > 0 ) );

	if ( CWnd *pWnd = GetDlgItem( IDC_CF_H_PROFILE_BROWSE_BUTTON ) )
	{
		pWnd->EnableWindow( ( pRMFieldSet != 0 ) || ( fieldSets.size() > 0 ) );
	}
}

void CRMGFieldHeightsDialog::LoadFieldToControls()
{
	bCreateControls = true;
	if ( pRMFieldSet != 0 )
	{
		CValuesCollector<int> sizeMinCollector( "...", 0 );
		CValuesCollector<int> sizeMaxCollector( "...", 0 );
		CValuesCollector<std::string> profileCollector( "...", "" );
		CValuesCollector<float> positiveRatioCollector( "...", 0.0f );
		CValuesCollector<float> heightCollector( "...", 0.0f );
		
		for ( std::vector<struct SRMFieldSet*>::const_iterator fieldIterator = fieldSets.begin(); fieldIterator != fieldSets.end(); ++fieldIterator )
		{
			SRMFieldSet *pFieldSet = *fieldIterator;
			sizeMinCollector.AddValue( pFieldSet->patternSize.min, "%d" );
			sizeMaxCollector.AddValue( pFieldSet->patternSize.max, "%d" );
			profileCollector.AddValue( pFieldSet->szProfileFileName, "%s" );
			heightCollector.AddValue( pFieldSet->fHeight, "%.2f" );
			positiveRatioCollector.AddValue( pFieldSet->fPositiveRatio * 100.0f, "%.2f" );
		}

		m_SizeMinEdit.SetWindowText( sizeMinCollector.GetStringValue().c_str() );
		m_SizeMaxEdit.SetWindowText( sizeMaxCollector.GetStringValue().c_str() );
		m_ProfileEdit.SetWindowText( profileCollector.GetStringValue().c_str() );
		m_HeightEdit.SetWindowText( heightCollector.GetStringValue().c_str() );
		m_PositiveRatioEdit.SetWindowText( positiveRatioCollector.GetStringValue().c_str() );
		m_PositiveRatioSlider.SetPos( positiveRatioCollector.GetValue() );
	}
	else
	{
		m_SizeMinEdit.SetWindowText( "" );
		m_SizeMaxEdit.SetWindowText( "" );
		m_ProfileEdit.SetWindowText( "" );
		m_PositiveRatioEdit.SetWindowText( "" );
		m_HeightEdit.SetWindowText( "" );
		m_PositiveRatioSlider.SetPos( 0 );
	}
	UpdateControls();
	bCreateControls = false;
}

void CRMGFieldHeightsDialog::OnOK() 
{
	if ( pRMGCreateFieldDialog )
	{
		pRMGCreateFieldDialog->OnOK();
	}
}

void CRMGFieldHeightsDialog::OnCancel() 
{
	if ( pRMGCreateFieldDialog )
	{
		pRMGCreateFieldDialog->OnCancel();
	}
}

void CRMGFieldHeightsDialog::OnChangeSizeMinEdit() 
{
	if ( !bCreateControls )
	{
		bCreateControls = true;
		if ( pRMFieldSet != 0 )
		{
			CString strBuffer;
			m_SizeMinEdit.GetWindowText( strBuffer );
			int nNewMinSize = 0;
			if ( ( sscanf( strBuffer, "%d", &nNewMinSize ) == 1 ) && ( nNewMinSize > 0 ) && ( nNewMinSize < 17 ) )
			{
				CValuesCollector<int> sizeMaxCollector( "...", 0 );
				for ( std::vector<struct SRMFieldSet*>::const_iterator fieldIterator = fieldSets.begin(); fieldIterator != fieldSets.end(); ++fieldIterator )
				{
					SRMFieldSet *pFieldSet = *fieldIterator;
					pFieldSet->patternSize.min = nNewMinSize;
					if ( pFieldSet->patternSize.max < pFieldSet->patternSize.min )
					{
						pFieldSet->patternSize.max = pFieldSet->patternSize.min;
					}
					sizeMaxCollector.AddValue( pFieldSet->patternSize.max, "%d" );
					pRMGCreateFieldDialog->UpdateFieldList( pFieldSet );
				}
				m_SizeMaxEdit.SetWindowText( sizeMaxCollector.GetStringValue().c_str() );
			}
		}
		bCreateControls = false;
	}
}

void CRMGFieldHeightsDialog::OnChangeSizeMaxEdit() 
{
	if ( !bCreateControls )
	{
		bCreateControls = true;
		if ( pRMFieldSet != 0 )
		{
			CString strBuffer;
			m_SizeMaxEdit.GetWindowText( strBuffer );
			int nNewMaxSize = 0;
			if ( ( sscanf( strBuffer, "%d", &nNewMaxSize ) == 1 ) && ( nNewMaxSize > 0 ) && ( nNewMaxSize < 17 ) )
			{
				CValuesCollector<int> sizeMinCollector( "...", 0 );
				for ( std::vector<struct SRMFieldSet*>::const_iterator fieldIterator = fieldSets.begin(); fieldIterator != fieldSets.end(); ++fieldIterator )
				{
					SRMFieldSet *pFieldSet = *fieldIterator;
					pFieldSet->patternSize.max = nNewMaxSize;
					if ( pFieldSet->patternSize.min > pFieldSet->patternSize.max )
					{
						pFieldSet->patternSize.min = pFieldSet->patternSize.max;
					}
					sizeMinCollector.AddValue( pFieldSet->patternSize.min, "%d" );
					pRMGCreateFieldDialog->UpdateFieldList( pFieldSet );
				}
				m_SizeMinEdit.SetWindowText( sizeMinCollector.GetStringValue().c_str() );
			}
		}
		bCreateControls = false;
	}
}

void CRMGFieldHeightsDialog::OnChangeHeightEdit() 
{
	if ( !bCreateControls )
	{
		bCreateControls = true;
		if ( pRMFieldSet != 0 )
		{
			CString strBuffer;
			m_HeightEdit.GetWindowText( strBuffer );
			float fNewHeight = 0.0f;
			if ( ( sscanf( strBuffer, "%f", &fNewHeight ) == 1 ) && ( fNewHeight >= 0.0f ) && ( fNewHeight <= 5.0f ) )
			{
				for ( std::vector<struct SRMFieldSet*>::const_iterator fieldIterator = fieldSets.begin(); fieldIterator != fieldSets.end(); ++fieldIterator )
				{
					SRMFieldSet *pFieldSet = *fieldIterator;
					pFieldSet->fHeight = fNewHeight;
					pRMGCreateFieldDialog->UpdateFieldList( pFieldSet );
				}
			}
		}
		bCreateControls = false;
	}
}

void CRMGFieldHeightsDialog::OnChangePositiveRatioEdit() 
{
	if ( !bCreateControls )
	{
		bCreateControls = true;
		if ( pRMFieldSet != 0 )
		{
			CString strBuffer;
			m_PositiveRatioEdit.GetWindowText( strBuffer );
			float fNewRatio = 0.0f;
			if ( ( sscanf( strBuffer, "%f", &fNewRatio ) == 1 ) && ( fNewRatio >= 0.0f ) && ( fNewRatio <= 100.0f ) )
			{
				for ( std::vector<struct SRMFieldSet*>::const_iterator fieldIterator = fieldSets.begin(); fieldIterator != fieldSets.end(); ++fieldIterator )
				{
					SRMFieldSet *pFieldSet = *fieldIterator;
					pFieldSet->fPositiveRatio = fNewRatio / 100.0f;
					pRMGCreateFieldDialog->UpdateFieldList( pFieldSet );
				}
				m_PositiveRatioSlider.SetPos( static_cast<int>( fNewRatio ) );
			}
		}
		bCreateControls = false;
	}
}

void CRMGFieldHeightsDialog::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	CResizeDialog ::OnHScroll( nSBCode, nPos, pScrollBar );
	if ( !bCreateControls )
	{
		bCreateControls = true;
		if ( pRMFieldSet != 0 )
		{
			CString strBuffer;
			float fNewRatio = m_PositiveRatioSlider.GetPos();
			for ( std::vector<struct SRMFieldSet*>::const_iterator fieldIterator = fieldSets.begin(); fieldIterator != fieldSets.end(); ++fieldIterator )
			{
				SRMFieldSet *pFieldSet = *fieldIterator;
				pFieldSet->fPositiveRatio = fNewRatio / 100.0f;
				pRMGCreateFieldDialog->UpdateFieldList( pFieldSet );
			}
			m_PositiveRatioEdit.SetWindowText( NStr::Format( "%.2f", fNewRatio ) );
		}
		bCreateControls = false;
	}
}

void CRMGFieldHeightsDialog::OnChangeProfileEdit() 
{
	if ( !bCreateControls )
	{
		bCreateControls = true;
		if ( pRMFieldSet != 0 )
		{
			CString strBuffer;
			m_ProfileEdit.GetWindowText( strBuffer );
			
			if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
			{
				if ( CPtr<IDataStream> pDataStream = pDataStorage->OpenStream( strBuffer + ".tga", STREAM_ACCESS_READ ) )
				{
					for ( std::vector<struct SRMFieldSet*>::const_iterator fieldIterator = fieldSets.begin(); fieldIterator != fieldSets.end(); ++fieldIterator )
					{
						SRMFieldSet *pFieldSet = *fieldIterator;
						pFieldSet->szProfileFileName = strBuffer;
						pRMGCreateFieldDialog->UpdateFieldList( pFieldSet );
					}
				}
			}
		}
		bCreateControls = false;
	}
}

void CRMGFieldHeightsDialog::OnProfileBrowseButton() 
{
	if ( pRMGCreateFieldDialog )
	{
		bCreateControls = true;
		std::string szInitialDir;
		int nSlashPosition = pRMGCreateFieldDialog->resizeDialogOptions.szParameters[4].rfind( '\\' );
		if ( nSlashPosition != std::string::npos )
		{
			szInitialDir =  pRMGCreateFieldDialog->resizeDialogOptions.szParameters[4].substr( 0, nSlashPosition );
		}
		
		CFileDialog fileDialog( true, ".tga", "", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "TGA files (*.tga)|*.tga|All Files (*.*)|*.*||" );
		fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
		fileDialog.m_ofn.lpstrFile[0] = 0;			
		fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
		fileDialog.m_ofn.lpstrInitialDir = szInitialDir.c_str();

		if ( fileDialog.DoModal() == IDOK )
		{
			std::string szFileName = fileDialog.GetPathName();
			pRMGCreateFieldDialog->resizeDialogOptions.szParameters[4] = szFileName;
			NStr::ToLower( szFileName );
			if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
			{
				std::string szStorageName = pDataStorage->GetName();
				NStr::ToLower( szStorageName );
				if ( szFileName.find( szStorageName ) == 0 )
				{
					szFileName = szFileName.substr( szStorageName.size() );
					if ( szFileName.rfind( ".tga" ) == szFileName.size() - 4 )
					{
						szFileName = szFileName.substr( 0, szFileName.size() - 4 );
					}
					m_ProfileEdit.SetWindowText( szFileName.c_str() );
				}
			}
		}
	}
	bCreateControls = false;
	OnChangeProfileEdit();
}
