#include "StdAfx.h"

#include "../RandomMapGen/Resource_Types.h"
#include "../RandomMapGen/MapInfo_Types.h"

#include "frames.h"
#include "TemplateEditorframe1.h"
#include "RMG_CreateTemplateDialog.h"
#include "RMG_TemplateGraphPropertiesDialog.h"
#include "RMG_TemplateVSOPropertiesDialog.h"
#include "RMG_TemplateFieldPropertiesDialog.h"
#include "TabSimpleObjectsDiplomacyDialog.h"

#include "UnitCreation.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const char *CT_TEMPLATES_XML_NAME = "Templates";
const char *CT_TEMPLATES_FILE_NAME = "Editor\\DefaultTemplates";
const char *CT_TEMPLATES_DIALOG_TITLE = "Templates Composer";

const int   CT_TEMPLATES_COLUMN_START = 0;
const int   CT_TEMPLATES_COLUMN_COUNT = 13;
const char *CT_TEMPLATES_COLUMN_NAME  [CT_TEMPLATES_COLUMN_COUNT] = { "Path", "Size", "Season", "Players", "Graphs", "Fields", "VSO", "Season Folder", "Supported Settings", "Used Script IDs", "Used Script Areas", "Script Name", "MOD" };
const int   CT_TEMPLATES_COLUMN_FORMAT[CT_TEMPLATES_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
const int		CT_TEMPLATES_COLUMN_WIDTH [CT_TEMPLATES_COLUMN_COUNT] = { 200, 60, 60, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80 };

const int   CT_FIELDS_COLUMN_START = CT_TEMPLATES_COLUMN_COUNT;
const int   CT_FIELDS_COLUMN_COUNT = 5;
const char *CT_FIELDS_COLUMN_NAME  [CT_FIELDS_COLUMN_COUNT] = { "Path", "Weight", "Default", "Terrain Shells", "Objects Shells" };
const int   CT_FIELDS_COLUMN_FORMAT[CT_FIELDS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
const int		CT_FIELDS_COLUMN_WIDTH [CT_FIELDS_COLUMN_COUNT] = { 200, 60, 60, 80, 80 };

const int   CT_VSO_COLUMN_START = CT_TEMPLATES_COLUMN_COUNT + CT_FIELDS_COLUMN_COUNT;
const int   CT_VSO_COLUMN_COUNT = 4;
const char *CT_VSO_COLUMN_NAME  [CT_VSO_COLUMN_COUNT] = { "Path", "Weight", "Width", "Opacity" };
const int   CT_VSO_COLUMN_FORMAT[CT_VSO_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT };
const	int		CT_VSO_COLUMN_WIDTH [CT_VSO_COLUMN_COUNT] = { 200, 60, 80, 80 };

const int   CT_GRAPHS_COLUMN_START = CT_TEMPLATES_COLUMN_COUNT + CT_FIELDS_COLUMN_COUNT + CT_VSO_COLUMN_COUNT;
const int   CT_GRAPHS_COLUMN_COUNT = 9;
const char *CT_GRAPHS_COLUMN_NAME  [CT_GRAPHS_COLUMN_COUNT] = { "Path", "Weight", "Nodes", "Links", "Season", "Season Folder", "Supported Settings", "Used Script IDs", "Used Script Areas" };
const int   CT_GRAPHS_COLUMN_FORMAT[CT_GRAPHS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
const int		CT_GRAPHS_COLUMN_WIDTH [CT_GRAPHS_COLUMN_COUNT] = { 200, 60, 80, 80, 80, 80, 80, 80, 80 };

const char *CT_DEFAULT_FIELD_LABEL = "Yes";
const int		CT_DEFAULT_GRAPH_WEIGHT = 1;
const int		CT_DEFAULT_VSO_WEIGHT = 1;
const int		CT_DEFAULT_FIELD_WEIGHT = 1;

int CALLBACK CT_TemplatesCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGCreateTemplateDialog* pTemplateDialog = reinterpret_cast<CRMGCreateTemplateDialog*>( lParamSort );

	CString strItem1 = pTemplateDialog->m_TemplatesList.GetItemText( lParam1, pTemplateDialog->nSortColumn );
	CString strItem2 = pTemplateDialog->m_TemplatesList.GetItemText( lParam2, pTemplateDialog->nSortColumn );
	if ( pTemplateDialog->bTemplatesSortParam[pTemplateDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

int CALLBACK CT_FieldsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGCreateTemplateDialog* pTemplateDialog = reinterpret_cast<CRMGCreateTemplateDialog*>( lParamSort );

	CString strItem1 = pTemplateDialog->m_FieldsList.GetItemText( lParam1, pTemplateDialog->nSortColumn );
	CString strItem2 = pTemplateDialog->m_FieldsList.GetItemText( lParam2, pTemplateDialog->nSortColumn );
	if ( pTemplateDialog->bFieldsSortParam[pTemplateDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

int CALLBACK CT_VSOCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGCreateTemplateDialog* pTemplateDialog = reinterpret_cast<CRMGCreateTemplateDialog*>( lParamSort );

	CString strItem1 = pTemplateDialog->m_VSOList.GetItemText( lParam1, pTemplateDialog->nSortColumn );
	CString strItem2 = pTemplateDialog->m_VSOList.GetItemText( lParam2, pTemplateDialog->nSortColumn );
	if ( pTemplateDialog->bVSOSortParam[pTemplateDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

int CALLBACK CT_TemplateGraphsCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGCreateTemplateDialog* pTemplateDialog = reinterpret_cast<CRMGCreateTemplateDialog*>( lParamSort );

	CString strItem1 = pTemplateDialog->m_GraphsList.GetItemText( lParam1, pTemplateDialog->nSortColumn );
	CString strItem2 = pTemplateDialog->m_GraphsList.GetItemText( lParam2, pTemplateDialog->nSortColumn );
	if ( pTemplateDialog->bGraphsSortParam[pTemplateDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

const int CRMGCreateTemplateDialog::vID[] = 
{
	IDC_RMG_CT_DELIMITER_00,										//0

	IDC_RMG_CT_TEMPLATES_LABEL,									//1
	IDC_RMG_CT_TEMPLATES_LIST,									//2
	IDC_RMG_CT_ADD_TEMPLATE_BUTTON,							//3
	IDC_RMG_CT_DELETE_TEMPLATE_BUTTON,					//4
	IDC_RMG_CT_TEMPLATE_UNITS_BUTTON,						//5
	IDC_RMG_CT_CHECK_TEMPLATES_BUTTON,					//6

	IDC_RMG_CT_GRAPHS_LABEL,										//7
	IDC_RMG_CT_GRAPHS_LIST,											//8
	IDC_RMG_CT_ADD_GRAPH_BUTTON,								//9
	IDC_RMG_CT_DELETE_GRAPH_BUTTON,							//10
	IDC_RMG_CT_GRAPH_PROPERTIES_BUTTON,					//11
	
	IDC_RMG_CT_VSO_LABEL,												//12
	IDC_RMG_CT_VSO_LIST,												//13
	IDC_RMG_CT_ADD_VSO_BUTTON,									//14
	IDC_RMG_CT_DELETE_VSO_BUTTON,								//15
	IDC_RMG_CT_VSO_PROPERTIES_BUTTON,						//16
	
	IDC_RMG_CT_FIELDS_LABEL,										//17
	IDC_RMG_CT_FIELDS_LIST,											//18
	IDC_RMG_CT_ADD_FIELD_BUTTON,								//19
	IDC_RMG_CT_DELETE_FIELD_BUTTON,							//20
	IDC_RMG_CT_FIELD_PROPERTIES_BUTTON,					//21
	
	IDC_RMG_CT_SCRIPT_FILE_NAME_LABEL,					//22
	IDC_RMG_CT_SCRIPT_FILE_NAME_EDIT,						//23
	IDC_RMG_CT_SCRIPT_FILE_NAME_BROWSE_BUTTON,	//24
	
	IDC_RMG_CT_SAVE_BUTTON,											//25
	IDOK,																				//26
	IDCANCEL,																		//27
	
	IDC_RMG_CT_TEMPLATE_DIPLOMACY_BUTTON,				//28

	IDC_RMG_CT_MOD_LABEL,												//29
	IDC_RMG_CT_MOD_COMBO_BOX,										//30
};

CRMGCreateTemplateDialog::CRMGCreateTemplateDialog( CWnd* pParent )
	: CResizeDialog( CRMGCreateTemplateDialog::IDD, pParent ), nSortColumn( 0 ), bCreateControls( true )
{
	SetControlStyle( IDC_RMG_CT_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );

	SetControlStyle( IDC_RMG_CT_TEMPLATES_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_RMG_CT_TEMPLATES_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_RMG_CT_ADD_TEMPLATE_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_RMG_CT_DELETE_TEMPLATE_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_RMG_CT_TEMPLATE_UNITS_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_RMG_CT_CHECK_TEMPLATES_BUTTON, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDC_RMG_CT_GRAPHS_LABEL, ANCHORE_LEFT_TOP | ANCHORE_HOR_CENTER | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_RMG_CT_GRAPHS_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_RMG_CT_ADD_GRAPH_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_TOP );
	SetControlStyle( IDC_RMG_CT_DELETE_GRAPH_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_TOP );
	SetControlStyle( IDC_RMG_CT_GRAPH_PROPERTIES_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_TOP );
	
	SetControlStyle( IDC_RMG_CT_VSO_LABEL, ANCHORE_LEFT_BOTTOM | RESIZE_HOR,  0.5f, 0.5f, 0.5f, 1.0f  );
	SetControlStyle( IDC_RMG_CT_VSO_LIST, ANCHORE_LEFT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f  );
	SetControlStyle( IDC_RMG_CT_ADD_VSO_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
	SetControlStyle( IDC_RMG_CT_DELETE_VSO_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
	SetControlStyle( IDC_RMG_CT_VSO_PROPERTIES_BUTTON, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM );
	
	SetControlStyle( IDC_RMG_CT_FIELDS_LABEL, ANCHORE_RIGHT_TOP | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_RMG_CT_FIELDS_LIST, ANCHORE_RIGHT_TOP | RESIZE_HOR_VER, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_RMG_CT_ADD_FIELD_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_RMG_CT_DELETE_FIELD_BUTTON, ANCHORE_RIGHT_TOP );
	SetControlStyle( IDC_RMG_CT_FIELD_PROPERTIES_BUTTON, ANCHORE_RIGHT_TOP );
	
	SetControlStyle( IDC_RMG_CT_SCRIPT_FILE_NAME_LABEL, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_RMG_CT_SCRIPT_FILE_NAME_EDIT, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
	SetControlStyle( IDC_RMG_CT_SCRIPT_FILE_NAME_BROWSE_BUTTON, ANCHORE_RIGHT_BOTTOM );
	
	SetControlStyle( IDC_RMG_CT_SAVE_BUTTON, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDOK, ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( IDCANCEL, ANCHORE_RIGHT_BOTTOM );

	SetControlStyle( IDC_RMG_CT_TEMPLATE_DIPLOMACY_BUTTON, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDC_RMG_CT_MOD_LABEL, ANCHORE_HOR_CENTER | ANCHORE_BOTTOM, 0.5f, 0.5f, 1.0f, 1.0f );
	SetControlStyle( IDC_RMG_CT_MOD_COMBO_BOX, ANCHORE_RIGHT_BOTTOM | RESIZE_HOR, 0.5f, 0.5f, 0.5f, 1.0f );
}

void CRMGCreateTemplateDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RMG_CT_SCRIPT_FILE_NAME_EDIT, m_ScriptFileNameEdit);
	DDX_Control(pDX, IDC_RMG_CT_VSO_LIST, m_VSOList);
	DDX_Control(pDX, IDC_RMG_CT_FIELDS_LIST, m_FieldsList);
	DDX_Control(pDX, IDC_RMG_CT_GRAPHS_LIST, m_GraphsList);
	DDX_Control(pDX, IDC_RMG_CT_TEMPLATES_LIST, m_TemplatesList);
	DDX_Control(pDX, IDC_RMG_CT_MOD_COMBO_BOX, m_MODComboBox );
}

BEGIN_MESSAGE_MAP(CRMGCreateTemplateDialog, CResizeDialog)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_CT_VSO_LIST, OnColumnclickVsoList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_CT_TEMPLATES_LIST, OnColumnclickTemplatesList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_CT_GRAPHS_LIST, OnColumnclickGraphsList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_CT_FIELDS_LIST, OnColumnclickFieldsList)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVEAS, OnFileSaveas)
	ON_COMMAND(ID_FILE_EXIT, OnFileExit)
	ON_BN_CLICKED(IDC_RMG_CT_ADD_TEMPLATE_BUTTON, OnAddTemplateButton)
	ON_BN_CLICKED(IDC_RMG_CT_DELETE_TEMPLATE_BUTTON, OnDeleteTemplateButton)
	ON_BN_CLICKED(IDC_RMG_CT_TEMPLATE_UNITS_BUTTON, OnTemplateUnitsButton)
	ON_BN_CLICKED(IDC_RMG_CT_CHECK_TEMPLATES_BUTTON, OnCheckTemplatesButton)
	ON_COMMAND(IDC_RMG_CT_TEMPLATE_UNITS_MENU, OnTemplateUnitsMenu)
	ON_COMMAND(IDC_RMG_CT_ADD_TEMPLATE_MENU, OnAddTemplateMenu)
	ON_COMMAND(IDC_RMG_CT_DELETE_TEMPLATE_MENU, OnDeleteTemplateMenu)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_CT_TEMPLATES_LIST, OnItemchangedTemplatesList)
	ON_NOTIFY(NM_DBLCLK, IDC_RMG_CT_TEMPLATES_LIST, OnDblclkTemplatesList)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_CT_TEMPLATES_LIST, OnRclickTemplatesList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_CT_TEMPLATES_LIST, OnKeydownTemplatesList)
	ON_EN_CHANGE(IDC_RMG_CT_SCRIPT_FILE_NAME_EDIT, OnChangeRmgFileNameEdit)
	ON_BN_CLICKED(IDC_RMG_CT_SCRIPT_FILE_NAME_BROWSE_BUTTON, OnScriptFileNameBrowseButton)
	ON_BN_CLICKED(IDC_RMG_CT_ADD_GRAPH_BUTTON, OnAddGraphButton)
	ON_COMMAND(IDC_RMG_CT_ADD_GRAPH_MENU, OnAddGraphMenu)
	ON_BN_CLICKED(IDC_RMG_CT_DELETE_GRAPH_BUTTON, OnDeleteGraphButton)
	ON_COMMAND(IDC_RMG_CT_DELETE_GRAPH_MENU, OnDeleteGraphMenu)
	ON_BN_CLICKED(IDC_RMG_CT_GRAPH_PROPERTIES_BUTTON, OnGraphPropertiesButton)
	ON_COMMAND(IDC_RMG_CT_GRAPH_PROPERTIES_MENU, OnGraphPropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_RMG_CT_GRAPHS_LIST, OnDblclkGraphsList)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_CT_GRAPHS_LIST, OnRclickGraphsList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_CT_GRAPHS_LIST, OnItemchangedGraphsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_CT_GRAPHS_LIST, OnKeydownGraphsList)
	ON_BN_CLICKED(IDC_RMG_CT_SAVE_BUTTON, OnSaveButton)
	ON_BN_CLICKED(IDC_RMG_CT_ADD_VSO_BUTTON, OnAddVsoButton)
	ON_BN_CLICKED(IDC_RMG_CT_DELETE_VSO_BUTTON, OnDeleteVsoButton)
	ON_BN_CLICKED(IDC_RMG_CT_VSO_PROPERTIES_BUTTON, OnVsoPropertiesButton)
	ON_COMMAND(IDC_RMG_CT_ADD_VSO_MENU, OnAddVsoMenu)
	ON_COMMAND(IDC_RMG_CT_DELETE_VSO_MENU, OnDeleteVsoMenu)
	ON_COMMAND(IDC_RMG_CT_VSO_PROPERTIES_MENU, OnVsoPropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_RMG_CT_VSO_LIST, OnDblclkVsoList)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_CT_VSO_LIST, OnRclickVsoList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_CT_VSO_LIST, OnItemchangedVsoList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_CT_VSO_LIST, OnKeydownVsoList)
	ON_BN_CLICKED(IDC_RMG_CT_ADD_FIELD_BUTTON, OnAddFieldButton)
	ON_BN_CLICKED(IDC_RMG_CT_DELETE_FIELD_BUTTON, OnDeleteFieldButton)
	ON_BN_CLICKED(IDC_RMG_CT_FIELD_PROPERTIES_BUTTON, OnFieldPropertiesButton)
	ON_COMMAND(IDC_RMG_CT_ADD_FIELD_MENU, OnAddFieldMenu)
	ON_COMMAND(IDC_RMG_CT_DELETE_FIELD_MENU, OnDeleteFieldMenu)
	ON_COMMAND(IDC_RMG_CT_FIELD_PROPERTIES_MENU, OnFieldPropertiesMenu)
	ON_NOTIFY(NM_DBLCLK, IDC_RMG_CT_FIELDS_LIST, OnDblclkFieldsList)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_CT_FIELDS_LIST, OnRclickFieldsList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_CT_FIELDS_LIST, OnItemchangedFieldsList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_CT_FIELDS_LIST, OnKeydownFieldsList)
	ON_BN_CLICKED(IDC_RMG_CT_TEMPLATE_DIPLOMACY_BUTTON, OnTemplateDiplomacyButton)
	ON_COMMAND(IDC_RMG_CT_TEMPLATE_DIPLOMACY_MENU, OnTemplateDiplomacyMenu)
	ON_CBN_SELCHANGE(IDC_RMG_CT_MOD_COMBO_BOX, OnSelchangeModComboBox)
END_MESSAGE_MAP()

BOOL CRMGCreateTemplateDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( resizeDialogOptions.szParameters.size() < 7 )
	{
		resizeDialogOptions.szParameters.resize( 7, "" );
	}
	if ( resizeDialogOptions.szParameters[6].empty() )
	{
		resizeDialogOptions.szParameters[6] = CT_TEMPLATES_FILE_NAME;
	}
	if ( resizeDialogOptions.nParameters.size() < ( CT_TEMPLATES_COLUMN_COUNT + CT_FIELDS_COLUMN_COUNT + CT_VSO_COLUMN_COUNT + CT_GRAPHS_COLUMN_COUNT ) )
	{
		resizeDialogOptions.nParameters.resize( CT_TEMPLATES_COLUMN_COUNT + CT_FIELDS_COLUMN_COUNT + CT_VSO_COLUMN_COUNT + CT_GRAPHS_COLUMN_COUNT, 0 );
	}
	
	CreateControls();
	LoadTemplatesList();
	UpdateControls();
	return true;
}	

bool CRMGCreateTemplateDialog::LoadTemplatesList()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}

	SetWindowText( NStr::Format( "%s - [%s]", CT_TEMPLATES_DIALOG_TITLE, resizeDialogOptions.szParameters[6] ) );
	BeginWaitCursor();
	LoadDataResource( resizeDialogOptions.szParameters[6], "", false, 0, CT_TEMPLATES_XML_NAME, templates );
	
	m_TemplatesList.DeleteAllItems();
	for ( CRMTemplatesHashMap::const_iterator templateIterator = templates.begin();  templateIterator != templates.end(); ++templateIterator )
	{
		int nNewItem = m_TemplatesList.InsertItem( LVIF_TEXT, 0, templateIterator->first.c_str(), 0, 0, 0, 0 );
		if ( nNewItem == ( -1 ) )
		{
			EndWaitCursor();
			return false;
		}

		SetTemplateItem( nNewItem, templateIterator->second );
	}
	LoadTemplateToControls();
	EndWaitCursor();
	return true;
}

bool CRMGCreateTemplateDialog::SaveTemplatesList()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}

	SetWindowText( NStr::Format( "%s - [%s]", CT_TEMPLATES_DIALOG_TITLE, resizeDialogOptions.szParameters[6] ) );
	BeginWaitCursor();
	for ( CRMTemplatesHashMap::const_iterator templateIterator = templates.begin();  templateIterator != templates.end(); ++templateIterator )
	{
		CPtr<IDataStream> pStreamXML = CreateFileStream( ( pDataStorage->GetName() + templateIterator->first + ".xml" ).c_str(), STREAM_ACCESS_WRITE );
		if ( pStreamXML == 0 )
		{
			return false;
		}

		SRMTemplate rmTemplate = templateIterator->second;

		SQuickLoadMapInfo quickLoadMapInfo;
		quickLoadMapInfo.FillFromRMTemplate( rmTemplate );

		CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::WRITE );
		CTreeAccessor saver = pSaver;
		saver.Add( RMGC_TEMPLATE_XML_NAME, &rmTemplate );
		saver.Add( RMGC_QUICK_LOAD_MAP_INFO_NAME, &quickLoadMapInfo );
	}

	if ( !SaveDataResource( resizeDialogOptions.szParameters[6], "", false, 0, CT_TEMPLATES_XML_NAME, templates ) )
	{
		EndWaitCursor();
		return false;
	}
	EndWaitCursor();
	return true;
}

