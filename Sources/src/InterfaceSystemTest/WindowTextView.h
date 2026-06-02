
#if !defined(AFX_WINDOWTEXTVIEW_H__1660DBF3_B2C3_40F5_B322_906F49DC1A41__INCLUDED_)
#define AFX_WINDOWTEXTVIEW_H__1660DBF3_B2C3_40F5_B322_906F49DC1A41__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Window.h"

interface IGFXText;
class CWindowTextView : public CWindow, public ITextView
{
	OBJECT_COMPLETE_METHODS(CWindowTextView);
	DECLARE_CLONABLE_CLASS;
	DECLARE_SERIALIZE

	std::string szKey;											// initial text key
	DWORD dwColor;													// color
	CPtr<IGFXText> pGfxText;								// text to display
	int /*EGFXFontFormat*/ format;					// text formatting
	std::string szFontName;
	int nRedLineSpace;

	bool InitHeight();
public:
	CWindowTextView() {  }
	CWindowTextView( int TEST );
	
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	virtual int STDCALL operator&( IDataTree &ss );

	virtual bool STDCALL SetText( const std::wstring &szText );
};
#endif // !defined(AFX_WINDOWTEXTVIEW_H__1660DBF3_B2C3_40F5_B322_906F49DC1A41__INCLUDED_)
