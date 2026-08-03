
#include "StdAfx.h"
#include "..\Image\image.h"
#include "ThumbList.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


SThumbItems::SThumbItems() : nSelectedItem(0), thumbDataList(0)
{
}


CThumbList::CThumbList( bool bHorz )
{
	m_pActiveThumbItems = 0;
	bHorizontal = bHorz;
	bValidIML = false;
}

CThumbList::~CThumbList()
{
}


BEGIN_MESSAGE_MAP(CThumbList, CWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_NOTIFY(NM_CLICK, IDC_LIST_THUMB_CONTROL, OnClickListThumb)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_THUMB_CONTROL, OnDblClickListThumb)
	ON_NOTIFY(LVN_KEYDOWN, IDC_LIST_THUMB_CONTROL, OnKeydownListThumb)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_THUMB_CONTROL, OnItemStateChanged)
END_MESSAGE_MAP()


int CThumbList::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

/*
	DWORD dwStyle = LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_ALIGNLEFT | LVS_ICON |
		WS_CHILD | WS_VISIBLE | WS_BORDER;
*/

	DWORD dwStyle = LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_ICON |
		WS_CHILD | WS_VISIBLE | WS_BORDER;

	if ( bHorizontal )
		dwStyle |= LVS_ALIGNLEFT;
	else
		dwStyle |= LVS_ALIGNTOP;

	bool bCreated = m_ListThumbnail.Create( dwStyle, CRect(0, 0, 0, 0), this, IDC_LIST_THUMB_CONTROL );
	NI_ASSERT(bCreated);

	m_ListThumbnail.SetIconSpacing( THUMBNAIL_WIDTH + THUMBNAIL_SPACE_X, THUMBNAIL_WIDTH + THUMBNAIL_SPACE_Y );
	m_ListThumbnail.ShowWindow( SW_SHOW );
  m_ListThumbnail.UpdateWindow();

	return 0;
}

void CThumbList::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);

	if ( ::IsWindow(m_ListThumbnail) )
	{
		m_ListThumbnail.SetWindowPos( NULL, 0, 0, cx, cy,
			SWP_NOMOVE |
			SWP_NOACTIVATE |
			SWP_NOZORDER );

		m_ListThumbnail.Arrange( LVA_ALIGNTOP );
	}
}

void CThumbList::OnItemStateChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if ( pNMListView->uChanged == LVIF_STATE && pNMListView->uNewState == 3 )
	{
		int nID = GetDlgCtrlID();
		GetParent()->PostMessage( WM_THUMB_LIST_SELECT, nID );
	}
}

void CThumbList::OnClickListThumb(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if ( !m_pActiveThumbItems )
	{
		*pResult = 0;
		return;
	}

	NMITEMACTIVATE *pActivate = (NMITEMACTIVATE*)pNMHDR;
	if ( pActivate->iItem < 0 )
	{
		CRect rcClient;
		m_ListThumbnail.GetClientRect( &rcClient );
		const int nItemCount = m_ListThumbnail.GetItemCount();
		for ( int i=0; i<nItemCount; ++i )
		{
			CRect rcItem;
			if ( m_ListThumbnail.GetItemRect( i, &rcItem, LVIR_BOUNDS ) )
			{
				if ( bHorizontal )
				{
					rcItem.top = rcClient.top;
					rcItem.bottom = rcClient.bottom;
				}
				else
				{
					rcItem.left = rcClient.left;
					rcItem.right = rcClient.right;
				}
				if ( rcItem.PtInRect( pActivate->ptAction ) )
				{
					SelectItem( i );
					GetParent()->PostMessage( WM_THUMB_LIST_SELECT, GetDlgCtrlID() );
					break;
				}
			}
		}
	}
	*pResult = 0;
}

void CThumbList::OnDblClickListThumb(NMHDR* pNMHDR, LRESULT* pResult)
{
	if ( !m_pActiveThumbItems )
	{
		*pResult = 0;
		return;
	}

	int nID = GetDlgCtrlID();
	GetParent()->PostMessage( WM_THUMB_LIST_DBLCLK, nID );
	*pResult = 0;
}