void CRMGCreateTemplateDialog::GetAllMODs( std::vector<std::string> *pMODsList )
{
	if ( pMODsList )
	{
		pMODsList->clear();
		const CMODCollector &rMODCollector = g_frameManager.GetTemplateEditorFrame()->modCollector;
		for ( CMODCollector::TMODNodesList::const_iterator modNodeIterator = rMODCollector.availableMODs.begin(); modNodeIterator != rMODCollector.availableMODs.end(); ++modNodeIterator )
		{
			pMODsList->push_back( modNodeIterator->first );
		}
		std::sort( pMODsList->begin(), pMODsList->end() );
	}
}

bool CRMGCreateTemplateDialog::LoadTemplateToControls()
{
	bCreateControls = true;
	m_GraphsList.DeleteAllItems();
	m_VSOList.DeleteAllItems();
	m_FieldsList.DeleteAllItems();
	m_ScriptFileNameEdit.SetWindowText( "" );

	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		for ( int nGraphIndex = 0; nGraphIndex < rTemplate.graphs.size(); ++nGraphIndex )
		{
			int nNewItem = m_GraphsList.InsertItem(  LVIF_TEXT, 0, rTemplate.graphs[nGraphIndex].c_str(), 0, 0, 0, 0 ); 
			if ( nNewItem != ( -1 ) )
			{
				SRMGraph graph;
				LoadDataResource( rTemplate.graphs[nGraphIndex], "", false, 0, RMGC_GRAPH_XML_NAME, graph );
				SetGraphItem( nNewItem, rTemplate.graphs.GetWeight( nGraphIndex ), graph );
			}
		}

		for ( int nVSOIndex = 0; nVSOIndex < rTemplate.vso.size(); ++nVSOIndex )
		{
			int nNewItem = m_VSOList.InsertItem( LVIF_TEXT, 0, rTemplate.vso[nVSOIndex].szVSODescFileName.c_str(), 0, 0, 0, 0 ); 
			if ( nNewItem != ( -1 ) )
			{
				SetVSOItem( nNewItem, rTemplate.vso.GetWeight( nVSOIndex ), rTemplate.vso[nVSOIndex] );
			}
		}

		for ( int nFieldIndex = 0; nFieldIndex < rTemplate.fields.size(); ++nFieldIndex )
		{
			int nNewItem = m_FieldsList.InsertItem( LVIF_TEXT, 0, rTemplate.fields[nFieldIndex].c_str(), 0, 0, 0, 0 ); 
			if ( nNewItem != ( -1 ) )
			{
				SRMFieldSet field;
				LoadDataResource( rTemplate.fields[nFieldIndex], "", false, 0, RMGC_FIELDSET_XML_NAME, field );
				SetFieldItem( nNewItem, rTemplate.fields.GetWeight( nFieldIndex ), ( rTemplate.nDefaultFieldIndex == nFieldIndex ), field );
			}
		}

		m_ScriptFileNameEdit.SetWindowText( rTemplate.szScriptFile.c_str() );

		const std::string szTemplateMODKey = CMODCollector::GetKey( rTemplate.szMODName, rTemplate.szMODVersion );
		
		szMODNameBackup = rTemplate.szMODName;
		szMODVersionBackup = rTemplate.szMODVersion;

		if ( szTemplateMODKey.empty() )
		{
			m_MODComboBox.SelectString( -1, RMGC_NO_MOD_FOLDER );
		}
		else
		{
			if ( m_MODComboBox.SelectString( -1, szTemplateMODKey.c_str() ) < 0 )
			{
				m_MODComboBox.SelectString( -1, RMGC_OWN_MOD_FOLDER );
			}
		}

		/**
		int nSelection = m_PlaceComboBox.FindStringExact( -1, rTemplate.szPlace.c_str() );
		if ( nSelection == CB_ERR )
		{
			if ( m_PlaceComboBox.GetCount() > 0 )
			{
				CString szBuffer;
				m_PlaceComboBox.GetLBText( 0, szBuffer );
				rTemplate.szPlace = szBuffer;
			}
		}
		m_PlaceComboBox.SelectString( -1, rTemplate.szPlace.c_str() );
		/**/

		/**
		int nForestsAmbientSoundsIndex = m_ForestsAmbientSoundsComboBox.FindStringExact( -1, rTemplate.szForestAmbientSounds.c_str() );
		if ( nForestsAmbientSoundsIndex != CB_ERR )
		{
			m_ForestsAmbientSoundsComboBox.SetCurSel( nForestsAmbientSoundsIndex );
		}
		
		int nForestsCircleSoundsIndex = m_ForestsCircleSoundsComboBox.FindStringExact( -1, rTemplate.szForestCircleSounds.c_str() );
		if ( nForestsCircleSoundsIndex != CB_ERR )
		{
			m_ForestsCircleSoundsComboBox.SetCurSel( nForestsCircleSoundsIndex );
		}
		/**/
	}
	
	bCreateControls = false;
	return true;
}

