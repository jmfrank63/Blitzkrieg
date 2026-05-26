#include "StdAfx.h"
#include "..\Common\StingrayCompat.h"

#include "resource.h"
#include "CreateRandomMapDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CCreateRandomMapProgress::CCreateRandomMapProgress()
{
}

CCreateRandomMapProgress::CCreateRandomMapProgress( CWnd *pWnd, int nNumSteps, const std::string &rszTitle, const std::string &rszMessage )
{
	progressDialog.Create( IDD_PROGRESS, pWnd );
	if ( progressDialog.GetSafeHwnd() != 0 )
	{
		progressDialog.ShowWindow( SW_SHOW ); 
		progressDialog.SetWindowText( rszTitle.c_str() );
		progressDialog.SetProgressRange( 0, nNumSteps ); 
		progressDialog.SetProgressMessage( rszMessage.c_str() );
	}
}

CCreateRandomMapProgress::~CCreateRandomMapProgress()
{
	if ( progressDialog.GetSafeHwnd() != 0 )
	{
		progressDialog.DestroyWindow();
	}
}

void CCreateRandomMapProgress::SetNumSteps( const int nRange, const float fPercentage )
{
	if ( progressDialog.GetSafeHwnd() != 0 )
	{
		progressDialog.SetProgressRange( 0, nRange );
	}
}

