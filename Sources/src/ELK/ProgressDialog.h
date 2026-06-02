#if !defined(__ELK_PROGRESS_DIALOG__)
#define __ELK_PROGRESS_DIALOG__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CProgressDialog : public CDialog
{
public:
	CProgressDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_PROGRESS };
	CStatic	m_ProgressLabel;
	CProgressCtrl	m_ProgressBar;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	static const DWORD START_TIMER_ID;
	static const DWORD START_TIMER_INTERVAL;

  DWORD dwStartTimer;

  void SetStartTimer();
  void KillStartTimer();
  void OnStartTimer();
	
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT nIDEvent);
	DECLARE_MESSAGE_MAP()

public:
	void SetProgressMessage( const std::string &rszProgressMessage );
	void SetProgressRange( int nStart, int nFinish );
	void SetProgressPosition( int nPosition );
	void IterateProgressPosition();
};
#endif // !defined(__ELK_PROGRESS_DIALOG__)