bool CRMGCreateTemplateDialog::SaveTemplateFromControls()
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];
		SetTemplateItem( nFocusedItem, rTemplate );
	}
	return true;
}

void CRMGCreateTemplateDialog::SetTemplateItem( int nItem, const SRMTemplate &rTemplate )
{
	m_TemplatesList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%4dx%-4d", rTemplate.size.x, rTemplate.size.y ), 0, 0, 0, 0 );
	std::string szSeasonName;
	RMGGetSeasonNameString( rTemplate.nSeason, rTemplate.szSeasonFolder, &szSeasonName );
	m_TemplatesList.SetItem( nItem, 2, LVIF_TEXT, szSeasonName.c_str(), 0, 0, 0, 0 );
	
	int nSide0Count = 0;
	int nSide1Count = 0;
	for ( int nPlayerIndex = 0; nPlayerIndex < ( rTemplate.diplomacies.size() - 1 ); ++nPlayerIndex )
	{
		if ( rTemplate.diplomacies[nPlayerIndex] == 0 )
		{
			++nSide0Count;
		}
		else if ( rTemplate.diplomacies[nPlayerIndex] == 1 )
		{
			++nSide1Count;
		}
	}

	m_TemplatesList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%2d / %d / %d", rTemplate.diplomacies.size(), nSide0Count, nSide1Count  ), 0, 0, 0, 0 );
	
	m_TemplatesList.SetItem( nItem, 4, LVIF_TEXT, NStr::Format( "%4d", rTemplate.graphs.size() ), 0, 0, 0, 0 );
	m_TemplatesList.SetItem( nItem, 5, LVIF_TEXT, NStr::Format( "%4d", rTemplate.fields.size() ), 0, 0, 0, 0 );
	m_TemplatesList.SetItem( nItem, 6, LVIF_TEXT, NStr::Format( "%4d", rTemplate.vso.size() ), 0, 0, 0, 0 );
	m_TemplatesList.SetItem( nItem, 7, LVIF_TEXT, rTemplate.szSeasonFolder.c_str(), 0, 0, 0, 0 );
	
	std::string szSettingsNames;
	std::list<std::string> settings;
	if ( rTemplate.GetSupportedSettings( &settings ) > 0 )
	{
		for ( std::list<std::string>::const_iterator settingNameIterator = settings.begin(); settingNameIterator != settings.end(); ++settingNameIterator )
		{
			if ( settingNameIterator == settings.begin() )
			{
				szSettingsNames = ( *settingNameIterator );
			}
			else
			{
				szSettingsNames += std::string( "; " ) + ( *settingNameIterator );
			}
		}
	}
	m_TemplatesList.SetItem( nItem, 8, LVIF_TEXT, szSettingsNames.c_str(), 0, 0, 0, 0 );
	
	std::string szUsedScriptIDs;
	RMGGetUsedScriptIDsString( rTemplate.usedScriptIDs, &szUsedScriptIDs );
	m_TemplatesList.SetItem( nItem, 9, LVIF_TEXT, szUsedScriptIDs.c_str(), 0, 0, 0, 0 );
	
	std::string szUsedScripAreas;
	RMGGetUsedScriptAreasString( rTemplate.usedScriptAreas, &szUsedScripAreas );
	m_TemplatesList.SetItem( nItem, 10, LVIF_TEXT, szUsedScripAreas.c_str(), 0, 0, 0, 0 );
	
	m_TemplatesList.SetItem( nItem, 11, LVIF_TEXT, rTemplate.szScriptFile.c_str(), 0, 0, 0, 0 );
	m_TemplatesList.SetItem( nItem, 12, LVIF_TEXT, ( CMODCollector::GetKey( rTemplate.szMODName, rTemplate.szMODVersion ) ).c_str(), 0, 0, 0, 0 );
}

void CRMGCreateTemplateDialog::SetGraphItem( int nItem, int nWeight, const SRMGraph &rGraph )
{
	m_GraphsList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%4d", nWeight ), 0, 0, 0, 0 );
	m_GraphsList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%4d", rGraph.nodes.size() ), 0, 0, 0, 0 );
	m_GraphsList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%4d", rGraph.links.size() ), 0, 0, 0, 0 );

	std::string szSeasonName;
	RMGGetSeasonNameString( rGraph.nSeason, rGraph.szSeasonFolder, &szSeasonName );
	m_GraphsList.SetItem( nItem, 4, LVIF_TEXT, szSeasonName.c_str(), 0, 0, 0, 0 );
	m_GraphsList.SetItem( nItem, 5, LVIF_TEXT, rGraph.szSeasonFolder.c_str(), 0, 0, 0, 0 );
	
	std::string szSettingsNames;
	std::list<std::string> settings;
	if ( rGraph.GetSupportedSettings( &settings ) > 0 )
	{
		for ( std::list<std::string>::const_iterator settingNameIterator = settings.begin(); settingNameIterator != settings.end(); ++settingNameIterator )
		{
			if ( settingNameIterator == settings.begin() )
			{
				szSettingsNames = ( *settingNameIterator );
			}
			else
			{
				szSettingsNames += std::string( "; " ) + ( *settingNameIterator );
			}
		}
	}
	m_GraphsList.SetItem( nItem, 6, LVIF_TEXT, szSettingsNames.c_str(), 0, 0, 0, 0 );
	
	std::string szUsedScriptIDs;
	RMGGetUsedScriptIDsString( rGraph.usedScriptIDs, &szUsedScriptIDs );
	m_GraphsList.SetItem( nItem, 7, LVIF_TEXT, szUsedScriptIDs.c_str(), 0, 0, 0, 0 );
	
	std::string szUsedScripAreas;
	RMGGetUsedScriptAreasString( rGraph.usedScriptAreas, &szUsedScripAreas );
	m_GraphsList.SetItem( nItem, 8, LVIF_TEXT, szUsedScripAreas.c_str(), 0, 0, 0, 0 );
}

