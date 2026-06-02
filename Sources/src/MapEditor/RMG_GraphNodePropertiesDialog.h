#if !defined(__RMG_Graph_Node_Properties_Dialog__)
#define __RMG_Graph_Node_Properties_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class CRMGGraphNodePropertiesDialog : public CResizeDialog
{
public:
	CRMGGraphNodePropertiesDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_RMG_GRAPH_NODE_PROPERTIES };
	CEdit	m_ContainerPathEdit;
	CString	m_strSize;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	afx_msg void OnContainerBrowseButton();
	afx_msg void OnChangeContainerEdit();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 300; }
	virtual int GetMinimumYDimension() { return 125; }
	virtual std::string GetXMLOptionsLabel() { return "CRMGGraphNodePropertiesDialog"; }
	virtual bool GetDrawGripper() { return true; }

public:
	std::string szContainerInitialFileName;
};
#endif // !defined(__RMG_Graph_Node_Properties_Dialog__)
