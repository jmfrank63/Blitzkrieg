#if !defined(AFX_PROGRESSDIALOG_H__F242B4EF_D0CD_4F69_8A36_27AADC3C6372__INCLUDED_)
#define AFX_PROGRESSDIALOG_H__F242B4EF_D0CD_4F69_8A36_27AADC3C6372__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CProgressDialog : public CDialog
{
public:
	CProgressDialog(CWnd* pParent = NULL);   // standard constructor

	enum { IDD = IDD_PROGRESS_DIALOG };
	CProgressCtrl	m_batchProgress;
	CString	m_projectName;

	int m_nSize;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

public:
	void Init( int nSize ) { m_nSize = nSize; }
	void SetPosition( int nPos ) { m_batchProgress.SetPos( nPos ); }
	void SetProjectName( const char *pszProjectName ) { m_projectName = pszProjectName; UpdateData( FALSE ); }

protected:

	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};


#endif // !defined(AFX_PROGRESSDIALOG_H__F242B4EF_D0CD_4F69_8A36_27AADC3C6372__INCLUDED_)
