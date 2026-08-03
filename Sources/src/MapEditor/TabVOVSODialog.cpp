#include "StdAfx.h"
#include "editor.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"
#include "TabVOVSODialog.h"
#include "../Misc/FileUtils.h"
#include "../RandomMapGen/Resource_Types.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CTabVOVSODialog::vID[] = 
{
	IDC_VSO_OBJECTS_LIST_LABEL,		//0
	IDC_VSO_OBJECTS_LIST,					//1

	IDC_VSO_SINGLE_WIDTH_RADIO,		//2
	IDC_VSO_MUlTI_WIDTH_RADIO,		//3
	IDC_VSO_ALL_WIDTH_RATIO,			//4

	IDC_VSO_SECTION_DELIMITER_00,	//5
	
	IDC_VSO_WIDTH_LABEL_LEFT,			//6
	IDC_VSO_WIDTH,								//7
	IDC_VSO_WIDTH_LABEL_RIGHT,		//8

	IDC_VSO_OPACITY_LABEL_LEFT,		//9
	IDC_VSO_OPACITY,							//10
	IDC_VSO_OPACITY_LABEL_RIGHT,	//11
};

/**
DWORD CTabVOVSOImageList::Fill( int nImageIndex )
{
	::EnterCriticalSection( GetCriticalSection() );
	
	::LeaveCriticalSection( GetCriticalSection() );
	return ERROR_SUCCESS;
}
/**/

CTabVOVSODialog::CTabVOVSODialog( CWnd* pParent )
	: CResizeDialog( CTabVOVSODialog::IDD, pParent ), bWidthChanged( false ), isSetEditCtrlValue( true )
{

	SetControlStyle( IDC_VSO_OBJECTS_LIST_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_VSO_OBJECTS_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
	
	SetControlStyle( IDC_VSO_SINGLE_WIDTH_RADIO, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_VSO_MUlTI_WIDTH_RADIO, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_VSO_ALL_WIDTH_RATIO, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_VSO_SECTION_DELIMITER_00, ANCHORE_LEFT_TOP | RESIZE_HOR );
	
	SetControlStyle( IDC_VSO_WIDTH_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_VSO_WIDTH, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_VSO_WIDTH_LABEL_RIGHT, ANCHORE_RIGHT_TOP );

	SetControlStyle( IDC_VSO_OPACITY_LABEL_LEFT, ANCHORE_LEFT_TOP );
	SetControlStyle( IDC_VSO_OPACITY, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_VSO_OPACITY_LABEL_RIGHT, ANCHORE_RIGHT_TOP );
}

BEGIN_MESSAGE_MAP( CTabVOVSODialog, CResizeDialog )
	ON_NOTIFY( LVN_ITEMCHANGED, IDC_VSO_OBJECTS_LIST, OnItemchangedVsoObjectsList )
	ON_BN_CLICKED( IDC_VSO_SINGLE_WIDTH_RADIO, OnVsoSingleWidthRadio )
	ON_BN_CLICKED( IDC_VSO_MUlTI_WIDTH_RADIO, OnVsoMultiWidthRatio )
	ON_BN_CLICKED( IDC_VSO_ALL_WIDTH_RATIO, OnVsoAllWidthRatio )
	ON_EN_CHANGE( IDC_VSO_WIDTH, OnChangeVsoWidth )
	ON_EN_CHANGE( IDC_VSO_OPACITY, OnChangeVsoOpacity )
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CTabVOVSODialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	if ( resizeDialogOptions.nParameters.size() < 1 )
	{
		resizeDialogOptions.nParameters.resize( 1 );
		resizeDialogOptions.nParameters[0] = CW_SINGLE;
	}

	if ( resizeDialogOptions.fParameters.size() < 2 )
	{
		resizeDialogOptions.fParameters.resize( 2 );
		resizeDialogOptions.fParameters[0] = 3.0f;
		resizeDialogOptions.fParameters[1] = 1.0f;
	}

	isSetEditCtrlValue = true;
	SetDlgItemText( IDC_VSO_WIDTH, NStr::Format("%.2f", resizeDialogOptions.fParameters[0] ) );
	SetDlgItemText( IDC_VSO_OPACITY, NStr::Format("%.2f", resizeDialogOptions.fParameters[1] * 100.0f ) );
	isSetEditCtrlValue = false;

	CheckRadioButton( IDC_VSO_SINGLE_WIDTH_RADIO, IDC_VSO_ALL_WIDTH_RATIO, IDC_VSO_SINGLE_WIDTH_RADIO + resizeDialogOptions.nParameters[0] );
	return TRUE;
}

