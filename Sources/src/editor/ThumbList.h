#if !defined(AFX_THUMBLIST_H__5F1FB4A5_8A47_46CC_891C_DCA8A658B640__INCLUDED_)
#define AFX_THUMBLIST_H__5F1FB4A5_8A47_46CC_891C_DCA8A658B640__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define IDC_LIST_THUMB_CONTROL 1015
const UINT WM_THUMB_LIST_SELECT	= WM_USER + 25;		// изменился выделенный item в Thumb List control, посылается родителю
const UINT WM_THUMB_LIST_DBLCLK = WM_USER + 26;		// double click on the Thumb List control, посылается родителю
const UINT WM_THUMB_LIST_DELETE = WM_USER + 27;		// пользователь нажал delete в контроле Thumb List

const int THUMBNAIL_WIDTH  = 100;
const int THUMBNAIL_HEIGHT = 100;
const int THUMBNAIL_SPACE_X  = 10;
const int THUMBNAIL_SPACE_Y  = 20;


using namespace std;
struct SThumbData
{
	int nImageId;							//индекс item в image list
	DWORD dwData;							//здесь будут храниться пользовательские данные, lParam
	string szThumbName;				//имя item

	SThumbData() : nImageId(-1), dwData(0), szThumbName("") {}
};
typedef list<SThumbData> CListOfThumbData;

struct SThumbItems
{
	CListOfThumbData thumbDataList;			// vector holding the image data
	int nSelectedItem;

	SThumbItems();
};


class CThumbList : public CWnd
{
public:
	CThumbList( bool bHorz = false );
	virtual ~CThumbList();
	
private:
	CListCtrl	m_ListThumbnail;
	SThumbItems *m_pActiveThumbItems;
	bool bHorizontal;
	bool bValidIML;

public:
	void SetActiveThumbItems( SThumbItems *pNewActiveThumbs, CImageList *pIML );
	void LoadAllImagesFromDir( SThumbItems *pThumbItems, CImageList *pIML, const char *szDir, bool bShowAlpha = false );
	int LoadImageToImageList( CImageList *pIML, char *szFileName, const char *szDir, bool bShowAlpha = false );

/*
	void LoadImagesFromFileNameList( SThumbItems *pThumbItems, const char *szDir );
*/

	void LoadImageIndexFromThumbs( SThumbItems *pAllItems, CImageList *pIML );


	int InsertItemToEnd( const char *szItemName, int nImageIndex );


	bool HasValidImageList() { return bValidIML; }
	CImageList *GetCurrentImageList() { return m_ListThumbnail.GetImageList(LVSIL_NORMAL); }
	int GetItemIndexWithUserData( DWORD dwData );
	int GetSelectedItemIndex();
	string GetItemName( int nIndex );
	int GetItemImageIndex( int nIndex );
	void SelectItem( int nIndex );
	void DeleteItem( int nIndex );
	
	void  SetUserDataForItem( int nItemIndex, DWORD dwData );
	DWORD GetUserDataForItem( int nItemIndex );
	
	int GetThumbsCount() { return m_ListThumbnail.GetItemCount(); }

	void TestInsertSomeItems();

protected:
	void CreateListElements();


public:

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClickListThumb(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblClickListThumb(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownListThumb(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemStateChanged(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_THUMBLIST_H__5F1FB4A5_8A47_46CC_891C_DCA8A658B640__INCLUDED_)
