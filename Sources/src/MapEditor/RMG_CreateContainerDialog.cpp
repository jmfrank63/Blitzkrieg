#include "StdAfx.h"
#include "editor.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"

#include "RMG_PatchPropertiesDialog.h"
#include "RMG_CreateContainerDialog.h"

#include "..\RandomMapGen\Resource_Types.h"
#include "..\RandomMapGen\MapInfo_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE 
static char THIS_FILE[] = __FILE__;
#endif

const char *CC_CONTAINERS_XML_NAME = "Containers";
const char *CC_CONTAINERS_FILE_NAME = "Editor\\DefaultContainers";
const char *CC_CONTAINERS_DIALOG_TITLE = "Containers Composer";

const int   CC_CONTAINERS_COLUMN_COUNT = 12;
const char *CC_CONTAINERS_COLUMN_NAME  [CC_CONTAINERS_COLUMN_COUNT] = { "Path", "Size", "Count", "NORTH (0)", "EAST (90)", "SOUTH (180)", "WEST (270)", "Season", "Season Folder", "Supported Settings", "Used Script IDs", "Used Script Areas" };
const int   CC_CONTAINERS_COLUMN_FORMAT[CC_CONTAINERS_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
int					CC_CONTAINERS_COLUMN_WIDTH [CC_CONTAINERS_COLUMN_COUNT] = { 200, 60, 60, 80, 80, 80, 80, 80, 80, 80, 80, 80 };

const int   CC_PATCHES_COLUMN_COUNT = 7;
const char *CC_PATCHES_COLUMN_NAME  [CC_PATCHES_COLUMN_COUNT] = { "Path", "Size", "Setting", "NORTH (0)", "EAST (90)", "SOUTH (180)", "WEST (270)" };
const int   CC_PATCHES_COLUMN_FORMAT[CC_PATCHES_COLUMN_COUNT] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_LEFT };
int					CC_PATCHES_COLUMN_WIDTH [CC_PATCHES_COLUMN_COUNT] = { 200, 60, 120, 80, 80, 80, 80 };

const char *CC_SET_DIRECTION = "Yes";

int CALLBACK CC_ContainersCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGCreateContainerDialog* pContainerDialog = reinterpret_cast<CRMGCreateContainerDialog*>( lParamSort );

	CString strItem1 = pContainerDialog->m_ContainersList.GetItemText( lParam1, pContainerDialog->nSortColumn );
	CString strItem2 = pContainerDialog->m_ContainersList.GetItemText( lParam2, pContainerDialog->nSortColumn );
	if ( pContainerDialog->bContainersSortParam[pContainerDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

int CALLBACK CC_PatchesCompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	CRMGCreateContainerDialog* pContainerDialog = reinterpret_cast<CRMGCreateContainerDialog*>( lParamSort );

	CString strItem1 = pContainerDialog->m_PatchesList.GetItemText( lParam1, pContainerDialog->nSortColumn );
	CString strItem2 = pContainerDialog->m_PatchesList.GetItemText( lParam2, pContainerDialog->nSortColumn );
	if ( pContainerDialog->bPatchesSortParam[pContainerDialog->nSortColumn] )
	{
		return strcmp( strItem1, strItem2 );
	}
	else 
	{
		return strcmp( strItem2, strItem1 );
	}
}

const int CRMGCreateContainerDialog::vID[] = 
{
	IDC_RMG_CC_CONTAINERS_LABEL,						//0
	IDC_RMG_CC_CONTAINERS_LIST,							//1
	IDC_RMG_CC_PATCHES_LABEL,								//2
	IDC_RMG_CC_PATCHES_LIST,								//3
	IDC_RMG_CC_ADD_CONTAINER_BUTTON,				//4
	IDC_RMG_CC_DELETE_CONTAINER_BUTTON,			//5
	IDC_RMG_CC_CONTAINER_PROPERTIES_BUTTON,	//6
	IDC_RMG_CC_ADD_BUTTON,									//7
	IDC_RMG_CC_DELETE_BUTTON,								//8
	IDC_RMG_CC_PROPERTIES_BUTTON,						//9
	IDC_RMG_CC_SAVE_BUTTON,									//10
	IDOK,																		//11
	IDCANCEL,																//12
	IDC_RMG_CC_DELIMITER_00,								//13
	IDC_RMG_CC_CHECK_CONTAINERS_BUTTON,			//14
};

CRMGCreateContainerDialog::CRMGCreateContainerDialog( CWnd* pParent )
	: CResizeDialog( CRMGCreateContainerDialog::IDD, pParent ), isChanged( false ), bSomeDeleted( false ), nSortColumn( 0 )
{

	SetControlStyle( vID[0], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[1], ANCHORE_LEFT_TOP | RESIZE_HOR_VER, 0.5f, 0.5f, 1.0f, 0.5f );
	SetControlStyle( vID[2], ANCHORE_LEFT | ANCHORE_VER_CENTER | RESIZE_HOR );
	SetControlStyle( vID[3], ANCHORE_LEFT_BOTTOM | RESIZE_HOR_VER, 0.5f, 0.5f, 1.0f, 0.5f );
	SetControlStyle( vID[4], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[5], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[6], ANCHORE_RIGHT_TOP );
	SetControlStyle( vID[7], ANCHORE_RIGHT | ANCHORE_VER_CENTER );
	SetControlStyle( vID[8], ANCHORE_RIGHT | ANCHORE_VER_CENTER );
	SetControlStyle( vID[9], ANCHORE_RIGHT | ANCHORE_VER_CENTER );
	SetControlStyle( vID[10], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[11], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[12], ANCHORE_RIGHT_BOTTOM );
	SetControlStyle( vID[13], ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( vID[14], ANCHORE_RIGHT_TOP );
}

void CRMGCreateContainerDialog::DoDataExchange( CDataExchange* pDX )
{
	CResizeDialog::DoDataExchange( pDX );
	DDX_Control(pDX, IDC_RMG_CC_CONTAINERS_LIST, m_ContainersList);
	DDX_Control(pDX, IDC_RMG_CC_PATCHES_LIST, m_PatchesList);
}

BEGIN_MESSAGE_MAP(CRMGCreateContainerDialog, CResizeDialog )
	ON_BN_CLICKED(IDC_RMG_CC_ADD_BUTTON, OnAddButton)
	ON_BN_CLICKED(IDC_RMG_CC_DELETE_BUTTON, OnDeleteButton)
	ON_BN_CLICKED(IDC_RMG_CC_PROPERTIES_BUTTON, OnPropertiesButton)
	ON_BN_CLICKED(IDC_RMG_CC_ADD_CONTAINER_BUTTON, OnAddContainerButton)
	ON_BN_CLICKED(IDC_RMG_CC_DELETE_CONTAINER_BUTTON, OnDeleteContainerButton)
	ON_BN_CLICKED(IDC_RMG_CC_CONTAINER_PROPERTIES_BUTTON, OnContainerPropertiesButton)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_CC_PATCHES_LIST, OnItemchangedPatchesList)
	ON_NOTIFY(NM_DBLCLK, IDC_RMG_CC_PATCHES_LIST, OnDblclkPatchesList)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_CC_PATCHES_LIST, OnRclickPatchesList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_CC_PATCHES_LIST, OnKeydownPatchesList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_CC_PATCHES_LIST, OnColumnclickPatchesList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RMG_CC_CONTAINERS_LIST, OnItemchangedContainersList)
	ON_NOTIFY(NM_DBLCLK, IDC_RMG_CC_CONTAINERS_LIST, OnDblclkContainersList)
	ON_NOTIFY(NM_RCLICK, IDC_RMG_CC_CONTAINERS_LIST, OnRclickContainersList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_RMG_CC_CONTAINERS_LIST, OnKeydownContainersList)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_RMG_CC_CONTAINERS_LIST, OnColumnclickContainersList)
	ON_COMMAND(IDC_RMG_CC_ADD_MENU, OnAddMenu)
	ON_COMMAND(IDC_RMG_CC_DELETE_MENU, OnDeleteMenu)
	ON_COMMAND(IDC_RMG_CC_PROPERTIES_MENU, OnPropertiesMenu)
	ON_COMMAND(IDC_RMG_CC_ADD_CONTAINER_MENU, OnAddContainerMenu)
	ON_COMMAND(IDC_RMG_CC_DELETE_CONTAINER_MENU, OnDeleteContainerMenu)
	ON_BN_CLICKED(IDC_RMG_CC_SAVE_BUTTON, OnSaveButton)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVEAS, OnFileSaveas)
	ON_COMMAND(ID_FILE_EXIT, OnFileExit)
	ON_BN_CLICKED(IDC_RMG_CC_CHECK_CONTAINERS_BUTTON, OnCheckContainersButton)
