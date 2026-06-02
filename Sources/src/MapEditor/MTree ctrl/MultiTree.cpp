
#include "stdafx.h"
#include "MultiTree.h"

#include "CTreeItem.h"
#include <algorithm>
#include <stdlib.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CMultiTree::CMultiTree() : m_editedItem(0)
{
}

CMultiTree::~CMultiTree()
{
}


BEGIN_MESSAGE_MAP(CMultiTree, CTreeCtrl)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CREATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()



BOOL CMultiTree::SetItemData(HTREEITEM hItem, DWORD dwData)
{
	ITreeItem *pItem = (ITreeItem *)CTreeCtrl::GetItemData(hItem);
	if(!pItem)
		return FALSE;
	pItem->SetData( dwData );
	return CTreeCtrl::SetItemData(hItem, (LPARAM)pItem);
}
DWORD CMultiTree::GetItemData(HTREEITEM hItem) const
{
	ITreeItem *pItem = (ITreeItem *)CTreeCtrl::GetItemData(hItem);
	if(!pItem)
		return NULL;
	return pItem->GetData();
}

HTREEITEM CMultiTree::InsertItem(LPCTSTR lpszItem, HTREEITEM hParent, HTREEITEM hInsertAfter)
{
	ITreeItem *pItem = new CSTreeItem;
	pItem->SetItemName(lpszItem);
	return CTreeCtrl::InsertItem(TVIF_PARAM|TVIF_TEXT, "", 0, 0, 0, 0, (LPARAM)pItem, hParent, hInsertAfter);
}

HTREEITEM CMultiTree::InsertItem( LPCTSTR lpszItem, int nImage, int nSelectedImage, HTREEITEM hParent, HTREEITEM hInsertAfter)
{
	ITreeItem *pItem = new CSTreeItem;
	pItem->SetItemName(lpszItem);
	return CTreeCtrl::InsertItem(TVIF_PARAM|TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE, "", nImage, nSelectedImage, 0, 0, (LPARAM)pItem, hParent, hInsertAfter);
}

HTREEITEM CMultiTree::InsertItem(UINT nMask, LPCTSTR lpszItem, int nImage, int nSelectedImage, UINT nState, UINT nStateMask, LPARAM lParam, HTREEITEM hParent, HTREEITEM hInsertAfter )
{
	ITreeItem *pItem = new CSTreeItem;
	pItem->SetItemName(lpszItem);
	pItem->SetData( lParam );
	return CTreeCtrl::InsertItem(TVIF_PARAM|TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE, "", nImage, nSelectedImage, nState, nStateMask, (LPARAM)pItem, hParent, hInsertAfter);
}

HTREEITEM CMultiTree::InsertItemEx(LPCTSTR lpszItem, HTREEITEM hParent , HTREEITEM hInsertAfter  ,TREEITEMTYPES type )
{
	ITreeItem *pItem ;
	switch ( type )
	{
		case truefalseTreeItem :
			pItem = new CTrueFalseTreeItem;
			break;
		case  procentTreeItem:
			pItem = new CProcentTreeItem;
			break;
		case  numComboBoxItem:
			pItem = new CNumComboBoxTreeItem;
			break;
		case  emptyItem:
			pItem = new CEmptyTreeItem;
			break;
		case  propertieItem:
			pItem = new CPropertieTreeItem;
			break;
		case  propertieItemCombo:
			pItem = new CComboBoxTreeItemPropertieTreeItem;
			break;
		case  propertieItemDir:
			pItem = new CDirChosePropertieTreeItem;
			break;	
		case  propertieItemFile:
			pItem = new CFileChosePropertieTreeItem;
			break;
		case  propertieItemUnits:
			pItem = new CUnitsPropertieTreeItem;
			break;

			
	};

	pItem->SetItemName(lpszItem);
	return CTreeCtrl::InsertItem(TVIF_PARAM|TVIF_TEXT, "", 0, 0, 0, 0, (LPARAM)pItem, hParent, hInsertAfter);
}

