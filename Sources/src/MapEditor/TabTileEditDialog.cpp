#include "stdafx.h"
#include "editor.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"
#include "TabTileEditDialog.h"

#include "..\Image\Image.h"

#include "frames.h"
#include "TemplateEditorFrame1.h"

#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\IB_Types.h"
#include "RMG_FieldTilePropertiesDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const int CTabTileEditDialog::vID[] = 
{
	IDC_TE_TILES_LABEL,				//0		
	IDC_TE_TILES_LIST,				//1
};

CTabTileEditDialog::CTabTileEditDialog( CWnd* pParent )
	: CResizeDialog( CTabTileEditDialog::IDD, pParent )
{

	SetControlStyle( IDC_TE_TILES_LABEL, ANCHORE_LEFT_TOP | RESIZE_HOR );
	SetControlStyle( IDC_TE_TILES_LIST, ANCHORE_LEFT_TOP | RESIZE_HOR_VER );
}

void CTabTileEditDialog::DoDataExchange(CDataExchange* pDX)
{
	CResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TE_TILES_LIST, m_TilesList);
}

BEGIN_MESSAGE_MAP(CTabTileEditDialog, CResizeDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_NOTIFY(NM_RCLICK, IDC_TE_TILES_LIST, OnRclickilesList)
	ON_NOTIFY(NM_DBLCLK, IDC_TE_TILES_LIST, OnDblclkTilesList)
	ON_COMMAND(IDC_TE_TILE_PROPERTIES_MENU, OnTilePropertiesMenu)
END_MESSAGE_MAP()

BOOL CTabTileEditDialog::OnInitDialog() 
{
	CResizeDialog::OnInitDialog();
	
	CreateImageList();
	return TRUE;
}

void CTabTileEditDialog::OnDestroy() 
{
	CResizeDialog::OnDestroy();
	tilesImageList.DeleteImageList();
	seasonTilesIndices.clear();
}

void CTabTileEditDialog::OnSize(UINT nType, int cx, int cy) 
{
	CResizeDialog::OnSize( nType, cx, cy );
	
	if ( ::IsWindow( m_TilesList.m_hWnd ) )
	{
		m_TilesList.Arrange( LVA_DEFAULT );
	}
}

void CTabTileEditDialog::DeleteImageList()
{
	m_TilesList.DeleteAllItems();
	tilesImageList.DeleteImageList();
	seasonTilesIndices.clear();
}

void CTabTileEditDialog::CreateImageList()
{
	DWORD dwTime = GetTickCount();

	DeleteImageList();

	CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>();
	CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( pDataStorage && pODB && pImageProcessor )
	{
		
		CPtr<IImage> pMaskImage = 0;
		if ( CPtr<IDataStream> pMaskStream = pDataStorage->OpenStream( "editor\\terrain\\tilemask.tga", STREAM_ACCESS_READ ) )
		{
			pMaskImage = pImageProcessor->LoadImage( pMaskStream );
		}
		NI_ASSERT_T( pMaskImage != 0, NStr::Format( "CTabTileEditDialog::CreateImageList, Can't load scdale mask: editor\\terrain\\tilemask.tga" ) );
		
		const COLORREF zeroColor = RGB( 0, 0, 0 );
		CBitmap defaultObjectBitmap;
		defaultObjectBitmap.LoadBitmap( IDB_DEFAULT_OBJECT_IMAGE );
		CBitmap objectBitmap;

		m_TilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILE_SPACE_X,
																TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILE_SPACE_Y );
		m_TilesList.Arrange( LVA_DEFAULT );
		
		tilesImageList.Create( TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT, ILC_COLOR24, 0, 10 );
		m_TilesList.SetImageList( &tilesImageList, LVSIL_NORMAL );
		
		int nImageAdded = 0;
		{
			const int nImageAddedIndex = tilesImageList.Add( &defaultObjectBitmap, zeroColor );
			NI_ASSERT_T( nImageAddedIndex >= 0, NStr::Format( "CTabTileEditDialog::CreateImageList, Can't add image to image list, defaultObjectBitmap" ) );
			++nImageAdded;
		}
		for ( int nSeason = 0; nSeason < CMapInfo::SEASON_COUNT; ++nSeason )
		{
			CPtr<IDataStream> pImageStream = pDataStorage->OpenStream( NStr::Format( "%stileset%s", CMapInfo::SEASON_FOLDERS[nSeason], GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ).c_str() ), STREAM_ACCESS_READ );
			CPtr<IDataStream> pDescStream = pDataStorage->OpenStream( NStr::Format( "%stileset.xml", CMapInfo::SEASON_FOLDERS[nSeason] ), STREAM_ACCESS_READ );
			if ( pImageStream && pDescStream )
			{
				CPtr<IImage> pImage;
				{
					CPtr<IDDSImage> pDDSImage = pImageProcessor->LoadDDSImage( pImageStream );
					pImage = pImageProcessor->Decompress( pDDSImage );
				}
				STilesetDesc tilesetDesc;
				{
					CTreeAccessor tree = CreateDataTreeSaver( pDescStream, IDataTree::READ );
					tree.Add( "tileset", &tilesetDesc );		
				}			
				
				for ( int nTileIndex = 0; nTileIndex < tilesetDesc.terrtypes.size(); ++nTileIndex )
				{
					const int nMapIndex = tilesetDesc.terrtypes[nTileIndex].tiles[0].nIndex;
					const STileMapsDesc &rTileMapsDesc = tilesetDesc.tilemaps[nMapIndex];

					CPtr<IImage> pTempImage;
					if ( rTileMapsDesc.maps[3].y > rTileMapsDesc.maps[0].y )
					{
						pTempImage = pImageProcessor->CreateImage( ( pImage->GetSizeX() * ( rTileMapsDesc.maps[1].x - rTileMapsDesc.maps[2].x ) ) + 0.1f,
																											 ( pImage->GetSizeY() * ( rTileMapsDesc.maps[3].y - rTileMapsDesc.maps[0].y ) ) + 0.1f );
						const CRect sourceRect = CRect( rTileMapsDesc.maps[2].x * pImage->GetSizeX(),
																						rTileMapsDesc.maps[0].y * pImage->GetSizeY(),
																						rTileMapsDesc.maps[1].x * pImage->GetSizeX(),
																						rTileMapsDesc.maps[3].y * pImage->GetSizeY() );
						pTempImage->CopyFrom( pImage, &sourceRect, 0, 0 );
					}
					else
					{
						pTempImage = pImageProcessor->CreateImage( ( pImage->GetSizeX() * ( rTileMapsDesc.maps[1].x - rTileMapsDesc.maps[2].x ) ) + 0.1f,
																											 ( pImage->GetSizeY() * ( rTileMapsDesc.maps[0].y - rTileMapsDesc.maps[3].y ) ) + 0.1f );
						
						const CRect sourceRect = CRect( rTileMapsDesc.maps[2].x * pImage->GetSizeX(),
																						rTileMapsDesc.maps[3].y * pImage->GetSizeY(),
																						rTileMapsDesc.maps[1].x * pImage->GetSizeX(),
																						rTileMapsDesc.maps[0].y * pImage->GetSizeY() );
						pTempImage->CopyFrom( pImage, &sourceRect, 0, 0 );
						pTempImage->FlipY();
					}

					const float fRate = Min( ( 1.0f * TEFConsts::THUMBNAILTILE_WIDTH ) / pTempImage->GetSizeX(),
																	 ( 1.0f * TEFConsts::THUMBNAILTILE_HEIGHT ) / pTempImage->GetSizeY() );
					
					CPtr<IImage> pScaleImage = pImageProcessor->CreateScale( pTempImage, fRate, ISM_LANCZOS3 );
					
					const int nSizeX = pScaleImage->GetSizeX();
					const int nSizeY = pScaleImage->GetSizeY();

					if ( pMaskImage )
					{
						CPtr<IImage> pScaleMaskImage = pImageProcessor->CreateScale( pMaskImage, fRate, ISM_LANCZOS3 );

						CRect modulateRect;
						modulateRect.top = 0;
						modulateRect.left = 0;
						modulateRect.right = nSizeX;
						modulateRect.bottom = nSizeY;
						
						pScaleImage->ModulateColorFrom(	pScaleMaskImage, &modulateRect, 0, 0 );
					}
					
					if ( nSizeY < TEFConsts::THUMBNAILTILE_HEIGHT )
					{
						int nUp = ( TEFConsts::THUMBNAILTILE_HEIGHT - nSizeY ) / 2;
						if ( CPtr<IImage> pCenteredImage = pImageProcessor->CreateImage( TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT ) )
						{
							pCenteredImage->Set( 0 );
							CRect sourceRect = CRect( 0, 0, nSizeX, nSizeY );
							pCenteredImage->CopyFrom( pScaleImage, &sourceRect, 0, nUp );
							pScaleImage = pCenteredImage;
						}
					}
					else if ( nSizeX < TEFConsts::THUMBNAILTILE_WIDTH )
					{
						int nLeft = ( TEFConsts::THUMBNAILTILE_WIDTH - nSizeX ) / 2;
						if ( CPtr<IImage> pCenteredImage = pImageProcessor->CreateImage( TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT ) )
						{
							pCenteredImage->Set( 0 );
							CRect sourceRect = CRect( 0, 0, nSizeX, nSizeY );
							pCenteredImage->CopyFrom( pScaleImage, &sourceRect, nLeft, 0 );
							pScaleImage = pCenteredImage;
						}
					}

					objectBitmap.DeleteObject();
					
					BITMAPINFO bmi;
					bmi.bmiHeader.biSize  = sizeof( bmi.bmiHeader );
					bmi.bmiHeader.biWidth  = TEFConsts::THUMBNAILTILE_WIDTH;
					bmi.bmiHeader.biHeight = -TEFConsts::THUMBNAILTILE_HEIGHT;
					bmi.bmiHeader.biPlanes = 1;
					bmi.bmiHeader.biBitCount = 32;
					bmi.bmiHeader.biCompression = BI_RGB;
					bmi.bmiHeader.biSizeImage = 0;
					bmi.bmiHeader.biClrUsed = 0;
	
					CDC *pDC = GetDC();
					HBITMAP hbm = CreateCompatibleBitmap( pDC->m_hDC, TEFConsts::THUMBNAILTILE_WIDTH, TEFConsts::THUMBNAILTILE_HEIGHT );
					::SetDIBits( pDC->m_hDC, hbm, 0, TEFConsts::THUMBNAILTILE_HEIGHT, pScaleImage->GetLFB(), &bmi, DIB_RGB_COLORS );
					ReleaseDC( pDC );
					objectBitmap.Attach( hbm );

					int nImageAddedIndex = tilesImageList.Add( &objectBitmap, zeroColor );
					if ( nImageAddedIndex < 0 )
					{
						nImageAddedIndex = tilesImageList.Add( &defaultObjectBitmap, zeroColor );
						NI_ASSERT_T( nImageAddedIndex >= 0, NStr::Format( "CTabTileEditDialog::CreateImageList, Can't add image to image list, season %s, tile index: %d ", CMapInfo::SEASON_FOLDERS[nSeason], nTileIndex ) );
					}
					seasonTilesIndices[MAKELPARAM( nSeason, nTileIndex )] = nImageAddedIndex;
					++nImageAdded;
				}			
			}
		}
		tilesImageList.SetImageCount( nImageAdded );
	}
	dwTime = GetTickCount() - dwTime;
	NStr::DebugTrace( "CTabTileEditDialog::CreateTilesImageList: %d ms\n", dwTime );
}

