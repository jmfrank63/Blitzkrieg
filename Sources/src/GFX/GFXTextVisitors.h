#ifndef __GFXTEXTVISITORS_H__
#define __GFXTEXTVISITORS_H__
#pragma ONCE
#include "../Formats/fmtFont.h"
class CTextWidthVisitor
{
public:
	void operator()( const SFontFormat::SCharDesc &character, const float sx, const float sy ) const {  }
};
class CTextNoClipVisitor
{
	std::vector<SGFXLVertex> &vertices;	// vertices to fill
	std::vector<WORD> &indices;						// indices to fill
	const float h;												// text height
	const DWORD dwColor;									// color of the text
	const DWORD dwSpecular;								// text's specular component
public:
	CTextNoClipVisitor( std::vector<SGFXLVertex> &_vertices, std::vector<WORD> &_indices, 
		                  const float _h, const DWORD _dwColor, const DWORD _dwSpecular )
		: vertices( _vertices ), indices( _indices ), h( _h ), dwColor( _dwColor ), dwSpecular( _dwSpecular ) {  }
	void operator()( const SFontFormat::SCharDesc &character, const float sx, const float sy ) const;
};
class CTextClipVisitor
{
	std::vector<SGFXLVertex> &vertices;	// vertices to fill
	std::vector<WORD> &indices;						// indices to fill
	const CTRect<float> rcClipRect;				// clipping bounds
	const float h;												// text height
	const DWORD dwColor;									// color of the text
	const DWORD dwSpecular;								// text's specular component
public:
	CTextClipVisitor( std::vector<SGFXLVertex> &_vertices, std::vector<WORD> &_indices, const CTRect<float> &_rcClipRect,
		                const float _h, const DWORD _dwColor, const DWORD _dwSpecular )
		: vertices( _vertices ), indices( _indices ), rcClipRect( _rcClipRect ), 
		  h( _h ), dwColor( _dwColor ), dwSpecular( _dwSpecular ) {  }
	void operator()( const SFontFormat::SCharDesc &character, const float sx, const float sy ) const;
};
#endif // __GFXTEXTVISITORS_H__