void CRMGCreateTemplateDialog::SetVSOItem( int nItem, int nWeight, const SRMVSODesc &rVSODesc )
{
	m_VSOList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%4d", nWeight ), 0, 0, 0, 0 );
	m_VSOList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%.2f", rVSODesc.fWidth / fWorldCellSize ), 0, 0, 0, 0 );
	m_VSOList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%.2f", rVSODesc.fOpacity * 100.0f ), 0, 0, 0, 0 );
}

void CRMGCreateTemplateDialog::SetFieldItem( int nItem, int nWeight, bool bDefault, const SRMFieldSet &rFieldSet )
{
	m_FieldsList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%4d", nWeight ), 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 2, LVIF_TEXT, bDefault ? CT_DEFAULT_FIELD_LABEL : "", 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%4d", rFieldSet.tilesShells.size() ), 0, 0, 0, 0 );
	m_FieldsList.SetItem( nItem, 4, LVIF_TEXT, NStr::Format( "%4d", rFieldSet.objectsShells.size() ), 0, 0, 0, 0 );
}

void CRMGCreateTemplateDialog::OnOK() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CT_TEMPLATES_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CT_TEMPLATES_COLUMN_START] = m_TemplatesList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < CT_FIELDS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CT_FIELDS_COLUMN_START] = m_FieldsList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < CT_VSO_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CT_VSO_COLUMN_START] = m_VSOList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < CT_GRAPHS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CT_GRAPHS_COLUMN_START] = m_GraphsList.GetColumnWidth( nColumnIndex );
	}
	
	SaveTemplatesList();
	CResizeDialog::OnOK();
}

void CRMGCreateTemplateDialog::OnCancel() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CT_TEMPLATES_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CT_TEMPLATES_COLUMN_START] = m_TemplatesList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < CT_FIELDS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CT_FIELDS_COLUMN_START] = m_FieldsList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < CT_VSO_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CT_VSO_COLUMN_START] = m_VSOList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < CT_GRAPHS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CT_GRAPHS_COLUMN_START] = m_GraphsList.GetColumnWidth( nColumnIndex );
	}
	CResizeDialog::OnCancel();
}

void CRMGCreateTemplateDialog::CreateControls()
{
	bCreateControls = true;
	
	
	m_TemplatesList.SetExtendedStyle( m_TemplatesList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CT_TEMPLATES_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + CT_TEMPLATES_COLUMN_START] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + CT_TEMPLATES_COLUMN_START] = CT_TEMPLATES_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_TemplatesList.InsertColumn( nColumnIndex, CT_TEMPLATES_COLUMN_NAME[nColumnIndex], CT_TEMPLATES_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + CT_TEMPLATES_COLUMN_START], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bTemplatesSortParam.push_back( true );
	}
	
	m_FieldsList.SetExtendedStyle( m_FieldsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CT_FIELDS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + CT_FIELDS_COLUMN_START] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + CT_FIELDS_COLUMN_START] = CT_FIELDS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_FieldsList.InsertColumn( nColumnIndex, CT_FIELDS_COLUMN_NAME[nColumnIndex], CT_FIELDS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + CT_FIELDS_COLUMN_START], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bFieldsSortParam.push_back( true );
	}

	m_VSOList.SetExtendedStyle( m_VSOList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CT_VSO_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + CT_VSO_COLUMN_START] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + CT_VSO_COLUMN_START] = CT_VSO_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_VSOList.InsertColumn( nColumnIndex, CT_VSO_COLUMN_NAME[nColumnIndex], CT_VSO_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + CT_VSO_COLUMN_START], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bVSOSortParam.push_back( true );
	}

	m_GraphsList.SetExtendedStyle( m_GraphsList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CT_GRAPHS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + CT_GRAPHS_COLUMN_START] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + CT_GRAPHS_COLUMN_START] = CT_GRAPHS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_GraphsList.InsertColumn( nColumnIndex, CT_GRAPHS_COLUMN_NAME[nColumnIndex], CT_GRAPHS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + CT_GRAPHS_COLUMN_START], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bGraphsSortParam.push_back( true );
	}

	/**
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	if ( pFrame )
	{
		if ( !pFrame->msHelper.IsInitialized() )
		{
			pFrame->msHelper.Initialize();	
		}
		for ( std::list<std::string>::const_iterator soundIterator = pFrame->msHelper.soundsList.begin(); soundIterator != pFrame->msHelper.soundsList.end(); ++soundIterator )
		{
			m_ForestsAmbientSoundsComboBox.AddString( soundIterator->c_str() );
			m_ForestsCircleSoundsComboBox.AddString( soundIterator->c_str() );
		}
	}
	/**/

	std::vector<std::string> modsFolders;
	GetAllMODs( &modsFolders );
	modsFolders.push_back( RMGC_NO_MOD_FOLDER );
	modsFolders.push_back( RMGC_OWN_MOD_FOLDER );
	for ( std::vector<std::string>::const_iterator modFolderIterator = modsFolders.begin(); modFolderIterator != modsFolders.end(); ++ modFolderIterator )
	{
		m_MODComboBox.AddString( modFolderIterator->c_str() );
	}

	bCreateControls = false;
}

void CRMGCreateTemplateDialog::ClearControls()
{
}

