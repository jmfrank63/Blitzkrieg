#include "StdAfx.h"

#include "editor.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"

#include "TabTerrainFieldsDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTabTerrainFieldsDialog::CTabTerrainFieldsDialog( CWnd* pParent )
	: CResizeDialog( CTabTerrainFieldsDialog::IDD, pParent ), bCreateControls( true )
{
	
	SetControlStyle( IDC_FIELD_FILEDS_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_FIELD_BROWSE, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_FIELD_FIELD_COMBO, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_FIELD_RANDOMIZE_POLYGON_CHECK_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_FIELD_MIN_LENGTH_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_FIELD_MIN_LENGTH_EDIT, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_FIELD_WIDTH_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_FIELD_WIDTH_EDIT, ANCHORE_RIGHT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_FIELD_DISTURBANCE_LABEL, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_FIELD_DISTURBANCE_EDIT, ANCHORE_RIGHT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_FIELD_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_FIELD_REMOVE_OBJECTS_CHECK_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_FIELD_DELIMITER_01, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_FIELD_FILL_TERRAIN_CHECK_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_FIELD_FILL_OBJECTS_CHECK_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_FIELD_FILL_HEIGHTS_CHECK_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_FIELD_DELIMITER_02, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_FIELD_CHECK_PASSABILITY_CHECK_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_FIELD_UPDATE_MAP_CHECK_BOX, ANCHORE_LEFT_TOP | RESIZE_HOR );
}

BEGIN_MESSAGE_MAP( CTabTerrainFieldsDialog, CResizeDialog )
	ON_CBN_SELCHANGE(IDC_FIELD_FIELD_COMBO, OnSelchangeFieldFieldCombo)
	ON_CBN_EDITCHANGE(IDC_FIELD_FIELD_COMBO, OnEditchangeFieldFieldCombo)
	ON_BN_CLICKED(IDC_FIELD_BROWSE, OnFieldBrowse)
	ON_EN_CHANGE(IDC_FIELD_MIN_LENGTH_EDIT, OnChangeFieldMinLengthEdit)
	ON_EN_CHANGE(IDC_FIELD_WIDTH_EDIT, OnChangeFieldWidthEdit)
	ON_EN_CHANGE(IDC_FIELD_DISTURBANCE_EDIT, OnChangeFieldDisturbanceEdit)
	ON_BN_CLICKED(IDC_FIELD_RANDOMIZE_POLYGON_CHECK_BOX, OnFieldRandomizePolygonCheckBox)
	ON_BN_CLICKED(IDC_FIELD_REMOVE_OBJECTS_CHECK_BOX, OnFieldRemoveObjectsCheckBox)
	ON_BN_CLICKED(IDC_FIELD_FILL_TERRAIN_CHECK_BOX, OnFieldFillTerrainCheckBox)
	ON_BN_CLICKED(IDC_FIELD_FILL_OBJECTS_CHECK_BOX, OnFieldFillObjectsCheckBox)
	ON_BN_CLICKED(IDC_FIELD_FILL_HEIGHTS_CHECK_BOX, OnFieldFillHeightsCheckBox)
	ON_BN_CLICKED(IDC_FIELD_UPDATE_MAP_CHECK_BOX, OnFieldUpdateMapCheckBox)
	ON_BN_CLICKED(IDC_FIELD_CHECK_PASSABILITY_CHECK_BOX, OnFieldCheckPassabilityCheckBox)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CTabTerrainFieldsDialog::DoDataExchange( CDataExchange* pDX )
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FIELD_FIELD_COMBO, m_Fields);
	DDX_Control(pDX, IDC_FIELD_MIN_LENGTH_EDIT, m_MinLength);
	DDX_Control(pDX, IDC_FIELD_WIDTH_EDIT, m_Width);
	DDX_Control(pDX, IDC_FIELD_DISTURBANCE_EDIT, m_Disturbance);
	DDX_Control(pDX, IDC_FIELD_RANDOMIZE_POLYGON_CHECK_BOX, m_RandomizePolygonButton);
	DDX_Control(pDX, IDC_FIELD_REMOVE_OBJECTS_CHECK_BOX, m_RemoveObjectsButton);
	DDX_Control(pDX, IDC_FIELD_FILL_TERRAIN_CHECK_BOX, m_FillTerrainButton);
	DDX_Control(pDX, IDC_FIELD_FILL_OBJECTS_CHECK_BOX, m_FillObjectsButton);
	DDX_Control(pDX, IDC_FIELD_FILL_HEIGHTS_CHECK_BOX, m_FillHeightsButton);
	DDX_Control(pDX, IDC_FIELD_UPDATE_MAP_CHECK_BOX, m_UpdateMapButton);
	DDX_Control(pDX, IDC_FIELD_CHECK_PASSABILITY_CHECK_BOX, m_CheckPassabilityButton);
}