void CCreateRandomMapProgress::Step()
{
	if ( progressDialog.GetSafeHwnd() != 0 )
	{
		progressDialog.IterateProgressPosition();
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CCreateRandomMapDialog::vID[] = 
{
	IDC_CRM_TEMPLATE_LABEL,									//0
	IDC_CRM_TEMPLATE_EDIT,									//1
	IDC_CRM_TEMPLATE_BROWSE_BUTTON,					//2
	IDC_CRM_CONTEXT_LABEL,									//3
	IDC_CRM_CONTEXT_EDIT,										//4
	IDC_CRM_CONTEXT_BROWSE_BUTTON,					//5
	IDC_CRM_GRAPH_LABEL,										//6
	IDC_CRM_GRAPH_EDIT,											//7
	IDC_CRM_SETTING_LABEL,									//8
	IDC_CRM_SETTING_COMBOBOX,								//9
	IDC_CRM_DIRECTIONS_RADIOBUTTONS_LABEL,	//10
	IDC_CRM_DIRECTION_RADIO_0,							//11
	IDC_CRM_DIRECTION_RADIO_90,							//12
	IDC_CRM_DIRECTION_RADIO_180,						//13
	IDC_CRM_DIRECTION_RADIO_270,						//14
	IDC_CRM_LEVEL_RADIOBUTTONS_LABEL,				//15
	IDC_CRM_LEVEL_RADIO_0,									//16
	IDC_CRM_LEVEL_RADIO_1,									//17
	IDC_CRM_LEVEL_RADIO_2,									//18
	IDC_CRM_SAVE_AS_BZM_CHECKBOX,						//19
	IDC_CRM_SAVE_AS_DDS_CHECKBOX,						//20
	IDC_CRM_MAP_LABEL,											//22
	IDC_CRM_MAP_EDIT,												//23
	IDC_CRM_MAP_BROWSE_BUTTON,							//24
	IDOK,																		//25
	IDCANCEL,																//26
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CCreateRandomMapDialog::CCreateRandomMapDialog( CWnd* pParent )
	: CResizeDialog( CCreateRandomMapDialog::IDD, pParent ), bCreateControls( true )
{
	//{{AFX_DATA_INIT(CCreateRandomMapDialog)
	m_SaveAsBZM = false;
	m_SaveAsDDS = false;
	//}}AFX_DATA_INIT

	SetControlStyle( IDC_CRM_TEMPLATE_LABEL,								ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CRM_TEMPLATE_EDIT,									ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_CRM_TEMPLATE_BROWSE_BUTTON,				ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_CRM_CONTEXT_LABEL,									ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CRM_CONTEXT_EDIT,									ANCHORE_LEFT_TOP | RESIZE_HOR);
	SetControlStyle( IDC_CRM_CONTEXT_BROWSE_BUTTON,					ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_CRM_GRAPH_LABEL,										ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CRM_GRAPH_EDIT,										ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_CRM_SETTING_LABEL,									ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CRM_SETTING_COMBOBOX,							ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_CRM_DIRECTIONS_RADIOBUTTONS_LABEL,	ANCHORE_LEFT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_CRM_DIRECTION_RADIO_0,							ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CRM_DIRECTION_RADIO_90,						ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CRM_DIRECTION_RADIO_180,						ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_CRM_DIRECTION_RADIO_270,						ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_CRM_LEVEL_RADIOBUTTONS_LABEL,			ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_CRM_LEVEL_RADIO_0,									ANCHORE_TOP | ANCHORE_HOR_CENTER);
	SetControlStyle( IDC_CRM_LEVEL_RADIO_1,									ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_CRM_LEVEL_RADIO_2,									ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_CRM_SAVE_AS_BZM_CHECKBOX,					ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CRM_SAVE_AS_DDS_CHECKBOX,					ANCHORE_TOP | ANCHORE_HOR_CENTER );
	SetControlStyle( IDC_CRM_MAP_LABEL,											ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_CRM_MAP_EDIT,											ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_CRM_MAP_BROWSE_BUTTON,							ANCHORE_RIGHT_TOP );
	SetControlStyle( IDOK,																	ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
	SetControlStyle( IDCANCEL,															ANCHORE_BOTTOM | ANCHORE_HOR_CENTER );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP( CCreateRandomMapDialog )
	DDX_Control(pDX, IDC_CRM_SETTING_COMBOBOX, m_Setting);
	DDX_Control(pDX, IDC_CRM_TEMPLATE_EDIT, m_Template);
	DDX_Control(pDX, IDC_CRM_MAP_EDIT, m_Map);
	DDX_Control(pDX, IDC_CRM_GRAPH_EDIT, m_Graph);
	DDX_Control(pDX, IDC_CRM_CONTEXT_EDIT, m_Context);
	DDX_Check(pDX, IDC_CRM_SAVE_AS_BZM_CHECKBOX, m_SaveAsBZM);
	DDX_Check(pDX, IDC_CRM_SAVE_AS_DDS_CHECKBOX, m_SaveAsDDS);
	//}}AFX_DATA_MAP
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CCreateRandomMapDialog, CResizeDialog)
	//{{AFX_MSG_MAP(CCreateRandomMapDialog)
	ON_EN_CHANGE(IDC_CRM_GRAPH_EDIT, OnChangeCrmGraphEdit)
	ON_EN_CHANGE(IDC_CRM_MAP_EDIT, OnChangeCrmMapEdit)
	ON_CBN_SELCHANGE(IDC_CRM_SETTING_COMBOBOX, OnSelchangeCrmSettingCombobox)
	ON_BN_CLICKED(IDC_CRM_MAP_BROWSE_BUTTON, OnCrmMapBrowseButton)
	ON_BN_CLICKED(IDC_CRM_TEMPLATE_BROWSE_BUTTON, OnCrmTemplateBrowseButton)
	ON_BN_CLICKED(IDC_CRM_CONTEXT_BROWSE_BUTTON, OnCrmContextBrowseButton)
	ON_CBN_EDITCHANGE(IDC_CRM_CONTEXT_EDIT, OnEditchangeCrmContextEdit)
	ON_CBN_SELCHANGE(IDC_CRM_CONTEXT_EDIT, OnSelchangeCrmContextEdit)
	ON_CBN_EDITCHANGE(IDC_CRM_TEMPLATE_EDIT, OnEditchangeCrmTemplateEdit)
	ON_CBN_SELCHANGE(IDC_CRM_TEMPLATE_EDIT, OnSelchangeCrmTemplateEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::ValidatePath( std::string *pszPath ) 
{
	if ( IDataStorage *pDataStorage = GetSingleton<IDataStorage>() )
	{
		std::string szStorageName = pDataStorage->GetName();
		NStr::ToLower( szStorageName );
		NStr::ToLower( ( *pszPath ) );
		int nPos = pszPath->find( szStorageName );
		if ( nPos == 0 )
		{
			( *pszPath ) = pszPath->substr( nPos + szStorageName.size() );
		}
		( *pszPath ) = pszPath->substr( 0, pszPath->rfind( '.' ) );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CCreateRandomMapDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	bCreateControls = true;
	//0 Template
	//1 Context
	//2	Setting
	//3	Map
	if ( resizeDialogOptions.szParameters.size() < 4 )
	{
		resizeDialogOptions.szParameters.resize( 4, "" );
	}

	//0	Graph
	//1 Direction
	//2 Level
	//3 Save As BZM
	//3 Save As DDS
	if ( resizeDialogOptions.nParameters.size() < 5 )
	{
		resizeDialogOptions.nParameters.resize( 5, 0 );
	}

	for ( std::vector<std::string>::const_iterator templateIterator = szTemplates.begin(); templateIterator != szTemplates.end(); ++templateIterator )
	{
		m_Template.AddString( templateIterator->c_str() );
	}
	for ( std::vector<std::string>::const_iterator contextIterator = szContexts.begin(); contextIterator != szContexts.end(); ++contextIterator )
	{
		m_Context.AddString( contextIterator->c_str() );
	}
	for ( std::vector<std::string>::const_iterator placeIterator = szSettings.begin(); placeIterator != szSettings.end(); ++placeIterator )
	{
		m_Setting.AddString( placeIterator->c_str() );
	}
	m_Template.SelectString( -1, resizeDialogOptions.szParameters[0].c_str() );
	m_Context.SelectString( -1, resizeDialogOptions.szParameters[1].c_str() );
	m_Setting.SelectString( -1, resizeDialogOptions.szParameters[2].c_str() );
	
	m_Map.SetWindowText( resizeDialogOptions.szParameters[3].c_str() );

	m_Graph.SetWindowText( NStr::Format( "%d", resizeDialogOptions.nParameters[0] ) );
	CheckRadioButton( IDC_CRM_DIRECTION_RADIO_0,
										IDC_CRM_DIRECTION_RADIO_270,
										IDC_CRM_DIRECTION_RADIO_0 + resizeDialogOptions.nParameters[1] );
	CheckRadioButton( IDC_CRM_LEVEL_RADIO_0,
										IDC_CRM_LEVEL_RADIO_2,
										IDC_CRM_LEVEL_RADIO_0 + resizeDialogOptions.nParameters[2] );
	CheckDlgButton( IDC_CRM_SAVE_AS_BZM_CHECKBOX, resizeDialogOptions.nParameters[3] > 0 ? BST_CHECKED : BST_UNCHECKED );
	CheckDlgButton( IDC_CRM_SAVE_AS_DDS_CHECKBOX, resizeDialogOptions.nParameters[4] > 0 ? BST_CHECKED : BST_UNCHECKED );
	
	UpdateControls();

	bCreateControls = false;
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void	CCreateRandomMapDialog::UpdateControls()
{
	CString strText = NStr::Format( "%d", &( resizeDialogOptions.nParameters[0] ) );
	if ( !bCreateControls )
	{
		m_Template.GetWindowText( strText );
		resizeDialogOptions.szParameters[0] = std::string( LPCTSTR( strText ) );
		ValidatePath( &( resizeDialogOptions.szParameters[0] ) );

		m_Context.GetWindowText( strText );
		resizeDialogOptions.szParameters[1] = std::string( LPCTSTR( strText ) );
		ValidatePath( &( resizeDialogOptions.szParameters[1] ) );

		m_Setting.GetWindowText( strText );
		resizeDialogOptions.szParameters[2] = std::string( LPCTSTR( strText ) );
		ValidatePath( &( resizeDialogOptions.szParameters[2] ) );

		m_Map.GetWindowText( strText );
		resizeDialogOptions.szParameters[3] = std::string( LPCTSTR( strText ) );
		ValidatePath( &( resizeDialogOptions.szParameters[3] ) );

		m_Graph.GetWindowText( strText );
		sscanf( LPCTSTR( strText ), "%d", &( resizeDialogOptions.nParameters[0] ) );
		
		resizeDialogOptions.nParameters[1] = GetCheckedRadioButton( IDC_CRM_DIRECTION_RADIO_0, IDC_CRM_DIRECTION_RADIO_270 ) - IDC_CRM_DIRECTION_RADIO_0;
		resizeDialogOptions.nParameters[2] = GetCheckedRadioButton( IDC_CRM_LEVEL_RADIO_0, IDC_CRM_LEVEL_RADIO_2 ) - IDC_CRM_LEVEL_RADIO_0;
		resizeDialogOptions.nParameters[3] = IsDlgButtonChecked( IDC_CRM_SAVE_AS_BZM_CHECKBOX );
		resizeDialogOptions.nParameters[4] = IsDlgButtonChecked( IDC_CRM_SAVE_AS_DDS_CHECKBOX );
	}
	
	if ( CWnd *pWnd = GetDlgItem( IDOK ) )
	{
		pWnd->EnableWindow( ( !resizeDialogOptions.szParameters[0].empty() ) &&
												( !resizeDialogOptions.szParameters[1].empty() ) && 
												( !resizeDialogOptions.szParameters[2].empty() ) && 
												( !resizeDialogOptions.szParameters[3].empty() ) &&
												( !strText.IsEmpty() ) );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnOK() 
{
	UpdateControls();
	CResizeDialog::OnOK();
}

void CCreateRandomMapDialog::OnCancel() 
{
	UpdateControls();
	CResizeDialog::OnCancel();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnChangeCrmGraphEdit() 
{
	UpdateControls();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnChangeCrmMapEdit() 
{
	UpdateControls();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnSelchangeCrmSettingCombobox() 
{
	UpdateControls();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnCrmTemplateBrowseButton() 
{
	CString strDialogTitle = _T( "Open Template" );
	CString strFileName;
	CString strFolderName;

	std::string szStorageName;
	if ( IDataStorage *pDataStorage = GetSingleton<IDataStorage>() )
	{
		szStorageName = pDataStorage->GetName();
	}
	m_Template.GetWindowText( strFileName );
	strFileName = szStorageName.c_str() + strFileName;

	int nSlashPos = strFileName.ReverseFind( '\\' );
	if ( nSlashPos >= 0 )
	{
		strFolderName = strFileName.Left( nSlashPos );
	}
	
	CFileDialog fileDialog( true, ".xml", "", OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "XML files (*.xml)|*.xml|All Files (*.*)|*.*||" );
	fileDialog.m_ofn.lpstrTitle = strDialogTitle;
	fileDialog.m_ofn.lpstrInitialDir = strFolderName;
	
	if ( fileDialog.DoModal() == IDOK )
	{
		bCreateControls = true;

		resizeDialogOptions.szParameters[0] = std::string( LPCTSTR( fileDialog.GetPathName() ) );
		ValidatePath( &( resizeDialogOptions.szParameters[0] ) );
		m_Template.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );

		UpdateControls();

		bCreateControls = false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnCrmContextBrowseButton() 
{
	CString strDialogTitle = _T( "Open Context" );
	CString strFileName;
	CString strFolderName;

	std::string szStorageName;
	if ( IDataStorage *pDataStorage = GetSingleton<IDataStorage>() )
	{
		szStorageName = pDataStorage->GetName();
	}
	m_Context.GetWindowText( strFileName );
	strFileName = szStorageName.c_str() + strFileName;

	int nSlashPos = strFileName.ReverseFind( '\\' );
	if ( nSlashPos >= 0 )
	{
		strFolderName = strFileName.Left( nSlashPos );
	}
	
	CFileDialog fileDialog( true, ".xml", "", OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "XML files (*.xml)|*.xml|All Files (*.*)|*.*||" );
	fileDialog.m_ofn.lpstrTitle = strDialogTitle;
	fileDialog.m_ofn.lpstrInitialDir = strFolderName;
	
	if ( fileDialog.DoModal() == IDOK )
	{
		bCreateControls = true;

		resizeDialogOptions.szParameters[1] = std::string( LPCTSTR( fileDialog.GetPathName() ) );
		ValidatePath( &( resizeDialogOptions.szParameters[1] ) );
		m_Context.SetWindowText( resizeDialogOptions.szParameters[1].c_str() );

		UpdateControls();

		bCreateControls = false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnCrmMapBrowseButton() 
{
	CString strDialogTitle = _T( "Open Map" );
	CString strFileName;
	CString strFolderName;

	std::string szStorageName;
	if ( IDataStorage *pDataStorage = GetSingleton<IDataStorage>() )
	{
		szStorageName = pDataStorage->GetName();
	}
	m_Map.GetWindowText( strFileName );
	strFileName = szStorageName.c_str() + strFileName;

	int nSlashPos = strFileName.ReverseFind( '\\' );
	if ( nSlashPos >= 0 )
	{
		strFolderName = strFileName.Left( nSlashPos );
	}
	
	CFileDialog fileDialog( true, ".bzm", "", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "All supported Files (*.bzm; *.xml)|*.bzm; *.xml|XML files (*.xml)|*.xml|BZM files (*.bzm)|*.bzm|All Files (*.*)|*.*||" );
	fileDialog.m_ofn.lpstrTitle = strDialogTitle;
	fileDialog.m_ofn.lpstrInitialDir = strFolderName;
	
	if ( fileDialog.DoModal() == IDOK )
	{
		bCreateControls = true;

		resizeDialogOptions.szParameters[3] = std::string( LPCTSTR( fileDialog.GetPathName() ) );
		ValidatePath( &( resizeDialogOptions.szParameters[3] ) );
		m_Map.SetWindowText( resizeDialogOptions.szParameters[3].c_str() );

		UpdateControls();

		bCreateControls = false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnEditchangeCrmContextEdit() 
{
	UpdateControls();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnSelchangeCrmContextEdit() 
{
	UpdateControls();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnEditchangeCrmTemplateEdit() 
{
	UpdateControls();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCreateRandomMapDialog::OnSelchangeCrmTemplateEdit() 
{
	UpdateControls();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// basement storage  
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
void CCreateRandomMapDialog::OnPAKBrowseButton() 
{
	CString strDialogTitle;
	CString strFileName;
	CString strFolderName;

	strDialogTitle.LoadString( IDS_IFP_BROWSE_FOR_PAK_DIALOG_TITLE );

	m_PAKEdit.GetWindowText( strFileName );
	int nSlashPos = strFileName.ReverseFind( '\\' );
	if ( nSlashPos >= 0 )
	{
		strFolderName = strFileName.Left( nSlashPos );
	}
	
	CFileDialog fileDialog( true, ".pak", strFileName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "PAK files (*.pak)|*.pak|All Files (*.*)|*.*||", this );
	fileDialog.m_ofn.lpstrTitle = strDialogTitle;
	fileDialog.m_ofn.lpstrInitialDir = strFolderName;
	
	if ( fileDialog.DoModal() == IDOK )
	{
		strFileName = fileDialog.GetPathName();

		int nSlashPos = strFileName.ReverseFind( '.' );
		if ( nSlashPos < 0 )
		{
			strFileName += ".pak";
		}

		resizeDialogOptions.szParameters[0] = strFileName;
		m_PAKEdit.SetWindowText( resizeDialogOptions.szParameters[0].c_str() );

		UpdateControls();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CImportFromPAKDialog::OnChangePAKBrowseEdit() 
{
	CString strFolderName;
	m_PAKEdit.GetWindowText( strFolderName );
	resizeDialogOptions.szParameters[0] = strFolderName;
	UpdateControls();
}


/**/