void CRMGCreateTemplateDialog::UpdateControls()
{
	int nFocusedTemplate = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	CWnd* pWnd = 0;
	if ( pWnd = GetDlgItem( IDC_RMG_CT_DELETE_TEMPLATE_BUTTON ) )
	{
		pWnd->EnableWindow( m_TemplatesList.GetSelectedCount() > 0 );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_UNITS_BUTTON ) )
	{
		pWnd->EnableWindow( nFocusedTemplate != ( -1 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_DIPLOMACY_BUTTON ) )
	{
		pWnd->EnableWindow( nFocusedTemplate != ( -1 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_CHECK_TEMPLATES_BUTTON ) )
	{
		pWnd->EnableWindow( m_TemplatesList.GetItemCount() > 0 );
	}
	
	int nFocusedGraph = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( pWnd = GetDlgItem( IDC_RMG_CT_ADD_GRAPH_BUTTON ) )
	{
		pWnd->EnableWindow( nFocusedTemplate != ( -1 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_DELETE_GRAPH_BUTTON ) )
	{
		pWnd->EnableWindow( ( nFocusedTemplate != ( -1 ) ) && ( m_GraphsList.GetSelectedCount() > 0 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_GRAPH_PROPERTIES_BUTTON ) )
	{
		pWnd->EnableWindow( ( nFocusedTemplate != ( -1 ) ) && ( nFocusedGraph != ( -1 ) ) );
	}

	int nFocusedVSO = m_VSOList.GetNextItem( -1, LVNI_FOCUSED );
	if ( pWnd = GetDlgItem( IDC_RMG_CT_ADD_VSO_BUTTON ) )
	{
		pWnd->EnableWindow( nFocusedTemplate != ( -1 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_DELETE_VSO_BUTTON ) )
	{
		pWnd->EnableWindow( ( nFocusedTemplate != ( -1 ) ) && ( m_VSOList.GetSelectedCount() > 0 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_VSO_PROPERTIES_BUTTON ) )
	{
		pWnd->EnableWindow( ( nFocusedTemplate != ( -1 ) ) && ( nFocusedVSO != ( -1 ) ) );
	}

	int nFocusedField = m_FieldsList.GetNextItem( -1, LVNI_FOCUSED );
	if ( pWnd = GetDlgItem( IDC_RMG_CT_ADD_FIELD_BUTTON ) )
	{
		pWnd->EnableWindow( nFocusedTemplate != ( -1 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_DELETE_FIELD_BUTTON ) )
	{
		pWnd->EnableWindow( ( nFocusedTemplate != ( -1 ) ) && ( m_FieldsList.GetSelectedCount() > 0 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_FIELD_PROPERTIES_BUTTON ) )
	{
		pWnd->EnableWindow( ( nFocusedTemplate != ( -1 ) ) && ( nFocusedField != ( -1 ) ) );
	}
	
	if ( pWnd = GetDlgItem( IDC_RMG_CT_SCRIPT_FILE_NAME_EDIT ) )
	{
		pWnd->EnableWindow( nFocusedTemplate != ( -1 ) );
	}
	if ( pWnd = GetDlgItem( IDC_RMG_CT_SCRIPT_FILE_NAME_BROWSE_BUTTON ) )
	{
		pWnd->EnableWindow( nFocusedTemplate != ( -1 ) );
	}

	if ( pWnd = GetDlgItem( IDC_RMG_CT_MOD_COMBO_BOX ) )
	{
		pWnd->EnableWindow( nFocusedTemplate != ( -1 ) );
	}
}

void CRMGCreateTemplateDialog::OnColumnclickTemplatesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CT_TEMPLATES_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CT_TEMPLATES_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_TemplatesList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_TemplatesList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_TemplatesList.SortItems( CT_TemplatesCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bTemplatesSortParam[nSortColumn] = !bTemplatesSortParam[nSortColumn];
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnColumnclickFieldsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CT_FIELDS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CT_FIELDS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_FieldsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_FieldsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_FieldsList.SortItems( CT_FieldsCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bFieldsSortParam[nSortColumn] = !bFieldsSortParam[nSortColumn];
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnColumnclickVsoList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CT_VSO_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CT_VSO_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_VSOList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_VSOList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_VSOList.SortItems( CT_VSOCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bVSOSortParam[nSortColumn] = !bVSOSortParam[nSortColumn];
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnColumnclickGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CT_GRAPHS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CT_GRAPHS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_GraphsList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_GraphsList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_GraphsList.SortItems( CT_TemplateGraphsCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bGraphsSortParam[nSortColumn] = !bGraphsSortParam[nSortColumn];
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnFileNew() 
{
	SaveTemplatesList();
	resizeDialogOptions.szParameters[6] = CT_TEMPLATES_FILE_NAME;
	LoadTemplatesList();
	UpdateControls();
}

void CRMGCreateTemplateDialog::OnFileOpen() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	SaveTemplatesList();

	CFileDialog fileDialog( true, ".xml", "", 0, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[0].c_str();
	
	if ( fileDialog.DoModal() == IDOK )
	{
		std::string szFilePath = fileDialog.GetPathName();
		std::string szStorageName = pDataStorage->GetName();
		NStr::ToLower( szFilePath );
		NStr::ToLower( szStorageName );
		if ( szFilePath.find( szStorageName ) != 0 )
		{
			return;
		}

		int nPointIndex = szFilePath.rfind( "." );
		if ( nPointIndex >= 0 )
		{
			szFilePath = szFilePath.substr( 0, nPointIndex );
		}

		int nSlashIndex = szFilePath.rfind( "\\" );
		if ( nSlashIndex >= 0 )
		{
			resizeDialogOptions.szParameters[0] = szFilePath.substr( 0, nSlashIndex );
		}

		szFilePath = szFilePath.substr( szStorageName.size() );
		resizeDialogOptions.szParameters[6] = szFilePath;
		
		LoadTemplatesList();
		UpdateControls();
	}
}

void CRMGCreateTemplateDialog::OnFileSave() 
{
	SaveTemplatesList();
}

void CRMGCreateTemplateDialog::OnSaveButton() 
{
	SaveTemplatesList();
}

void CRMGCreateTemplateDialog::OnFileSaveas() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	CFileDialog fileDialog( false, ".xml", "", OFN_OVERWRITEPROMPT, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[0].c_str();
	
	if ( fileDialog.DoModal() == IDOK )
	{
		std::string szFilePath = fileDialog.GetPathName();
		std::string szStorageName = pDataStorage->GetName();
		NStr::ToLower( szFilePath );
		NStr::ToLower( szStorageName );
		if ( szFilePath.find( szStorageName ) != 0 )
		{
			return;
		}

		int nPointIndex = szFilePath.rfind( "." );
		if ( nPointIndex >= 0 )
		{
			szFilePath = szFilePath.substr( 0, nPointIndex );
		}

		int nSlashIndex = szFilePath.rfind( "\\" );
		if ( nSlashIndex >= 0 )
		{
			resizeDialogOptions.szParameters[0] = szFilePath.substr( 0, nSlashIndex );
		}

		szFilePath = szFilePath.substr( szStorageName.size() );
		resizeDialogOptions.szParameters[6] = szFilePath;
		SaveTemplatesList();
	}
}

void CRMGCreateTemplateDialog::OnFileExit() 
{
	OnOK();
}

void CRMGCreateTemplateDialog::OnAddTemplateButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	CFileDialog fileDialog( true, ".xml", "", OFN_ALLOWMULTISELECT, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
	fileDialog.m_ofn.lpstrFile[0] = 0;			
	fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[1].c_str();

	if ( fileDialog.DoModal() == IDOK )
	{
		BeginWaitCursor();
		POSITION position = fileDialog.GetStartPosition();
		while ( position )
		{
			std::string szFilePath = fileDialog.GetNextPathName( position );
			std::string szStorageName = pDataStorage->GetName();
			NStr::ToLower( szFilePath );
			NStr::ToLower( szStorageName );
			if ( szFilePath.find( szStorageName ) != 0 )
			{
				return;
			}

			int nPointIndex = szFilePath.rfind( "." );
			if ( nPointIndex >= 0 )
			{
				szFilePath = szFilePath.substr( 0, nPointIndex );
			}

			int nSlashIndex = szFilePath.rfind( "\\" );
			if ( nSlashIndex >= 0 )
			{
				resizeDialogOptions.szParameters[1] = szFilePath.substr( 0, nSlashIndex );
			}

			szFilePath = szFilePath.substr( szStorageName.size() );
			
			SRMTemplate templateToAdd;
			templateToAdd.size.x = 0;
			templateToAdd.size.y = 0;
			templateToAdd.nSeason = 0;
			templateToAdd.szSeasonFolder.clear();
			templateToAdd.FillDefaultDiplomacies();
			templateToAdd.unitCreation.units.resize( templateToAdd.diplomacies.size() - 1, SUnitCreation() );
			templateToAdd.unitCreation.Validate();
			templateToAdd.vCameraAnchor = VNULL3;
			LoadDataResource( szFilePath, "", false, 0, RMGC_TEMPLATE_XML_NAME, templateToAdd );
			
			LVFINDINFO findInfo;
			findInfo.flags = LVFI_STRING;
			findInfo.psz = szFilePath.c_str();

			int nOldItem = m_TemplatesList.FindItem( &findInfo, -1 );
			if ( nOldItem != ( -1 ) )
			{
				m_TemplatesList.DeleteItem( nOldItem );
			}
			
			int nNewItem = m_TemplatesList.InsertItem( LVIF_TEXT, 0, szFilePath.c_str(), 0, 0, 0, 0 );
			if ( nNewItem != ( -1 ) )
			{
				templates[szFilePath] = templateToAdd;
				SetTemplateItem( nNewItem, templateToAdd );
			}
		}		
		LoadTemplateToControls();
		UpdateControls();
		EndWaitCursor();
	}
	delete[] fileDialog.m_ofn.lpstrFile;
}

void CRMGCreateTemplateDialog::OnDeleteTemplateButton() 
{
	CString strTitle;
	strTitle.LoadString( IDR_EDITORTYPE );
	if ( MessageBox( "Do you really want to DELETE selected Templates?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
	{
		int nSelectedItem = m_TemplatesList.GetNextItem( -1, LVIS_SELECTED );
		if ( nSelectedItem != ( -1 ) )
		{
			bCreateControls = true;
			while ( nSelectedItem != ( -1 ) )
			{
				std::string szKey = m_TemplatesList.GetItemText( nSelectedItem, 0 );
				m_TemplatesList.DeleteItem( nSelectedItem );
				templates.erase( szKey );
				nSelectedItem = m_TemplatesList.GetNextItem( -1, LVIS_SELECTED );
			}
			bCreateControls = false;
			LoadTemplateToControls();
			UpdateControls();
		}
	}
}

void CRMGCreateTemplateDialog::OnTemplateUnitsButton()
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		CTemplateUnitsDialog dlg( this );
		dlg.unitCreation = rTemplate.unitCreation;
		dlg.DoModal();
		rTemplate.unitCreation = dlg.unitCreation.Mutate();
		rTemplate.unitCreation.Validate();
		UpdateControls();
	}
}

void CRMGCreateTemplateDialog::OnTemplateDiplomacyButton() 
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		CTabSimpleObjectsDiplomacyDialog diplomacyDialog;
		diplomacyDialog.diplomacies = rTemplate.diplomacies;
		diplomacyDialog.SetType( rTemplate.nType );
		diplomacyDialog.SetAttackingSide( rTemplate.nAttackingSide );
		if ( diplomacyDialog.DoModal() == IDOK )
		{
			rTemplate.diplomacies = diplomacyDialog.diplomacies;
			rTemplate.unitCreation.units.resize( rTemplate.diplomacies.size() - 1, SUnitCreation() );
			rTemplate.unitCreation.Validate();
			rTemplate.nType = diplomacyDialog.GetType();
			rTemplate.nAttackingSide = diplomacyDialog.GetAttackingSide();
			SetTemplateItem( nFocusedItem, rTemplate );
		}
		UpdateControls();
	}
}

void CRMGCreateTemplateDialog::OnDblclkTemplatesList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_UNITS_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnTemplateUnitsButton();
		}
	}
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnRclickTemplatesList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 5 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_TEMPLATE_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_ADD_TEMPLATE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_TEMPLATE_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_DELETE_TEMPLATE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_DIPLOMACY_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_TEMPLATE_DIPLOMACY_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_UNITS_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_TEMPLATE_UNITS_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnKeydownTemplatesList( NMHDR* pNMHDR, LRESULT* pResult ) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_TEMPLATE_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddTemplateButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_TEMPLATE_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteTemplateButton();
			}
		}
	}
	else if (  pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_UNITS_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnTemplateUnitsButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_RETURN )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_DIPLOMACY_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnTemplateDiplomacyButton();
			}
		}
	}
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnAddTemplateMenu() 
{
	OnAddTemplateButton();
}

void CRMGCreateTemplateDialog::OnDeleteTemplateMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_TEMPLATE_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnDeleteTemplateButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnTemplateUnitsMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_UNITS_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnTemplateUnitsButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnTemplateDiplomacyMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_TEMPLATE_DIPLOMACY_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnTemplateDiplomacyButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnCheckTemplatesButton() 
{
}

void CRMGCreateTemplateDialog::OnItemchangedTemplatesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if ( !bCreateControls )
	{
		LoadTemplateToControls();
	}
	UpdateControls();
	*pResult = 0;
}

/**
void CRMGCreateTemplateDialog::OnSelchangeForestsAmbientSoundCombo() 
{
	if ( !bCreateControls )
	{
		int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedItem != ( -1 ) )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			CString szBuffer;
			m_ForestsAmbientSoundsComboBox.GetWindowText( szBuffer );
			rTemplate.szForestAmbientSounds = szBuffer;
			SaveTemplateFromControls();
		}	
	}
}
/**/

/**
void CRMGCreateTemplateDialog::OnSelchangeForestsCircleSoundCombo() 
{
	if ( !bCreateControls )
	{
		int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedItem != ( -1 ) )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			CString szBuffer;
			m_ForestsCircleSoundsComboBox.GetWindowText( szBuffer );
			rTemplate.szForestCircleSounds = szBuffer;
			SaveTemplateFromControls();
		}	
	}
}
/**/

void CRMGCreateTemplateDialog::OnChangeRmgFileNameEdit() 
{
	if ( !bCreateControls )
	{
		int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedItem != ( -1 ) )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			CString szBuffer;
			
			m_ScriptFileNameEdit.GetWindowText( szBuffer );
			rTemplate.szScriptFile = szBuffer;

			SaveTemplateFromControls();
		}	
	}
}