END_MESSAGE_MAP()

BOOL CRMGCreateContainerDialog::OnInitDialog() 
{
	CResizeDialog ::OnInitDialog();

	if ( resizeDialogOptions.szParameters.size() < 4 )
	{
		resizeDialogOptions.szParameters.resize( 4, "" );
	}
	if ( resizeDialogOptions.szParameters[3].empty() )
	{
		resizeDialogOptions.szParameters[3] = CC_CONTAINERS_FILE_NAME;
	}
	if ( resizeDialogOptions.nParameters.size() < ( CC_CONTAINERS_COLUMN_COUNT + CC_PATCHES_COLUMN_COUNT ) )
	{
		resizeDialogOptions.nParameters.resize( CC_CONTAINERS_COLUMN_COUNT + CC_PATCHES_COLUMN_COUNT, 0 );
	}
	CreateControls();
	LoadContainersList();
	UpdateControls();
	return TRUE;
}

void CRMGCreateContainerDialog::OnOK() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CC_CONTAINERS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_ContainersList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < CC_PATCHES_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CC_CONTAINERS_COLUMN_COUNT] = m_PatchesList.GetColumnWidth( nColumnIndex );
	}

	SaveContainersList();
	CResizeDialog::OnOK();
}

void CRMGCreateContainerDialog::OnCancel() 
{
	for ( int nColumnIndex = 0; nColumnIndex < CC_CONTAINERS_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex] = m_ContainersList.GetColumnWidth( nColumnIndex );
	}
	for ( int nColumnIndex = 0; nColumnIndex < CC_PATCHES_COLUMN_COUNT; ++nColumnIndex )
	{
		resizeDialogOptions.nParameters[nColumnIndex + CC_CONTAINERS_COLUMN_COUNT] = m_PatchesList.GetColumnWidth( nColumnIndex );
	}

	CResizeDialog::OnCancel();
}

