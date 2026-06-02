#if !defined(__Property_Editor_SelectStringsDialog__)
#define __Property_Editor_SelectStringsDialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "ResizeDialog.h"
#include "UnitCreation.h"
class CPESelectStringsDialog : public CResizeDialog
{
public:
	std::list<std::string> *pAvailiableStrings;
	std::vector<std::string> *pSelectedStrings;
	std::string szDialogName;

	CPESelectStringsDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_PE_SELECT_STRINGS };
	CCheckListBox	stringList;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
protected:
	const static int vID[];

	virtual int GetMinimumXDimension() { return 200; }
	virtual int GetMinimumYDimension() { return 200; }
	virtual std::string GetXMLOptionsLabel() { return "CPESelectStringsDialog"; }
	virtual bool GetDrawGripper() { return true; }


	void CreateList();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
};
#endif // !defined(#define __Property_Editor_SelectStringsDialog__)
