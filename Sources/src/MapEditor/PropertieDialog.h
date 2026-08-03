#if !defined(AFX_PROPERTIEDIALOG_H__FF1DD028_95A1_43DC_A550_756C736FBFE5__INCLUDED_)
#define AFX_PROPERTIEDIALOG_H__FF1DD028_95A1_43DC_A550_756C736FBFE5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"

#define IDC_PC_TREE ( IDC_PIN_BUTTON + 1 )
#include "PushPin.h"
#include ".\\MTree ctrl\\FrameTree.h"
#include "ResizeDialog.h"
class CPropertieDialog : public CResizeDialog
{
	CPtr<IManipulator> m_pCurrentObject;
	std::map< std::string, HTREEITEM > m_insertedNodes;

public:
	void AddObjectWithProp( IManipulator *ptr );
	void UpdateObjectProp();
	IManipulator* GetCurrentManipulator();
	std::map<std::string, HTREEITEM> m_varHandles;
	void	ClearVariables();
	int		GetVariable( std::string &name );
	void	AddRootVariable( std::string &str, int variable );

	void	AddManipulatorVariable( std::string &str, IManipulator *ptr ); // добавляет пустые промежуточные nod'ы + конечный( редактируемый ) node

	HTREEITEM	AddEmptyNode( std::string &str, HTREEITEM hPARoot = TVI_ROOT ); // node который не содержит данных
	HTREEITEM	AddPropertieNode( std::string &str, std::string &propName,IManipulator *ptr, HTREEITEM hPARoot = TVI_ROOT ); 

	CPropertieDialog(CWnd* pParent = NULL);
 
	enum { IDD = IDD_PROPERTY };
	CPushPinButton	m_checkButton;

	public:
	virtual void OnOK();
	virtual void OnCancel();

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);

protected:
CFrameTree m_tree;

	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPinButton();
	DECLARE_MESSAGE_MAP()

	virtual int GetMinimumXDimension() { return 200; }
	virtual int GetMinimumYDimension() { return 100; }
	virtual std::string GetXMLOptionsLabel() { return "CPropertiyDialog"; }
	virtual bool GetDrawGripper() { return true; }
};

#endif // !defined(AFX_PROPERTIEDIALOG_H__FF1DD028_95A1_43DC_A550_756C736FBFE5__INCLUDED_)
