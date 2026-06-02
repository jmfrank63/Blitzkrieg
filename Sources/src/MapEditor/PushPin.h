

#ifndef __PUSHPIN_H__
#define __PUSHPIN_H__



class CPushPinButton : public CButton
{
public:
	CPushPinButton();

	BOOL Create(const RECT& rect, CWnd* pParentWnd, UINT nID);
	void SetBitmapIDs(UINT uiCombnBmpID, BOOL b6Buttons = TRUE, CWnd* pWndMessageTo = NULL);

	void SetPinned(BOOL bPinned);
	BOOL IsPinned() const;

	void ReloadBitmaps(); 

protected:
  enum ButtonState
  {
	  UNPINNED_NORMAL,
	  UNPINNED_FLYBY,
	  UNPINNED_DOWN,
	  PINNED_NORMAL,
	  PINNED_DOWN,
	  PINNED_FLYBY,
	  MAX_BUTTON_INDEX,
  };

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd *pWnd);
	afx_msg LPARAM OnMouseLeave(WPARAM wParam, LPARAM lParam);

	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void PreSubclassWindow();

	void SizeToContent();
	void LoadBitmaps();

	DECLARE_MESSAGE_MAP()
	CRect       m_MaxRect;
	BOOL        m_bCaptured;
  ButtonState m_buttonState;
	BOOL        m_bTrackLeave;
	CWnd*	      m_pWndMessageTo;
	UINT	      m_uiCombnBmpID;
	int		      m_nButtons;
	CBitmap     m_bmpCombined;
};

#endif //__PUSHPIN_H__