void CRMGCreateTemplateDialog::OnSelchangeModComboBox() 
{
	if ( !bCreateControls )
	{
		int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedItem != ( -1 ) )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			CString strBuffer;
			const int nStringNumber = m_MODComboBox.GetCurSel();
			if ( nStringNumber >= 0 )
			{
				m_MODComboBox.GetLBText( nStringNumber, strBuffer );
			
				const std::string szNewTemplateMODKey = strBuffer;
				
				if ( szNewTemplateMODKey != RMGC_OWN_MOD_FOLDER )
				{
					if ( szNewTemplateMODKey == RMGC_NO_MOD_FOLDER )
					{
						rTemplate.szMODName.clear();
						rTemplate.szMODVersion.clear();
					}
					else
					{
						if ( g_frameManager.GetTemplateEditorFrame()->modCollector.availableMODs.find( szNewTemplateMODKey ) != g_frameManager.GetTemplateEditorFrame()->modCollector.availableMODs.end() )
						{
							const CMODCollector::CMODNode &rMODNode = g_frameManager.GetTemplateEditorFrame()->modCollector.availableMODs[szNewTemplateMODKey];
							rTemplate.szMODName = rMODNode.szMODName;
							rTemplate.szMODVersion = rMODNode.szMODVersion;
						}
					}
				}
				else
				{
					rTemplate.szMODName = szMODNameBackup;
					rTemplate.szMODVersion = szMODVersionBackup;
				}
				SaveTemplateFromControls();
			}
		}	
	}
}

/**
void CRMGCreateTemplateDialog::OnSelchangePlaceCombo() 
{
	if ( !bCreateControls )
	{
		int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedItem != ( -1 ) )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			CString szBuffer;
			
			m_PlaceComboBox.GetWindowText( szBuffer );
			rTemplate.szPlace = szBuffer;

			SaveTemplateFromControls();
		}	
	}
}
/**/
/**
void CRMGCreateTemplateDialog::OnChangePlaceEdit() 
{
	if ( !bCreateControls )
	{
		int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedItem != ( -1 ) )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			CString szBuffer;
			
			m_PlaceEdit.GetWindowText( szBuffer );
			rTemplate.szPlace = szBuffer;

			SaveTemplateFromControls();
		}	
	}
}
/**/
void CRMGCreateTemplateDialog::OnScriptFileNameBrowseButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		CFileDialog fileDialog( true, ".lua", "", OFN_FILEMUSTEXIST | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, "LUA files (*.lua)|*.lua||" );
		fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[5].c_str();

		if ( fileDialog.DoModal() == IDOK )
		{
			BeginWaitCursor();
			POSITION position = fileDialog.GetStartPosition();
			while ( position )
			{
				std::string szFilePath = fileDialog.GetNextPathName( position );
				std::string szStorageName = pDataStorage->GetName();
				NStr::ToLower( szFilePath );
				NStr::ToLower( szStorageName );
				if ( szFilePath.find( szStorageName ) != 0 )
				{
					return;
				}

				int nPointIndex = szFilePath.rfind( "." );
				if ( nPointIndex >= 0 )
				{
					szFilePath = szFilePath.substr( 0, nPointIndex );
				}

				int nSlashIndex = szFilePath.rfind( "\\" );
				if ( nSlashIndex >= 0 )
				{
					resizeDialogOptions.szParameters[5] = szFilePath.substr( 0, nSlashIndex );
				}
				
				szFilePath = szFilePath.substr( szStorageName.size() );
				
				m_ScriptFileNameEdit.SetWindowText( szFilePath.c_str() );
			}
		}
	}
}

void CRMGCreateTemplateDialog::OnAddGraphButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		CFileDialog fileDialog( true, ".xml", "", OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, "XML files (*.xml)|*.xml||" );
		fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
		fileDialog.m_ofn.lpstrFile[0] = 0;			
		fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
		fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[2].c_str();
		
		if ( fileDialog.DoModal() == IDOK )
		{
			BeginWaitCursor();
			POSITION position = fileDialog.GetStartPosition();
			while ( position )
			{
				std::string szFilePath = fileDialog.GetNextPathName( position );
				std::string szStorageName = pDataStorage->GetName();
				NStr::ToLower( szFilePath );
				NStr::ToLower( szStorageName );
				if ( szFilePath.find( szStorageName ) != 0 )
				{
					return;
				}

				int nPointIndex = szFilePath.rfind( "." );
				if ( nPointIndex >= 0 )
				{
					szFilePath = szFilePath.substr( 0, nPointIndex );
				}

				int nSlashIndex = szFilePath.rfind( "\\" );
				if ( nSlashIndex >= 0 )
				{
					resizeDialogOptions.szParameters[2] = szFilePath.substr( 0, nSlashIndex );
				}

				szFilePath = szFilePath.substr( szStorageName.size() );
				
				LVFINDINFO findInfo;
				findInfo.flags = LVFI_STRING;
				findInfo.psz = szFilePath.c_str();

				int nOldItem = m_GraphsList.FindItem( &findInfo, -1 );
				if ( nOldItem != ( -1 ) )
				{
					bCreateControls = true;
					m_GraphsList.DeleteItem( nOldItem );
					for ( int nIndex = 0; nIndex < rTemplate.graphs.size(); ++nIndex )
					{
						if ( rTemplate.graphs[nIndex] == szFilePath )
						{
							rTemplate.graphs.erase( nIndex );
							break;
						}
					}
					if ( rTemplate.graphs.empty() )
					{
						rTemplate.size.x = 0;
						rTemplate.size.y = 0;
						rTemplate.nSeason = 0;
						rTemplate.szSeasonFolder.clear();
						rTemplate.usedScriptIDs.clear();
						rTemplate.usedScriptAreas.clear();
					}
					bCreateControls = false;
				}

				SRMGraph graph;
				if ( !LoadDataResource( szFilePath, ".bzm", false, 0, RMGC_GRAPH_XML_NAME, graph ) )
				{
					return;
				}

				if ( rTemplate.graphs.empty() )
				{
					rTemplate.size = graph.size;
					rTemplate.nSeason = graph.nSeason;
					rTemplate.szSeasonFolder = graph.szSeasonFolder;
					rTemplate.usedScriptIDs =  graph.usedScriptIDs;
					rTemplate.usedScriptAreas = graph.usedScriptAreas;
				}
				else
				{
					bool bCritical = false;
					std::string szCheckResult;
					
					if ( !( rTemplate.size == graph.size ) )
					{
						szCheckResult += NStr::Format( "Invalid Size:\r\nTemplate: [%dx%d]\r\nGraph: [%dx%d].\r\n", rTemplate.size.x, rTemplate.size.y, graph.size.x, graph.size.y );
						bCritical = true;
					}
					if ( rTemplate.nSeason != graph.nSeason )
					{
						std::string szSeason0;
						std::string szSeason1;
						RMGGetSeasonNameString( rTemplate.nSeason, rTemplate.szSeasonFolder, &szSeason0 );
						RMGGetSeasonNameString( graph.nSeason, graph.szSeasonFolder, &szSeason1 );
						szCheckResult += NStr::Format( "Invalid Season:\r\nTemplate: %s\r\nGraph: %s.\r\n", szSeason0.c_str(), szSeason1.c_str() );
						bCritical = true;
					}
					std::string szStringToCompare0 = rTemplate.szSeasonFolder;
					std::string szStringToCompare1 = graph.szSeasonFolder;
					NStr::ToLower( szStringToCompare0 );
					NStr::ToLower( szStringToCompare1 );
					if ( szStringToCompare0.compare( szStringToCompare1 ) != 0 )
					{
						szCheckResult += NStr::Format( "Invalid Season Folder:\r\nTemplate: <%s>\r\nGraph: <%s>.\r\n", szStringToCompare0.c_str(), szStringToCompare1.c_str() );
						bCritical = true;
					}
					if ( !( rTemplate.usedScriptIDs == graph.usedScriptIDs ) )
					{
						std::string szUsedScriptIDs0;
						std::string szUsedScriptIDs1;
						RMGGetUsedScriptIDsString( rTemplate.usedScriptIDs, &szUsedScriptIDs0 );
						RMGGetUsedScriptIDsString( graph.usedScriptIDs, &szUsedScriptIDs1 );
						szCheckResult += NStr::Format( "Different ScriptIDs used:\r\nTemplate: <%s>\r\nGraph: <%s>.\r\n", szUsedScriptIDs0.c_str(), szUsedScriptIDs1.c_str() );
					}
					if ( !( rTemplate.usedScriptAreas == graph.usedScriptAreas ) )
					{
						std::string szUsedScriptAreas0;
						std::string szUsedScriptAreas1;
						RMGGetUsedScriptAreasString( rTemplate.usedScriptAreas, &szUsedScriptAreas0 );
						RMGGetUsedScriptAreasString( graph.usedScriptAreas, &szUsedScriptAreas1 );
						szCheckResult += NStr::Format( "Different ScriptAreas used:\r\nTemplate: <%s>\r\nGraph: <%s>.\r\n", szUsedScriptAreas0.c_str(), szUsedScriptAreas1.c_str() );
					}
					if ( !szCheckResult.empty() )
					{
						CString strTitle;
						strTitle.LoadString( IDR_EDITORTYPE );
						if ( bCritical )
						{
							MessageBox( NStr::Format( "Can't Add Graph to Template!\r\nGraph <%s>,\r\nTemplate <%s>,\r\n%s",
																				szFilePath.c_str(),
																				szKey.c_str(),
																				szCheckResult.c_str() ), 
													strTitle,
													MB_ICONEXCLAMATION | MB_OK );
							return;
						}
						else
						{
							MessageBox( NStr::Format( "Warninig!\r\nGraph <%s>,\r\nTemplate <%s>,\r\n%s",
																				szFilePath.c_str(),
																				szKey.c_str(),
																				szCheckResult.c_str() ),
													strTitle,
													MB_ICONEXCLAMATION | MB_OK );
						}
					}
				}

				bCreateControls = true;
				int nNewItem = m_GraphsList.InsertItem( LVIF_TEXT, 0, szFilePath.c_str(), 0, 0, 0, 0 );
				if ( nNewItem == ( -1 ) )
				{
					bCreateControls = false;
					return;
				}
				rTemplate.graphs.push_back( szFilePath, CT_DEFAULT_GRAPH_WEIGHT );
				SetGraphItem( nNewItem, CT_DEFAULT_GRAPH_WEIGHT, graph );
				bCreateControls = false;
			}		
			SaveTemplateFromControls();
			UpdateControls();
			EndWaitCursor();
		}
		delete[] fileDialog.m_ofn.lpstrFile;
	}
}