void CMultiTree::SafeDeleteItem( HTREEITEM item )
{
	ITreeItem *pItem = GetTreeItemPtr( item );
	if ( pItem != 0 )
	{
		delete pItem;
		CTreeCtrl::SetItemData( item, 0 );
		pItem = 0;
	}
	if ( CTreeCtrl::ItemHasChildren( item ) )
	{
		HTREEITEM childItem = CTreeCtrl::GetChildItem( item );
		while ( childItem != 0 )
		{
			SafeDeleteItem( childItem );
			childItem = CTreeCtrl::GetNextItem( childItem, TVGN_NEXT );
		}
	}
}

void CMultiTree::SafeDeleteAllItems()
{
	HTREEITEM item = CTreeCtrl::GetRootItem();
	while ( item != 0 )
	{
		SafeDeleteItem( item );
		item = CTreeCtrl::GetNextItem( item, TVGN_NEXT );
	}
	CTreeCtrl::DeleteAllItems();
}

void CMultiTree::OnPaint() 
{

	CPaintDC paintDC(this);

	CRect clientRect;
	GetClientRect( &clientRect );


	CDC dc;
	int nRes = dc.CreateCompatibleDC( &paintDC );
	CBitmap bmp;
	nRes = bmp.CreateCompatibleBitmap( &paintDC, clientRect.Width(), clientRect.Height() );
	CBitmap *pOldBitmap = dc.SelectObject( &bmp );

	
	CRect rcClip, rcClient;
	dc.GetClipBox( &rcClip );
	GetClientRect(&rcClient);
	CRgn rgn;
	rgn.CreateRectRgnIndirect( &rcClip );
	dc.SelectClipRgn(&rgn);
	rgn.DeleteObject();
	COLORREF m_wndColor = GetSysColor( COLOR_WINDOW );
	dc.SetTextColor(m_wndColor);

	CWnd::DefWindowProc( WM_PAINT, (WPARAM)dc.m_hDC, 0 );

	HTREEITEM hItem = GetFirstVisibleItem();
	int n = GetVisibleCount();
	dc.FillSolidRect(GetColumnWidth(0),0,rcClient.Width(),rcClient.Height(),m_wndColor);
	
	CFont *pFontDC;
	CFont fontDC, boldFontDC;
	LOGFONT logfont;

	CFont *pFont = GetFont();
	pFont->GetLogFont( &logfont );

	fontDC.CreateFontIndirect( &logfont );
	pFontDC = dc.SelectObject( &fontDC );

	logfont.lfWeight = 400;
	boldFontDC.CreateFontIndirect( &logfont );
	
	int itemHeight = 1;
	while(hItem!=NULL && n>=0)
	{
		CRect rect;

		UINT selflag = TVIS_DROPHILITED | TVIS_SELECTED;
	
		if ( !(GetItemState( hItem, selflag ) & selflag ))
		{
				dc.SetBkMode(TRANSPARENT);
				dc.SetBkColor( m_wndColor );

			GetItemRect( hItem, &rect, FALSE );				// without
			rect.left =  0;
			rect.right = GetFullWidth();
		
			CString sItem;
			ITreeItem* pItem = (ITreeItem*)CTreeCtrl::GetItemData(hItem);
			if ( pItem )
			{
				sItem = pItem->GetItemName() != "" ? pItem->GetItemName() .c_str() : "";

				if( pItem->ifBold() )
				{
					dc.SelectObject( &boldFontDC );
				}
			}

			CRect m_labelRect;
			GetItemRect( hItem, &m_labelRect, TRUE );
			GetItemRect( hItem, &rect, FALSE );
			if(m_wndHeader.GetItemCount()>1)
				rect.left = __min(int (m_labelRect.left), int (GetColumnWidth(0)));
			else
				rect.left = m_labelRect.left;
			rect.right = GetFullWidth();
			
			DrawItemText(&dc, sItem, CRect(rect.left+2, rect.top, GetColumnWidth(0), rect.bottom), GetColumnWidth(0)-rect.left-2, LVCFMT_LEFT );

			if ( pItem )
			{
				sItem = pItem->GetNormalProperty() != "" ? pItem->GetNormalProperty() .c_str() : "";
			}
			DrawItemText(&dc, sItem, CRect(GetColumnWidth(0) + 2, rect.top, GetColumnWidth(1) + GetColumnWidth(0), rect.bottom), GetColumnWidth(1) - 2, LVCFMT_LEFT );
			
			itemHeight = rect.Height();
			if ( pItem )
			{
				if( pItem->ifBold() )
				{
					dc.SelectObject( &fontDC );
				}
			}
		}
		else
		{
			GetItemRect( hItem, &rect, FALSE );				// without
			rect.left =  0;
			rect.right = GetFullWidth();



			COLORREF m_highlightColor = ::GetSysColor (COLOR_HIGHLIGHT);

			CBrush brush(m_highlightColor);
			dc.FillRect (rect, &brush);

			dc.DrawFocusRect (rect);
		
			ITreeItem* pItem = (ITreeItem*)CTreeCtrl::GetItemData(hItem);
			CString sItem;
			if ( pItem )
			{
				sItem = pItem->GetItemName() != "" ? pItem->GetItemName() .c_str() : "";
				if(pItem->ifBold())
				{
					dc.SelectObject( &boldFontDC );
				}
			}

			dc.SetBkColor(m_highlightColor);

			dc.SetTextColor(::GetSysColor (COLOR_HIGHLIGHTTEXT));
			CRect m_labelRect;
			GetItemRect( hItem, &m_labelRect, TRUE );
			GetItemRect( hItem, &rect, FALSE );
			if(m_wndHeader.GetItemCount()>1)
				rect.left = __min (int (m_labelRect.left), int (GetColumnWidth(0)));
			else
				rect.left = m_labelRect.left;
			rect.right = GetFullWidth();

			DrawItemText(&dc, sItem, CRect(rect.left+2, rect.top, GetColumnWidth(0), rect.bottom), GetColumnWidth(0)-rect.left-2, LVCFMT_LEFT );
			if ( pItem )
			{
				sItem = pItem->GetNormalProperty() != "" ? pItem->GetNormalProperty() .c_str() : "";
			}
			DrawItemText(&dc, sItem, CRect(GetColumnWidth(0) + 2, rect.top, GetColumnWidth(1) + GetColumnWidth(0), rect.bottom), GetColumnWidth(1) - 2, LVCFMT_LEFT );

			itemHeight = rect.Height();

			if ( pItem )
			{
				if(pItem->ifBold())
				{
					dc.SelectObject( &fontDC );
				}
			}
			dc.SetTextColor(::GetSysColor (COLOR_WINDOWTEXT ));

		}

		hItem = GetNextVisibleItem( hItem );
		n--;
	}
	
	CPen pen( PS_JOIN_BEVEL, 0, RGB( 0, 0, 0 ) );
  CPen* pOldPen = (CPen*)dc.SelectObject(&pen);
/*	while( yTmp < rcClient.Height() )
	{

		dc.MoveTo( 0, yTmp);
		dc.LineTo( GetFullWidth(), yTmp );
		yTmp += itemHeight;
	}*/
	dc.MoveTo( GetColumnWidth(0) - 1, 0);
	dc.LineTo( GetColumnWidth(0) - 1, rcClient.Height() );
  dc.SelectObject( pOldPen );
   
	dc.SelectObject( pFontDC );

	paintDC.BitBlt( clientRect.left, clientRect.top, clientRect.Width(), clientRect.Height(), &dc, 0, 0, SRCCOPY );
	dc.SelectObject( pOldBitmap );
}

