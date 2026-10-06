#include "ui_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace NResourceModel
{

namespace
{

const char *const s_pszClipboardRoot = "BlitzkriegUiClip";

struct SAttr
{
	std::string szName;
	size_t nValueBegin, nValueEnd;	// inside the quotes
};

// One element with its byte spans in the text: [nBegin, nOpenEnd) is the start tag,
// [nCloseBegin, nEnd) the end tag (both empty for <a/>).
struct SElem
{
	std::string szName;
	size_t nBegin = 0, nOpenEnd = 0, nCloseBegin = 0, nEnd = 0;
	bool bSelfClosing = false;
	int nParent = -1;
	std::vector<SAttr> attrs;
	std::vector<int> kids;
};

int LineOf( const std::string &szText, size_t nOffset )
{
	int nLine = 1;
	for ( size_t i = 0; i < nOffset && i < szText.size(); ++i )
		if ( szText[i] == '\n' )
			++nLine;
	return nLine;
}

std::string Where( const std::string &szName, const std::string &szText, size_t nOffset )
{
	return szName + ":" + std::to_string( LineOf( szText, nOffset ) ) + ": ";
}

bool IsSpace( char c )
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool IsNameChar( char c )
{
	return !IsSpace( c ) && c != '=' && c != '>' && c != '/' && c != '<' && c != '"' && c != '\'';
}

// A tolerant reader for the XML subset the game's screens use. It keeps spans and never
// rewrites text; comments, CDATA and processing instructions are skipped.
bool ParseXml( const std::string &szText, const std::string &szName, std::vector<SElem> &elems, std::string &szError )
{
	elems.clear();
	std::vector<int> stack;
	bool bRootDone = false;
	size_t n = szText.compare( 0, 3, "\xEF\xBB\xBF" ) == 0 ? 3 : 0;
	while ( n < szText.size() )
	{
		if ( szText[n] != '<' )
		{
			if ( !IsSpace( szText[n] ) && stack.empty() )
			{
				szError = Where( szName, szText, n ) + "text outside the root element";
				return false;
			}
			++n;
			continue;
		}
		const struct { const char *pszOpen, *pszClose, *pszWhat; } skipped[] = {
			{ "<?", "?>", "processing instruction" }, { "<!--", "-->", "comment" }, { "<![CDATA[", "]]>", "CDATA section" } };
		bool bSkipped = false;
		for ( const auto &s : skipped )
			if ( szText.compare( n, std::char_traits<char>::length( s.pszOpen ), s.pszOpen ) == 0 )
			{
				const size_t nEnd = szText.find( s.pszClose, n + std::char_traits<char>::length( s.pszOpen ) );
				if ( nEnd == std::string::npos )
				{
					szError = Where( szName, szText, n ) + "unterminated " + s.pszWhat;
					return false;
				}
				n = nEnd + std::char_traits<char>::length( s.pszClose );
				bSkipped = true;
				break;
			}
		if ( bSkipped )
			continue;
		if ( szText.compare( n, 2, "<!" ) == 0 )
		{
			const size_t nEnd = szText.find( '>', n );
			if ( nEnd == std::string::npos )
			{
				szError = Where( szName, szText, n ) + "unterminated declaration";
				return false;
			}
			n = nEnd + 1;
			continue;
		}
		if ( szText.compare( n, 2, "</" ) == 0 )
		{
			const size_t nNameBegin = n + 2;
			size_t nNameEnd = nNameBegin;
			while ( nNameEnd < szText.size() && IsNameChar( szText[nNameEnd] ) )
				++nNameEnd;
			const std::string szClosed = szText.substr( nNameBegin, nNameEnd - nNameBegin );
			size_t nEnd = nNameEnd;
			while ( nEnd < szText.size() && IsSpace( szText[nEnd] ) )
				++nEnd;
			if ( nEnd >= szText.size() || szText[nEnd] != '>' )
			{
				szError = Where( szName, szText, n ) + "malformed end tag </" + szClosed;
				return false;
			}
			if ( stack.empty() )
			{
				szError = Where( szName, szText, n ) + "unbalanced element: </" + szClosed + "> closes nothing";
				return false;
			}
			SElem &open = elems[stack.back()];
			if ( open.szName != szClosed )
			{
				szError = Where( szName, szText, n ) + "unbalanced element: </" + szClosed + "> closes <" + open.szName + "> opened at line " +
				          std::to_string( LineOf( szText, open.nBegin ) );
				return false;
			}
			open.nCloseBegin = n;
			open.nEnd = nEnd + 1;
			stack.pop_back();
			n = nEnd + 1;
			continue;
		}
		// A start tag.
		SElem elem;
		elem.nBegin = n;
		size_t i = n + 1;
		const size_t nNameBegin = i;
		while ( i < szText.size() && IsNameChar( szText[i] ) )
			++i;
		elem.szName = szText.substr( nNameBegin, i - nNameBegin );
		if ( elem.szName.empty() )
		{
			szError = Where( szName, szText, n ) + "'<' with no element name";
			return false;
		}
		for ( ;; )
		{
			while ( i < szText.size() && IsSpace( szText[i] ) )
				++i;
			if ( i >= szText.size() )
			{
				szError = Where( szName, szText, n ) + "unterminated start tag <" + elem.szName;
				return false;
			}
			if ( szText[i] == '>' )
			{
				++i;
				break;
			}
			if ( szText[i] == '/' && i + 1 < szText.size() && szText[i + 1] == '>' )
			{
				elem.bSelfClosing = true;
				i += 2;
				break;
			}
			SAttr attr;
			const size_t nAttrBegin = i;
			while ( i < szText.size() && IsNameChar( szText[i] ) )
				++i;
			attr.szName = szText.substr( nAttrBegin, i - nAttrBegin );
			while ( i < szText.size() && IsSpace( szText[i] ) )
				++i;
			if ( attr.szName.empty() || i >= szText.size() || szText[i] != '=' )
			{
				szError = Where( szName, szText, nAttrBegin ) + "malformed attribute in <" + elem.szName + ">: " + ( attr.szName.empty() ? szText.substr( nAttrBegin, 1 ) : attr.szName ) + " has no '='";
				return false;
			}
			++i;
			while ( i < szText.size() && IsSpace( szText[i] ) )
				++i;
			if ( i >= szText.size() || ( szText[i] != '"' && szText[i] != '\'' ) )
			{
				szError = Where( szName, szText, nAttrBegin ) + "attribute " + attr.szName + " of <" + elem.szName + "> has no quoted value";
				return false;
			}
			const size_t nQuoteEnd = szText.find( szText[i], i + 1 );
			if ( nQuoteEnd == std::string::npos )
			{
				szError = Where( szName, szText, nAttrBegin ) + "attribute " + attr.szName + " of <" + elem.szName + "> has an unterminated value";
				return false;
			}
			attr.nValueBegin = i + 1;
			attr.nValueEnd = nQuoteEnd;
			i = nQuoteEnd + 1;
			elem.attrs.push_back( attr );
		}
		elem.nOpenEnd = i;
		if ( elem.bSelfClosing )
		{
			elem.nCloseBegin = elem.nEnd = i;
		}
		if ( stack.empty() )
		{
			if ( bRootDone )
			{
				szError = Where( szName, szText, n ) + "second root element <" + elem.szName + ">";
				return false;
			}
			bRootDone = true;
		}
		else
			elem.nParent = stack.back();
		const int nIndex = static_cast<int>( elems.size() );
		if ( elem.nParent >= 0 )
			elems[elem.nParent].kids.push_back( nIndex );
		elems.push_back( elem );
		if ( !elem.bSelfClosing )
			stack.push_back( nIndex );
		n = i;
	}
	if ( !stack.empty() )
	{
		const SElem &open = elems[stack.back()];
		szError = Where( szName, szText, open.nBegin ) + "unbalanced element: <" + open.szName + "> is never closed";
		return false;
	}
	if ( elems.empty() )
	{
		szError = szName + ":1: no root element";
		return false;
	}
	return true;
}

std::string Decode( const std::string &sz )
{
	std::string out;
	for ( size_t i = 0; i < sz.size(); ++i )
	{
		static const struct { const char *pszEntity; char c; } entities[] = { { "&amp;", '&' }, { "&lt;", '<' }, { "&gt;", '>' }, { "&quot;", '"' }, { "&apos;", '\'' } };
		bool bDone = false;
		if ( sz[i] == '&' )
			for ( const auto &e : entities )
				if ( sz.compare( i, std::char_traits<char>::length( e.pszEntity ), e.pszEntity ) == 0 )
				{
					out += e.c;
					i += std::char_traits<char>::length( e.pszEntity ) - 1;
					bDone = true;
					break;
				}
		if ( !bDone )
			out += sz[i];
	}
	return out;
}

std::string Escape( const std::string &sz, char cQuote )
{
	std::string out;
	for ( char c : sz )
		if ( c == '&' )
			out += "&amp;";
		else if ( c == '<' )
			out += "&lt;";
		else if ( c == cQuote )
			out += cQuote == '"' ? "&quot;" : "&apos;";
		else
			out += c;
	return out;
}

// A parsed text that edits itself. Every edit changes the text and re-parses, so element
// indices (document order) stay valid across edits that add no element before them, and
// the helpers below never hold a span across a change.
struct SDoc
{
	std::string szText, szName;
	std::vector<SElem> e;
	std::vector<int> win;	// window -> element, in document order
	std::vector<int> winParent;

	bool Load( std::string &szError )
	{
		if ( !ParseXml( szText, szName, e, szError ) )
			return false;
		win.clear();
		winParent.clear();
		Collect( 0, -1 );
		return true;
	}
	void Collect( int nElem, int nParentWin )
	{
		const int nWin = static_cast<int>( win.size() );
		win.push_back( nElem );
		winParent.push_back( nParentWin );
		for ( int nKid : e[nElem].kids )
			if ( e[nKid].szName == "Children" )
				for ( int nItem : e[nKid].kids )
					if ( e[nItem].szName == "item" )
						Collect( nItem, nWin );
	}
	int ChildNamed( int nElem, const char *pszName ) const
	{
		for ( int nKid : e[nElem].kids )
			if ( e[nKid].szName == pszName )
				return nKid;
		return -1;
	}
	const SAttr *Attr( int nElem, const std::string &szAttr ) const
	{
		for ( const SAttr &a : e[nElem].attrs )
			if ( a.szName == szAttr )
				return &a;
		return nullptr;
	}
	bool GetAttr( int nElem, const char *pszAttr, std::string *pValue ) const
	{
		const SAttr *pAttr = Attr( nElem, pszAttr );
		if ( pAttr == nullptr )
			return false;
		*pValue = Decode( szText.substr( pAttr->nValueBegin, pAttr->nValueEnd - pAttr->nValueBegin ) );
		return true;
	}
	// Sets one attribute: the value span when it exists, otherwise appended to the start tag.
	bool SetAttr( int nElem, const std::string &szAttr, const std::string &szValue, std::string &szError )
	{
		const SAttr *pAttr = Attr( nElem, szAttr );
		if ( pAttr != nullptr )
		{
			const char cQuote = szText[pAttr->nValueBegin - 1];
			szText.replace( pAttr->nValueBegin, pAttr->nValueEnd - pAttr->nValueBegin, Escape( szValue, cQuote ) );
		}
		else
		{
			const SElem &el = e[nElem];
			size_t nAt = el.bSelfClosing ? el.nOpenEnd - 2 : el.nOpenEnd - 1;
			while ( nAt > el.nBegin && IsSpace( szText[nAt - 1] ) )
				--nAt;
			szText.insert( nAt, " " + szAttr + "=\"" + Escape( szValue, '"' ) + "\"" );
		}
		return Load( szError );
	}
	// <a/> becomes <a></a> so it can take children.
	bool Expand( int nElem, std::string &szError )
	{
		const SElem &el = e[nElem];
		if ( !el.bSelfClosing )
			return true;
		size_t nAt = el.nOpenEnd - 2;
		while ( nAt > el.nBegin && IsSpace( szText[nAt - 1] ) )
			--nAt;
		szText.replace( nAt, el.nOpenEnd - nAt, "></" + el.szName + ">" );
		return Load( szError );
	}
	std::string Nl() const { return szText.find( "\r\n" ) != std::string::npos ? "\r\n" : "\n"; }
	// The indentation of the line an element starts on, when nothing else is on the line.
	bool IndentOf( int nElem, std::string *pIndent ) const
	{
		size_t nStart = e[nElem].nBegin;
		while ( nStart > 0 && ( szText[nStart - 1] == ' ' || szText[nStart - 1] == '\t' ) )
			--nStart;
		if ( nStart == 0 || szText[nStart - 1] != '\n' )
			return false;
		*pIndent = szText.substr( nStart, e[nElem].nBegin - nStart );
		return true;
	}
};

std::string FormatNumber( float f )
{
	char sz[64];
	if ( f == static_cast<float>( static_cast<long long>( f ) ) )
		std::snprintf( sz, sizeof( sz ), "%lld", static_cast<long long>( f ) );
	else
		std::snprintf( sz, sizeof( sz ), "%g", f );
	return sz;
}

bool ParseNumber( const std::string &sz, double *pValue )
{
	char *pEnd = nullptr;
	if ( sz.size() > 1 && sz[0] == '0' && ( sz[1] == 'x' || sz[1] == 'X' ) )
		*pValue = static_cast<double>( std::strtoull( sz.c_str(), &pEnd, 16 ) );
	else
		*pValue = std::strtod( sz.c_str(), &pEnd );
	return pEnd != sz.c_str() && *pEnd == 0;
}

// The engine reads these as 32 bit integers and wraps the big hex ids (0xAC07A918).
int ToInt32( double v )
{
	return static_cast<int>( static_cast<unsigned>( static_cast<long long>( v ) ) );
}

// Inserts a new <szElement/> child of window element nWin: at the front (nAfter < 0) or after
// element nAfter, keeping the layout whitespace the window's first child sits in.
bool InsertChild( SDoc &doc, int nWin, int nAfter, const std::string &szChild, std::string &szError )
{
	if ( !doc.Expand( nWin, szError ) )
		return false;
	const SElem &win = doc.e[nWin];
	std::string szSep;
	size_t nRun = win.nOpenEnd;
	while ( nRun < doc.szText.size() && IsSpace( doc.szText[nRun] ) )
		++nRun;
	const std::string szRun = doc.szText.substr( win.nOpenEnd, nRun - win.nOpenEnd );
	if ( szRun.find( '\n' ) != std::string::npos )
		szSep = szRun;
	const size_t nAt = nAfter < 0 ? win.nOpenEnd : doc.e[nAfter].nEnd;
	doc.szText.insert( nAt, szSep + szChild );
	return doc.Load( szError );
}

// Sets WindowPos or WindowSize (szElement) of window element nWin; an unchanged value keeps
// its original text.
bool SetPair( SDoc &doc, int nWin, const char *pszElement, float a, float b, std::string &szError )
{
	int nChild = doc.ChildNamed( nWin, pszElement );
	if ( nChild < 0 )
	{
		const bool bSize = std::string( pszElement ) == "WindowSize";
		const int nPos = doc.ChildNamed( nWin, "WindowPos" );
		const std::string szNew = std::string( "<" ) + pszElement + " x=\"" + FormatNumber( a ) + "\" y=\"" + FormatNumber( b ) + "\"/>";
		if ( !InsertChild( doc, nWin, bSize ? nPos : -1, szNew, szError ) )
			return false;
		return true;
	}
	const float values[2] = { a, b };
	const char *const names[2] = { "x", "y" };
	for ( int i = 0; i < 2; ++i )
	{
		std::string szOld;
		double fOld = 0;
		if ( doc.GetAttr( nChild, names[i], &szOld ) && ParseNumber( szOld, &fOld ) && static_cast<float>( fOld ) == values[i] )
			continue;
		if ( !doc.SetAttr( nChild, names[i], FormatNumber( values[i] ), szError ) )
			return false;
	}
	return true;
}

void ReadWindow( const SDoc &doc, int nWinIndex, SUiWindow &w )
{
	const int nElem = doc.win[nWinIndex];
	std::string sz;
	double v = 0;
	w.nParent = doc.winParent[nWinIndex];
	w.nLine = LineOf( doc.szText, doc.e[nElem].nBegin );
	if ( doc.GetAttr( nElem, "ClassTypeID", &sz ) && ParseNumber( sz, &v ) )
		w.nClassTypeID = static_cast<unsigned>( ToInt32( v ) );
	if ( doc.GetAttr( nElem, "ElementID", &sz ) && ParseNumber( sz, &v ) )
		w.nElementID = ToInt32( v );
	if ( doc.GetAttr( nElem, "PositionFlag", &sz ) && ParseNumber( sz, &v ) )
		w.nPositionFlag = ToInt32( v );
	if ( doc.GetAttr( nElem, "VisibleFlag", &sz ) && ParseNumber( sz, &v ) )
		w.nVisibleFlag = ToInt32( v );
	const int nPos = doc.ChildNamed( nElem, "WindowPos" ), nSize = doc.ChildNamed( nElem, "WindowSize" );
	w.bHasPos = nPos >= 0;
	w.bHasSize = nSize >= 0;
	if ( nPos >= 0 )
	{
		if ( doc.GetAttr( nPos, "x", &sz ) && ParseNumber( sz, &v ) ) w.x = static_cast<float>( v );
		if ( doc.GetAttr( nPos, "y", &sz ) && ParseNumber( sz, &v ) ) w.y = static_cast<float>( v );
	}
	if ( nSize >= 0 )
	{
		if ( doc.GetAttr( nSize, "x", &sz ) && ParseNumber( sz, &v ) ) w.w = static_cast<float>( v );
		if ( doc.GetAttr( nSize, "y", &sz ) && ParseNumber( sz, &v ) ) w.h = static_cast<float>( v );
	}
}

void RenameRoot( SDoc &doc, const std::string &szNewName )
{
	const SElem root = doc.e[0];
	if ( !root.bSelfClosing )
		doc.szText.replace( root.nCloseBegin + 2, root.szName.size(), szNewName );
	doc.szText.replace( root.nBegin + 1, root.szName.size(), szNewName );
}

// The root element's text as an <item> chunk.
bool ChunkFromRoot( SDoc &doc, std::string &szChunk, std::string &szError )
{
	if ( doc.e[0].szName != "item" )
	{
		RenameRoot( doc, "item" );
		if ( !doc.Load( szError ) )
			return false;
	}
	szChunk = doc.szText.substr( doc.e[0].nBegin, doc.e[0].nEnd - doc.e[0].nBegin );
	return true;
}

}

const SUiWindow *CUiScreen::Find( int nId ) const
{
	for ( const SUiWindow &w : m_windows )
		if ( w.nId == nId )
			return &w;
	return nullptr;
}

// Re-reads the windows after an edit that neither adds nor removes one, keeping their ids.
bool CUiScreen::Reindex( std::string &szError )
{
	SDoc doc;
	doc.szText = m_szText;
	doc.szName = m_szName;
	if ( !doc.Load( szError ) )
		return false;
	if ( doc.win.size() != m_windows.size() )
	{
		szError = m_szName + ": internal error: window count changed by an edit that adds or removes none";
		return false;
	}
	for ( size_t i = 0; i < doc.win.size(); ++i )
	{
		SUiWindow w;
		w.nId = m_windows[i].nId;
		ReadWindow( doc, static_cast<int>( i ), w );
		// A parent is held as an id, not as an index.
		w.nParent = w.nParent < 0 ? -1 : m_windows[w.nParent].nId;
		m_windows[i] = w;
	}
	return true;
}

bool CUiScreen::Open( const std::string &szText, const std::string &szName, std::string &szError )
{
	SDoc doc;
	doc.szText = szText;
	doc.szName = szName;
	if ( !doc.Load( szError ) )
		return false;
	const std::string &szRoot = doc.e[0].szName;
	if ( szRoot != "base" && szRoot != "GUI_Composer_Project" )
	{
		szError = Where( szName, szText, doc.e[0].nBegin ) + "unknown root <" + szRoot + ">, a screen is <base> or <GUI_Composer_Project>";
		return false;
	}
	m_szText = szText;
	m_szName = szName;
	m_szRoot = szRoot;
	m_windows.assign( doc.win.size(), SUiWindow() );
	m_nNextId = 0;
	for ( size_t i = 0; i < doc.win.size(); ++i )
	{
		SUiWindow w;
		w.nId = m_nNextId++;
		ReadWindow( doc, static_cast<int>( i ), w );
		w.nParent = w.nParent < 0 ? -1 : m_windows[w.nParent].nId;
		m_windows[i] = w;
	}
	return true;
}

bool CUiScreen::Validate( std::string &szError ) const
{
	for ( const SUiWindow &w : m_windows )
		if ( w.nParent >= 0 && !w.bHasPos )
		{
			char sz[32];
			std::snprintf( sz, sizeof( sz ), "0x%08X", w.nClassTypeID );
			szError = m_szName + ":" + std::to_string( w.nLine ) + ": window " + std::to_string( w.nId ) + " (ClassTypeID " + sz + ") has no WindowPos";
			return false;
		}
	return true;
}

bool CUiScreen::GetAttribute( int nId, const std::string &szName, std::string &szValue, std::string &szError ) const
{
	size_t nIndex = 0;
	while ( nIndex < m_windows.size() && m_windows[nIndex].nId != nId )
		++nIndex;
	if ( nIndex == m_windows.size() )
	{
		szError = m_szName + ": no window with id " + std::to_string( nId );
		return false;
	}
	SDoc doc;
	doc.szText = m_szText;
	doc.szName = m_szName;
	if ( !doc.Load( szError ) )
		return false;
	szValue.clear();
	return doc.GetAttr( doc.win[nIndex], szName.c_str(), &szValue );
}

bool CUiScreen::SetAttribute( int nId, const std::string &szName, const std::string &szValue, std::string &szError )
{
	size_t nIndex = 0;
	while ( nIndex < m_windows.size() && m_windows[nIndex].nId != nId )
		++nIndex;
	if ( nIndex == m_windows.size() )
	{
		szError = m_szName + ": no window with id " + std::to_string( nId );
		return false;
	}
	SDoc doc;
	doc.szText = m_szText;
	doc.szName = m_szName;
	if ( !doc.Load( szError ) || !doc.SetAttr( doc.win[nIndex], szName, szValue, szError ) )
		return false;
	m_szText = doc.szText;
	return Reindex( szError );
}

bool CUiScreen::SetRect( int nId, int nPositionFlag, float x, float y, float w, float h, std::string &szError )
{
	size_t nIndex = 0;
	while ( nIndex < m_windows.size() && m_windows[nIndex].nId != nId )
		++nIndex;
	if ( nIndex == m_windows.size() )
	{
		szError = m_szName + ": no window with id " + std::to_string( nId );
		return false;
	}
	SDoc doc;
	doc.szText = m_szText;
	doc.szName = m_szName;
	if ( !doc.Load( szError ) )
		return false;
	// Window elements stay at the same index while only attributes change; a created
	// WindowPos or WindowSize adds elements after the window, so the window is re-found by index
	// in doc.win (document order of windows does not change).
	const SUiWindow &old = m_windows[nIndex];
	std::string szOld;
	const bool bHasFlag = doc.GetAttr( doc.win[nIndex], "PositionFlag", &szOld );
	if ( bHasFlag ? old.nPositionFlag != nPositionFlag : nPositionFlag != 0x0011 )
	{
		// Keep the notation the file uses; a new attribute follows the shipped screens' hex.
		char sz[32];
		const bool bHex = bHasFlag ? ( szOld.size() > 1 && ( szOld[1] == 'x' || szOld[1] == 'X' ) ) : true;
		std::snprintf( sz, sizeof( sz ), bHex ? "0x%04X" : "%d", nPositionFlag );
		if ( !doc.SetAttr( doc.win[nIndex], "PositionFlag", sz, szError ) )
			return false;
	}
	if ( !SetPair( doc, doc.win[nIndex], "WindowPos", x, y, szError ) || !SetPair( doc, doc.win[nIndex], "WindowSize", w, h, szError ) )
		return false;
	m_szText = doc.szText;
	return Reindex( szError );
}

// Inserts the chunks (each one <item> element) as the last children of window nParent.
bool CUiScreen::InsertChunks( int nParent, const std::vector<std::string> &chunks, std::vector<int> *pNewIds, std::vector<int> *pAllNew, std::string &szError )
{
	size_t nIndex = 0;
	while ( nIndex < m_windows.size() && m_windows[nIndex].nId != nParent )
		++nIndex;
	if ( nIndex == m_windows.size() )
	{
		szError = m_szName + ": no window with id " + std::to_string( nParent );
		return false;
	}
	int nNewWindows = 0;
	for ( const std::string &szChunk : chunks )
	{
		SDoc chunk;
		chunk.szText = szChunk;
		chunk.szName = "new window";
		if ( !chunk.Load( szError ) )
			return false;
		nNewWindows += static_cast<int>( chunk.win.size() );
	}
	SDoc doc;
	doc.szText = m_szText;
	doc.szName = m_szName;
	if ( !doc.Load( szError ) )
		return false;
	const std::string szNl = doc.Nl();
	const int nParentElem = doc.win[nIndex];
	int nChildren = doc.ChildNamed( nParentElem, "Children" );

	std::string szParentIndent, szChildrenIndent, szItemIndent;
	const bool bLayout = doc.IndentOf( nParentElem, &szParentIndent );
	szChildrenIndent = szParentIndent + "\t";
	szItemIndent = szChildrenIndent + "\t";
	if ( nChildren >= 0 && doc.IndentOf( nChildren, &szChildrenIndent ) )
		szItemIndent = szChildrenIndent + "\t";
	if ( nChildren >= 0 && !doc.e[nChildren].kids.empty() )
		doc.IndentOf( doc.e[nChildren].kids[0], &szItemIndent );

	auto items = [&]( const std::string &szIndent )
	{
		std::string sz;
		for ( const std::string &szChunk : chunks )
			sz += ( bLayout ? szNl + szIndent : std::string() ) + szChunk;
		return sz;
	};
	size_t nInsertAt = 0;	// where the first new text lands, to count the windows before it
	if ( nChildren >= 0 && !doc.e[nChildren].kids.empty() )
	{
		nInsertAt = doc.e[doc.e[nChildren].kids.back()].nEnd;
		doc.szText.insert( nInsertAt, items( szItemIndent ) );
	}
	else if ( nChildren >= 0 )
	{
		SElem el = doc.e[nChildren];
		const std::string szClose = bLayout ? szNl + szChildrenIndent : std::string();
		if ( el.bSelfClosing )
		{
			size_t nAt = el.nOpenEnd - 2;
			while ( nAt > el.nBegin && IsSpace( doc.szText[nAt - 1] ) )
				--nAt;
			nInsertAt = el.nBegin;
			doc.szText.replace( nAt, el.nOpenEnd - nAt, ">" + items( szItemIndent ) + szClose + "</Children>" );
		}
		else
		{
			bool bBlank = true;
			for ( size_t i = el.nOpenEnd; i < el.nCloseBegin; ++i )
				bBlank = bBlank && IsSpace( doc.szText[i] );
			// Text between the tags that is not blank (a comment) stays; the windows go after it.
			size_t nAt = el.nCloseBegin;
			while ( !bBlank && nAt > el.nOpenEnd && IsSpace( doc.szText[nAt - 1] ) )
				--nAt;
			nInsertAt = bBlank ? el.nOpenEnd : nAt;
			doc.szText.replace( nInsertAt, bBlank ? el.nCloseBegin - el.nOpenEnd : 0, items( szItemIndent ) + szClose );
		}
	}
	else
	{
		if ( !doc.Expand( nParentElem, szError ) )
			return false;
		const SElem &parent = doc.e[nParentElem];
		size_t nAt = parent.nCloseBegin;
		while ( nAt > parent.nOpenEnd && IsSpace( doc.szText[nAt - 1] ) )
			--nAt;
		const std::string szClose = bLayout ? szNl + szChildrenIndent : std::string();
		nInsertAt = nAt;
		doc.szText.insert( nAt, ( bLayout ? szNl + szChildrenIndent : std::string() ) + "<Children>" + items( szItemIndent ) + szClose + "</Children>" );
	}
	std::vector<int> ids;
	for ( const SUiWindow &w : m_windows )
		ids.push_back( w.nId );
	std::vector<int> fresh;
	for ( int i = 0; i < nNewWindows; ++i )
		fresh.push_back( m_nNextId++ );

	SDoc after;
	after.szText = doc.szText;
	after.szName = m_szName;
	if ( !after.Load( szError ) )
		return false;
	// The new windows start at or after the insertion offset and every old window before it
	// precedes them in document order.
	size_t nBefore = 0;
	while ( nBefore < after.win.size() && after.e[after.win[nBefore]].nBegin < nInsertAt )
		++nBefore;
	ids.insert( ids.begin() + nBefore, fresh.begin(), fresh.end() );
	if ( after.win.size() != ids.size() )
	{
		szError = m_szName + ": internal error: an insert produced " + std::to_string( after.win.size() ) + " windows, expected " + std::to_string( ids.size() );
		return false;
	}
	m_szText = after.szText;
	m_windows.assign( ids.size(), SUiWindow() );
	for ( size_t i = 0; i < ids.size(); ++i )
	{
		SUiWindow w;
		w.nId = ids[i];
		ReadWindow( after, static_cast<int>( i ), w );
		w.nParent = w.nParent < 0 ? -1 : ids[w.nParent];
		m_windows[i] = w;
	}
	if ( pNewIds != nullptr )
	{
		// The top-level pasted windows are the new ones whose parent is not new.
		pNewIds->clear();
		for ( int nId : fresh )
			if ( Find( nId )->nParent == nParent )
				pNewIds->push_back( nId );
	}
	if ( pAllNew != nullptr )
		*pAllNew = fresh;
	return true;
}

int CUiScreen::InsertFromTemplate( int nParent, const std::string &szTemplate, float x, float y, std::string &szError )
{
	SDoc doc;
	doc.szText = szTemplate;
	doc.szName = "template";
	if ( !doc.Load( szError ) )
		return -1;
	if ( doc.e[0].szName != "base" )
	{
		szError = Where( "template", szTemplate, doc.e[0].nBegin ) + "a template's root is <base>, not <" + doc.e[0].szName + ">";
		return -1;
	}
	std::string szChunk;
	if ( !ChunkFromRoot( doc, szChunk, szError ) )
		return -1;
	SDoc item;
	item.szText = szChunk;
	item.szName = "template";
	if ( !item.Load( szError ) || !SetPair( item, 0, "WindowPos", x, y, szError ) )
		return -1;
	std::vector<int> ids;
	if ( !InsertChunks( nParent, { item.szText }, &ids, nullptr, szError ) || ids.empty() )
		return -1;
	return ids[0];
}

bool CUiScreen::Delete( const std::vector<int> &ids, std::string &szError )
{
	SDoc doc;
	doc.szText = m_szText;
	doc.szName = m_szName;
	if ( !doc.Load( szError ) )
		return false;
	std::vector<size_t> spans;	// window indices to remove
	for ( int nId : ids )
	{
		size_t nIndex = 0;
		while ( nIndex < m_windows.size() && m_windows[nIndex].nId != nId )
			++nIndex;
		if ( nIndex == m_windows.size() )
		{
			szError = m_szName + ": no window with id " + std::to_string( nId );
			return false;
		}
		if ( nIndex == 0 )
		{
			szError = m_szName + ": the root window cannot be deleted";
			return false;
		}
		spans.push_back( nIndex );
	}
	std::sort( spans.begin(), spans.end() );
	spans.erase( std::unique( spans.begin(), spans.end() ), spans.end() );
	std::vector<bool> gone( m_windows.size(), false );
	for ( size_t i = 0; i < m_windows.size(); ++i )
	{
		if ( std::binary_search( spans.begin(), spans.end(), i ) )
			gone[i] = true;
		else if ( doc.winParent[i] >= 0 && gone[doc.winParent[i]] )
			gone[i] = true;
	}
	// Erase from the back so earlier offsets stay valid; a window inside an erased one is
	// covered by its ancestor's span.
	for ( size_t i = m_windows.size(); i-- > 0; )
	{
		if ( !gone[i] || ( doc.winParent[i] >= 0 && gone[doc.winParent[i]] ) )
			continue;
		const SElem &el = doc.e[doc.win[i]];
		size_t nFrom = el.nBegin;
		while ( nFrom > 0 && IsSpace( doc.szText[nFrom - 1] ) )
			--nFrom;
		doc.szText.erase( nFrom, el.nEnd - nFrom );
		// Offsets of earlier windows are before nFrom and unaffected; reparse only at the end.
	}
	std::vector<SUiWindow> remaining;
	for ( size_t i = 0; i < m_windows.size(); ++i )
		if ( !gone[i] )
			remaining.push_back( m_windows[i] );
	SDoc after;
	after.szText = doc.szText;
	after.szName = m_szName;
	if ( !after.Load( szError ) )
		return false;
	if ( after.win.size() != remaining.size() )
	{
		szError = m_szName + ": internal error: a delete left " + std::to_string( after.win.size() ) + " windows, expected " + std::to_string( remaining.size() );
		return false;
	}
	m_szText = after.szText;
	m_windows = remaining;
	for ( size_t i = 0; i < m_windows.size(); ++i )
	{
		const int nId = m_windows[i].nId;
		ReadWindow( after, static_cast<int>( i ), m_windows[i] );
		m_windows[i].nId = nId;
		m_windows[i].nParent = after.winParent[i] < 0 ? -1 : m_windows[after.winParent[i]].nId;
	}
	return true;
}

std::string CUiScreen::CopyText( const std::vector<int> &ids ) const
{
	SDoc doc;
	doc.szText = m_szText;
	doc.szName = m_szName;
	std::string szError;
	std::string szOut = std::string( "<" ) + s_pszClipboardRoot + ">";
	if ( !doc.Load( szError ) )
		return szOut + "</" + s_pszClipboardRoot + ">";
	std::vector<bool> selected( m_windows.size(), false );
	for ( size_t i = 1; i < m_windows.size(); ++i )
		selected[i] = std::find( ids.begin(), ids.end(), m_windows[i].nId ) != ids.end();
	for ( size_t i = 1; i < m_windows.size(); ++i )
	{
		// Outermost only: a selected window's selected descendants travel inside it.
		bool bInside = false;
		for ( int p = doc.winParent[i]; p >= 0; p = doc.winParent[p] )
			bInside = bInside || selected[p];
		if ( !selected[i] || bInside )
			continue;
		const SElem &el = doc.e[doc.win[i]];
		szOut += "\n" + doc.szText.substr( el.nBegin, el.nEnd - el.nBegin );
	}
	return szOut + "\n</" + s_pszClipboardRoot + ">";
}

bool CUiScreen::Paste( int nParent, const std::string &szClipboard, float dx, float dy, bool bUniqueIds, std::vector<int> *pNewIds, std::vector<SUiElementIdChange> *pChanged, std::string &szError )
{
	if ( pChanged != nullptr )
		pChanged->clear();
	// The ElementIDs the screen uses, nested windows included; -1 is the engine's "no id".
	std::set<int> used;
	for ( const SUiWindow &w : m_windows )
		if ( w.nElementID != -1 )
			used.insert( w.nElementID );
	std::vector<SUiElementIdChange> changes;
	int nPastedWindow = 0;	// the pasted windows in document order, all chunks together
	SDoc clip;
	clip.szText = szClipboard;
	clip.szName = "clipboard";
	if ( !ParseXml( szClipboard, "clipboard", clip.e, szError ) )
		return false;
	if ( clip.e[0].szName != s_pszClipboardRoot )
	{
		szError = "clipboard: not windows copied from a screen (root <" + clip.e[0].szName + ">)";
		return false;
	}
	std::vector<std::string> chunks;
	for ( int nKid : clip.e[0].kids )
	{
		if ( clip.e[nKid].szName != "item" )
			continue;
		SDoc item;
		item.szText = szClipboard.substr( clip.e[nKid].nBegin, clip.e[nKid].nEnd - clip.e[nKid].nBegin );
		item.szName = "clipboard";
		if ( !item.Load( szError ) )
			return false;
		SUiWindow w;
		ReadWindow( item, 0, w );
		if ( ( dx != 0 || dy != 0 || !w.bHasPos ) && !SetPair( item, 0, "WindowPos", w.x + dx, w.y + dy, szError ) )
			return false;
		// SetAttr re-parses and adds no element, so window k stays item.win[k].
		for ( size_t k = 0; k < item.win.size(); ++k, ++nPastedWindow )
		{
			std::string szId;
			double v = 0;
			if ( !bUniqueIds || !item.GetAttr( item.win[k], "ElementID", &szId ) || !ParseNumber( szId, &v ) )
				continue;
			const int nOld = ToInt32( v );
			if ( nOld == -1 )
				continue;
			if ( used.insert( nOld ).second )
				continue;
			// The next free id above the old one, wrapping past INT_MAX; -1 is skipped.
			int nNew = nOld;
			do
				nNew = static_cast<int>( static_cast<unsigned>( nNew ) + 1u );
			while ( nNew == -1 || used.count( nNew ) != 0 );
			used.insert( nNew );
			char sz[32];
			const bool bHex = szId.size() > 1 && ( szId[1] == 'x' || szId[1] == 'X' );
			if ( bHex )
				std::snprintf( sz, sizeof( sz ), "0x%X", static_cast<unsigned>( nNew ) );
			else
				std::snprintf( sz, sizeof( sz ), "%d", nNew );
			if ( !item.SetAttr( item.win[k], "ElementID", sz, szError ) )
				return false;
			SUiElementIdChange change;
			change.nWindow = nPastedWindow;	// an index here, the model id once inserted
			change.nOld = nOld;
			change.nNew = nNew;
			changes.push_back( change );
		}
		chunks.push_back( item.szText );
	}
	if ( chunks.empty() )
	{
		szError = "clipboard: holds no windows";
		return false;
	}
	std::vector<int> made;
	if ( !InsertChunks( nParent, chunks, pNewIds, &made, szError ) )
		return false;
	for ( SUiElementIdChange &change : changes )
		change.nWindow = made[change.nWindow];
	if ( pChanged != nullptr )
		*pChanged = changes;
	return true;
}

std::string CUiScreen::SaveAsBase() const
{
	if ( m_szRoot == "base" )
		return m_szText;
	SDoc doc;
	std::string szError;
	doc.szText = m_szText;
	doc.szName = m_szName;
	if ( !doc.Load( szError ) )
		return m_szText;
	RenameRoot( doc, "base" );
	return doc.szText;
}

}
