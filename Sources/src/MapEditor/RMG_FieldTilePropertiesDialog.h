#if !defined(__RMG_Field_Tile_Properties_Dialog__)
#define __RMG_Field_Tile_Properties_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class CRMGFieldTilePropertiesDialog : public CResizeDialog
{
public:
	CRMGFieldTilePropertiesDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_RMG_CF_TS_TILE_PROPERTIES };
	CStatic	m_Icon;
	CString	m_szName;
	CString	m_szStats;
	CString	m_szVariants;
	CString	m_szWeight;

	bool bDisableEditWeight;
	HICON hIcon;
	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

protected:
	virtual bool GetDrawGripper() { return false; }

public:
};

#endif // !defined(__RMG_Field_Tile_Properties_Dialog__)