BOOL CTabTerrainFieldsDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();

	if ( resizeDialogOptions.szParameters.size() < 2 )
	{
		resizeDialogOptions.szParameters.resize( 2 );	
		resizeDialogOptions.szParameters[0].clear();
		if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
		{
			resizeDialogOptions.szParameters[1] = std::string( pDataStorage->GetName() ) + "scenarios\\fieldsets\\";
		}
		else
		{
			resizeDialogOptions.szParameters[1].clear();
		}
	}

	if ( resizeDialogOptions.nParameters.size() < 7 )
	{
		resizeDialogOptions.nParameters.resize( 7 );
		resizeDialogOptions.nParameters[0] = 1;
		resizeDialogOptions.nParameters[1] = 0;
		resizeDialogOptions.nParameters[2] = 1;
		resizeDialogOptions.nParameters[3] = 1;
		resizeDialogOptions.nParameters[4] = 1;
		resizeDialogOptions.nParameters[5] = 1;
		resizeDialogOptions.nParameters[6] = 1;
	}

	if ( resizeDialogOptions.fParameters.size() < 3 )
	{
		resizeDialogOptions.fParameters.resize( 3 );
		resizeDialogOptions.fParameters[0] = 4.0f;
		resizeDialogOptions.fParameters[1] = 0.3f;
		resizeDialogOptions.fParameters[2] = 0.1f;
	}

	CreateControls();
	return true;
}

void CTabTerrainFieldsDialog::CreateControls()
{
	bCreateControls = true;
	const std::string szFieldsFolder( "scenarios\\fieldsets\\" );

	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		std::list<std::string> files;
		if ( pFrame->GetEnumFilesInDataStorage( szFieldsFolder, &files ) )
		{
			for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
			{
				std::string szFileName = *fileIterator;
				NStr::ToLower( szFileName );
				if ( szFileName.rfind( ".xml" ) == szFileName.size() - 4 )
				{
					szFileName = szFileName.substr( 0, szFileName.size() - 4 );
				}
				int nAddedItem = m_Fields.AddString( szFileName.c_str() );
			}
		}
	}	

	m_RandomizePolygonButton.SetCheck( resizeDialogOptions.nParameters[0] );
	m_RemoveObjectsButton.SetCheck( resizeDialogOptions.nParameters[1] );
	m_FillTerrainButton.SetCheck( resizeDialogOptions.nParameters[2] );
	m_FillObjectsButton.SetCheck( resizeDialogOptions.nParameters[3] );
	m_FillHeightsButton.SetCheck( resizeDialogOptions.nParameters[4] );
	m_UpdateMapButton.SetCheck( resizeDialogOptions.nParameters[5] );
	m_CheckPassabilityButton.SetCheck( resizeDialogOptions.nParameters[6] );

	SetControlsToActualValues();
	bCreateControls = false;
}

void CTabTerrainFieldsDialog::OnFieldBrowse() 
{
	std::string szInitialDir;
	int nSlashPosition = resizeDialogOptions.szParameters[1].rfind( '\\' );
	if ( nSlashPosition != std::string::npos )
	{
		szInitialDir =  resizeDialogOptions.szParameters[1].substr( 0, nSlashPosition );
	}
	
	CFileDialog fileDialog( true, ".xml", "", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "XML files (*.xml)|*.xml|All Files (*.*)|*.*||" );
	fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
	fileDialog.m_ofn.lpstrFile[0] = 0;			
	fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
	fileDialog.m_ofn.lpstrInitialDir = szInitialDir.c_str();

	if ( fileDialog.DoModal() == IDOK )
	{
		std::string szFileName = fileDialog.GetPathName();
		resizeDialogOptions.szParameters[1] = szFileName;
		NStr::ToLower( szFileName );
		if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
		{
			std::string szStorageName = pDataStorage->GetName();
			NStr::ToLower( szStorageName );
			if ( szFileName.find( szStorageName ) == 0 )
			{
				szFileName = szFileName.substr( szStorageName.size() );
				if ( szFileName.rfind( ".xml" ) == szFileName.size() - 4 )
				{
					szFileName = szFileName.substr( 0, szFileName.size() - 4 );
				}
				m_Fields.SetWindowText( szFileName.c_str() );
				OnEditchangeFieldFieldCombo();
			}
		}
	}
}

void CTabTerrainFieldsDialog::OnSelchangeFieldFieldCombo() 
{
	if ( !bCreateControls )
	{
		int nSelectedIndex = m_Fields.GetCurSel();
		if ( nSelectedIndex != CB_ERR )
		{
			CString strBuffer;
			m_Fields.GetLBText( nSelectedIndex, strBuffer );
			resizeDialogOptions.szParameters[0] = strBuffer;
		}
	}
}

