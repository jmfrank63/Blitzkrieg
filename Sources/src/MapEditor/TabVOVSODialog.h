#if !defined(__Tabs__VO_VSO_Dialog__)
#define __Tabs__VO_VSO_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AsyncImageList.h"
#include "ResizeDialog.h"
/**
class CTabVOVSOImageList : public CAsyncImageList
{
	virtual DWORD Fill( int nImageIndex );
};
/**/

class CTabVOVSODialog : public CResizeDialog
{
protected:
	afx_msg void OnItemchangedVsoObjectsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnVsoSingleWidthRadio();
	afx_msg void OnVsoMultiWidthRatio();
	afx_msg void OnVsoAllWidthRatio();
	afx_msg void OnChangeVsoWidth();
	afx_msg void OnChangeVsoOpacity();
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];
	std::string szDialogName;
	virtual std::string GetXMLOptionsLabel() { return szDialogName; }

protected:
	virtual BOOL OnInitDialog();

public:
	enum CHANGE_WIDTH
	{
		CW_SINGLE = 0,
		CW_MULTI = 1,
		CW_ALL = 2,
	};
	
	bool bWidthChanged;
	bool isSetEditCtrlValue;
	std::string szVSOFolder;

	void SetDialogName( const std::string &rszDialogName ) { szDialogName = rszDialogName; }
	
	CTabVOVSODialog( CWnd* pParent = NULL );
	void SetOpacity( float _fOpacity );
	void SetWidth( float _fWidth );

	CImageList vsoImageList;
	void CreateVSOList( const std::string &rVSOFolder );
	void SetListLabel( const std::string &rszLabel );
	bool GetDescriptionName( std::string *pVSODescName );
	
	void DeleteImageList();
	void CreateImageList();
	
	enum { IDD = IDD_TAB_VO_VECTOR_STRIPE_OBJECTS };
};

#endif // !defined(__Tabs__VO_VSO_Dialog__)
