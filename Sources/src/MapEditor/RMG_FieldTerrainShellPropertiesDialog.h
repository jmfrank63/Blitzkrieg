#if !defined(__RMG_Field_Terrain_Shell_Properties_Dialog__)
#define __RMG_Field_Terrain_Shell_Properties_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class CRMGFieldTerrainShellPropertiesDialog : public CResizeDialog
{
public:
	CRMGFieldTerrainShellPropertiesDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_RMG_CF_TS_PROPERTIES };
	CString	m_szWidth;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	DECLARE_MESSAGE_MAP()

protected:
	virtual bool GetDrawGripper() { return false; }
};
#endif // !defined(__RMG_Field_Terrain_Shell_Properties_Dialog__)
