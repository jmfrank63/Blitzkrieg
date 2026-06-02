#if !defined(AFX_RoadEditorWnd_H__B695C508_0D00_475E_8FF7_C9693CE02D39__INCLUDED_)
#define AFX_RoadEditorWnd_H__B695C508_0D00_475E_8FF7_C9693CE02D39__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000



class CRoadEditorWnd : public CWnd
{
	
public:
	CRoadEditorWnd(); 

public:

public:

	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);

public:
	virtual ~CRoadEditorWnd();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_RoadEditorWnd_H__B695C508_0D00_475E_8FF7_C9693CE02D39__INCLUDED_)