void CThumbList::OnKeydownListThumb(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDow = (LV_KEYDOWN*)pNMHDR;
	*pResult = 0;

	if ( pLVKeyDow->wVKey == VK_DELETE )
	{
		int nID = GetDlgCtrlID();
		GetParent()->PostMessage( WM_THUMB_LIST_DELETE, nID );

/*
		int nSelected = GetSelectedItemIndex();
		if ( nSelected != -1 )
		{
			DWORD dwData = GetUserDataForItem( nSelected );
			if ( dwData == 0 )			//Значит это не Selected Thumb List
				return;

			ASSERT( bHorizontal == true );			//это дополнительное условие что выбран Selected Thumb List
			int nID = GetDlgCtrlID();
			GetParent()->PostMessage( WM_THUMB_LIST_DELETE, nID );
			DeleteItem( nSelected );

		}
*/
	}
}

void CThumbList::SetActiveThumbItems( SThumbItems *pNewActiveThumbs, CImageList *pIML )
{
	if ( m_pActiveThumbItems == pNewActiveThumbs )
		return;

	m_pActiveThumbItems = pNewActiveThumbs;
	if ( m_pActiveThumbItems == 0 )
	{
		m_ListThumbnail.DeleteAllItems();
		return;
	}

	if ( pIML )			//Если валидный image list то его устанавливаю, иначе оставляю старый
	{
		bValidIML = true;
		m_ListThumbnail.SetImageList( pIML, LVSIL_NORMAL);
	}

	CreateListElements();
}

void CThumbList::CreateListElements()
{
	m_ListThumbnail.DeleteAllItems();

	int i = 0;
	for ( CListOfThumbData::iterator it=m_pActiveThumbItems->thumbDataList.begin(); it!=m_pActiveThumbItems->thumbDataList.end(); ++it )
	{
		m_ListThumbnail.InsertItem( i, it->szThumbName.c_str(), it->nImageId );
		m_ListThumbnail.SetItemData( i, it->dwData );
		i++;
	}

	m_ListThumbnail.SetItemState( m_pActiveThumbItems->nSelectedItem, LVIS_SELECTED, LVIS_SELECTED );
	m_ListThumbnail.Arrange( LVA_ALIGNTOP );
}