void CTabTileEditDialog::CreateTilesList( const std::string &rszSeasonFolder, int nSelectedTileIndex )
{
	m_TilesList.DeleteAllItems();
	m_TilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILE_SPACE_X,
															TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILE_SPACE_Y );
	m_TilesList.Arrange( LVA_DEFAULT );

	if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
	{
		std::string szSeasonFolderToFind = rszSeasonFolder;
		NStr::ToLower( szSeasonFolderToFind );
		for ( int nSeason = 0; nSeason < CMapInfo::SEASON_COUNT; ++nSeason )
		{
			std::string szSeasonFolder = CMapInfo::SEASON_FOLDERS[nSeason];
			NStr::ToLower( szSeasonFolder );
			if ( szSeasonFolderToFind == szSeasonFolder )
			{
				STilesetDesc tilesetDesc;
				if ( LoadDataResource( NStr::Format( "%stileset", CMapInfo::SEASON_FOLDERS[nSeason] ), "", false, 0, "tileset", tilesetDesc ) )
				{
					for ( int nTileIndex = 0; nTileIndex < tilesetDesc.terrtypes.size(); ++nTileIndex )
					{
						const int nImageIndex = seasonTilesIndices[MAKELPARAM( nSeason, nTileIndex )];
						const int nInsertedItem = m_TilesList.InsertItem( nTileIndex , tilesetDesc.terrtypes[nTileIndex].szName.c_str() , nImageIndex );
						m_TilesList.SetItemData( nInsertedItem, nTileIndex );
						if ( nTileIndex == nSelectedTileIndex )
						{
							m_TilesList.SetItemState( nInsertedItem, LVIS_SELECTED, LVIS_SELECTED );
						}
					}
				}
				break;
			}
		}
	}
	m_TilesList.SetIconSpacing( TEFConsts::THUMBNAILTILE_WIDTH + TEFConsts::THUMBNAILTILE_SPACE_X,
															TEFConsts::THUMBNAILTILE_HEIGHT + TEFConsts::THUMBNAILTILE_SPACE_Y );
	m_TilesList.Arrange( LVA_DEFAULT );
}

