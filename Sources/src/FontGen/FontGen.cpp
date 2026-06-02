#include "StdAfx.h"

#include "FontGen.h"

#include "..\Image\Image.h"
#include "..\Image\ImageHelper.h"
#include "..\Misc\FileUtils.h"
#include "..\Misc\StrProc.h"

#include <strstream>
const int nLeadingPixels = 2;
struct SFontInfo
{
  HFONT hFont;													// HFONT used to draw with this font
  TEXTMETRIC tm;												// text metrics, e.g. character height
	std::vector<ABC> abc;									// character ABC widths
	std::vector<KERNINGPAIR> kps;					// kernging pairs
	int nTextureSizeX, nTextureSizeY;			// estimated texture size
	std::unordered_map<WORD, WORD> translate;	// ANSI => UNICODE translation table
	WORD Translate( WORD code ) const
	{
		std::unordered_map<WORD, WORD>::const_iterator pos = translate.find( code );
		NI_ASSERT_T( pos != translate.end(), NStr::Format("Can't find code for symbol %d to re-map", code) );
		return pos->second;
	}
  SFontInfo() : hFont( 0 ), nTextureSizeX( 0 ), nTextureSizeY( 0 ) {  }
  virtual ~SFontInfo() { if ( hFont ) DeleteObject( hFont ); }
};
inline bool IsFit( const SFontInfo &fi, DWORD dwNumChars, DWORD dwSizeX, DWORD dwSizeY )
{
  return ( dwSizeX / (fi.tm.tmAveCharWidth + 2) ) * ( dwSizeY / fi.tm.tmHeight ) >= dwNumChars;
}
bool EstimateTextureSize( SFontInfo &fi, DWORD dwNumChars )
{
  for ( int i=6; i<13; ++i )
  {
    if ( IsFit( fi, dwNumChars, 1L << i, 1L << (i - 1) ) )
    {
      fi.nTextureSizeX = 1L << i;
      fi.nTextureSizeY = 1L << (i - 1);
      return true;
    }
    else if ( IsFit( fi, dwNumChars, 1L << i, 1L << i ) )
    {
      fi.nTextureSizeX = fi.nTextureSizeY = 1L << i;
      return true;
    }
  }
  return false;
}
struct SKPZeroFunctional
{
  bool operator()( const KERNINGPAIR &kp ) const { return kp.iKernAmount == 0; }
};
void MeasureFont( HDC hdc, SFontInfo &fi, std::vector<WORD> &chars )
{
  GetTextMetrics( hdc, &fi.tm );
	if ( std::find( chars.begin(), chars.end(), fi.tm.tmDefaultChar ) == chars.end() )
		chars.push_back( fi.tm.tmDefaultChar );
	std::sort( chars.begin(), chars.end() );
	fi.abc.resize( chars.size() );
	fi.kps.resize( chars.size() * chars.size() );
	if ( !GetCharABCWidths( hdc, chars[0], chars[0], &( fi.abc[0] ) ) )
  {
		ABC abc;
		Zero( abc );
		std::fill( fi.abc.begin(), fi.abc.end(), abc );
    SIZE size;
		for ( int i=0; i<chars.size(); ++i )
		{
			const wchar_t szChar[2] = { static_cast<wchar_t>( chars[i] ), 0 };
			GetTextExtentPoint32W( hdc, szChar, 1, &size );
      fi.abc[i].abcB = size.cx;
		}
  }
	else
	{
		for ( int i=0; i<chars.size(); ++i )
			GetCharABCWidths( hdc, chars[i], chars[i], &( fi.abc[i] ) );
	}
	KERNINGPAIR kernpair;
	Zero( kernpair );
	std::fill( fi.kps.begin(), fi.kps.end(), kernpair );
  GetKerningPairs( hdc, chars.size()*chars.size(), &( fi.kps[0] ) );
  fi.kps.erase( std::remove_if( fi.kps.begin(), fi.kps.end(), SKPZeroFunctional() ), fi.kps.end() );
  if ( EstimateTextureSize( fi, chars.size() ) == false )
    throw 1; // too large texture !!!
  int x = 0, y = 0;
	for ( int i=0; i<chars.size(); ++i )
	{
    int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
    if ( x + nNextCharShift + nLeadingPixels > fi.nTextureSizeX )
    {
      ++y;
      x = 0;
      if ( (y + 1) * fi.tm.tmHeight > fi.nTextureSizeY )
      {
        if ( fi.nTextureSizeX == fi.nTextureSizeY ) // if we have 1:1 sizes, make it 2:1
          fi.nTextureSizeX <<= 1;
        else                                   // else, if we have 2:1 already, make it 2:2 :)
          fi.nTextureSizeY = fi.nTextureSizeX;
        break;
      }
    }
    x += nLeadingPixels;
    x += nNextCharShift;
	}
}
void LoadFont( HWND hWnd, SFontInfo &fi, int nHeight, int nWeight, bool bItalic, DWORD dwCharSet, 
							 bool bAntialias, DWORD dwPitch, const std::string &szFaceName, std::vector<WORD> &chars )
{
  if ( fi.hFont )
  { 
    DeleteObject( fi.hFont ); 
    fi.hFont = 0;
  }
	const std::wstring szWideFaceName = NStr::ToUnicode( szFaceName );
	fi.hFont = ::CreateFontW( nHeight, 0, 0, 0, nWeight, bItalic, FALSE, FALSE, 
                           dwCharSet, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                           bAntialias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY,
						   dwPitch, szWideFaceName.c_str() );
  HDC hdc = GetDC( hWnd );
  HFONT hOldFont = (HFONT)::SelectObject( hdc, fi.hFont );
  MeasureFont( hdc, fi, chars );
	{
		CHARSETINFO cs;
		BOOL bRetVal = TranslateCharsetInfo( (DWORD*)dwCharSet, &cs, TCI_SRCCHARSET );
		NI_ASSERT_T( bRetVal == TRUE, "Can't translate charset info" );
		NStr::SetCodePage( cs.ciACP );
		std::string szCharacters;
		szCharacters.resize( chars.size() );
		for ( int i = 0; i != chars.size(); ++i )
			szCharacters[i] = chars[i];
		std::wstring szUNICODE;
		NStr::ToUnicode( &szUNICODE, szCharacters );
		for ( int i = 0; i != chars.size(); ++i )
			fi.translate[ chars[i] ]= szUNICODE[i];
	}
  ::SelectObject( hdc, hOldFont );
  ReleaseDC( hWnd, hdc );
}
bool DrawFont( HDC hdc, const SFontInfo &fi, const std::vector<WORD> &chars )
{
  int x = 0, y = 0;
	for ( int i=0; i<chars.size(); ++i )
	{
    WORD wChar = chars[i];
    int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
    if ( x + nNextCharShift + nLeadingPixels > fi.nTextureSizeX )
    {
      ++y;
      x = 0;
      if ( (y + 1) * fi.tm.tmHeight > fi.nTextureSizeY )
        return false;
    }
    x += nLeadingPixels;
		const wchar_t szChar[2] = { static_cast<wchar_t>( chars[i] ), 0 };
		TextOutW( hdc, x - fi.abc[i].abcA, y*fi.tm.tmHeight, szChar, 1 );
    x += nNextCharShift;
	}
  return true;
}
IImage* CreateFontImage( const SFontInfo &fi, const std::vector<WORD> &chars )
{
  int width = fi.nTextureSizeX;//16 * fi.tm.tmMaxCharWidth;
  int height = fi.nTextureSizeY;//14 * fi.tm.tmHeight;
  BYTE *pBitmapBits = 0;
  BITMAPINFO bmi;
  memset( &bmi.bmiHeader, 0, sizeof(bmi.bmiHeader) );
  bmi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER );
  bmi.bmiHeader.biWidth       = fi.nTextureSizeX;
  bmi.bmiHeader.biHeight      = fi.nTextureSizeY;
  bmi.bmiHeader.biPlanes      = 1;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biBitCount    = 24;
  bmi.bmiHeader.biSizeImage   = abs( bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * bmi.bmiHeader.biBitCount / 8 );
  HDC hDC = CreateCompatibleDC( 0 );
  HBITMAP hbmBitmap = CreateDIBSection( hDC, &bmi, DIB_RGB_COLORS, (void**)&pBitmapBits, 0, 0 );
  HBITMAP hOldBmp = (HBITMAP)SelectObject( hDC, hbmBitmap );
  HFONT hOldFont = (HFONT)SelectObject( hDC, fi.hFont );
  SelectObject( hDC, GetStockObject(BLACK_BRUSH) );
  Rectangle( hDC, 0, 0, width, height );
  SetBkMode( hDC, TRANSPARENT );           // do not fill character background
  SetTextColor( hDC, RGB(255, 255, 255) ); // text color white
  SetTextAlign( hDC, TA_TOP );
  DrawFont( hDC, fi, chars );
  SelectObject( hDC, hOldFont );
  SelectObject( hDC, hOldBmp );
  std::vector<DWORD> imagedata( fi.nTextureSizeX * fi.nTextureSizeY );
  for ( int i=0, j=0; i<fi.nTextureSizeX * fi.nTextureSizeY * 3; i+=3, ++j )
  {
		DWORD a = pBitmapBits[i];
    DWORD c = pBitmapBits[i];//( a != 0 ? 255 : pBitmapBits[i] );
    imagedata[j] = (a << 24) | (c << 16) | (c << 8) | c;
  }
	IImage *pImage = GetImageProcessor()->CreateImage( fi.nTextureSizeX, fi.nTextureSizeY, &(imagedata[0]) );
	pImage->FlipY(); // flip image due to bottom-left bitmap orientation

	return pImage;
}
SFontFormat* CreateFontFormat( const std::string &szFaceName, const SFontInfo &fi, const std::vector<WORD> &chars )
{
	SFontFormat *pFF = new SFontFormat();
	pFF->szFaceName = szFaceName;
	NStr::ToLower( pFF->szFaceName );
	pFF->metrics.nHeight = fi.tm.tmHeight;
	pFF->metrics.nAscent = fi.tm.tmAscent;
	pFF->metrics.nDescent = fi.tm.tmDescent;
	pFF->metrics.nInternalLeading = fi.tm.tmInternalLeading;
	pFF->metrics.nExternalLeading = fi.tm.tmExternalLeading;
	pFF->metrics.nAveCharWidth = fi.tm.tmAveCharWidth;
	pFF->metrics.nMaxCharWidth = fi.tm.tmMaxCharWidth;
	pFF->metrics.wDefaultChar = fi.Translate( fi.tm.tmDefaultChar );
	pFF->metrics.cCharSet = fi.tm.tmCharSet;
	for ( int i=0; i<fi.kps.size(); ++i )
	{
		DWORD dwFirst = fi.Translate( fi.kps[i].wFirst );
		DWORD dwSecond = fi.Translate( fi.kps[i].wSecond );
		pFF->kerns[ (dwFirst << 16) | dwSecond ] = fi.kps[i].iKernAmount;
	}
  int x = 0, y = 0;
	for ( int i=0; i<chars.size(); ++i )
	{
		BYTE ansicode = chars[i];
		WORD unicode = fi.Translate( chars[i] );
		SFontFormat::SCharDesc &chardesc = pFF->chars[unicode];
    int nNextCharShift = fi.abc[i].abcB + abs( fi.abc[i].abcC );
    if ( x + nNextCharShift + nLeadingPixels > fi.nTextureSizeX )
    {
      ++y;
      x = 0;
    }
    x += nLeadingPixels;
    chardesc.fA = fi.abc[i].abcA;
    chardesc.fB = fi.abc[i].abcB;
    chardesc.fC = fi.abc[i].abcC;
    chardesc.fWidth = fi.abc[i].abcB + ( fi.abc[i].abcC > 0 ? fi.abc[i].abcC : 0 );
    chardesc.x1 = float( x + 0.5f ) / fi.nTextureSizeX;
    chardesc.y1 = float( y * fi.tm.tmHeight + 0.5f ) / fi.nTextureSizeY;
    chardesc.x2 = float( x + chardesc.fWidth + 0.5f ) / fi.nTextureSizeX;
    chardesc.y2 = float( ( y + 1 ) * fi.tm.tmHeight + 0.5f ) / fi.nTextureSizeY;
    x += nNextCharShift;
	}
	pFF->metrics.fSpaceWidth = pFF->chars[ fi.Translate(32) ].fWidth;
	return pFF;
}