void CRMGCreateContainerDialog::OnAddButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	int nFocusedItem = m_ContainersList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{

		CFileDialog fileDialog( true, ".xml", "", OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, "All supported Files (*.bzm; *.xml)|*.bzm; *.xml|XML files (*.xml)|*.xml|BZM files (*.bzm)|*.bzm|All Files (*.*)|*.*||" );
		
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
				
				LVFINDINFO findInfo;
				findInfo.flags = LVFI_STRING;
				findInfo.psz = szFilePath.c_str();

				std::string szKey = m_ContainersList.GetItemText( nFocusedItem, 0 );
				SRMContainer &rContainer = containers[szKey];
				
				int nOldItem = m_PatchesList.FindItem( &findInfo, -1 );
				if ( nOldItem != ( -1 ) )
				{
					m_PatchesList.DeleteItem( nOldItem );
				}

				CMapInfo mapInfo;
				if ( !LoadTypedSuperLatestDataResource( szFilePath, ".bzm", 1, mapInfo ) )
				{
					return;
				}
				std::string szCheckResult;
				if ( rContainer.patches.empty() )
				{
					rContainer.nSeason = mapInfo.nSeason;
					rContainer.szSeasonFolder = mapInfo.szSeasonFolder;
					mapInfo.GetUsedScriptIDs( &( rContainer.usedScriptIDs ) );
					mapInfo.GetUsedScriptAreas( &( rContainer.usedScriptAreas ) );
				}
				else
				{
					if ( rContainer.nSeason != mapInfo.nSeason )
					{
						std::string szSeason0;
						std::string szSeason1;
						RMGGetSeasonNameString( rContainer.nSeason, rContainer.szSeasonFolder, &szSeason0 );
						RMGGetSeasonNameString( mapInfo.nSeason, mapInfo.szSeasonFolder, &szSeason1 );
						szCheckResult += NStr::Format( "Invalid Season:\r\nContainer: %s\r\nPatch: %s.\r\n", szSeason0.c_str(), szSeason1.c_str() );
					}
					std::string szStringToCompare0 = rContainer.szSeasonFolder;
					std::string szStringToCompare1 = mapInfo.szSeasonFolder;
					NStr::ToLower( szStringToCompare0 );
					NStr::ToLower( szStringToCompare1 );
					if ( szStringToCompare0.compare( szStringToCompare1 ) != 0 )
					{
						szCheckResult += NStr::Format( "Invalid Season Folder:\r\nContainer: <%s>\r\nPatch: <%s>.\r\n", szStringToCompare0.c_str(), szStringToCompare1.c_str() );
					}
					CUsedScriptIDs usedScriptIDs;
					mapInfo.GetUsedScriptIDs( &usedScriptIDs );
					if ( !( usedScriptIDs == rContainer.usedScriptIDs ) )
					{
						std::string szUsedScriptIDs0;
						std::string szUsedScriptIDs1;
						RMGGetUsedScriptIDsString( rContainer.usedScriptIDs, &szUsedScriptIDs0 );
						RMGGetUsedScriptIDsString( usedScriptIDs, &szUsedScriptIDs1 );
						szCheckResult += NStr::Format( "Invalid ScriptIDs:\r\nContainer: <%s>\r\nPatch: <%s>.\r\n", szUsedScriptIDs0.c_str(), szUsedScriptIDs1.c_str() );
					}
					CUsedScriptAreas usedScriptAreas;
					mapInfo.GetUsedScriptAreas( &usedScriptAreas );
					if ( !( usedScriptAreas == rContainer.usedScriptAreas ) )
					{
						std::string szUsedScriptAreas0;
						std::string szUsedScriptAreas1;
						RMGGetUsedScriptAreasString( rContainer.usedScriptAreas, &szUsedScriptAreas0 );
						RMGGetUsedScriptAreasString( usedScriptAreas, &szUsedScriptAreas1 );
						szCheckResult += NStr::Format( "Invalid ScriptAreas:\r\nContainer: <%s>\r\nPatch: <%s>.\r\n", szUsedScriptAreas0.c_str(), szUsedScriptAreas1.c_str() );
					}
					if ( !szCheckResult.empty() )
					{
						CString strTitle;
						strTitle.LoadString( IDR_EDITORTYPE );
						MessageBox( NStr::Format( "Can't Add Patch to Container!\r\nPatch <%s>,\r\nContainer <%s>,\r\n%s",
																			szFilePath.c_str(),
																			szKey.c_str(),
																			szCheckResult.c_str() ),
												strTitle,
												MB_ICONEXCLAMATION | MB_OK );
						return;
					}
				}

				int nNewItem = m_PatchesList.InsertItem( LVIF_TEXT, 0, szFilePath.c_str(), 0, 0, 0, 0 );
				if ( nNewItem == ( -1 ) )
				{
					return;
				}
				m_PatchesList.SetItem( nNewItem, 1, LVIF_TEXT, NStr::Format( "%4dx%-4d", mapInfo.terrain.patches.GetSizeX(), mapInfo.terrain.patches.GetSizeY() ), 0, 0, 0, 0 );
				m_PatchesList.SetItem( nNewItem, 2, LVIF_TEXT, "", 0, 0, 0, 0 );
				m_PatchesList.SetItem( nNewItem, 3, LVIF_TEXT, CC_SET_DIRECTION, 0, 0, 0, 0 );
				m_PatchesList.SetItem( nNewItem, 4, LVIF_TEXT, CC_SET_DIRECTION, 0, 0, 0, 0 );
				m_PatchesList.SetItem( nNewItem, 5, LVIF_TEXT, CC_SET_DIRECTION, 0, 0, 0, 0 );
				m_PatchesList.SetItem( nNewItem, 6, LVIF_TEXT, CC_SET_DIRECTION, 0, 0, 0, 0 );
			}		
			SaveContainerFromControls();
			UpdateControls();
			
			EndWaitCursor();
		}
		delete[] fileDialog.m_ofn.lpstrFile;
	}
}

void CRMGCreateContainerDialog::OnDeleteButton() 
{
	int nFocusedItem = m_ContainersList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );
		if ( MessageBox( "Do you really want to DELETE selected Patches?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
		{
			int nSelectedItem = m_PatchesList.GetNextItem( -1, LVIS_SELECTED );
			while ( nSelectedItem != ( -1 ) )
			{
				m_PatchesList.DeleteItem( nSelectedItem );
				nSelectedItem = m_PatchesList.GetNextItem( -1, LVIS_SELECTED );
			}
			SaveContainerFromControls();
			UpdateControls();
		}
	}
}