void CTabTileEditDialog::OnRclickilesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu tabsMenu;
	tabsMenu.LoadMenu( IDM_TAB_POPUP_MENUS );
	CMenu *pMenu = tabsMenu.GetSubMenu( 4 );
	if ( pMenu )
	{
		pMenu->EnableMenuItem( IDC_TE_TILE_PROPERTIES_MENU, ( m_TilesList.GetSelectedCount() > 0 ) ? MF_ENABLED : MF_GRAYED );
		CPoint point;
		GetCursorPos( &point );
		pMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this, 0 );
	}
	tabsMenu.DestroyMenu();
	*pResult = 0;
}

void CTabTileEditDialog::OnDblclkTilesList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	ShowTileProperties();
	*pResult = 0;
}

void CTabTileEditDialog::ShowTileProperties()
{
	if ( m_TilesList.GetSelectedCount() > 0 )
	{
		int nSelectedItem = m_TilesList.GetNextItem( CB_ERR, LVNI_SELECTED );
		if (	nSelectedItem != CB_ERR )
		{
			int nSelectedTile = m_TilesList.GetItemData( nSelectedItem );
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				if ( ( nSelectedTile > 0 ) && ( nSelectedTile < pFrame->descrTile.terrtypes.size() ) )
				{
					CRMGFieldTilePropertiesDialog fieldTilePropertiesDialog;
					fieldTilePropertiesDialog.bDisableEditWeight = true;
					fieldTilePropertiesDialog.m_szName = pFrame->descrTile.terrtypes[nSelectedTile].szName.c_str();
					fieldTilePropertiesDialog.m_szVariants = NStr::Format( "%d", pFrame->descrTile.terrtypes[nSelectedTile].tiles.size() );
					
					LVITEM item;
					item.mask = LVIF_IMAGE;
					item.iItem = nSelectedItem;
					item.iSubItem = 0;
					m_TilesList.GetItem( &item );
					
					fieldTilePropertiesDialog.hIcon = tilesImageList.ExtractIcon( item.iImage );
					fieldTilePropertiesDialog.DoModal();
				}
			}
		}
	}
}

void CTabTileEditDialog::OnTilePropertiesMenu() 
{
	ShowTileProperties();
}
