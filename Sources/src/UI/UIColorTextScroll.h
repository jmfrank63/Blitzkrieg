#ifndef __UICOLORTEXTSCROLL_H__
#define __UICOLORTEXTSCROLL_H__
#pragma ONCE
#include "UIScrollText.h"

typedef std::pair< CPtr<ITextDialog>, CPtr<IGFXText> > CVisibleString;
class CUIColorTextScroll : public CUIScrollTextBox
{
	DECLARE_SERIALIZE;
public:
	class CColorTextEntry : public IRefCount
	{
		DECLARE_SERIALIZE;
		OBJECT_COMPLETE_METHODS( CColorTextEntry );
		int nHeight;												// height of this item
		int nY;															// Y position of this item
		DWORD dwCaptionColor;								// color of this item
		DWORD dwEntryColor;											// color of this item
		std::wstring szCaptionString;
		int entryStartX;									// position of entry start.
		std::vector<std::wstring> entryStrings;
		CVisibleString entry;
		CVisibleString caption;

		CVisibleString CreateString( const std::wstring &szSource, const int nWidth, const DWORD dwColor, const int nRedLine );
	public:
		CColorTextEntry() {  }
		CColorTextEntry( const wchar_t *pszCaptionText, const DWORD dwCaptionColor,
										 const wchar_t *pszEntryText, const DWORD dwEntryColor,
										 const int nY, const int nWidth );

		const int GetSizeY() const { return nHeight; }
		int Get1LineHeight() const;
		void Visit( interface ISceneVisitor *pVisitor, const CTRect<float> &border, const int nYOffset );
	};
private:

	typedef std::pair<DWORD,DWORD> CColorPair;
	std::vector< CColorPair > colors;						// color indexes (CAPTION,ENTRY)
	
	typedef std::vector< CPtr<CColorTextEntry> > CTextEntrys;
	CTextEntrys textEntrys;
	int nCurrentYSize;

protected:
	virtual void RepositionText() {  }

public:
	CUIColorTextScroll() : nCurrentYSize( 0 ) {  }
	~CUIColorTextScroll() {  }

	virtual void STDCALL AppendMessage( const WORD *pszCaption, const WORD *pszMessage,
																			const enum IUIColorTextScroll::EColorEntrys color );

	virtual void STDCALL SetWindowText( int nState, const WORD *pszText );

	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	
	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
};
class CUIColorTextScrollBridge : public IUIColorTextScroll, public CUIColorTextScroll
{
	OBJECT_NORMAL_METHODS( CUIColorTextScrollBridge );
	DECLARE_SUPER( CUIColorTextScroll );
	DEFINE_UICONTAINER_BRIDGE;

	virtual void STDCALL AppendMessage( const WORD *pszCaption, const WORD *pszMessage, 
																			const enum IUIColorTextScroll::EColorEntrys color = IUIColorTextScroll::E_COLOR_DEFAULT ) 
	{ CSuper::AppendMessage( pszCaption, pszMessage, color ); }
};
#endif // __UICOLORTEXTSCROLL_H__