void CRMGCreateContainerDialog::OnPropertiesButton()
{
	int nFocusedItem = m_ContainersList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{

		int nSelectedItem = m_PatchesList.GetNextItem( -1, LVIS_SELECTED );
		int n0Count = 0;
		int n90Count = 0;
		int n180Count = 0;
		int n270Count = 0;
		int nSelectedItemCount = 0;
		CRMGPatchPropertiesDialog patchPropertiesDialog;
		while ( nSelectedItem != ( -1 ) )
		{
			CString strBuffer = m_PatchesList.GetItemText( nSelectedItem, 0 );
			patchPropertiesDialog.m_strPath = strBuffer;
			strBuffer = m_PatchesList.GetItemText( nSelectedItem, 1 );
			patchPropertiesDialog.m_strSize = strBuffer;
			
			strBuffer = m_PatchesList.GetItemText( nSelectedItem, 2 );
			if ( strBuffer.IsEmpty() )
			{
				patchPropertiesDialog.szPlace = RMGC_ANY_SETTING_NAME;						
			}
			else
			{
				patchPropertiesDialog.szPlace = strBuffer;						
			}

			strBuffer = m_PatchesList.GetItemText( nSelectedItem, 3 );
			if ( !strBuffer.IsEmpty() )
			{
				++n0Count;
			}
			strBuffer = m_PatchesList.GetItemText( nSelectedItem, 4 );
			if ( !strBuffer.IsEmpty() )
			{
				++n90Count;
			}
			strBuffer = m_PatchesList.GetItemText( nSelectedItem, 5 );
			if ( !strBuffer.IsEmpty() )
			{
				++n180Count;
			}
			strBuffer = m_PatchesList.GetItemText( nSelectedItem, 6 );
			if ( !strBuffer.IsEmpty() )
			{
				++n270Count;
			}

			++nSelectedItemCount;
			nSelectedItem = m_PatchesList.GetNextItem( nSelectedItem, LVIS_SELECTED );
		}

		if ( nSelectedItemCount > 0 )
		{
			if ( nSelectedItemCount > 1 )
			{
				patchPropertiesDialog.m_strPath = "Multiple selection...";
				patchPropertiesDialog.m_strSize = "...";
			}
			patchPropertiesDialog.m_n0   = ( n0Count   > 0 ) ? ( ( n0Count   != nSelectedItemCount ) ? 2 : 1 ) : 0;
			patchPropertiesDialog.m_n90  = ( n90Count  > 0 ) ? ( ( n90Count  != nSelectedItemCount ) ? 2 : 1 ) : 0;
			patchPropertiesDialog.m_n180 = ( n180Count > 0 ) ? ( ( n180Count != nSelectedItemCount ) ? 2 : 1 ) : 0;
			patchPropertiesDialog.m_n270 = ( n270Count > 0 ) ? ( ( n270Count != nSelectedItemCount ) ? 2 : 1 ) : 0;

			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				std::list<std::string> files;
				BeginWaitCursor();
				if ( pFrame->GetEnumFilesInDataStorage( RMGC_SETTING_DEFAULT_FOLDER, &files ) )
				{
					for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
					{
						std::string szFileName = ( *fileIterator ).substr( 0, ( *fileIterator ).find( "." ) );
						patchPropertiesDialog.szPlaces.push_back( szFileName );
					}
				}
				EndWaitCursor();
			}
			patchPropertiesDialog.szPlaces.push_back( RMGC_ANY_SETTING_NAME );

			if ( patchPropertiesDialog.DoModal() == IDOK )
			{
				nSelectedItem = m_PatchesList.GetNextItem( -1, LVIS_SELECTED );
				while ( nSelectedItem != ( -1 ) )
				{
					m_PatchesList.SetItem( nSelectedItem, 2, LVIF_TEXT, ( patchPropertiesDialog.szPlace == std::string( RMGC_ANY_SETTING_NAME ) ) ? "" : patchPropertiesDialog.szPlace.c_str(), 0, 0, 0, 0 );

					if ( patchPropertiesDialog.m_n0 < 2 )
					{
						m_PatchesList.SetItem( nSelectedItem, 3, LVIF_TEXT, patchPropertiesDialog.m_n0 ? CC_SET_DIRECTION : "", 0, 0, 0, 0 );
					}
					if ( patchPropertiesDialog.m_n90 < 2 )
					{
						m_PatchesList.SetItem( nSelectedItem, 4, LVIF_TEXT, patchPropertiesDialog.m_n90 ? CC_SET_DIRECTION : "", 0, 0, 0, 0 );
					}
					if ( patchPropertiesDialog.m_n180 < 2 )
					{
						m_PatchesList.SetItem( nSelectedItem, 5, LVIF_TEXT, patchPropertiesDialog.m_n180 ? CC_SET_DIRECTION : "", 0, 0, 0, 0 );
					}
					if ( patchPropertiesDialog.m_n270 < 2 )
					{
						m_PatchesList.SetItem( nSelectedItem, 6, LVIF_TEXT, patchPropertiesDialog.m_n270 ? CC_SET_DIRECTION : "", 0, 0, 0, 0 );
					}
					nSelectedItem = m_PatchesList.GetNextItem( nSelectedItem, LVIS_SELECTED );
				}			
				SaveContainerFromControls();
				UpdateControls();
			}
		}
	}
}

void CRMGCreateContainerDialog::OnAddContainerButton()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	/**
	if ( isChanged && IsValidContainerEntered() )
	{
		SaveContainerFromControls();
	}
	/**/

	CFileDialog fileDialog( true, ".xml", "", OFN_ALLOWMULTISELECT, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrFile = new char[0xFFFF];
	fileDialog.m_ofn.lpstrFile[0] = 0;			
	fileDialog.m_ofn.nMaxFile = 0xFFFF - 1; //на всякий пожарный
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[0].c_str();

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
				resizeDialogOptions.szParameters[0] = szFilePath.substr( 0, nSlashIndex );
			}

			szFilePath = szFilePath.substr( szStorageName.size() );

			SRMContainer container;
			container.size.x = 0;
			container.size.y = 0;
			container.nSeason = 0;
			container.szSeasonFolder.clear();
			LoadDataResource( szFilePath, "", false, 0, RMGC_CONTAINER_XML_NAME, container );
			
			LVFINDINFO findInfo;
			findInfo.flags = LVFI_STRING;
			findInfo.psz = szFilePath.c_str();

			int nOldItem = m_ContainersList.FindItem( &findInfo, -1 );
			if ( nOldItem != ( -1 ) )
			{
				m_ContainersList.DeleteItem( nOldItem );
			}
			
			int nNewItem = m_ContainersList.InsertItem( LVIF_TEXT, 0, szFilePath.c_str(), 0, 0, 0, 0 );
			if ( nNewItem != ( -1 ) )
			{
				containers[szFilePath] = container;
				SetContainerItem( nNewItem, container );
			}
		}		
		LoadContainerToControls();
		EndWaitCursor();
	}
	delete[] fileDialog.m_ofn.lpstrFile;
}

