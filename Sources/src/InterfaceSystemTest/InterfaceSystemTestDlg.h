
#if !defined(AFX_INTERFACESYSTEMTESTDLG_H__927E1D5C_FCEB_442F_840D_5AB43D69CBE5__INCLUDED_)
#define AFX_INTERFACESYSTEMTESTDLG_H__927E1D5C_FCEB_442F_840D_5AB43D69CBE5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SceneVisitor.h"
#include "UIScreen.h"

class CInterfaceSystemTestDlg : public CDialog, public ISceneVisitor
{
	struct SColorLine
	{
		std::pair<CVec3,CVec3> v;
		DWORD color;
		int width;
		
		SColorLine( const CVec3 *_coord, const DWORD _color, const int _width )
			: v( _coord[0], _coord[1] ), color( _color ), width( _width )
		{
		}
	};
	std::list<SColorLine> lines;
	
	CPtr<CScreen> pScreen;

public:
	CInterfaceSystemTestDlg(CWnd* pParent = NULL);	// standard constructor

	enum { IDD = IDD_INTERFACESYSTEMTEST_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


	virtual void STDCALL VisitBoldLine( CVec3 *corners, float fWidth, DWORD color );

protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_INTERFACESYSTEMTESTDLG_H__927E1D5C_FCEB_442F_840D_5AB43D69CBE5__INCLUDED_)

