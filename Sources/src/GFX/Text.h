#ifndef __TEXT_H__
#define __TEXT_H__
#pragma ONCE
#include "..\Main\TextSystem.h"
inline int GetLength( const wchar_t *pszString )
{
	int nCounter = 0;
	while ( *pszString++ != 0 )
		++nCounter;
	return nCounter;
}
struct SPreFormattedText
{
	struct SLine
	{
		struct SWord
		{
			const wchar_t *pszBegin;					// word begin pointer
			const wchar_t *pszEnd;						// word end pointer
			int nNumPreSpaces;								// number of spaces before this word
			SWord( const wchar_t *_pszBegin, const wchar_t *_pszEnd, const int _nNumPreSpaces = 0 ) 
				: pszBegin( _pszBegin ), pszEnd( _pszEnd ), nNumPreSpaces( _nNumPreSpaces ) {  }
			void operator=( const SWord &w ) { pszBegin = w.pszBegin; pszEnd = w.pszEnd; nNumPreSpaces = w.nNumPreSpaces; }
		};
		enum EProperties
		{
			PROP_MIDDLE_LINE	= 0x00000000,
			PROP_FIRST_LINE		= 0x00000001,
			PROP_LAST_LINE		= 0x00000002,
		};
		float fWidth;												// spaceless line width
		float fPreSpace;										// first word first letter 'A' parameter - to avoid first-letter-in-line clipping bug
		std::list<SWord> words;							// this line words (begin/end pairs)
		int nNumWords;											// number of words  in this line (to avoid 'size()' call to list)
		int nNumSpaces;											// number of spaces in this line (to avoid num spaces counting by words)
		DWORD properties;										// properties
		SLine() 
			: fWidth( 0 ), fPreSpace( 0 ), nNumWords( 0 ), nNumSpaces( 0 ), properties( 0 ) {  }
		const bool IsFirstLine() const { return properties & PROP_FIRST_LINE; }
		const bool IsLastLine() const { return properties & PROP_LAST_LINE; }
	};
  float fWidth;													// width, this text was formatted with
  std::vector<SLine> lines;							// pre-formatted text lines
	SPreFormattedText() : fWidth( 0 ) {  }
	void Clear() { lines.clear(); }
  int GetNumLines() const { return lines.size(); }
  float GetWidth() const { return fWidth; }
};
class CGFXText : public IGFXText
{
	OBJECT_NORMAL_METHODS( CGFXText );
	DECLARE_SERIALIZE;
	mutable SPreFormattedText pft;				// pre-formatted text...
	mutable bool bPreFormatted;						// is text already pre-formatted ?
	mutable bool bPreFormattedLine;				// is text already pre-formatted as a single line?
	CPtr<IText> pText;										// text source
	float fWidth;													// format width
	DWORD dwDefColor;											// default color
	CPtr<IGFXFont> pFont;									// font to draw this text with
	bool bRedLine;												// enable red line
	float fRedLineSize;										// size of the red line
	void PreFormat() const;
	void PreFormatLine() const;
	void SetPreFormatted( bool _bPreFormatted ) const 
	{ 
		bPreFormatted = _bPreFormatted; 
		bPreFormattedLine = _bPreFormatted;
		if ( !_bPreFormatted ) 
			pft.Clear(); 
	}
	const float GetRedLine() const { return fRedLineSize; }
	void SetupRedLine();
	CGFXText();
	template <class TVisitor>
		const float VisitText( const wchar_t *pszStringBegin, const wchar_t *pszStringEnd, float sx, const float sy, TVisitor &visitor ) const
		{
			const SFontFormat &format = pFont->GetFormat();
		  wchar_t wLastChar = 0;
			while( pszStringBegin != pszStringEnd )
			{
				wchar_t c = *pszStringBegin++;
				const SFontFormat::SCharDesc &character = format.GetChar( c );
				sx += character.fA;
				sx += format.GetKern( wLastChar, c );
				visitor( character, sx, sy );
				sx += character.fB + character.fC;
				wLastChar = c;
			}
			return sx;
		}
	float FillGeometryDataNoClip( const wchar_t *pszStringBegin, const wchar_t *pszStringEnd,
                                float sx, const float sy, const CTRect<float> &rcClipRect, 
                                const DWORD dwColor, const DWORD dwSpecular,
                                std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
	float FillGeometryDataClip( const wchar_t *pszStringBegin, const wchar_t *pszStringEnd,
                              float sx, const float sy, const CTRect<float> &rcClipRect, 
                              const DWORD dwColor, const DWORD dwSpecular,
                              std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
  bool FillGeometryDataLeft( const SPreFormattedText::SLine &line, float sx, const float sy, const RECT &rect,
                             const DWORD dwColor, const DWORD dwSpecular, DWORD dwClipFlags,
                             std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
  bool FillGeometryDataRight( const SPreFormattedText::SLine &line, float sx, const float sy, const RECT &rect,
                              const DWORD dwColor, const DWORD dwSpecular, DWORD dwClipFlags,
                              std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
  bool FillGeometryDataCenter( const SPreFormattedText::SLine &line, float sx, const float sy, const RECT &rect,
                               const DWORD dwColor, const DWORD dwSpecular, DWORD dwClipFlags,
                               std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
  bool FillGeometryDataJustify( const SPreFormattedText::SLine &line, float sx, const float sy, const RECT &rect,
                                const DWORD dwColor, const DWORD dwSpecular, DWORD dwClipFlags,
                                std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
public:
	virtual void STDCALL SetFont( IGFXFont *_pFont ) { pFont = _pFont; SetupRedLine(); SetPreFormatted( false ); }
	virtual void STDCALL SetText( interface IText *_pText ) { pText = _pText; SetPreFormatted( false ); }
	virtual interface IText* STDCALL GetText() { return pText; }
	virtual void STDCALL SetWidth( int nWidth ) { if ( fWidth != nWidth ) { fWidth = nWidth; SetPreFormatted( false ); } }
	virtual void STDCALL SetColor( DWORD color ) { dwDefColor = color; }
	virtual void STDCALL EnableRedLine( bool bEnable ) { if ( bEnable != bRedLine ) { bRedLine = bEnable; SetupRedLine(); SetPreFormatted( false ); } }
	virtual void STDCALL SetRedLine( const int nRedLineIndention ) { fRedLineSize = nRedLineIndention; }
	virtual void STDCALL SetChanged() { SetPreFormatted(false); }
  bool FillGeometryData( DWORD dwFlags, const RECT &rect, float sy, DWORD dwColor, const DWORD dwSpecular,
                         std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
	IGFXFont* GetFont() { return pFont; }
	virtual int STDCALL GetNumLines() const;
	virtual int STDCALL GetLineSpace() const;
	virtual int STDCALL GetWidth( int nNumCharacters = -1 ) const;
};
#endif // __TEXT_H__