void CRMGCreateContainerDialog::OnDeleteContainerButton()
{
	CString strTitle;
	strTitle.LoadString( IDR_EDITORTYPE );
	if ( MessageBox( "Do you really want to DELETE selected Containers?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
	{
		int nSelectedItem = m_ContainersList.GetNextItem( -1, LVIS_SELECTED );
		if ( nSelectedItem != ( -1 ) )
		{
			/**
			if ( isChanged && IsValidContainerEntered() )
			{
				SaveContainerFromControls();
			}
			/**/

			bSomeDeleted = true;
			while ( nSelectedItem != ( -1 ) )
			{
				std::string szKey = m_ContainersList.GetItemText( nSelectedItem, 0 );
				containers.erase( szKey );
				m_ContainersList.DeleteItem( nSelectedItem );
				nSelectedItem = m_ContainersList.GetNextItem( -1, LVIS_SELECTED );
			}
			bSomeDeleted = false;
			LoadContainerToControls();
		}
	}
}

void CRMGCreateContainerDialog::OnContainerPropertiesButton() 
{
}

void CRMGCreateContainerDialog::OnItemchangedPatchesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	UpdateControls();
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnDblclkPatchesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	OnPropertiesButton();
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnRclickPatchesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 0 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[7] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CC_ADD_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( vID[8] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CC_DELETE_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( vID[9] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CC_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnKeydownPatchesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[7] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[8] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteButton();
			}
		}
	}
	else if (  pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[9] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnPropertiesButton();
			}
		}
	}
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnColumnclickPatchesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CC_PATCHES_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CC_PATCHES_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_PatchesList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_PatchesList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_PatchesList.SortItems( CC_PatchesCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bPatchesSortParam[nSortColumn] = !bPatchesSortParam[nSortColumn];
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnItemchangedContainersList(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if ( !bSomeDeleted )
	{
		LoadContainerToControls();
	}
	UpdateControls();
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnDblclkContainersList(NMHDR* pNMHDR, LRESULT* pResult)
{
	OnContainerPropertiesButton();
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnRclickContainersList(NMHDR* pNMHDR, LRESULT* pResult)
{
	CMenu composersMenu;
	composersMenu.LoadMenu( IDM_RMG_COMPOSERS_POPUP_MENUS );
	CMenu *pMenu = composersMenu.GetSubMenu( 1 );
	if ( pMenu )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[4] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CC_ADD_CONTAINER_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		if ( CWnd* pWnd = GetDlgItem( vID[5] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CC_DELETE_CONTAINER_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		/**
		if ( CWnd* pWnd = GetDlgItem( vID[6] ) )
		{
			pMenu->EnableMenuItem( IDC_RMG_CC_CONTAINER_PROPERTIES_MENU, pWnd->IsWindowEnabled() ? MF_ENABLED : MF_GRAYED );
		}
		/**/

		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	composersMenu.DestroyMenu();
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnKeydownContainersList(NMHDR* pNMHDR, LRESULT* pResult)
{
	LV_KEYDOWN* pLVKeyDown = (LV_KEYDOWN*)pNMHDR;
	if (  pLVKeyDown->wVKey == VK_INSERT )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[4] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnAddContainerButton();
			}
		}
	}
	else if ( pLVKeyDown->wVKey == VK_DELETE )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[5] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnDeleteContainerButton();
			}
		}
	}
	else if (  pLVKeyDown->wVKey == VK_SPACE )
	{
		if ( CWnd* pWnd = GetDlgItem( vID[6] ) )
		{
			if ( pWnd->IsWindowEnabled() )
			{
				OnContainerPropertiesButton();
			}
		}
	}
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnColumnclickContainersList(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	NI_ASSERT_T( ( pNMListView->iSubItem >= 0 ) && ( pNMListView->iSubItem < CC_CONTAINERS_COLUMN_COUNT ),
							 NStr::Format( "Invalid sort parameter: %d (0...%d)", pNMListView->iSubItem, CC_CONTAINERS_COLUMN_COUNT - 1 ) );
	
	nSortColumn = pNMListView->iSubItem;
	int nItemCount = m_ContainersList.GetItemCount();
	if ( nItemCount > 0 )
	{
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			m_ContainersList.SetItemData( nItemIndex, nItemIndex );	
		}
		m_ContainersList.SortItems( CC_ContainersCompareFunc, reinterpret_cast<LPARAM>( this ) );
	}
	bContainersSortParam[nSortColumn] = !bContainersSortParam[nSortColumn];
	*pResult = 0;
}

void CRMGCreateContainerDialog::OnAddMenu() 
{
	OnAddButton();
}

void CRMGCreateContainerDialog::OnDeleteMenu() 
{
	OnDeleteButton();
}

void CRMGCreateContainerDialog::OnPropertiesMenu() 
{
	OnPropertiesButton();
}

void CRMGCreateContainerDialog::OnAddContainerMenu() 
{
	OnAddContainerButton();
}

void CRMGCreateContainerDialog::OnDeleteContainerMenu() 
{
	OnDeleteContainerButton();
}

/**
void CRMGCreateContainerDialog::OnContainerPropertiesMenu() 
{
	OnContainerPropertiesButton();
}
/**/

bool CRMGCreateContainerDialog::LoadContainersList()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}

	SetWindowText( NStr::Format( "%s - [%s]", CC_CONTAINERS_DIALOG_TITLE, resizeDialogOptions.szParameters[3] ) );
	BeginWaitCursor();
	LoadDataResource( resizeDialogOptions.szParameters[3], "", false, 0, CC_CONTAINERS_XML_NAME, containers );
	
	m_ContainersList.DeleteAllItems();
	for ( CRMContainersHashMap::const_iterator containerIterator = containers.begin();  containerIterator != containers.end(); ++containerIterator )
	{
		int nNewItem = m_ContainersList.InsertItem( LVIF_TEXT, 0, containerIterator->first.c_str(), 0, 0, 0, 0 );
		if ( nNewItem == ( -1 ) )
		{
			EndWaitCursor();
			return false;
		}

		SetContainerItem( nNewItem, containerIterator->second );
	}
	LoadContainerToControls();
	EndWaitCursor();
	return true;
}

void CRMGCreateContainerDialog::OnSaveButton() 
{
	SaveContainersList();
}

