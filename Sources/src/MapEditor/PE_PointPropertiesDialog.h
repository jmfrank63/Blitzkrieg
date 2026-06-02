#if !defined(__Property_Editor_PointPropertiesDialog__)
#define __Property_Editor_PointPropertiesDialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class CPEPointPropertiesDialog : public CResizeDialog
{
public:
	CPEPointPropertiesDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_PE_POINT_PROPERTIES };
	CString	m_strXCoord;
	CString	m_strYCoord;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 200; }
	virtual int GetMinimumYDimension() { return 110; }
	virtual std::string GetXMLOptionsLabel() { return "CPEPointPropertiesDialog"; }
	virtual bool GetDrawGripper() { return true; }

public:
};

#endif // !defined(__Property_Editor_PointPropertiesDialog__)