int CMultiTree::GetColumnWidth(int nCol)
{
	HD_ITEM hItem;
	hItem.mask = HDI_WIDTH;
	if(!m_wndHeader.GetItem(nCol, &hItem))
		return 0;
	return hItem.cxy;
}

int CMultiTree::GetFullWidth()
{
	int w = 0;
	for(int i=0;i < m_wndHeader.GetItemCount();i++)
	{
		w += GetColumnWidth(i);
	}
	return w;
}

void CMultiTree::DrawItemText (CDC* pDC, CString &text, CRect &rect, int nWidth, int nFormat)
{
    bool  bNeedDots = false;
    int nMaxWidth = nWidth - 4;

    while ((text.GetLength()>0) && (pDC->GetTextExtent((LPCTSTR) text).cx > (nMaxWidth - 4))) 
		{
        text = text.Left (text.GetLength () - 1);
        bNeedDots = true;
    }

    if (bNeedDots) {
        if (text.GetLength () >= 1)
            text = text.Left (text.GetLength () - 1);
        text += "...";
    }
	  rect.right = rect.left + nMaxWidth;

    UINT nStyle = DT_VCENTER | DT_SINGLELINE;
    if (nFormat == LVCFMT_LEFT)
        nStyle |= DT_LEFT;
    else if (nFormat == LVCFMT_CENTER)
        nStyle |= DT_CENTER;
    else // nFormat == LVCFMT_RIGHT
        nStyle |= DT_RIGHT;
		if((text.GetLength()>0) && (rect.right>rect.left))
			pDC->DrawText (text, rect, nStyle);
}