void CRMGCreateTemplateDialog::OnDeleteGraphButton() 
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );
		if ( MessageBox( "Do you really want to DELETE selected Graphs?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			bCreateControls = true;
			int nSelectedItem = m_GraphsList.GetNextItem( -1, LVIS_SELECTED );
			while ( nSelectedItem != ( -1 ) )
			{
				std::string szGraphPath = m_GraphsList.GetItemText( nSelectedItem, 0 );
				m_GraphsList.DeleteItem( nSelectedItem );

				for ( int nIndex = 0; nIndex < rTemplate.graphs.size(); ++nIndex )
				{
					if ( rTemplate.graphs[nIndex] == szGraphPath )
					{
						rTemplate.graphs.erase( nIndex );
						break;
					}
				}
				if ( rTemplate.graphs.empty() )
				{
					rTemplate.size.x = 0;
					rTemplate.size.y = 0;
					rTemplate.nSeason = 0;
					rTemplate.szSeasonFolder.clear();
					rTemplate.usedScriptIDs.clear();
					rTemplate.usedScriptAreas.clear();
				}

				nSelectedItem = m_GraphsList.GetNextItem( -1, LVIS_SELECTED );
			}
			bCreateControls = false;
			SaveTemplateFromControls();
			UpdateControls();
		}
	}
}

void CRMGCreateTemplateDialog::OnGraphPropertiesButton() 
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		int nFocusedGraphItem = m_GraphsList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedGraphItem != ( -1 ) )
		{
			std::string szGraphKey = m_GraphsList.GetItemText( nFocusedGraphItem, 0 );
		
			for ( int nGraphIndex = 0; nGraphIndex < rTemplate.graphs.size(); ++nGraphIndex )
			{
				if ( rTemplate.graphs[nGraphIndex] == szGraphKey )
				{
					CRMGTemplateGraphPropertiesDialog templateGraphPropertiesDialog;
					templateGraphPropertiesDialog.m_strPath = szGraphKey.c_str();
					templateGraphPropertiesDialog.m_strStats.Format( "Overall weight: %d, average weight: %.2f", rTemplate.graphs.weight(), ( 1.0f * rTemplate.graphs.weight()  ) / rTemplate.graphs.size() );
					templateGraphPropertiesDialog.m_strWeight.Format( "%d", rTemplate.graphs.GetWeight( nGraphIndex ) );
					if ( templateGraphPropertiesDialog.DoModal() == IDOK )
					{
						int nBuffer = rTemplate.graphs.GetWeight( nGraphIndex );
						if ( sscanf( templateGraphPropertiesDialog.m_strWeight, "%d", &nBuffer ) > 0 )
						{
							rTemplate.graphs.SetWeight( nGraphIndex, nBuffer );
							SRMGraph graph;
							LoadDataResource( rTemplate.graphs[nGraphIndex], "", false, 0, RMGC_GRAPH_XML_NAME, graph );
							SetGraphItem( nFocusedGraphItem, rTemplate.graphs.GetWeight( nGraphIndex ), graph );
							SaveTemplateFromControls();
							UpdateControls();
						}
					}
					break;
				}
			}
		}
	}
}

void CRMGCreateTemplateDialog::OnAddGraphMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_GRAPH_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnAddGraphButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnDeleteGraphMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_GRAPH_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnDeleteGraphButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnGraphPropertiesMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_GRAPH_PROPERTIES_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnGraphPropertiesButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnDblclkGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_GRAPH_PROPERTIES_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnGraphPropertiesButton();
		}
	}
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnRclickGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 6 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_GRAPH_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_ADD_GRAPH_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_GRAPH_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_DELETE_GRAPH_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_GRAPH_PROPERTIES_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_GRAPH_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnItemchangedGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	UpdateControls();
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnKeydownGraphsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_GRAPH_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddGraphButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_GRAPH_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteGraphButton();
			}
		}
	}
	else if (  pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_GRAPH_PROPERTIES_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnGraphPropertiesButton();
			}
		}
	}
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnAddVsoButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		CFileDialog fileDialog( true, ".xml", "", OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, "XML files (*.xml)|*.xml||" );
		fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
		fileDialog.m_ofn.lpstrFile[0] = 0;			
		fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
		fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[3].c_str();
		
		if ( fileDialog.DoModal() == IDOK )
		{
			BeginWaitCursor();
			POSITION position = fileDialog.GetStartPosition();
			while ( position )
			{
				std::string szFilePath = fileDialog.GetNextPathName( position );
				std::string szStorageName = pDataStorage->GetName();
				NStr::ToLower( szFilePath );
				NStr::ToLower( szStorageName );
				if ( szFilePath.find( szStorageName ) != 0 )
				{
					return;
				}

				int nPointIndex = szFilePath.rfind( "." );
				if ( nPointIndex >= 0 )
				{
					szFilePath = szFilePath.substr( 0, nPointIndex );
				}

				int nSlashIndex = szFilePath.rfind( "\\" );
				if ( nSlashIndex >= 0 )
				{
					resizeDialogOptions.szParameters[3] = szFilePath.substr( 0, nSlashIndex );
				}

				szFilePath = szFilePath.substr( szStorageName.size() );
				
				LVFINDINFO findInfo;
				findInfo.flags = LVFI_STRING;
				findInfo.psz = szFilePath.c_str();

				int nOldItem = m_VSOList.FindItem( &findInfo, -1 );
				if ( nOldItem != ( -1 ) )
				{
					bCreateControls = true;
					m_VSOList.DeleteItem( nOldItem );
					for ( int nIndex = 0; nIndex < rTemplate.vso.size(); ++nIndex )
					{
						if ( rTemplate.vso[nIndex].szVSODescFileName == szFilePath )
						{
							rTemplate.vso.erase( nIndex );
							break;
						}
					}
					bCreateControls = false;
				}

				SRMVSODesc vso;
				vso.szVSODescFileName = szFilePath;

				bCreateControls = true;
				int nNewItem = m_VSOList.InsertItem( LVIF_TEXT, 0, szFilePath.c_str(), 0, 0, 0, 0 );
				if ( nNewItem == ( -1 ) )
				{
					bCreateControls = false;
					return;
				}
				rTemplate.vso.push_back( vso, CT_DEFAULT_VSO_WEIGHT );
				SetVSOItem( nNewItem, CT_DEFAULT_VSO_WEIGHT, vso );
				bCreateControls = false;
			}		
			SaveTemplateFromControls();
			UpdateControls();
			EndWaitCursor();
		}
		delete[] fileDialog.m_ofn.lpstrFile;
	}
}

void CRMGCreateTemplateDialog::OnDeleteVsoButton() 
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );
		if ( MessageBox( "Do you really want to DELETE selected VSO?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			bCreateControls = true;
			int nSelectedItem = m_VSOList.GetNextItem( -1, LVIS_SELECTED );
			while ( nSelectedItem != ( -1 ) )
			{
				std::string szVSOPath = m_VSOList.GetItemText( nSelectedItem, 0 );
				m_VSOList.DeleteItem( nSelectedItem );

				for ( int nIndex = 0; nIndex < rTemplate.vso.size(); ++nIndex )
				{
					if ( rTemplate.vso[nIndex].szVSODescFileName == szVSOPath )
					{
						rTemplate.vso.erase( nIndex );
						break;
					}
				}
				nSelectedItem = m_VSOList.GetNextItem( -1, LVIS_SELECTED );
			}
			bCreateControls = false;
			SaveTemplateFromControls();
			UpdateControls();
		}
	}
}

void CRMGCreateTemplateDialog::OnVsoPropertiesButton() 
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		int nFocusedVSOItem = m_VSOList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedVSOItem != ( -1 ) )
		{
			std::string szVSOKey = m_VSOList.GetItemText( nFocusedVSOItem, 0 );
		
			for ( int nVSOIndex = 0; nVSOIndex < rTemplate.vso.size(); ++nVSOIndex )
			{
				if ( rTemplate.vso[nVSOIndex].szVSODescFileName == szVSOKey )
				{
					CRMGTemplateVSOPropertiesDialog templateVSOPropertiesDialog;
					templateVSOPropertiesDialog.m_strPath = szVSOKey.c_str();
					templateVSOPropertiesDialog.m_strStats.Format( "Overall weight: %d, average weight: %.2f", rTemplate.vso.weight(), ( 1.0f * rTemplate.vso.weight()  ) / rTemplate.vso.size() );
					templateVSOPropertiesDialog.m_strWeight.Format( "%d", rTemplate.vso.GetWeight( nVSOIndex ) );
					templateVSOPropertiesDialog.m_strWidth.Format( "%.2f", rTemplate.vso[nVSOIndex].fWidth / fWorldCellSize );
					templateVSOPropertiesDialog.m_strOpacity.Format( "%.2f", rTemplate.vso[nVSOIndex].fOpacity * 100.0f );
					if ( templateVSOPropertiesDialog.DoModal() == IDOK )
					{
						int nBuffer = rTemplate.vso.GetWeight( nVSOIndex );
						if ( sscanf( templateVSOPropertiesDialog.m_strWeight, "%d", &nBuffer ) > 0 )
						{
							rTemplate.vso.SetWeight( nVSOIndex, nBuffer );
						}
						float fBuffer = rTemplate.vso[nVSOIndex].fWidth / fWorldCellSize;
						if ( sscanf( templateVSOPropertiesDialog.m_strWidth, "%g", &fBuffer ) > 0 )
						{
							rTemplate.vso[nVSOIndex].fWidth = fBuffer * fWorldCellSize;
						}
						fBuffer = rTemplate.vso[nVSOIndex].fOpacity * 100.0f;
						if ( sscanf( templateVSOPropertiesDialog.m_strOpacity, "%g", &fBuffer ) > 0 )
						{
							rTemplate.vso[nVSOIndex].fOpacity = fBuffer / 100.0f;
						}
						SetVSOItem( nFocusedVSOItem, rTemplate.vso.GetWeight( nVSOIndex ), rTemplate.vso[nVSOIndex] );
						SaveTemplateFromControls();
						UpdateControls();
					}
					break;
				}
			}
		}
	}
}