bool CRMGCreateContainerDialog::SaveContainersList()
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}

	SetWindowText( NStr::Format( "%s - [%s]", CC_CONTAINERS_DIALOG_TITLE, resizeDialogOptions.szParameters[3] ) );
	BeginWaitCursor();
	for ( CRMContainersHashMap::const_iterator containerIterator = containers.begin();  containerIterator != containers.end(); ++containerIterator )
	{
		SRMContainer container = containerIterator->second;
		SaveDataResource( containerIterator->first, "", false, 0, RMGC_CONTAINER_XML_NAME, container );
	}

	if ( !SaveDataResource( resizeDialogOptions.szParameters[3], "", false, 0, CC_CONTAINERS_XML_NAME, containers ) )
	{
		EndWaitCursor();
		return false;
	}
	EndWaitCursor();
	return true;
}

bool CRMGCreateContainerDialog::LoadContainerToControls()
{
	m_PatchesList.DeleteAllItems();
	int nFocusedItem = m_ContainersList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_ContainersList.GetItemText( nFocusedItem, 0 );
		const SRMContainer &rContainer = containers[szKey];

		int nPatchIndex = 0;
		for ( CRMPatchesList::const_iterator patchIterator = rContainer.patches.begin(); patchIterator != rContainer.patches.end(); ++patchIterator )
		{
			int nNewItem = m_PatchesList.InsertItem( LVIF_TEXT, 0, patchIterator->szFileName.c_str(), 0, 0, 0, 0 );
			if ( nNewItem == ( -1 ) )
			{
				return false;
			}
			m_PatchesList.SetItem( nNewItem, 1, LVIF_TEXT, NStr::Format( "%4dx%-4d", patchIterator->size.x, patchIterator->size.y ), 0, 0, 0, 0 );
			m_PatchesList.SetItem( nNewItem, 2, LVIF_TEXT, patchIterator->szPlace.c_str(), 0, 0, 0, 0 );
			
			for ( int nPresentFlagIndex = 0; nPresentFlagIndex < 4; ++nPresentFlagIndex )
			{
				bool isPresent = false;
				for ( std::vector<int>::const_iterator nIndicesIndex = rContainer.indices[nPresentFlagIndex].begin(); nIndicesIndex != rContainer.indices[nPresentFlagIndex].end(); ++nIndicesIndex )
				{
					if ( ( *nIndicesIndex ) == nPatchIndex )
					{
						isPresent = true;
						break;
					}
				}
				m_PatchesList.SetItem( nNewItem, 3 + nPresentFlagIndex, LVIF_TEXT, isPresent ? CC_SET_DIRECTION : "" , 0, 0, 0, 0 );
			}
			++nPatchIndex;
		}
	}
	return true;
}

bool CRMGCreateContainerDialog::SaveContainerFromControls()
{
	int nFocusedItem = m_ContainersList.GetNextItem( -1, LVNI_FOCUSED );
	if ( nFocusedItem != ( -1 ) )
	{
		std::string szKey = m_ContainersList.GetItemText( nFocusedItem, 0 );

		SRMContainer &rContainer = containers[szKey];
		
		rContainer.patches.clear();
		rContainer.indices[SRMContainer::ANGLE_0].clear();
		rContainer.indices[SRMContainer::ANGLE_90].clear();
		rContainer.indices[SRMContainer::ANGLE_180].clear();
		rContainer.indices[SRMContainer::ANGLE_270].clear();

		int nX = 0;
		int nY = 0;
		int nItemCount = m_PatchesList.GetItemCount();
		for ( int nItemIndex = 0; nItemIndex < nItemCount; ++nItemIndex )
		{
			SRMPatch patch;
			CString strBuffer = m_PatchesList.GetItemText( nItemIndex, 0 );
			patch.szFileName = strBuffer;
			strBuffer = m_PatchesList.GetItemText( nItemIndex, 2 );
			patch.szPlace = strBuffer;

			strBuffer = m_PatchesList.GetItemText( nItemIndex, 1 );
			sscanf( strBuffer, "%dx%d", &( patch.size.x ), &( patch.size.y ) );
			if ( nX < patch.size.x )
			{
				nX = patch.size.x;
			}
			if ( nY < patch.size.y )
			{
				nY = patch.size.y;
			}
			rContainer.patches.push_back( patch );
			for ( int nPresentFlagIndex = 0; nPresentFlagIndex < 4; ++nPresentFlagIndex )
			{
				strBuffer = m_PatchesList.GetItemText( nItemIndex, 3 + nPresentFlagIndex );
				if ( !strBuffer.IsEmpty() )
				{
					rContainer.indices[nPresentFlagIndex].push_back( rContainer.patches.size() - 1 );
				}
			}
		}
		rContainer.size.x = nX;
		rContainer.size.y = nY;

		if ( rContainer.patches.empty() )
		{
			rContainer.nSeason = 0;
			rContainer.szSeasonFolder.clear();
			rContainer.usedScriptIDs.clear();
			rContainer.usedScriptAreas.clear();
		}

		SetContainerItem( nFocusedItem, rContainer );
	}
	isChanged = false;
	return true;
}
	