void CTabVOVSODialog::OnSize( UINT nType, int cx, int cy ) 
{
	CResizeDialog::OnSize( nType, cx, cy );
	if ( CListCtrl *pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_VSO_OBJECTS_LIST ) ) )
	{
		pListCtrl->Arrange( LVA_DEFAULT );
	}
}

void CTabVOVSODialog::OnItemchangedVsoObjectsList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	*pResult = 0;
}

void CTabVOVSODialog::OnVsoSingleWidthRadio() 
{
	resizeDialogOptions.nParameters[0] = CW_SINGLE;
}

void CTabVOVSODialog::OnVsoMultiWidthRatio() 
{
	resizeDialogOptions.nParameters[0] = CW_MULTI;
}

void CTabVOVSODialog::OnVsoAllWidthRatio() 
{
	resizeDialogOptions.nParameters[0] = CW_ALL;
}

void CTabVOVSODialog::OnChangeVsoWidth() 
{
	if ( !isSetEditCtrlValue )
	{
		CString szValue;
		GetDlgItemText( IDC_VSO_WIDTH, szValue );
		float _fWidth = resizeDialogOptions.fParameters[0];
		if ( ( sscanf( szValue, "%g", &( resizeDialogOptions.fParameters[0] ) ) < 1 ) || ( resizeDialogOptions.fParameters[0] < 1.0f ) || ( resizeDialogOptions.fParameters[0] > 16.0f ) )
		{
			resizeDialogOptions.fParameters[0] = _fWidth;
			isSetEditCtrlValue = true;
			isSetEditCtrlValue = false;
		}
		else
		{
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				bWidthChanged = true;
				pFrame->inputStates.Update();
				bWidthChanged = false;
			}
		}
	}
}

void CTabVOVSODialog::OnChangeVsoOpacity() 
{
	if ( !isSetEditCtrlValue )
	{
		CString szValue;
		GetDlgItemText( IDC_VSO_OPACITY, szValue );
		float _fOpacity = resizeDialogOptions.fParameters[1];
		if ( ( sscanf( szValue, "%g", &resizeDialogOptions.fParameters[1] ) < 1 ) || ( resizeDialogOptions.fParameters[1] < 0.0f ) || ( resizeDialogOptions.fParameters[1] > 100.0f ) )
		{
			resizeDialogOptions.fParameters[1] = _fOpacity;
			isSetEditCtrlValue = true;
			isSetEditCtrlValue = false;
		}
		else
		{
			resizeDialogOptions.fParameters[1] /= 100.f;
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				bWidthChanged = true;
				pFrame->inputStates.Update();
				bWidthChanged = false;
			}
		}
	}
}

void CTabVOVSODialog::SetWidth( float _nWidth )
{
	if ( _nWidth < 1.0f )
	{
		_nWidth = 1;
	}
	else if ( _nWidth > 16.0f )
	{
		_nWidth = 16;
	}
	
	resizeDialogOptions.fParameters[0] = _nWidth;

	isSetEditCtrlValue = true;
	SetDlgItemText( IDC_VSO_WIDTH, NStr::Format( "%.2f", resizeDialogOptions.fParameters[0] ) );
	isSetEditCtrlValue = false;
}

void CTabVOVSODialog::SetOpacity( float _fOpacity )
{
	if ( _fOpacity < 0.0f )
	{
		_fOpacity = 0.0f;
	}
	else if ( _fOpacity > 1.0f )
	{
		_fOpacity = 1.0f;
	}
	
	resizeDialogOptions.fParameters[1] = _fOpacity;

	isSetEditCtrlValue = true;
	SetDlgItemText( IDC_VSO_OPACITY, NStr::Format("%.2f", resizeDialogOptions.fParameters[1] * 100.0f ) );
	isSetEditCtrlValue = false;
}

void CTabVOVSODialog::SetListLabel( const std::string &rszLabel )
{
	SetDlgItemText( IDC_VSO_OBJECTS_LIST_LABEL, rszLabel.c_str() );
}