int main( int argc, char *argv[] )
{
	{
		wchar_t buffer[2048];
		GetCurrentDirectoryW( 2048, buffer );
		std::wstring szCurrDir = buffer;
		if ( szCurrDir.empty() )
			szCurrDir = L".\\";
		else if ( szCurrDir[szCurrDir.size() - 1] != '\\' )
			szCurrDir += '\\';
		HMODULE hImage = LoadLibraryW( (szCurrDir + L"image.dll").c_str() );
		if ( hImage )
		{
			GETMODULEDESCRIPTOR pfnGetModuleDescriptor = reinterpret_cast<GETMODULEDESCRIPTOR>( GetProcAddress( hImage, "GetModuleDescriptor" ) );
			if ( pfnGetModuleDescriptor )
			{
				const SModuleDescriptor *pDesc = (*pfnGetModuleDescriptor)();
				if ( pDesc && pDesc->pFactory )
				{
					IImageProcessor *pIP = CreateObject<IImageProcessor>( pDesc->pFactory, IImageProcessor::tidTypeID );
					if ( pIP )
						RegisterSingleton( IImageProcessor::tidTypeID, pIP );
				}
			}
		}
	}
  std::vector<std::string> szParams( argc - 1 );
  for ( int i=0; i<argc - 1; ++i )
  {
    szParams[i] = argv[i + 1];
    NStr::ToLower( szParams[i] );
  }
  if ( szParams.empty() )
  {
    std::strstream ss;
    ss << "FontGenerator utility\n(C) Nival Interactive, 2000" << std::endl;
    ss << "Usage: FontGen.exe [options] <\"Font Face Name\">" << std::endl;
    ss << "   -h# \t\t font height (in pixels)" << std::endl;
    ss << "   -w# \t\t font weight (400 = normal. 100 <= w <= 900)" << std::endl;
    ss << "   -it \t\t italic" << std::endl;
    ss << "   -a0 \t\t non-antialiased quality" << std::endl;
    ss << "   -pitch \t font pitch (default, fixed, variable)" << std::endl;
    ss << "   -<charset>\t second character set" << std::endl;
    ss << "    charsets: ansi, baltic, chinesebig5, default, easteurope, gb2312," << std::endl;
    ss << "              greek, hangul, mac, oem, russian, shiftjis, symbol," << std::endl;
    ss << "              turkish, hebrew, arabic, thai" << std::endl << '\0';

    printf( ss.str() );

    return 0xDEAD;
  }
  std::unordered_map<std::string, DWORD> charsets;
  charsets["-ansi"]        = ANSI_CHARSET;
  charsets["-baltic"]      = BALTIC_CHARSET;
  charsets["-chinesebig5"] = CHINESEBIG5_CHARSET;
  charsets["-default"]     = DEFAULT_CHARSET;
  charsets["-easteurope"]  = EASTEUROPE_CHARSET;
  charsets["-gb2312"]      = GB2312_CHARSET;
  charsets["-greek"]       = GREEK_CHARSET;
  charsets["-hangul"]      = HANGUL_CHARSET;
  charsets["-mac"]         = MAC_CHARSET;
  charsets["-oem"]         = OEM_CHARSET;
  charsets["-russian"]     = RUSSIAN_CHARSET;
  charsets["-shiftjis"]    = SHIFTJIS_CHARSET;
  charsets["-symbol"]      = SYMBOL_CHARSET;
  charsets["-turkish"]     = TURKISH_CHARSET;
  charsets["-hebrew"]      = HEBREW_CHARSET;
  charsets["-arabic"]      = ARABIC_CHARSET;
  charsets["-thai"]        = THAI_CHARSET;
  std::unordered_map<std::string, DWORD> pitches;
  pitches["-default"]  = DEFAULT_PITCH;
  pitches["-fixed"]    = FIXED_PITCH;
  pitches["-variable"] = VARIABLE_PITCH;
  int nHeight = 20;
  int nWeight = 400;
  bool bItalic = false;
  bool bAntialias = true;
  DWORD dwPitch = pitches["-variable"];
  DWORD dwCharSet = charsets["-default"];
  std::string szFaceName = "Times New Roman";
  for ( std::vector<std::string>::const_iterator pos = szParams.begin(); pos != szParams.end(); ++pos )
  {
    if ( charsets.find(*pos) != charsets.end() )
      dwCharSet = charsets[*pos];
    else if ( pitches.find(*pos) != pitches.end() )
      dwPitch = pitches[*pos];
    else if ( pos->find( "-h" ) == 0 )
      nHeight = atoi( &((*pos)[0]) + 2 );
    else if ( pos->find( "-w" ) == 0 )
      nWeight = atoi( &((*pos)[0]) + 2 );
    else if ( *pos == "-it" )
      bItalic = true;
    else if ( *pos == "-a0" )
      bAntialias = true;
    else
      szFaceName = *pos;
  }
  NStr::TrimInside( szFaceName, '"' );
  printf( "generating font \"%s\" (%d:%d:%d:%d)\n", szFaceName.c_str(), nHeight, nWeight, bItalic, bAntialias );
  SFontInfo fi;
	std::vector<WORD> chars;
	chars.reserve( 256 );
	for ( int i=32; i<256; ++i ) 
		chars.push_back( i );
  LoadFont( GetDesktopWindow(), fi, nHeight, nWeight, bItalic, dwCharSet, bAntialias, dwPitch, szFaceName, chars );
  CPtr<IImage> pImage = CreateFontImage( fi, chars );
	SFontFormat *pFF = CreateFontFormat( szFaceName, fi, chars );
	printf( "saving...\n" );
	{
		CPtr<IDataStream> pStream = CreateFileStream( ".\\1.tfd", STREAM_ACCESS_WRITE );
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSaver;
		saver.Add( 1, pFF );
	}
	{
		CPtr<IDataStream> pStream = CreateFileStream( ".\\1.tga", STREAM_ACCESS_WRITE );
		GetImageProcessor()->SaveImageAsTGA( pStream, pImage );
	}
	return 0;
}
