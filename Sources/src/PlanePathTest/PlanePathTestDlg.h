
#if !defined(AFX_PLANEPATHTESTDLG_H__5543AEAD_6CEF_4FEE_BFAE_FF0E28DAC14C__INCLUDED_)
#define AFX_PLANEPATHTESTDLG_H__5543AEAD_6CEF_4FEE_BFAE_FF0E28DAC14C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "src\ComplexPathFraction.h"
#include "..\Misc\Spline.h"


class CPlanePathTestDlg : public CDialog
{

	CVec3 x0;														// initial placement & direection
	CVec3 v0;

	CVec3 x1;														// final placement & direction
	CVec3 v1;

	
	CVec3 x2;														// final placement & direction
	CVec3 v2;

	CPtr<CPathFractionArcLine3D> pBest;

	CPtr<CPathFractionArcLine3D> pBest1;

	void Recalc();

	void Draw();
	void DrawPointValue( CPaintDC &dc, const CVec3 &point ) const; 
	void DrawCircle( const CDirectedCircle &pC, CPaintDC &dc ) const;

public:
	CPlanePathTestDlg(CWnd* pParent = NULL);	// standard constructor

	enum { IDD = IDD_PLANEPATHTEST_DIALOG };
	int		m_StartSpeed;
	int		m_FinalSpeed;
	int		m_PathProgress;
	int		m_XAngle;
	int		m_YAngle;
	int		m_ZAngle;
	int		m_Zoom;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRecalc();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_PLANEPATHTESTDLG_H__5543AEAD_6CEF_4FEE_BFAE_FF0E28DAC14C__INCLUDED_)