void CRMGCreateTemplateDialog::OnAddVsoMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_VSO_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnAddVsoButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnDeleteVsoMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_VSO_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnDeleteVsoButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnVsoPropertiesMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_VSO_PROPERTIES_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnVsoPropertiesButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnDblclkVsoList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_VSO_PROPERTIES_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnVsoPropertiesButton();
		}
	}
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnRclickVsoList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 7 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_VSO_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_ADD_VSO_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_VSO_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_DELETE_VSO_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_VSO_PROPERTIES_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_VSO_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnItemchangedVsoList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	UpdateControls();
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnKeydownVsoList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_VSO_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddVsoButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_VSO_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteVsoButton();
			}
		}
	}
	else if (  pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_VSO_PROPERTIES_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnVsoPropertiesButton();
			}
		}
	}
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnAddFieldButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		CFileDialog fileDialog( true, ".xml", "", OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, "XML files (*.xml)|*.xml||" );
		fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
		fileDialog.m_ofn.lpstrFile[0] = 0;			
		fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
		fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[4].c_str();
		
		if ( fileDialog.DoModal() == IDOK )
		{
			BeginWaitCursor();
			std::string szDefaultFieldPath;
			if ( ( rTemplate.nDefaultFieldIndex >= 0 ) && ( rTemplate.nDefaultFieldIndex < rTemplate.fields.size() ) )
			{
				szDefaultFieldPath = rTemplate.fields[rTemplate.nDefaultFieldIndex];
			}
			rTemplate.nDefaultFieldIndex = -1;

			POSITION position = fileDialog.GetStartPosition();
			while ( position )
			{
				std::string szFilePath = fileDialog.GetNextPathName( position );
				std::string szStorageName = pDataStorage->GetName();
				NStr::ToLower( szFilePath );
				NStr::ToLower( szStorageName );
				if ( szFilePath.find( szStorageName ) != 0 )
				{
					return;
				}

				int nPointIndex = szFilePath.rfind( "." );
				if ( nPointIndex >= 0 )
				{
					szFilePath = szFilePath.substr( 0, nPointIndex );
				}

				int nSlashIndex = szFilePath.rfind( "\\" );
				if ( nSlashIndex >= 0 )
				{
					resizeDialogOptions.szParameters[4] = szFilePath.substr( 0, nSlashIndex );
				}

				szFilePath = szFilePath.substr( szStorageName.size() );
				
				LVFINDINFO findInfo;
				findInfo.flags = LVFI_STRING;
				findInfo.psz = szFilePath.c_str();

				int nOldItem = m_FieldsList.FindItem( &findInfo, -1 );
				if ( nOldItem != ( -1 ) )
				{
					bCreateControls = true;
					m_FieldsList.DeleteItem( nOldItem );
					for ( int nIndex = 0; nIndex < rTemplate.fields.size(); ++nIndex )
					{
						if ( rTemplate.fields[nIndex] == szFilePath )
						{
							rTemplate.fields.erase( nIndex );
							break;
						}
					}
					bCreateControls = false;
				}

				SRMFieldSet field;
				if ( !LoadDataResource( szFilePath, ".bzm", false, 0, RMGC_FIELDSET_XML_NAME, field ) )
				{
					return;
				}

				bCreateControls = true;
				int nNewItem = m_FieldsList.InsertItem( LVIF_TEXT, 0, szFilePath.c_str(), 0, 0, 0, 0 );
				if ( nNewItem == ( -1 ) )
				{
					bCreateControls = false;
					return;
				}
				rTemplate.fields.push_back( szFilePath, CT_DEFAULT_FIELD_WEIGHT );
				SetFieldItem( nNewItem, CT_DEFAULT_FIELD_WEIGHT, false, field );
				bCreateControls = false;
			}		
			
			bCreateControls = true;
			int nSelectedItem = m_FieldsList.GetNextItem( -1, LVNI_ALL );
			while ( nSelectedItem != ( -1 ) )
			{
				std::string szFieldPath = m_FieldsList.GetItemText( nSelectedItem, 0 );
				if ( szFieldPath == szDefaultFieldPath )
				{
					for ( int nIndex = 0; nIndex < rTemplate.fields.size(); ++nIndex )
					{
						if ( rTemplate.fields[nIndex] == szFieldPath )
						{
							rTemplate.nDefaultFieldIndex = nIndex;
							m_FieldsList.SetItem( nSelectedItem, 2, LVIF_TEXT, CT_DEFAULT_FIELD_LABEL, 0, 0, 0, 0 );
							break;
						}
					}
				}
				nSelectedItem = m_FieldsList.GetNextItem( nSelectedItem, LVNI_ALL );
			}
			bCreateControls = false;
			
			SaveTemplateFromControls();
			UpdateControls();
			EndWaitCursor();
		}
		delete[] fileDialog.m_ofn.lpstrFile;
	}
}

void CRMGCreateTemplateDialog::OnDeleteFieldButton()
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );
		if ( MessageBox( "Do you really want to DELETE selected Fields?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
		{
			std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
			SRMTemplate &rTemplate = templates[szKey];

			bCreateControls = true;
			int nSelectedItem = m_FieldsList.GetNextItem( -1, LVIS_SELECTED );
			while ( nSelectedItem != ( -1 ) )
			{
				std::string szFieldPath = m_FieldsList.GetItemText( nSelectedItem, 0 );
				m_FieldsList.DeleteItem( nSelectedItem );

				for ( int nIndex = 0; nIndex < rTemplate.fields.size(); ++nIndex )
				{
					if ( rTemplate.fields[nIndex] == szFieldPath )
					{
						if ( rTemplate.nDefaultFieldIndex == nIndex )
						{
							rTemplate.nDefaultFieldIndex = -1;
						}
						rTemplate.fields.erase( nIndex );
						break;
					}
				}
				nSelectedItem = m_FieldsList.GetNextItem( -1, LVIS_SELECTED );
			}
			bCreateControls = false;
			SaveTemplateFromControls();
			UpdateControls();
		}
	}
}

void CRMGCreateTemplateDialog::OnFieldPropertiesButton() 
{
	int nFocusedItem = m_TemplatesList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_TemplatesList.GetItemText( nFocusedItem, 0 );
		SRMTemplate &rTemplate = templates[szKey];

		int nFocusedFieldItem = m_FieldsList.GetNextItem( -1, LVNI_FOCUSED );
		if ( nFocusedFieldItem != ( -1 ) )
		{
			std::string szFieldKey = m_FieldsList.GetItemText( nFocusedFieldItem, 0 );
		
			for ( int nFieldIndex = 0; nFieldIndex < rTemplate.fields.size(); ++nFieldIndex )
			{
				if ( rTemplate.fields[nFieldIndex] == szFieldKey )
				{
					CRMGTemplateFieldPropertiesDialog templatefieldPropertiesDialog;
					templatefieldPropertiesDialog.m_strPath = szFieldKey.c_str();
					templatefieldPropertiesDialog.m_strStats.Format( "Overall weight: %d, average weight: %.2f", rTemplate.fields.weight(), ( 1.0f * rTemplate.fields.weight()  ) / rTemplate.fields.size() );
					templatefieldPropertiesDialog.m_strWeight.Format( "%d", rTemplate.fields.GetWeight( nFieldIndex ) );
					templatefieldPropertiesDialog.m_bDefault = ( rTemplate.nDefaultFieldIndex == nFieldIndex );
					if ( templatefieldPropertiesDialog.DoModal() == IDOK )
					{
						int nBuffer = rTemplate.fields.GetWeight( nFieldIndex );
						if ( sscanf( templatefieldPropertiesDialog.m_strWeight, "%d", &nBuffer ) > 0 )
						{
							rTemplate.fields.SetWeight( nFieldIndex, nBuffer );
						}
						if ( rTemplate.nDefaultFieldIndex == nFieldIndex || templatefieldPropertiesDialog.m_bDefault )
						{
							if ( ( rTemplate.nDefaultFieldIndex >= 0 ) && ( rTemplate.nDefaultFieldIndex < rTemplate.fields.size() ) )
							{
								bCreateControls = true;
								int nSelectedItem = m_FieldsList.GetNextItem( -1, LVNI_ALL );
								while ( nSelectedItem != ( -1 ) )
								{
									std::string szInnerFieldPath = m_FieldsList.GetItemText( nSelectedItem, 0 );
									if ( szInnerFieldPath == rTemplate.fields[rTemplate.nDefaultFieldIndex] )
									{
										m_FieldsList.SetItem( nSelectedItem, 2, LVIF_TEXT, "", 0, 0, 0, 0 );
										break;
									}
									nSelectedItem = m_FieldsList.GetNextItem( nSelectedItem, LVNI_ALL );
								}
								bCreateControls = false;
							}
							rTemplate.nDefaultFieldIndex = -1;
						}

						if ( templatefieldPropertiesDialog.m_bDefault )
						{
							rTemplate.nDefaultFieldIndex = nFieldIndex;
							bCreateControls = true;
							m_FieldsList.SetItem( nFocusedFieldItem, 2, LVIF_TEXT, CT_DEFAULT_FIELD_LABEL, 0, 0, 0, 0 );
							bCreateControls = false;
						}

						bCreateControls = true;
						m_FieldsList.SetItem( nFocusedFieldItem, 1, LVIF_TEXT, NStr::Format( "%4d", rTemplate.fields.GetWeight( nFieldIndex ) ), 0, 0, 0, 0 );
						bCreateControls = false;
						SaveTemplateFromControls();
						UpdateControls();
					}
					break;
				}
			}
		}
	}
}

void CRMGCreateTemplateDialog::OnAddFieldMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_FIELD_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnAddFieldButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnDeleteFieldMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_FIELD_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnDeleteFieldButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnFieldPropertiesMenu() 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_FIELD_PROPERTIES_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnFieldPropertiesButton();
		}
	}
}

void CRMGCreateTemplateDialog::OnDblclkFieldsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_FIELD_PROPERTIES_BUTTON ) )
	{
		if ( pWnd->IsWindowEnabled() )
		{
			OnFieldPropertiesButton();
		}
	}
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnRclickFieldsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 8 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_FIELD_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_ADD_FIELD_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_FIELD_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_DELETE_FIELD_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_FIELD_PROPERTIES_BUTTON ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CT_FIELD_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnItemchangedFieldsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	UpdateControls();
	*pResult = 0;
}

void CRMGCreateTemplateDialog::OnKeydownFieldsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_ADD_FIELD_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddFieldButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_DELETE_FIELD_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteFieldButton();
			}
		}
	}
	else if (  pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( IDC_RMG_CT_FIELD_PROPERTIES_BUTTON ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnFieldPropertiesButton();
			}
		}
	}
	*pResult = 0;
}