void CRMGCreateContainerDialog::SetContainerItem( int nItem, const SRMContainer &rContainer )
{
	m_ContainersList.SetItem( nItem, 1, LVIF_TEXT, NStr::Format( "%4dx%-4d", rContainer.size.x, rContainer.size.y ), 0, 0, 0, 0 );
	m_ContainersList.SetItem( nItem, 2, LVIF_TEXT, NStr::Format( "%4d", rContainer.patches.size() ), 0, 0, 0, 0 );
	m_ContainersList.SetItem( nItem, 3, LVIF_TEXT, NStr::Format( "%4d", rContainer.indices[SRMContainer::ANGLE_0].size() ), 0, 0, 0, 0 );
	m_ContainersList.SetItem( nItem, 4, LVIF_TEXT, NStr::Format( "%4d", rContainer.indices[SRMContainer::ANGLE_90].size() ), 0, 0, 0, 0 );
	m_ContainersList.SetItem( nItem, 5, LVIF_TEXT, NStr::Format( "%4d", rContainer.indices[SRMContainer::ANGLE_180].size() ), 0, 0, 0, 0 );
	m_ContainersList.SetItem( nItem, 6, LVIF_TEXT, NStr::Format( "%4d", rContainer.indices[SRMContainer::ANGLE_270].size() ), 0, 0, 0, 0 );
	
	std::string szSeasonName;
	RMGGetSeasonNameString( rContainer.nSeason, rContainer.szSeasonFolder, &szSeasonName );
	m_ContainersList.SetItem( nItem, 7, LVIF_TEXT, szSeasonName.c_str(), 0, 0, 0, 0 );
	m_ContainersList.SetItem( nItem, 8, LVIF_TEXT, rContainer.szSeasonFolder.c_str(), 0, 0, 0, 0 );

	std::string szSettingsNames;
	std::list<std::string> settings;
	if ( rContainer.GetSupportedSettings( &settings ) > 0 )
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
	m_ContainersList.SetItem( nItem, 9, LVIF_TEXT, szSettingsNames.c_str(), 0, 0, 0, 0 );

	std::string szUsedScriptIDs;
	RMGGetUsedScriptIDsString( rContainer.usedScriptIDs, &szUsedScriptIDs );
	m_ContainersList.SetItem( nItem, 10, LVIF_TEXT, szUsedScriptIDs.c_str(), 0, 0, 0, 0 );
	
	std::string szUsedScripAreas;
	RMGGetUsedScriptAreasString( rContainer.usedScriptAreas, &szUsedScripAreas );
	m_ContainersList.SetItem( nItem, 11, LVIF_TEXT, szUsedScripAreas.c_str(), 0, 0, 0, 0 );
}

bool CRMGCreateContainerDialog::IsValidContainerEntered()
{
	return true;
}

void CRMGCreateContainerDialog::UpdateControls()
{
	CWnd* pWnd = 0;
	if ( pWnd = GetDlgItem( vID[5] ) )
	{
		pWnd->EnableWindow( m_ContainersList.GetSelectedCount() > 0 );
	}
	if ( pWnd = GetDlgItem( vID[6] ) )
	{
		pWnd->EnableWindow( false );
	}
	if ( pWnd = GetDlgItem( vID[14] ) )
	{
		pWnd->EnableWindow( m_ContainersList.GetItemCount() > 0 );
	}
	if ( pWnd = GetDlgItem( vID[7] ) )
	{
		pWnd->EnableWindow( m_ContainersList.GetSelectedCount() > 0 );
	}
	if ( pWnd = GetDlgItem( vID[8] ) )
	{
		pWnd->EnableWindow( m_PatchesList.GetSelectedCount() > 0 );
	}
	if ( pWnd = GetDlgItem( vID[9] ) )
	{
		pWnd->EnableWindow( m_PatchesList.GetSelectedCount() > 0 );
	}
}

void CRMGCreateContainerDialog::CreateControls()
{
	m_ContainersList.SetExtendedStyle( m_ContainersList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CC_CONTAINERS_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex] = CC_CONTAINERS_COLUMN_WIDTH[nColumnIndex];
		}
		int nNewColumn = m_ContainersList.InsertColumn( nColumnIndex, CC_CONTAINERS_COLUMN_NAME[nColumnIndex], CC_CONTAINERS_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bContainersSortParam.push_back( true );
	}

	m_PatchesList.SetExtendedStyle( m_PatchesList.GetExtendedStyle() | LVS_EX_FULLROWSELECT );
	for ( int nColumnIndex = 0; nColumnIndex < CC_PATCHES_COLUMN_COUNT; ++nColumnIndex )
	{
		if ( resizeDialogOptions.nParameters[nColumnIndex + CC_CONTAINERS_COLUMN_COUNT] == 0 )
		{
			resizeDialogOptions.nParameters[nColumnIndex + CC_CONTAINERS_COLUMN_COUNT] = CC_PATCHES_COLUMN_WIDTH[nColumnIndex];
		}

		int nNewColumn = m_PatchesList.InsertColumn( nColumnIndex, CC_PATCHES_COLUMN_NAME[nColumnIndex], CC_PATCHES_COLUMN_FORMAT[nColumnIndex], resizeDialogOptions.nParameters[nColumnIndex + CC_CONTAINERS_COLUMN_COUNT], nColumnIndex );
		NI_ASSERT_T( nNewColumn == nColumnIndex,
								 NStr::Format("Invalid Column Index: %d (%d)", nNewColumn, nColumnIndex ) );
		bPatchesSortParam.push_back( true );
	}
}

void CRMGCreateContainerDialog::ClearControls()
{
	m_PatchesList.DeleteAllItems();
}

void CRMGCreateContainerDialog::OnFileNew() 
{
	SaveContainersList();
	resizeDialogOptions.szParameters[3] = CC_CONTAINERS_FILE_NAME;
	LoadContainersList();
	UpdateControls();
}

void CRMGCreateContainerDialog::OnFileOpen() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	SaveContainersList();

	CFileDialog fileDialog( true, ".xml", "", 0, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[2].c_str();
	
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
			resizeDialogOptions.szParameters[2] = szFilePath.substr( 0, nSlashIndex );
		}

		szFilePath = szFilePath.substr( szStorageName.size() );
		resizeDialogOptions.szParameters[3] = szFilePath;
		
		LoadContainersList();
		UpdateControls();
	}
}

void CRMGCreateContainerDialog::OnFileSave() 
{
	SaveContainersList();
}

void CRMGCreateContainerDialog::OnFileSaveas() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}


	CFileDialog fileDialog( false, ".xml", "", OFN_OVERWRITEPROMPT, "XML files (*.xml)|*.xml||" );
	fileDialog.m_ofn.lpstrInitialDir = resizeDialogOptions.szParameters[2].c_str();
	
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
			resizeDialogOptions.szParameters[2] = szFilePath.substr( 0, nSlashIndex );
		}

		szFilePath = szFilePath.substr( szStorageName.size() );
		resizeDialogOptions.szParameters[3] = szFilePath;
		SaveContainersList();
	}
}

void CRMGCreateContainerDialog::OnFileExit() 
{
	OnOK();
}

