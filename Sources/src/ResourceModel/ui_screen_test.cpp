// CUiScreen (ui_screen.*) against the engine's own reader, run from the comparator tier's
// executable because that is the one that hosts StreamIO:
//   1. every shipped Data/UI screen and Data/Editor/UI template opens and saves byte-identical,
//      and the model's windows equal what the engine's CDataTreeXML reads (position, size, flag);
//   2. a copy of MainMenu.xml is edited (move, resize with a new flag, insert Button00, delete a
//      control, copy and paste, insert under a leaf) and the engine reads back exactly those
//      changes, the window count changing by the inserts and deletes;
//   3. a GUI_Composer_Project root opens, saves as itself and as <base>, and the engine reads
//      the <base> one; moving a window back restores the original bytes;
//   4. malformed input and bad requests fail with the line or the reason.
#include "StdAfx.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "ui_screen.h"

namespace fs = std::filesystem;
using namespace NResourceModel;

namespace
{

// The slice of CSimpleWindow / CMultipleWindow::operator&(IDataTree&) an edit can touch,
// read through the same CDataTreeXML route CUIScreen::Load uses.
struct SEngineWindow
{
	CVec2 vPos, vSize;
	int nFlag;
	std::vector<SEngineWindow> children;

	SEngineWindow() : vPos( 0, 0 ), vSize( 0, 0 ), nFlag( 0x0011 ) {}
	int operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "WindowPos", &vPos );
		saver.Add( "WindowSize", &vSize );
		saver.Add( "PositionFlag", &nFlag );
		saver.Add( "Children", &children );
		return 0;
	}
};

struct SFlat
{
	float x, y, w, h;
	int nFlag;
	bool operator==( const SFlat &o ) const { return x == o.x && y == o.y && w == o.w && h == o.h && nFlag == o.nFlag; }
};

void Flatten( const SEngineWindow &win, std::vector<SFlat> *pOut )
{
	SFlat flat = { win.vPos.x, win.vPos.y, win.vSize.x, win.vSize.y, win.nFlag };
	pOut->push_back( flat );
	for ( const SEngineWindow &child : win.children )
		Flatten( child, pOut );
}

