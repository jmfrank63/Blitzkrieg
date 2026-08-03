#include "StdAfx.h"
#include "editor.h"
#include "FenceSetupWindow.h"
#include "TemplateEditorFrame1.h"
#include "frames.h"
#include "../Image/Image.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CFenceSetupWindow::CFenceSetupWindow( CWnd* pParent )
	: CResizeDialog( CFenceSetupWindow::IDD, pParent )
{
	SetControlStyle( IDC_VO_FENCES_LIST_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_VO_FENCES_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
}

void CFenceSetupWindow::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_VO_FENCES_LIST, fencesList);
}

BEGIN_MESSAGE_MAP(CFenceSetupWindow, CResizeDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

void CFenceSetupWindow::OnSize( UINT nType, int cx, int cy ) 
{
	CResizeDialog::OnSize( nType, cx, cy );
	if ( ::IsWindow( fencesList.m_hWnd ) )
	{
		fencesList.Arrange( LVA_DEFAULT );
	}
}

BOOL CFenceSetupWindow::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	CreateFencesList();
	return TRUE;
}

void CFenceSetupWindow::CreateFencesList()
{
	DWORD dwTime = GetTickCount();

	if ( IDataStorage* pDataStorage = GetSingleton<IDataStorage>() )
	{
		if ( CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>() )
		{
			if ( CPtr<IImageProcessor> pImageProseccor = GetImageProcessor() )
			{
				fencesMap.clear();

				CBitmap defaultObjectBitmap;
				defaultObjectBitmap.LoadBitmap( IDB_DEFAULT_OBJECT_IMAGE );
				
				CBitmap objectBitmap;
				
				fencesList.DeleteAllItems();
				fencesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + 
																	 TEFConsts::THUMBNAILTILE_SPACE_X,
																	 TEFConsts::THUMBNAILTILE_HEIGHT +
																	 TEFConsts::THUMBNAILTILE_SPACE_Y );

				fencesImageList.DeleteImageList();
				fencesImageList.Create( TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT, ILC_COLOR24, 0, 10 );
				int nImageAdded = 0;
				int nImageAddedIndex = 0;
				fencesList.SetImageList( &fencesImageList, LVSIL_NORMAL );

				const COLORREF zeroColor = RGB( 0, 0, 0 );

				if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
				{
					std::vector<std::string> fencesNames;

					int nObjectsCount = pODB->GetNumDescs();
					const SGDBObjectDesc *pDescriptions = pODB->GetAllDescs(); 
					for ( int nObjectIndex = 0; nObjectIndex < nObjectsCount; ++nObjectIndex ) 
					{
						if ( pDescriptions[nObjectIndex].eGameType == SGVOGT_FENCE )
						{
							fencesNames.push_back( pDescriptions[nObjectIndex].szKey );
						}
					}
					std::sort( fencesNames.begin(), fencesNames.end() );
					
					if ( !fencesNames.empty() )
					{
						for ( std::vector<std::string>::const_iterator fileIterator = fencesNames.begin(); fileIterator != fencesNames.end(); ++fileIterator )
						{
							if ( const SGDBObjectDesc *pDesc = pODB->GetDesc( fileIterator->c_str() ) )
							{
								const std::string szFenceFileName = pDesc->szPath;
								if ( CPtr<IDataStream> pDataStream = pDataStorage->OpenStream( ( szFenceFileName + std::string( "\\icon.tga" ) ).c_str(), STREAM_ACCESS_READ ) )
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
										nImageAddedIndex = fencesImageList.Add( &objectBitmap, zeroColor );
									}
									else
									{
										nImageAddedIndex = fencesImageList.Add( &defaultObjectBitmap, zeroColor );
									}
								}
								int item = fencesList.InsertItem( nImageAdded, pDesc->szKey.c_str(), nImageAddedIndex );
								fencesList.SetItemData( item, nImageAdded );
								fencesMap[nImageAdded] = pDesc->szKey;
								++nImageAdded;
							}
						}
						if ( nImageAdded > 0 )
						{
							fencesList.SetItemState( 0, LVNI_SELECTED, LVNI_SELECTED );
							fencesList.EnsureVisible( 0, false );
						}
						fencesList.Arrange( LVA_DEFAULT );			
						fencesImageList.SetImageCount( nImageAdded );
					}
				}
			}
		}
	}
	dwTime = GetTickCount() - dwTime;
	NStr::DebugTrace( "CFenceSetupWindow::CreateFencesList(): %d ms\n", dwTime );
}

std::string CFenceSetupWindow::GetFenceName()
{
	int nSelectedFenceIndex = fencesList.GetNextItem( -1, LVNI_SELECTED );
	if ( nSelectedFenceIndex >= 0 )
	{
		return fencesMap[fencesList.GetItemData( nSelectedFenceIndex )];
	}
	else
	{
		nSelectedFenceIndex = fencesList.GetNextItem( -1, LVNI_ALL );
		return fencesMap[fencesList.GetItemData( nSelectedFenceIndex )];
	}
}

void CFenceSetupWindow::OnDestroy() 
{
	CResizeDialog::OnDestroy();
	fencesImageList.DeleteImageList();
}

void CFenceSetupWindow::DeleteImageList()
{
	fencesMap.clear();
	fencesList.DeleteAllItems();
	fencesImageList.DeleteImageList();
}

void CFenceSetupWindow::CreateImageList()
{
	CreateFencesList();
}