void CTabVOVSODialog::CreateVSOList( const std::string &rVSOFolder )
{
	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		if ( CPtr<IImageProcessor> pImageProseccor = GetImageProcessor() )
		{
			CBitmap defaultObjectBitmap;
			defaultObjectBitmap.LoadBitmap( IDB_DEFAULT_OBJECT_IMAGE );
			
			CBitmap objectBitmap;
			
			if ( CListCtrl *pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_VSO_OBJECTS_LIST ) ) )
			{
				pListCtrl->DeleteAllItems();
				pListCtrl->SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + 
																	 TEFConsts::THUMBNAILTILE_SPACE_X,
																	 TEFConsts::THUMBNAILTILE_HEIGHT +
																	 TEFConsts::THUMBNAILTILE_SPACE_Y );

				vsoImageList.DeleteImageList();
				vsoImageList.Create( TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT, ILC_COLOR24, 0, 10 );
				int nImageAdded = 0;
				int nImageAddedIndex = 0;
				pListCtrl->SetImageList( &vsoImageList, LVSIL_NORMAL );

				const COLORREF zeroColor = RGB( 0, 0, 0 );

				szVSOFolder = rVSOFolder;
				if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
				{
					std::list<std::string> files;
					if ( pFrame->GetEnumFilesInDataStorage( rVSOFolder, &files ) )
					{

						std::vector<std::string> vsoNames;
						for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
						{
							vsoNames.push_back( *fileIterator );
						}
						std::sort( vsoNames.begin(),  vsoNames.end() );

						for ( std::vector<std::string>::const_iterator fileIterator = vsoNames.begin(); fileIterator != vsoNames.end(); ++fileIterator )
						{
							const std::string szImageFileName = ( *fileIterator ).substr( 0, ( *fileIterator ).rfind( '.' ) );
							{
								CPtr<IImage> pImage = LoadImageFromDDSImageResource( szImageFileName );
								if ( !pImage )
								{
									SVectorStripeObjectDesc vsoDesc;
									if ( LoadDataResource( szImageFileName, "", false, 0, "VSODescription", vsoDesc ) )
									{
										pImage = LoadImageFromDDSImageResource( vsoDesc.bottom.szTexture );
									}
								}
								if ( pImage )
								{
									if ( ( TEFConsts::THUMBNAILTILE_WIDTH != pImage->GetSizeX() ) ||
											 ( TEFConsts::THUMBNAILTILE_HEIGHT != pImage->GetSizeY() ) )
									{
										CPtr<IImage> pScaleImage = pImageProseccor->CreateScaleBySize( pImage, TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT, ISM_LANCZOS3 );
										pImage = pScaleImage;
									}
									objectBitmap.DeleteObject();
									
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
									::SetDIBits( pDC->m_hDC, hbm, 0, pImage->GetSizeY(), pImage->GetLFB(), &bmi, DIB_RGB_COLORS );
									ReleaseDC( pDC );
									objectBitmap.Attach( hbm );
									
									nImageAddedIndex = vsoImageList.Add( &objectBitmap, zeroColor );
								}
								else
								{
									nImageAddedIndex = vsoImageList.Add( &defaultObjectBitmap, zeroColor );
								}
							}
							std::string szFileName = ( *fileIterator ).substr( ( *fileIterator ).rfind( "\\" ) + 1 );
							szFileName = szFileName.substr( 0, szFileName.find( "." ) );
							int item = -1;
							item = pListCtrl->InsertItem( nImageAdded, szFileName.c_str(), nImageAddedIndex );
							++nImageAdded;
						}
						if ( !files.empty() )
						{
							pListCtrl->SetItemState( 0, LVNI_SELECTED, LVNI_SELECTED );
							pListCtrl->EnsureVisible( 0, false );
						}
						pListCtrl->Arrange( LVA_DEFAULT );			
						vsoImageList.SetImageCount( nImageAdded );
					}
				}
			}	
		}
	}
}

bool CTabVOVSODialog::GetDescriptionName( std::string *pVSODescName )
{
	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		if ( CListCtrl *pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_VSO_OBJECTS_LIST ) ) )
		{
			if ( !szVSOFolder.empty() )
			{
				const int item = pListCtrl->GetNextItem( -1, LVNI_SELECTED );
				if ( item >= 0 ) 
				{
					const std::string szVSODescName = szVSOFolder + std::string( pListCtrl->GetItemText( item, 0 ) );
					( *pVSODescName ) = szVSODescName;
					return true;
				}
			}
		}
	}
	return false;
}

void CTabVOVSODialog::OnDestroy() 
{
	CResizeDialog::SaveResizeDialogOptions();
	CResizeDialog::OnDestroy();
	vsoImageList.DeleteImageList();
}

void CTabVOVSODialog::DeleteImageList()
{
	if ( CListCtrl *pListCtrl = static_cast<CListCtrl*>( GetDlgItem( IDC_VSO_OBJECTS_LIST ) ) )
	{
		pListCtrl->DeleteAllItems();
	}
	vsoImageList.DeleteImageList();
}

void CTabVOVSODialog::CreateImageList()
{
}