bool ReadBytes( const fs::path &path, std::string *pBytes )
{
	std::ifstream file( path, std::ios::binary );
	if ( !file )
		return false;
	pBytes->assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

bool WriteBytes( const fs::path &path, const std::string &bytes )
{
	std::error_code error;
	fs::create_directories( path.parent_path(), error );
	std::ofstream file( path, std::ios::binary | std::ios::trunc );
	file.write( bytes.data(), static_cast<std::streamsize>( bytes.size() ) );
	return file.good();
}

// The windows the engine reads from a file under root szRoot, in document order.
bool EngineRead( const fs::path &file, const char *pszRoot, std::vector<SFlat> *pOut )
{
	CPtr<IDataStorage> pStorage = OpenStorage( ( file.parent_path().string() + "/" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( file.filename().string().c_str(), STREAM_ACCESS_READ ) : 0;
	CPtr<IDataTree> pDT = pStream != 0 ? CreateDataTreeSaver( pStream, IDataTree::READ, pszRoot ) : 0;
	if ( pDT == 0 )
		return false;
	SEngineWindow root;
	root.operator&( *pDT );
	pOut->clear();
	Flatten( root, pOut );
	return true;
}

SFlat Of( const SUiWindow &w )
{
	SFlat flat = { w.x, w.y, w.w, w.h, w.nPositionFlag };
	return flat;
}

bool ModelEqualsEngine( const CUiScreen &screen, const std::vector<SFlat> &engine )
{
	if ( screen.Windows().size() != engine.size() )
		return false;
	for ( size_t i = 0; i < engine.size(); ++i )
		if ( !( Of( screen.Windows()[i] ) == engine[i] ) )
			return false;
	return true;
}

std::string Str( const SFlat &f )
{
	return "(" + std::to_string( ( int )f.x ) + "," + std::to_string( ( int )f.y ) + " " + std::to_string( ( int )f.w ) + "x" + std::to_string( ( int )f.h ) + " flag " + std::to_string( f.nFlag ) + ")";
}

std::vector<fs::path> Xmls( const fs::path &dir )
{
	std::vector<fs::path> files;
	std::error_code error;
	for ( fs::recursive_directory_iterator it( dir, error ), end; !error && it != end; it.increment( error ) )
		if ( it->is_regular_file() && it->path().extension() == ".xml" )
			files.push_back( it->path() );
	std::sort( files.begin(), files.end() );
	return files;
}

bool Contains( const std::string &szText, const std::string &szNeedle )
{
	return szText.find( szNeedle ) != std::string::npos;
}

}

typedef bool ( *FCheck )( bool, const std::string & );

void RunUiScreenTests( const fs::path &data, const fs::path &scratchRoot, FCheck Check )
{
	const fs::path scratch = scratchRoot / "ui_screen";
	std::error_code error;
	fs::remove_all( scratch, error );
	fs::create_directories( scratch, error );
	std::string szError;

	// 1. Every shipped screen and template.
	int nScreens = 0, nTemplates = 0, nWindows = 0;
	bool bRoundTrip = true, bAgree = true;
	int nUnpositioned = 0;
	std::string szFirstBad;
	for ( const char *pszDir : { "UI", "Editor/UI" } )
		for ( const fs::path &file : Xmls( data / pszDir ) )
		{
			std::string szText;
			ReadBytes( file, &szText );
			const bool bScreen = std::string( pszDir ) == "UI";
			CUiScreen screen;
			if ( !screen.Open( szText, file.filename().string(), szError ) || screen.Save() != szText )
			{
				bRoundTrip = false;
				szFirstBad = szFirstBad.empty() ? file.string() + " " + szError : szFirstBad;
				continue;
			}
			++( bScreen ? nScreens : nTemplates );
			nWindows += static_cast<int>( screen.Windows().size() );
			if ( bScreen && !screen.Validate( szError ) )
				++nUnpositioned;
			std::vector<SFlat> engine;
			if ( !EngineRead( file, "base", &engine ) || !ModelEqualsEngine( screen, engine ) )
			{
				bAgree = false;
				szFirstBad = szFirstBad.empty() ? file.string() + " model and engine windows differ" : szFirstBad;
			}
		}
	Check( nScreens > 0 && nTemplates > 0 && bRoundTrip, "ui_screen: " + std::to_string( nScreens ) + " Data/UI screens and " + std::to_string( nTemplates ) + " Data/Editor/UI templates open and an unedited save is byte-identical " + szFirstBad );
	Check( bAgree, "ui_screen: the model's " + std::to_string( nWindows ) + " windows equal what the engine's reader sees in every one of them " + szFirstBad );
	Check( nUnpositioned < nScreens, "ui_screen: Validate flags " + std::to_string( nUnpositioned ) + " of " + std::to_string( nScreens ) + " shipped screens for a window without WindowPos (placed by PositionFlag alone), the rest are fully positioned" );

	// 2. Edits on a copy of MainMenu.xml.
	const fs::path mainMenu = data / "UI" / "MainMenu.xml";
	std::string szOriginal;
	ReadBytes( mainMenu, &szOriginal );
	const fs::path edited = scratch / "MainMenu.xml";
	std::vector<SFlat> before;
	EngineRead( mainMenu, "base", &before );
	CUiScreen screen;
	if ( !Check( screen.Open( szOriginal, "MainMenu.xml", szError ), "ui_screen: MainMenu.xml opens " + szError ) )
		return;
	// Three leaf controls under the root with a position and a size: move, resize, delete.
	std::vector<int> leaves;
	for ( size_t i = 1; i < screen.Windows().size() && leaves.size() < 3; ++i )
	{
		const SUiWindow &w = screen.Windows()[i];
		const bool bLeaf = i + 1 == screen.Windows().size() || screen.Windows()[i + 1].nParent != w.nId;
		if ( w.nParent == 0 && bLeaf && w.bHasPos && w.bHasSize )
			leaves.push_back( static_cast<int>( i ) );
	}
	if ( !Check( leaves.size() == 3, "ui_screen: MainMenu.xml has three leaf controls to edit" ) )
		return;
	const SUiWindow moved = screen.Windows()[leaves[0]], resized = screen.Windows()[leaves[1]], deleted = screen.Windows()[leaves[2]];
	const size_t nCount = screen.Windows().size();
	Check( screen.SetRect( moved.nId, moved.nPositionFlag, moved.x + 10, moved.y + 5, moved.w, moved.h, szError ), "ui_screen: move " + szError );
	Check( screen.SetRect( resized.nId, 0x0012, resized.x, resized.y, resized.w + 7, resized.h + 3, szError ), "ui_screen: resize " + szError );
	Check( screen.Windows()[leaves[0]].x == moved.x + 10 && screen.Windows()[leaves[0]].y == moved.y + 5 && screen.Windows()[leaves[0]].nId == moved.nId,
	       "ui_screen: the model shows the move at the same id" );
	std::string szButton;
	ReadBytes( data / "Editor" / "UI" / "Buttons" / "Button00.xml", &szButton );
	const int nNew = screen.InsertFromTemplate( 0, szButton, 300, 200, szError );
	Check( nNew > 0 && screen.Windows().size() == nCount + 1 && screen.Find( nNew ) != nullptr && screen.Find( nNew )->x == 300 && screen.Find( nNew )->y == 200 &&
	       screen.Find( nNew )->w == 71 && screen.Find( nNew )->nParent == 0,
	       "ui_screen: Button00 inserted under the root at 300,200 with its template size " + szError );
	Check( screen.Delete( { deleted.nId }, szError ) && screen.Windows().size() == nCount && screen.Find( deleted.nId ) == nullptr && screen.Find( nNew ) != nullptr,
	       "ui_screen: delete removes the control and keeps the other ids " + szError );
	Check( WriteBytes( edited, screen.Save() ), "ui_screen: the edited MainMenu.xml is written" );

	std::vector<SFlat> after;
	const bool bRead = EngineRead( edited, "base", &after );
	std::vector<SFlat> expected = before;
	expected[leaves[0]].x += 10;
	expected[leaves[0]].y += 5;
	expected[leaves[1]].w += 7;
	expected[leaves[1]].h += 3;
	expected[leaves[1]].nFlag = 0x0012;
	expected.erase( expected.begin() + leaves[2] );
	const SFlat button = { 300, 200, 71, 93, 17 };
	expected.push_back( button );
	Check( bRead && after.size() == before.size() && after == std::vector<SFlat>( expected ) && ModelEqualsEngine( screen, after ) == true,
	       "ui_screen: the engine reads exactly the move, the resize+flag, the inserted Button00 and the delete (" + std::to_string( before.size() ) + " windows before, " +
	       std::to_string( after.size() ) + " after) moved " + Str( before[leaves[0]] ) + "->" + Str( after[leaves[0]] ) + " resized " + Str( before[leaves[1]] ) + "->" + Str( after[leaves[1]] ) );
	{
		std::string szSaved = screen.Save();
		Check( szSaved.size() > 0 && Contains( szSaved, "<!--console-->" ) && Contains( szSaved, "<!-- edited with XML Spy" ), "ui_screen: comments survive the edits" );
	}

	// Copy and paste: the moved window and the new button, shifted by 20,30.
	{
		const std::string szClip = screen.CopyText( { moved.nId, nNew, 0 } );
		std::vector<int> pasted;
		const size_t nBefore = screen.Windows().size();
		Check( screen.Paste( 0, szClip, 20, 30, &pasted, szError ) && pasted.size() == 2 && screen.Windows().size() == nBefore + 2 &&
		       screen.Find( pasted[0] )->x == screen.Find( moved.nId )->x + 20 && screen.Find( pasted[1] )->y == screen.Find( nNew )->y + 30,
		       "ui_screen: copy and paste adds both windows shifted by 20,30 " + szError );
		std::vector<SFlat> engine;
		WriteBytes( scratch / "pasted.xml", screen.Save() );
		Check( EngineRead( scratch / "pasted.xml", "base", &engine ) && ModelEqualsEngine( screen, engine ) && engine.size() == nBefore + 2,
		       "ui_screen: the engine reads the pasted windows where the model says" );
		Check( !screen.Paste( 0, "<other/>", 0, 0, nullptr, szError ) && Contains( szError, "clipboard" ), "ui_screen: a clipboard that is not windows is refused: " + szError );
		Check( screen.Delete( pasted, szError ) && screen.Windows().size() == nBefore, "ui_screen: the pasted windows delete again" );
	}

	// A leaf has no Children: inserting creates them; a window without WindowPos gets both.
	{
		CUiScreen bare;
		const std::string szBare = "<base>\r\n\t<Children>\r\n\t\t<item ClassTypeID=\"1\"/>\r\n\t\t<item ClassTypeID=\"2\"></item>\r\n\t</Children>\r\n</base>";
		Check( bare.Open( szBare, "bare.xml", szError ) && bare.Windows().size() == 3 && !bare.Windows()[1].bHasPos && !bare.Validate( szError ) && Contains( szError, "bare.xml:3:" ) && Contains( szError, "WindowPos" ),
		       "ui_screen: Validate names the file, line and WindowPos of a window without one: " + szError );
		Check( bare.SetRect( bare.Windows()[1].nId, 0x0011, 4, 5, 6, 7, szError ) && bare.Windows()[1].bHasPos && bare.Windows()[1].w == 6, "ui_screen: SetRect creates WindowPos and WindowSize " + szError );
		const int nUnder = bare.InsertFromTemplate( bare.Windows()[1].nId, szButton, 1, 2, szError );
		const int nUnder2 = bare.InsertFromTemplate( bare.Windows()[2].nId, szButton, 3, 4, szError );
		Check( nUnder > 0 && nUnder2 > 0 && bare.Find( nUnder )->nParent == bare.Windows()[1].nId && bare.Find( nUnder2 )->nParent == bare.Windows()[2].nId && bare.Windows().size() == 5,
		       "ui_screen: inserting under a self-closing and an empty element makes their Children " + szError );
		WriteBytes( scratch / "bare.xml", bare.Save() );
		std::vector<SFlat> engine;
		Check( EngineRead( scratch / "bare.xml", "base", &engine ) && ModelEqualsEngine( bare, engine ) && engine.size() == 5 && engine[1].w == 6 && engine[2].x == 1,
		       "ui_screen: the engine reads the created Children and positions: " + bare.Save() );
	}

	// 3. The composer root, and putting a window back.
	{
		std::string szComposer = szOriginal;
		const size_t nOpen = szComposer.find( "<base " ), nClose = szComposer.rfind( "</base>" );
		szComposer.replace( nClose, 7, "</GUI_Composer_Project>" );
		szComposer.replace( nOpen, 5, "<GUI_Composer_Project" );
		CUiScreen composer;
		Check( composer.Open( szComposer, "composer.xml", szError ) && composer.RootName() == "GUI_Composer_Project" && composer.Save() == szComposer, "ui_screen: a GUI_Composer_Project root opens and saves as itself " + szError );
		const std::string szBase = composer.SaveAsBase();
		WriteBytes( scratch / "composer.xml", szComposer );
		WriteBytes( scratch / "as_base.xml", szBase );
		std::vector<SFlat> engineBase, engineComposer;
		Check( Contains( szBase, "<base " ) && Contains( szBase, "</base>" ) && !Contains( szBase, "GUI_Composer_Project" ) && szBase == szOriginal &&
		       EngineRead( scratch / "as_base.xml", "base", &engineBase ) && engineBase == before &&
		       EngineRead( scratch / "composer.xml", "GUI_Composer_Project", &engineComposer ) && engineComposer == before,
		       "ui_screen: SaveAsBase gives the original <base> screen, which the engine reads the same as the composer root" );

		CUiScreen back;
		back.Open( szOriginal, "MainMenu.xml", szError );
		const SUiWindow w = back.Windows()[leaves[0]];
		back.SetRect( w.nId, 0x0020, w.x + 3, w.y + 3, w.w + 4, w.h + 4, szError );
		const bool bChanged = back.Save() != szOriginal;
		back.SetRect( w.nId, w.nPositionFlag, w.x, w.y, w.w, w.h, szError );
		Check( bChanged && back.Save() == szOriginal, "ui_screen: setting a window's old rect back restores the original bytes" );
	}

	// 4. Errors name the file, line and reason.
	{
		struct { const char *pszText, *pszNeedle; const char *pszLine; } bad[] = {
			{ "<base>\r\n<Children>\r\n<item>\r\n</Children>\r\n</base>", "unbalanced element: </Children> closes <item> opened at line 3", ":4:" },
			{ "<base>\r\n<a>", "unbalanced element: <a> is never closed", ":2:" },
			{ "<base>\r\n</base>\r\n</base>", "unbalanced element", ":3:" },
			{ "<screen>\r\n</screen>", "unknown root <screen>", ":1:" },
			{ "<base>\r\n<!-- never ends\r\n</base>", "unterminated comment", ":2:" },
			{ "<base>\r\n<item x=1/>\r\n</base>", "no quoted value", ":2:" },
			{ "<base>\r\n<item x \"1\"/>\r\n</base>", "no '='", ":2:" },
			{ "<base/>\r\nstray", "text outside the root", ":2:" },
			{ "", "no root element", ":1:" },
		};
		bool bAll = true;
		std::string szWhat;
		for ( const auto &b : bad )
		{
			CUiScreen s;
			const bool bOpened = s.Open( b.pszText, "bad.xml", szError );
			if ( bOpened || !Contains( szError, std::string( "bad.xml" ) + b.pszLine ) || !Contains( szError, b.pszNeedle ) )
			{
				bAll = false;
				szWhat += std::string( "[" ) + b.pszNeedle + " -> " + szError + "] ";
			}
		}
		Check( bAll, "ui_screen: malformed input fails with the file, the line and the reason " + szWhat );
		CUiScreen s;
		s.Open( szOriginal, "MainMenu.xml", szError );
		Check( !s.Delete( { 0 }, szError ) && Contains( szError, "root" ), "ui_screen: the root cannot be deleted: " + szError );
		Check( !s.SetRect( 9999, 0, 0, 0, 0, 0, szError ) && Contains( szError, "9999" ), "ui_screen: an unknown id is refused: " + szError );
		Check( s.InsertFromTemplate( 0, "<GUI_Composer_Project/>", 0, 0, szError ) < 0 && Contains( szError, "<base>" ), "ui_screen: a template that is not <base> is refused: " + szError );
		Check( s.Save() == szOriginal, "ui_screen: refused edits leave the screen untouched" );
	}
}