void CTabTerrainFieldsDialog::OnEditchangeFieldFieldCombo() 
{
	if ( !bCreateControls )
	{
		CString strBuffer;
		m_Fields.GetWindowText( strBuffer );

		if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
		{
			if ( CPtr<IDataStream> pDataStream = pDataStorage->OpenStream( strBuffer + ".xml", STREAM_ACCESS_READ ) )
			{
				resizeDialogOptions.szParameters[0] = strBuffer;
			}
		}
	}
}

void CTabTerrainFieldsDialog::OnChangeFieldMinLengthEdit() 
{
	if ( !bCreateControls )
	{
		CString strBuffer;
		m_MinLength.GetWindowText( strBuffer );
		float fNewMinLength = 2.0f;
		if ( ( sscanf( strBuffer , "%f", &fNewMinLength ) == 1 ) && ( fNewMinLength >= 2.0f ) )
		{
			resizeDialogOptions.fParameters[0] = fNewMinLength;
		}
	}
}

void CTabTerrainFieldsDialog::OnChangeFieldWidthEdit() 
{
	if ( !bCreateControls )
	{
		CString strBuffer;
		m_Width.GetWindowText( strBuffer );
		float fNewWidth = 0.0f;
		if ( ( sscanf( strBuffer , "%f", &fNewWidth ) == 1 ) && ( fNewWidth >= 0.0f ) && ( fNewWidth <= 0.5f ) )
		{
			resizeDialogOptions.fParameters[1] = fNewWidth;
		}
	}
}

void CTabTerrainFieldsDialog::OnChangeFieldDisturbanceEdit() 
{
	if ( !bCreateControls )
	{
		CString strBuffer;
		m_Disturbance.GetWindowText( strBuffer );
		float fNewDisturbance = 0.0f;
		if ( ( sscanf( strBuffer , "%f", &fNewDisturbance ) == 1 ) && ( fNewDisturbance >= 0.0f ) && ( fNewDisturbance <= 1.0f ) )
		{
			resizeDialogOptions.fParameters[2] = fNewDisturbance;
		}
	}
}

void CTabTerrainFieldsDialog::OnFieldRandomizePolygonCheckBox() 
{
	if ( !bCreateControls )
	{
		resizeDialogOptions.nParameters[0] = m_RandomizePolygonButton.GetCheck();
	}
}

void CTabTerrainFieldsDialog::OnFieldRemoveObjectsCheckBox() 
{
	if ( !bCreateControls )
	{
		resizeDialogOptions.nParameters[1] = m_RemoveObjectsButton.GetCheck();
	}
}

void CTabTerrainFieldsDialog::OnFieldFillTerrainCheckBox() 
{
	if ( !bCreateControls )
	{
		resizeDialogOptions.nParameters[2] = m_FillTerrainButton.GetCheck();
	}
}

void CTabTerrainFieldsDialog::OnFieldFillObjectsCheckBox() 
{
	if ( !bCreateControls )
	{
		resizeDialogOptions.nParameters[3] = m_FillObjectsButton.GetCheck();
	}
}

void CTabTerrainFieldsDialog::OnFieldFillHeightsCheckBox() 
{
	if ( !bCreateControls )
	{
		resizeDialogOptions.nParameters[4] = m_FillHeightsButton.GetCheck();
	}
}

void CTabTerrainFieldsDialog::OnFieldUpdateMapCheckBox()
{
	if ( !bCreateControls )
	{
		resizeDialogOptions.nParameters[5] = m_UpdateMapButton.GetCheck();
	}
}

void CTabTerrainFieldsDialog::OnFieldCheckPassabilityCheckBox()
{
	if ( !bCreateControls )
	{
		resizeDialogOptions.nParameters[6] = m_CheckPassabilityButton.GetCheck();
	}
}

void CTabTerrainFieldsDialog::OnDestroy() 
{
	CResizeDialog::SaveResizeDialogOptions();
	CResizeDialog ::OnDestroy();
}

void CTabTerrainFieldsDialog::SetControlsToActualValues()
{
	bCreateControls = true;
	if (  m_Fields.SelectString( CB_ERR, resizeDialogOptions.szParameters[0].c_str() ) == CB_ERR )
	{
		m_Fields.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );
	}
	m_MinLength.SetWindowText( NStr::Format( "%.2f", resizeDialogOptions.fParameters[0] ) );
	m_Width.SetWindowText( NStr::Format( "%.2f", resizeDialogOptions.fParameters[1] ) );
	m_Disturbance.SetWindowText( NStr::Format( "%.2f", resizeDialogOptions.fParameters[2] ) );
	bCreateControls = false;
}
