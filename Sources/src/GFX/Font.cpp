#include "StdAfx.h"

#include "Font.h"
template <class TYPE>
inline int GetStrLen( const TYPE *pszString )
{
	int nCounter = 0;
	while ( *pszString++ != 0 )
		++nCounter;
	return nCounter;
}
int CFont::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &pTexture );
	return 0;
}
void CFont::SwapData( ISharedResource *pResource )
{
	CFont *pRes = dynamic_cast<CFont*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a CFont", return );
	std::swap( format, pRes->format );
}
bool CFont::FillGeometryData( const char *pszString, float sx, const float sy, 
                              const DWORD dwColor, const DWORD dwSpecular, 
                              std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
	const int nStrLen = GetStrLen( pszString );
	if ( nStrLen == 0 )
		return sx;                       // can't render zero-length string
	vertices.clear();
	indices.clear();
	vertices.reserve( nStrLen*4 );
  indices.reserve( nStrLen*6 );
	CTextNoClipVisitor visitor( vertices, indices, format.metrics.nHeight, dwColor, dwSpecular );
	VisitText( pszString, pszString + nStrLen, sx, sy, visitor );
	return true;
}
bool CFont::FillGeometryData( const wchar_t *pszString, float sx, const float sy, 
                              const DWORD dwColor, const DWORD dwSpecular, 
                              std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
	const int nStrLen = GetStrLen( pszString );
	if ( nStrLen == 0 )
		return sx;                       // can't render zero-length string
	vertices.clear();
	indices.clear();
	vertices.reserve( nStrLen*4 );
  indices.reserve( nStrLen*6 );
	CTextNoClipVisitor visitor( vertices, indices, format.metrics.nHeight, dwColor, dwSpecular );
	VisitText( pszString, pszString + nStrLen, sx, sy, visitor );
	return true;
}
int CFont::EstimateTextWidth( const char *pszString ) const
{
  return format.metrics.nAveCharWidth * strlen( pszString );
}
bool CFont::Load( const bool bPreLoad )
{
	SFontFormat localformat;
	{
		std::string szStreamName = GetSharedResourceFullName();
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
		NI_ASSERT_T( pStream != 0, NStr::Format("Can't open stream \"%s\" to load font", szStreamName.c_str()) );
		if ( pStream == 0 )
			return false;
		CPtr<IStructureSaver> pSS = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSS;
		saver.Add( 1, &localformat );
	}

	CPtr<IGFXTexture> pTexture = GetSingleton<ITextureManager>()->GetTexture( (szSharedResourceName + "\\1").c_str() );
	return Init( localformat, pTexture );
}