void CRMGCreateContainerDialog::OnCheckContainersButton() 
{
	IDataStorage* pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return;
	}

	BeginWaitCursor();
	SaveContainersList();

	std::string szStorageName = pDataStorage->GetName();
	NStr::ToLower( szStorageName );
	for ( CRMContainersHashMap::iterator containerIterator = containers.begin(); containerIterator != containers.end(); ++containerIterator )
	{
		containerIterator->second.size.x = 0;
		containerIterator->second.size.y = 0;
		containerIterator->second.nSeason = 0;
		containerIterator->second.szSeasonFolder.clear();
		containerIterator->second.usedScriptIDs.clear();
		containerIterator->second.usedScriptAreas.clear();

		int nX = 0;
		int nY = 0;
		for ( int nPatchIndex = 0; nPatchIndex < containerIterator->second.patches.size(); ++nPatchIndex )
		{
			if ( containerIterator->second.patches[nPatchIndex].szFileName.find( szStorageName ) == 0 )
			{
				containerIterator->second.patches[nPatchIndex].szFileName = containerIterator->second.patches[nPatchIndex].szFileName.substr( szStorageName.size() );
			}
			CMapInfo mapInfo;
			if ( !LoadTypedSuperLatestDataResource( containerIterator->second.patches[nPatchIndex].szFileName, ".bzm", 1, mapInfo ) )
			{
				CString strTitle;
				strTitle.LoadString( IDR_EDITORTYPE );
				MessageBox( NStr::Format( "Check FAILED!\r\nCan't load CMapInfo for patch <%s>,\r\ncontainer <%s>.\r\n",
																	containerIterator->second.patches[nPatchIndex].szFileName.c_str(),
																	containerIterator->first.c_str() ),
										strTitle,
										MB_ICONEXCLAMATION | MB_OK );
				LoadContainersList();
				EndWaitCursor();
				return;
			}

			if ( nPatchIndex == 0 )
			{
				containerIterator->second.nSeason = mapInfo.nSeason;
				containerIterator->second.szSeasonFolder = mapInfo.szSeasonFolder;
				mapInfo.GetUsedScriptIDs( &( containerIterator->second.usedScriptIDs ) );
				mapInfo.GetUsedScriptAreas( &( containerIterator->second.usedScriptAreas ) );
			}
			else
			{
				std::string szCheckResult;
				if ( containerIterator->second.nSeason != mapInfo.nSeason )
				{
					std::string szSeason0;
					std::string szSeason1;
					RMGGetSeasonNameString( containerIterator->second.nSeason, containerIterator->second.szSeasonFolder, &szSeason0 );
					RMGGetSeasonNameString( mapInfo.nSeason, mapInfo.szSeasonFolder, &szSeason1 );
					szCheckResult += NStr::Format( "Invalid Season:\r\nContainer: %s\r\nPatch: %s.\r\n", szSeason0.c_str(), szSeason1.c_str() );
				}
				std::string szStringToCompare0 = containerIterator->second.szSeasonFolder;
				std::string szStringToCompare1 = mapInfo.szSeasonFolder;
				NStr::ToLower( szStringToCompare0 );
				NStr::ToLower( szStringToCompare1 );
				if ( szStringToCompare0.compare( szStringToCompare1 ) != 0 )
				{
					szCheckResult += NStr::Format( "Invalid Season Folder:\r\nContainer: <%s>\r\nPatch: <%s>.\r\n", szStringToCompare0.c_str(), szStringToCompare1.c_str() );
				}
				CUsedScriptIDs usedScriptIDs;
				mapInfo.GetUsedScriptIDs( &usedScriptIDs );
				if ( !( usedScriptIDs == containerIterator->second.usedScriptIDs ) )
				{
					std::string szUsedScriptIDs0;
					std::string szUsedScriptIDs1;
					RMGGetUsedScriptIDsString( containerIterator->second.usedScriptIDs, &szUsedScriptIDs0 );
					RMGGetUsedScriptIDsString( usedScriptIDs, &szUsedScriptIDs1 );
					szCheckResult += NStr::Format( "Invalid ScriptIDs:\r\nContainer: <%s>\r\nPatch: <%s>.\r\n", szUsedScriptIDs0.c_str(), szUsedScriptIDs1.c_str() );
				}
				CUsedScriptAreas usedScriptAreas;
				mapInfo.GetUsedScriptAreas( &usedScriptAreas );
				if ( !( usedScriptAreas == containerIterator->second.usedScriptAreas ) )
				{
					std::string szUsedScriptAreas0;
					std::string szUsedScriptAreas1;
					RMGGetUsedScriptAreasString( containerIterator->second.usedScriptAreas, &szUsedScriptAreas0 );
					RMGGetUsedScriptAreasString( usedScriptAreas, &szUsedScriptAreas1 );
					szCheckResult += NStr::Format( "Invalid ScriptAreas:\r\nContainer: <%s>\r\nPatch: <%s>.\r\n", szUsedScriptAreas0.c_str(), szUsedScriptAreas1.c_str() );
				}
				if ( !szCheckResult.empty() )
				{
					CString strTitle;
					strTitle.LoadString( IDR_EDITORTYPE );
					MessageBox( NStr::Format( "Check FAILED!\r\nPatch <%s>,\r\nContainer <%s>,\r\n%s",
																		containerIterator->second.patches[nPatchIndex].szFileName.c_str(),
																		containerIterator->first.c_str(), szCheckResult.c_str() ),
											strTitle,
											MB_ICONEXCLAMATION | MB_OK );
					LoadContainersList();
					EndWaitCursor();
					return;
				}
			}
			if ( nX < containerIterator->second.patches[nPatchIndex].size.x )
			{
				nX = containerIterator->second.patches[nPatchIndex].size.x;
			}
			if ( nY < containerIterator->second.patches[nPatchIndex].size.y )
			{
				nY = containerIterator->second.patches[nPatchIndex].size.y;
			}
		}
		containerIterator->second.size.x = nX;
		containerIterator->second.size.y = nY;
	}
	SaveContainersList();
	LoadContainersList();
	EndWaitCursor();
}