void CMultiTree::OnLButtonDown(UINT nFlags, CPoint point) 
{
	SendMessage( WM_USER + 1);
	UINT flags;
	HTREEITEM m_selectedItem = HitTest(point, &flags);

	if((flags & TVHT_ONITEMRIGHT) || (flags & TVHT_ONITEMINDENT) ||
	   (flags & TVHT_ONITEM))
	{
		SelectItem(m_selectedItem);
	}

	if((m_wndHeader.GetItemCount()==0) || (point.x<GetColumnWidth(0)))
	{
		m_selectedItem = HitTest(point, &flags);
		if(flags & TVHT_ONITEMBUTTON)
		{
			Expand(m_selectedItem, TVE_TOGGLE);
		}
	}

	SetFocus();
}
ITreeItem* CMultiTree::GetTreeItemPtr(HTREEITEM hItem)
{
	return reinterpret_cast<ITreeItem*>(CTreeCtrl::GetItemData(hItem));
}

void CMultiTree::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	UINT flags;
	HTREEITEM m_selectedItem = HitTest(point, &flags);
	if( (flags & TVHT_ONITEMRIGHT ) && m_selectedItem  != NULL )
	{
		ITreeItem *pItem = (ITreeItem *)CTreeCtrl::GetItemData(m_selectedItem);
		if( pItem->BuildEditor( &m_selectedItem,this ) )					
			m_editedItem = m_selectedItem;
		else
			RedrawWindow();
	}	
}


BOOL CMultiTree::PreTranslateMessage(MSG* pMsg) 
{
	if ( pMsg->message == WM_KEYDOWN )
	{	 
		if ( pMsg->wParam == VK_RETURN )
		{
			HTREEITEM m_selectedItem = GetSelectedItem();
			if( m_selectedItem != NULL)
			{
				ITreeItem *pItem = (ITreeItem *)CTreeCtrl::GetItemData(m_selectedItem);
				if ( pItem )
				{
					if ( pItem->BuildEditor( &m_selectedItem,this ) )					
						m_editedItem = m_selectedItem;
					else
						RedrawWindow();
				}
			}			
		}
	}
	return CTreeCtrl::PreTranslateMessage(pMsg);
}

LRESULT CMultiTree::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if( message == (WM_USER + 1) && m_editedItem != 0)
	{
			bool bResult = false;
			ITreeItem *pItem = (ITreeItem *)CTreeCtrl::GetItemData(m_editedItem);
			if( pItem != NULL )
			{
				bResult = pItem->KillEditor( );
				if( !bResult )
				{
					m_editedItem = 0;
				}
			}
			if ( GetParent() && GetParent()->GetParent() && ( !bResult ) )
			{
				GetParent()->GetParent()->SendMessage( WM_USER + 1, 0, 0 );
			}
			return true;
	}
	if( message == (WM_USER + 2) && m_editedItem != 0)
	{
			ITreeItem *pItem = (ITreeItem *)CTreeCtrl::GetItemData(m_editedItem);
			if( pItem != NULL )
			{
				pItem->KillEditor( );			
				m_editedItem = 0;
			}
			return true;
	}
	return CTreeCtrl::DefWindowProc(message, wParam, lParam);
}

int CMultiTree::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CTreeCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	SetFont( GetParent()->GetParent()->GetFont(), false );
	m_wndHeader.SetFont( GetParent()->GetParent()->GetFont(), false );
	return 0;
}

void CMultiTree::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);

}