#pragma once
// The GUI sub-editor's screen model (D031). MFC's CGUIFrame edited the game screen itself:
// CUIScreen::Load read the root GUI_Composer_Project, and the game reads the same fields
// under <base> (UIScreen.cpp OpenLayoutStream). Neither needs a model of the whole screen
// here, only of its window tree, so the screen is kept as its own text plus an index of the
// windows. An edit rewrites just the bytes it changes (an attribute value, one inserted or
// removed <item>) and re-indexes; comments, layout whitespace and attribute order of
// everything else survive, and an unedited save is byte-identical. No engine or MFC types.
//
// A window is the root element and every <item> directly under a window's <Children>
// (CMultipleWindow::operator&). Window ids are handed out on open and on insert and are
// never reused, so an id survives edits to other windows; the root is always id 0.

#include <string>
#include <vector>

namespace NResourceModel
{

struct SUiWindow
{
	int nId = 0;
	int nParent = -1;	// -1 for the root
	int nLine = 1;		// where the window's element starts, for messages
	unsigned nClassTypeID = 0;
	int nElementID = -1;
	int nPositionFlag = 0x0011;	// CSimpleWindow's default when the attribute is absent
	int nVisibleFlag = 1;		// UI_SW_SHOW
	bool bHasPos = false;
	bool bHasSize = false;
	float x = 0, y = 0, w = 0, h = 0;
};

class CUiScreen
{
public:
	// szName only names the file in messages ("<name>:<line>: <reason>"). The root must be
	// <base> or <GUI_Composer_Project>; anything else, an unbalanced element, an unterminated
	// comment or a malformed attribute fails with the line.
	bool Open( const std::string &szText, const std::string &szName, std::string &szError );
	bool IsOpen() const { return !m_szText.empty(); }
	// Open's checks plus: every window below the root has a WindowPos. The editor and the
	// exporter use it; Open alone accepts a screen the game would also read.
	bool Validate( std::string &szError ) const;

	const std::vector<SUiWindow> &Windows() const { return m_windows; }	// document order
	const SUiWindow *Find( int nId ) const;
	const std::string &RootName() const { return m_szRoot; }

	// Rewrites only PositionFlag and the WindowPos and WindowSize attributes of window nId
	// (creating the two elements when the window has none).
	bool SetRect( int nId, int nPositionFlag, float x, float y, float w, float h, std::string &szError );
	bool SetAttribute( int nId, const std::string &szName, const std::string &szValue, std::string &szError );
	// szTemplate is a Data/Editor/UI/*/*.xml <base> document. Its root becomes an <item>
	// appended to nParent's Children (made when absent) with WindowPos set to x, y.
	// Returns the new window's id, or -1 with szError.
	int InsertFromTemplate( int nParent, const std::string &szTemplate, float x, float y, std::string &szError );
	// Removes the windows with their subtrees (the root cannot be removed).
	bool Delete( const std::vector<int> &ids, std::string &szError );
	// The clipboard form of the outermost windows among ids, as text; Paste reads it back,
	// moves each pasted window by (dx, dy) and appends them to nParent. newIds, when given,
	// gets the ids of the pasted top-level windows.
	std::string CopyText( const std::vector<int> &ids ) const;
	bool Paste( int nParent, const std::string &szClipboard, float dx, float dy, std::vector<int> *pNewIds, std::string &szError );

	const std::string &Save() const { return m_szText; }
	// The same screen under a <base> root, which is what the game loads from a mod.
	std::string SaveAsBase() const;

private:
	bool Reindex( std::string &szError );
	bool InsertChunks( int nParent, const std::vector<std::string> &chunks, std::vector<int> *pNewIds, std::string &szError );

	std::string m_szText, m_szName, m_szRoot;
	std::vector<SUiWindow> m_windows;
	int m_nNextId = 0;
};

}
