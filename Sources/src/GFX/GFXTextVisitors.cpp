#include "StdAfx.h"

#include "GFXTextVisitors.h"
#include "Clipping.h"
template <class TYPE>
inline const int ResizeToFit( std::vector<TYPE> &data, const int nAmount )
{
	const int nOldSize = data.size();
	data.resize( nOldSize + nAmount );
	return nOldSize;
}
void CTextNoClipVisitor::operator()( const SFontFormat::SCharDesc &character, const float sx, const float sy ) const
{
	const WORD wNumVertices = ResizeToFit( vertices, 4 );
	const int nNumIndices = ResizeToFit( indices, 6 );
  const float w = character.fWidth;
	SGFXLVertex *pVertices = &( vertices[wNumVertices] );
  pVertices->Setup( sx + 0, sy + h, 0, 1, dwColor, dwSpecular, character.x1, character.y2 ); pVertices++;
  pVertices->Setup( sx + 0, sy + 0, 0, 1, dwColor, dwSpecular, character.x1, character.y1 ); pVertices++;
  pVertices->Setup( sx + w, sy + h, 0, 1, dwColor, dwSpecular, character.x2, character.y2 ); pVertices++;
  pVertices->Setup( sx + w, sy + 0, 0, 1, dwColor, dwSpecular, character.x2, character.y1 ); pVertices++;
	WORD *pIndices = &( indices[nNumIndices] );
  *pIndices = wNumVertices + 2; pIndices++;
  *pIndices = wNumVertices + 1; pIndices++;
  *pIndices = wNumVertices + 0; pIndices++;
  *pIndices = wNumVertices + 1; pIndices++;
  *pIndices = wNumVertices + 2; pIndices++;
  *pIndices = wNumVertices + 3; pIndices++;
}
void CTextClipVisitor::operator()( const SFontFormat::SCharDesc &character, const float sx, const float sy ) const
{
	CTRect<float> rcGeom( sx, sy, sx + character.fWidth, sy + h );
	CTRect<float> rcMaps( character.x1, character.y1, character.x2, character.y2 );
	if ( ClipAARect(rcClipRect, &rcGeom, &rcMaps) == false )
		return;
	const WORD wNumVertices = ResizeToFit( vertices, 4 );
	const int nNumIndices = ResizeToFit( indices, 6 );
	SGFXLVertex *pVertices = &( vertices[wNumVertices] );
  pVertices->Setup( rcGeom.x1, rcGeom.y2, 0, 1, dwColor, dwSpecular, rcMaps.x1, rcMaps.y2 ); pVertices++;
  pVertices->Setup( rcGeom.x1, rcGeom.y1, 0, 1, dwColor, dwSpecular, rcMaps.x1, rcMaps.y1 ); pVertices++;
  pVertices->Setup( rcGeom.x2, rcGeom.y2, 0, 1, dwColor, dwSpecular, rcMaps.x2, rcMaps.y2 ); pVertices++;
  pVertices->Setup( rcGeom.x2, rcGeom.y1, 0, 1, dwColor, dwSpecular, rcMaps.x2, rcMaps.y1 ); pVertices++;
	WORD *pIndices = &( indices[nNumIndices] );
  *pIndices = wNumVertices + 2; pIndices++;
  *pIndices = wNumVertices + 1; pIndices++;
  *pIndices = wNumVertices + 0; pIndices++;
  *pIndices = wNumVertices + 1; pIndices++;
  *pIndices = wNumVertices + 2; pIndices++;
  *pIndices = wNumVertices + 3; pIndices++;
}
