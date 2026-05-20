#ifndef __FONT_H__
#define __FONT_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Formats\fmtFont.h"
#include "GFXHelper.h"
#include "GFXTextVisitors.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFont : public IGFXFont
{
	OBJECT_COMPLETE_METHODS( CFont );
	DECLARE_SERIALIZE;
	SHARED_RESOURCE_METHODS( nRefData.a, "Font.Metrics" );
	//
	SFontFormat format;										// all the font format data
  CPtr<IGFXBaseTexture> pTexture;				// font texture
	// visit text
	template <class TChar, class TVisitor>
		const float VisitText( const TChar *pszStringBegin, const TChar *pszStringEnd, float sx, const float sy, TVisitor &visitor ) const
		{
		  TChar tLastChar = 0;
			// visit all letters
			while( pszStringBegin != pszStringEnd )
			{
				TChar c = *pszStringBegin++;
				//
				const SFontFormat::SCharDesc &character = format.GetChar( c );
				// character's pre-space
				sx += character.fA;
				// add kern to this pair
				sx += format.GetKern( tLastChar, c );
				//
				visitor( character, sx, sy );
				//
				sx += character.fB + character.fC;
				tLastChar = c;
			}
			return sx;
		}
	// get precise width of the string
	template <class TChar>
		const float GetTextWidthLocal( const TChar *pszString, int nCounter ) const
		{
			return VisitText( pszString, pszString + nCounter, 0, 0, CTextWidthVisitor() );
		}
public:
	CFont() {  }
	//
	bool Init( const SFontFormat &_format, IGFXTexture *_pTexture )
	{
		format = _format;
		pTexture = _pTexture;
		return true;
	}
  //
	virtual const SFontFormat& STDCALL GetFormat() const { return format; }
	//
	virtual void STDCALL SwapData( ISharedResource *pResource );
  // font measurements
  virtual int STDCALL GetHeight() const { return format.metrics.nHeight; }
  virtual int STDCALL GetLineSpace() const { return format.GetLineSpace(); }
  virtual int STDCALL GetAscent() const { return format.metrics.nAscent; }
  virtual int STDCALL GetDescent() const { return format.metrics.nDescent; }
  virtual int STDCALL GetTextWidth( const char *pszString, int nCounter = 2000000000 ) const { return GetTextWidthLocal( pszString, nCounter ); }
	virtual int STDCALL GetTextWidth( const WORD *pszString, int nCounter = 2000000000 ) const { return GetTextWidthLocal( reinterpret_cast<const wchar_t*>( pszString ), nCounter ); }
  virtual int STDCALL EstimateTextWidth( const char *pszString ) const;
	// fill geometry data for string (w/o any special characters)
	bool FillGeometryData( const char *pszString, float sx, const float sy,
                         const DWORD dwColor, const DWORD dwSpecular,
                         std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
	bool FillGeometryData( const wchar_t *pszString, float sx, const float sy,
                         const DWORD dwColor, const DWORD dwSpecular,
                         std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const;
  //
	IGFXBaseTexture* GetTexture() { return pTexture; }
	// internal container clearing
	virtual void STDCALL ClearInternalContainer() {  }
	virtual bool STDCALL Load( const bool bPreLoad = false );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FONT_H__
