#if !defined(AFX_SINGLEICON_H__4E06F726_B237_428E_A65A_F30DF3C3A6AA__INCLUDED_)
#define AFX_SINGLEICON_H__4E06F726_B237_428E_A65A_F30DF3C3A6AA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CSingleIcon : public CWnd
{
public:
	CSingleIcon();
	virtual ~CSingleIcon();
	
private:
	std::string szImageName;
	CBitmap m_bmp;
	float m_fB, m_fC, m_fG;
	int m_nSizeX, m_nSizeY;

public:


public:
	void LoadBitmap( const char *pszFullFileName, const char *pszInvalidFileName );
	void ApplyGammaCorrection( float fBrightness, float fContrast, float fGamma );
	void SetImageSize( int nSizeX, int nSizeY ) { m_nSizeX = nSizeX; m_nSizeY = nSizeY; }

protected:
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_SINGLEICON_H__4E06F726_B237_428E_A65A_F30DF3C3A6AA__INCLUDED_)
