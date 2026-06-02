#include "stdafx.h"
#include "editor.h"
#include "BridgeSetupDialog.h"
#include "TemplateEditorFrame1.h"
#include "frames.h"
#include "..\Image\Image.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CBridgeSetupDialog::CBridgeSetupDialog( CWnd* pParent )
	: CResizeDialog( CBridgeSetupDialog::IDD, pParent )
{
	SetControlStyle( IDC_VO_BRIGDES_LIST_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_VO_BRIGDES_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
}

void CBridgeSetupDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_VO_BRIGDES_LIST, bridgesList);
}

BEGIN_MESSAGE_MAP(CBridgeSetupDialog, CResizeDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CBridgeSetupDialog::OnSize( UINT nType, int cx, int cy ) 
{
	CResizeDialog::OnSize( nType, cx, cy );
	if ( ::IsWindow( bridgesList.m_hWnd ) )
	{
		bridgesList.Arrange( LVA_DEFAULT );
	}
}

BOOL CBridgeSetupDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	CreateBridgesList();
	return TRUE;
}

void CBridgeSetupDialog::CreateBridgesList()
{
	DWORD dwTime = GetTickCount();

	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		if ( CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>() )
		{
			if ( CPtr<IImageProcessor> pImageProseccor = GetImageProcessor() )
			{
				bridgesMap.clear();

				CBitmap defaultObjectBitmap;
				defaultObjectBitmap.LoadBitmap( IDB_DEFAULT_OBJECT_IMAGE );
				
				CBitmap objectBitmap;
				
				bridgesList.DeleteAllItems();
				bridgesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + 
																		TEFConsts::THUMBNAILTILE_SPACE_X,
																		TEFConsts::THUMBNAILTILE_HEIGHT +
																		TEFConsts::THUMBNAILTILE_SPACE_Y );

				bridgesImageList.DeleteImageList();
				bridgesImageList.Create( TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT, ILC_COLOR24, 0, 10 );
				int nImageAdded = 0;
				int nImageAddedIndex = 0;
				bridgesList.SetImageList( &bridgesImageList, LVSIL_NORMAL );

				const COLORREF zeroColor = RGB( 0, 0, 0 );

				if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
				{
					std::vector<std::string> bridgesNames;

					int nObjectsCount = pODB->GetNumDescs();
					const SGDBObjectDesc *pDescriptions = pODB->GetAllDescs(); 
					for ( int nObjectIndex = 0; nObjectIndex < nObjectsCount; ++nObjectIndex ) 
					{
						if ( pDescriptions[nObjectIndex].eGameType == SGVOGT_BRIDGE )
						{
							bridgesNames.push_back( pDescriptions[nObjectIndex].szKey );
						}
					}
					std::sort( bridgesNames.begin(), bridgesNames.end() );
					
					if ( !bridgesNames.empty() )
					{
						for ( std::vector<std::string>::const_iterator fileIterator = bridgesNames.begin(); fileIterator != bridgesNames.end(); ++fileIterator )
						{
							if ( const SGDBObjectDesc *pDesc = pODB->GetDesc( fileIterator->c_str() ) )
							{
								const std::string szBridgeFileName = pDesc->szPath;
								if ( CPtr<IDataStream> pDataStream = pDataStorage->OpenStream( ( szBridgeFileName + std::string( "\\icon.tga" ) ).c_str(), STREAM_ACCESS_READ ) )
								{
									CPtr<IImage> pImage = pImageProseccor->LoadImage( pDataStream );
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
										nImageAddedIndex = bridgesImageList.Add( &objectBitmap, zeroColor );
									}
									else
									{
										nImageAddedIndex = bridgesImageList.Add( &defaultObjectBitmap, zeroColor );
									}
								}
								int item = bridgesList.InsertItem( nImageAdded, pDesc->szKey.c_str(), nImageAddedIndex );
								bridgesList.SetItemData( item, nImageAdded );
								bridgesMap[nImageAdded] = pDesc->szKey;
								++nImageAdded;
							}
						}
						if ( nImageAdded > 0 )
						{
							bridgesList.SetItemState( 0, LVNI_SELECTED, LVNI_SELECTED );
							bridgesList.EnsureVisible( 0, false );
						}
						bridgesList.Arrange( LVA_DEFAULT );			
						bridgesImageList.SetImageCount( nImageAdded );
					}
				}
			}
		}
	}
	dwTime = GetTickCount() - dwTime;
	NStr::DebugTrace( "CBridgeSetupDialog::CreateBridgesList(): %d ms\n", dwTime );
}

std::string CBridgeSetupDialog::GetBridgeName()
{
	int nSelectedBridgeIndex = bridgesList.GetNextItem( -1, LVNI_SELECTED );
	if ( nSelectedBridgeIndex >= 0 )
	{
		return bridgesMap[bridgesList.GetItemData( nSelectedBridgeIndex )];
	}
	else
	{
		nSelectedBridgeIndex = bridgesList.GetNextItem( -1, LVNI_ALL );
		return bridgesMap[bridgesList.GetItemData( nSelectedBridgeIndex )];
	}
}

void CBridgeSetupDialog::OnDestroy() 
{
	CResizeDialog::OnDestroy();
	bridgesImageList.DeleteImageList();
}

void CBridgeSetupDialog::DeleteImageList()
{
	bridgesMap.clear();
	bridgesList.DeleteAllItems();
	bridgesImageList.DeleteImageList();
}

void CBridgeSetupDialog::CreateImageList()
{
	CreateBridgesList();
}