int CThumbList::LoadImageToImageList( CImageList *pIML, char *szFileName, const char *szDir, bool bShowAlpha  )
{
	IImageProcessor *pImageProcessor = GetImageProcessor();
	CPtr<IDataStorage> pStorage = OpenStorage( szDir, STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	if ( !pStorage )
		return -1;

	CPtr<IDataStream> pStream = pStorage->OpenStream( szFileName, STREAM_ACCESS_READ );
	if ( !pStream )
		return -1;
	CPtr<IImage> pImage = pImageProcessor->LoadImage( pStream );
	if ( !pImage )
		return -1;
	
	
	int nSizeX = pImage->GetSizeX();
	int nSizeY = pImage->GetSizeY();
	double fRateX = (double) THUMBNAIL_WIDTH/nSizeX;
	double fRateY = (double) THUMBNAIL_HEIGHT/nSizeY;
	double fRate = min( fRateX, fRateY );
	CPtr<IImage> pScaleImage = pImageProcessor->CreateScale( pImage, fRate, ISM_BOX );
	NI_ASSERT( pScaleImage != 0 );
	
	nSizeX = pScaleImage->GetSizeX();
	nSizeY = pScaleImage->GetSizeY();
	
	
	if ( bShowAlpha )
	{
		SColor *p = pScaleImage->GetLFB();
		for ( int y=0; y<nSizeY; y++ )
		{
			for ( int x=0; x<nSizeX; x++ )
			{
				p[y*nSizeX + x].r = p[y*nSizeX + x].g = p[y*nSizeX + x].b = p[y*nSizeX + x].a;
			}
		}
	}

	if ( nSizeY < THUMBNAIL_HEIGHT )
	{
		int nUp = (THUMBNAIL_HEIGHT - nSizeY)/2;
		CPtr<IImage> pCenteredImage = pImageProcessor->CreateImage( THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT );
		pCenteredImage->Set( 0xff000000 );
		RECT rc = { 0, 0, nSizeX, nSizeY };
		pCenteredImage->CopyFrom( pScaleImage, &rc, 0, nUp );
		pScaleImage = pCenteredImage;
	}
	else if ( nSizeX < THUMBNAIL_WIDTH )
	{
		int nLeft = (THUMBNAIL_WIDTH - nSizeX)/2;
		CPtr<IImage> pCenteredImage = pImageProcessor->CreateImage( THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT );
		pCenteredImage->Set( 0xff000000 );
		RECT rc = { 0, 0, nSizeX, nSizeY };
		pCenteredImage->CopyFrom( pScaleImage, &rc, nLeft, 0 );
		pScaleImage = pCenteredImage;
	}
	
	nSizeX = pScaleImage->GetSizeX();
	nSizeY = pScaleImage->GetSizeY();
	NI_ASSERT( nSizeX == 100 );
	NI_ASSERT( nSizeY == 100 );

	{
		SColor *p = pScaleImage->GetLFB();
		for ( int y=0; y<nSizeY; y++ )
		{
			for ( int x=0; x<nSizeX; x++ )
			{
				SColor &color = p[y*nSizeX + x];
				int nAlpha = color.a;

				color.r = BYTE( color.r * nAlpha / 255 );
				color.g = BYTE( color.g * nAlpha / 255 );
				color.b = BYTE( color.b * nAlpha / 255 );
				color.a = 255;
			}
		}
	}
	CBitmap bitmap;
	BITMAPINFO bmi;
	memset( &bmi, 0, sizeof(bmi) );
	bmi.bmiHeader.biSize  = sizeof( bmi.bmiHeader );
	bmi.bmiHeader.biWidth  = nSizeX;
	bmi.bmiHeader.biHeight = -THUMBNAIL_HEIGHT;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	
	void *pBitmapBits = 0;
	HBITMAP hbm = CreateDIBSection( 0, &bmi, DIB_RGB_COLORS, &pBitmapBits, 0, 0 );
	if ( hbm == 0 || pBitmapBits == 0 )
		return -1;
	memcpy( pBitmapBits, pScaleImage->GetLFB(), THUMBNAIL_WIDTH * THUMBNAIL_HEIGHT * sizeof(SColor) );
	bitmap.Attach( hbm );
	
	const int nAddedImage = pIML->Add( &bitmap, RGB(255,0,255) );
	return nAddedImage;
}

void CThumbList::LoadAllImagesFromDir( SThumbItems *pThumbItems, CImageList *pIML, const char *szDir,  bool bShowAlpha )
{
	NI_ASSERT( pThumbItems != 0 );
	BeginWaitCursor();

	CString	strPattern;
	BOOL	bRC = TRUE;
	
	HANDLE hFind = NULL;
	WIN32_FIND_DATA ffData;

	pThumbItems->thumbDataList.clear();
	pIML->SetImageCount( 1 );							//Я хочу оставить одну иконку которая подсказывает о несуществовании картинки

	if ( strlen(szDir) > 0 )
		strPattern = (CString)szDir + "\\*.tga";
	else
		strPattern = "*.tga";
	hFind = ::FindFirstFile(strPattern, &ffData);

	if ( hFind != INVALID_HANDLE_VALUE )
	{
		bool bRC = false;
		do
		{
			if(!(ffData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)  &&
				!(ffData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)     &&
				!(ffData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)     &&
				!(ffData.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY))
			{
				SThumbData newItem;
				newItem.nImageId = LoadImageToImageList( pIML, ffData.cFileName, szDir, bShowAlpha );
				if ( newItem.nImageId != -1 )
				{
					char *temp = strchr( ffData.cFileName, '.' );
					if ( temp )
						*temp = '\0';
					newItem.szThumbName = ffData.cFileName;
					pThumbItems->thumbDataList.push_back( newItem );
				}
			}
			bRC = ::FindNextFile(hFind, &ffData);
		} while ( bRC );
		
		FindClose( hFind );
	}

	if ( pThumbItems == m_pActiveThumbItems )
		CreateListElements();

	EndWaitCursor();
}

void CThumbList::LoadImageIndexFromThumbs( SThumbItems *pAllItems, CImageList *pIML )
{
	NI_ASSERT( pAllItems != 0 && pIML != 0 );

	if ( pIML )			//Если валидный image list то его устанавливаю, иначе оставляю старый
	{
		m_ListThumbnail.SetImageList( pIML, LVSIL_NORMAL);
		bValidIML = true;
	}

	if ( !m_pActiveThumbItems )
		return;

	CListOfThumbData::iterator it=m_pActiveThumbItems->thumbDataList.begin();
	for ( ; it!=m_pActiveThumbItems->thumbDataList.end(); ++it )
	{
		it->nImageId = 0;				//красная иконка
		int nRes = -1;
	
		CListOfThumbData::iterator allIt=pAllItems->thumbDataList.begin();
		for ( ; allIt!=pAllItems->thumbDataList.end(); ++allIt )
		{
			nRes = strcmp( allIt->szThumbName.c_str(), it->szThumbName.c_str() );
			if ( nRes == 0 )
				break;
		}

		if ( nRes == 0 )
		{
			it->nImageId = allIt->nImageId;
		}
	}

	CreateListElements();
}

/*
void CThumbList::LoadImagesFromFileNameList( SThumbItems *pThumbItems, const char *szDir )
{
	NI_ASSERT( pThumbItems != 0 );
	
	pThumbItems->imageListThumb.SetImageCount( 0 );
	
	for ( CListOfThumbData::iterator it=pThumbItems->thumbDataList.begin(); it!=pThumbItems->thumbDataList.end(); ++it )
	{
		char szFileName[255];
		strcpy( szFileName, it->szThumbName.c_str() );
		strcat( szFileName, ".tga" );
		it->nImageId = LoadImageToImageList( &pThumbItems->imageListThumb, szFileName, szDir );
	}
	
	if ( pThumbItems == m_pActiveThumbItems )
		CreateListElements();
}
*/

int CThumbList::InsertItemToEnd( const char *szItemName, int nImageIndex )
{
	SThumbData newItem;
	newItem.nImageId = nImageIndex;
	newItem.szThumbName = szItemName;
	m_pActiveThumbItems->thumbDataList.push_back( newItem );

	int nItemCount = m_ListThumbnail.GetItemCount();
	m_ListThumbnail.InsertItem( nItemCount, szItemName, nImageIndex );
	m_ListThumbnail.Arrange( LVA_DEFAULT );
	return nItemCount;			//должен совпадать с результатом возвращаемым insertItem()
}

#ifdef OLD
int CThumbList::InsertItemAfterSelection( char *szFileName, const char *szDir )
{
	SThumbData newItem;
	newItem.nImageId = LoadImageToImageList( &m_pActiveThumbItems->imageListThumb, szFileName, szDir );
	if ( newItem.nImageId == -1 )
		return -1;

	char *temp = strchr( szFileName, '.' );
	if ( temp )
		*temp = '\0';
	newItem.szThumbName = szFileName;
	m_pActiveThumbItems->thumbDataList.push_back( newItem );

	int nSelItemIndex = GetSelectedItemIndex();
	if ( nSelItemIndex == -1 )
	{
		int nItemCount = m_ListThumbnail.GetItemCount();
		m_ListThumbnail.InsertItem( 0, szFileName, newItem.nImageId );
		m_ListThumbnail.Arrange( LVA_ALIGNTOP );
		return nItemCount;
	}

/*
	m_ListThumbnail.InsertItem( LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM,
		0, szFileName, 0, 0, newItem.nImageId, 0 );
*/

	m_ListThumbnail.InsertItem( nSelItemIndex, szFileName, newItem.nImageId );
	m_ListThumbnail.SetItemState( nSelItemIndex + 1, LVIS_SELECTED, LVIS_SELECTED );
	m_ListThumbnail.Arrange( LVA_ALIGNTOP );

	return nSelItemIndex;
}
#endif		//OLD

int CThumbList::GetSelectedItemIndex()
{

	POSITION pos = m_ListThumbnail.GetFirstSelectedItemPosition();
	if ( !pos )
		return -1;

	return m_ListThumbnail.GetNextSelectedItem( pos );


}

string CThumbList::GetItemName( int nIndex )
{
	string szResult = "";
	if ( nIndex == -1 )
		return szResult;
	
	LVITEM lvi;
	memset( &lvi, 0, sizeof(lvi) );
	lvi.iItem = nIndex;
	lvi.mask = LVIF_TEXT;
	char tempText[255];
	lvi.pszText = tempText;
	lvi.cchTextMax = sizeof( tempText );
	m_ListThumbnail.GetItem( &lvi );

	szResult = tempText;
	return szResult;
}

int CThumbList::GetItemImageIndex( int nIndex )
{
	string szResult = "";
	if ( nIndex == -1 )
		return -1;
	
	LVITEM lvi;
	memset( &lvi, 0, sizeof(lvi) );
	lvi.iItem = nIndex;
	lvi.mask = LVIF_IMAGE;
	m_ListThumbnail.GetItem( &lvi );
	return lvi.iImage;
}

void CThumbList::SelectItem( int nIndex )
{
	int nState = m_ListThumbnail.GetItemState( nIndex, LVIS_SELECTED );
	if ( nState == LVIS_SELECTED )
		return;


	m_ListThumbnail.SetItemState( nIndex, LVIS_SELECTED, LVIS_SELECTED );
	m_ListThumbnail.EnsureVisible( nIndex, 0 );

/*
	m_ListThumbnail.SetItem( nIndex, 0, LVIF_STATE, 0, 0, LVIS_SELECTED, LVIS_SELECTED, 0 );
	m_ListThumbnail.EnsureVisible( nIndex, 0 );
	m_ListThumbnail.SetSelectionMark( nIndex );
*/
}

void CThumbList::DeleteItem( int nIndex )
{
	int i = 0;
	for ( CListOfThumbData::iterator it=m_pActiveThumbItems->thumbDataList.begin(); it!=m_pActiveThumbItems->thumbDataList.end(); ++it )
	{
		NI_ASSERT( it->dwData != 0 );

		if ( i == nIndex )
		{
/*
			m_pActiveThumbItems->imageListThumb.Remove( it->nImageId );
*/
			m_pActiveThumbItems->thumbDataList.erase( it );
			break;
		}
		i++;
	}

	
	m_ListThumbnail.DeleteItem( nIndex );
	m_ListThumbnail.Arrange( LVA_ALIGNTOP );
	m_ListThumbnail.UpdateWindow();
}

void CThumbList::SetUserDataForItem( int nItemIndex, DWORD dwData )
{
	int i = 0;
	for ( CListOfThumbData::iterator it=m_pActiveThumbItems->thumbDataList.begin(); it!=m_pActiveThumbItems->thumbDataList.end(); ++it )
	{
		if ( i == nItemIndex )
		{
			it->dwData = dwData;
			m_ListThumbnail.SetItemData( i, dwData );
			break;
		}
		i++;
	}
}

DWORD CThumbList::GetUserDataForItem( int nItemIndex )
{
	NI_ASSERT( nItemIndex != -1 );

	int i = 0;
	for ( CListOfThumbData::iterator it=m_pActiveThumbItems->thumbDataList.begin(); it!=m_pActiveThumbItems->thumbDataList.end(); ++it )
	{

		if ( i == nItemIndex )
		{
			return it->dwData;
		}
		i++;
	}
	return 0;
}

int CThumbList::GetItemIndexWithUserData( DWORD dwData )
{
	NI_ASSERT( dwData != 0 );

	int nCount = m_ListThumbnail.GetItemCount();
	for ( int i=0; i<nCount; i++ )
	{
		if ( m_ListThumbnail.GetItemData(i) == dwData )
			return i;
	}
	return -1;
}

void CThumbList::TestInsertSomeItems()
{
/*
	m_ListThumbnail.InsertItem( 0, "list" );
	m_ListThumbnail.InsertItem( 0, "this" );
	m_ListThumbnail.InsertItem( 0, "control" );

	m_ListThumbnail.InsertItem( 2, "AAA" );
	m_ListThumbnail.InsertItem( 2, "CCCC" );
	m_ListThumbnail.InsertItem( 2, "BBBBB" );
	m_ListThumbnail.Arrange( LVA_ALIGNTOP );
*/
}
