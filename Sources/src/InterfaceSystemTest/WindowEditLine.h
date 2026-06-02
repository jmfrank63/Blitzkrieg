
#if !defined(AFX_WINDOWEDITLINE_H__7531A3C4_0749_49AD_8A73_4500671399F2__INCLUDED_)
#define AFX_WINDOWEDITLINE_H__7531A3C4_0749_49AD_8A73_4500671399F2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Window.h"
interface IGFXText;
class CWindowEditLine : public CWindow, public IEditLine
{
	OBJECT_COMPLETE_METHODS(CWindowEditLine)
	DECLARE_SERIALIZE
	DECLARE_CLONABLE_CLASS 

	CNCPtr<IGFXText> pGfxText;								// text to display
	NTimer::STime timeSegment;							// for counting segment times

	int nCursorPos;									//позиция курсора в текущей редактируемой строке
	bool bShowCursor;								//для мигания курсора
	bool bFocused;									//для отображения курсора
	int m_nBeginSel;								//начало выделения
	int m_nEndSel;									//конец выделения
	DWORD dwSelColor;								//цвет для выделенного текста
	int m_nBeginDragSel;						//начало выделения мышкой

	bool bNumericMode;							//вводятся только числа
	bool bGameSpySymbols;						//ограничение на вводимые символы
	bool bLocalPlayerNameMode;			// local player's name allowed symbols
	bool bFileNameSymbols;					//символы доступные для имени файла
	
	int nMaxLength;									//если эта переменная установлена, то включено ограничение на количество символов в тексте
	int nBeginText;		//с этой позиции начинается отображение текста szFullText
	bool bTextScroll;	//если установлена эта переменная, то можно вводить текст шире поля edit box

	std::wstring wszFullText;
	std::string szFontName;	
	DWORD dwColor;

	int nLeftSpace;
	int nRightSpace;
	int nYOffset;

	std::string szOnReturn;
	std::string szOnEscape;

	int GetSelection( const int nX );
	bool DeleteSelection();
	bool IsValidSymbol( const wchar_t chr ) const;
	void NotifyTextChanged();
	void EnsureCursorVisible();
	bool IsTextInsideEditLine();

	void FillWindowRectEditLine( CTRect<float> *pRect );

	void CreateText();
	void RegisteMessageSinks();
	void UnRegisteMessageSinks();
public:
	CWindowEditLine() : timeSegment( 0 ), nCursorPos( 0 ), bShowCursor( 1 ), bFocused( 0 ),
		bTextScroll( 0 ), nMaxLength( -1 ), bGameSpySymbols( 0 ), bFileNameSymbols( 0 ),
		m_nBeginSel( -1 ), m_nEndSel( -1 ), dwSelColor( 0xff2e401b ), m_nBeginDragSel( -1 ),
		nBeginText( 0 ), bNumericMode( 0 ), bLocalPlayerNameMode ( false ) 
	{
	}
	
	virtual void RemoveFocus();
	virtual void STDCALL Init();

	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	virtual void STDCALL Segment( const NTimer::STime timeDiff );

	virtual void STDCALL OnMouseMove( const CVec2 &_vPos, const int nButton );
	virtual void STDCALL OnButtonDown( const CVec2 &_vPos, const int nButton );
	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL OnChar( const wchar_t chr );

	virtual void STDCALL SetCursor( const int nPos );
	virtual void STDCALL SetSelection( const int nBegin, const int nEnd );
	virtual void STDCALL SetText( const wchar_t *pszText );
	virtual const wchar_t * STDCALL GetText() const { return wszFullText.c_str(); }

	void OnReturn( const struct SGameMessage &msg );
	void OnTab( const struct SGameMessage &msg );
	void OnBack( const struct SGameMessage &msg );
	void OnDelete( const struct SGameMessage &msg );
	void OnLeft( const struct SGameMessage &msg );
	void OnCtrlLeft( const struct SGameMessage &msg );
	void OnRight( const struct SGameMessage &msg );
	void OnCtrlRight( const struct SGameMessage &msg );
	void OnHome( const struct SGameMessage &msg );
	void OnEnd( const struct SGameMessage &msg );
	void OnEscape( const struct SGameMessage &msg );
};

#endif // !defined(AFX_WINDOWEDITLINE_H__7531A3C4_0749_49AD_8A73_4500671399F2__INCLUDED_)





















