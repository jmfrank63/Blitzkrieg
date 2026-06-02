#ifndef __IMAGEFRM_H__
#define __IMAGEFRM_H__

#include "ParentFrame.h"

class CImageFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CImageFrame)
public:
	CImageFrame();
	virtual ~CImageFrame() {}

	virtual void GFXDraw();
	virtual void ShowFrameWindows( int nCommand );
	virtual void Init( IGFX *_pGFX );
	
	void LoadImageTexture( const char *pszFileName );
	void UpdateImageCoordinates();

protected:
	CPtr<IGFXTexture> pKrestTexture;
	CPtr<IGFXTexture> pImageTexture;
	CVec2 vImageSize;
	CVec2 vImagePos;
	CTRect<float> rcImageMap;
	CScrollBar m_wndHScrollBar;
	CScrollBar m_wndVScrollBar;
	CVec2 vKrestPos;
	bool bShowKrest;

protected:
	void InitScrollBars();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};

#endif		//__IMAGEFRM_H__
