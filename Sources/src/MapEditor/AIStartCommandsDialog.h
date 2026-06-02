#if !defined(AFX_AISTARTCOMMANDS_H__F22377EE_48E0_4C11_8CA2_9403DAEEB59E__INCLUDED_)
#define AFX_AISTARTCOMMANDS_H__F22377EE_48E0_4C11_8CA2_9403DAEEB59E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ResizeDialog.h"
class CAIStartCommandsDialog : public CResizeDialog
{
public:
	CAIStartCommandsDialog(CWnd* pParent = NULL);

	enum { IDD = IDD_AI_START_COMMAND_PROPERTIES };


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnScpDeleteButton();
	afx_msg void OnItemchangedScpCommandsList(NMHDR* pNMHDR, LRESULT* pResult);
	virtual void OnCancel();
	afx_msg void OnScpEditButton();
	afx_msg void OnRclickScpCommandsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkScpCommandsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnScpDeleteMenu();
	afx_msg void OnScpEditMenu();
	afx_msg void OnKeydownScpCommandsList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAddButton();
	afx_msg void OnAddMenu();
	DECLARE_MESSAGE_MAP()

protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 300; }
	virtual int GetMinimumYDimension() { return 230; }
	virtual std::string GetXMLOptionsLabel() { return "CAIStartCommandsDialog"; }
	virtual bool GetDrawGripper() { return true; }

	void AddColumns();
	void AddElements();
	void UpdateButtons();

public:
	bool bAddCommand;
	class CTemplateEditorFrame* m_frame;
	TMutableAIStartCommandList m_startCommands;
	TMutableAIStartCommandList m_startCommandsUndo;
};

#endif // !defined(AFX_AISTARTCOMMANDS_H__F22377EE_48E0_4C11_8CA2_9403DAEEB59E__INCLUDED_)
