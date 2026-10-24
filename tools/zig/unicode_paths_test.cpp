// Cyrillic and CJK names through the game's own file helpers - NFile
// (Misc/FileUtils), the UTF-8 path helpers (Platform/Utf8Path.h) and the
// profile directories (StreamIO/ProfileNames.h). A Russian player's profile
// "Иван" used to be cut down to nothing and silently became "Player"; on
// Windows every narrow path call also read the UTF-8 bytes in the ANSI code
// page. Runs on every platform; Windows is where the file half matters.
#include "Misc/FileUtils.h"
#include "Platform/Utf8Path.h"
#include "StreamIO/ProfileNames.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <list>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#define CHECK( condition ) \
	do { if ( !( condition ) ) { std::fprintf( stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition ); std::exit( 1 ); } } while ( 0 )

static const std::string IVAN = u8"Иван";
static const std::string PETR = u8"Пётр";
static const std::string PETR_UPPER = u8"ПЁТР";
static const std::string YAMADA = u8"山田太郎";

static bool Contains( const std::vector<std::string> &names, const std::string &name )
{
	for ( const std::string &entry : names )
		if ( entry == name )
			return true;
	return false;
}

static void CheckNames()
{
	// What the player typed is the profile, kept as typed.
	CHECK( NProfile::NameFromTyped( std::u16string( u"Иван" ) ) == IVAN );
	CHECK( NProfile::NameFromTyped( std::u16string( u"山田太郎" ) ) == YAMADA );
	CHECK( NProfile::NameFromTyped( std::u16string( u"  Иван " ) ) == IVAN );
	CHECK( NProfile::NameFromTyped( std::u16string( u"Player" ) ) == "Player" );
	// A non-BMP letter arrives as a surrogate pair and stays one character.
	CHECK( NProfile::NameFromTyped( std::u16string( u"\U00020000x" ) ) == u8"\U00020000x" );
	CHECK( NProfile::DisplayName( IVAN ) == std::wstring( L"\x0418\x0432\x0430\x043d" ) );
	std::string roundTrip;
	NPlatform::Utf8FromUtf16( &roundTrip, NProfile::DisplayName( YAMADA ) );
	CHECK( roundTrip == YAMADA );

	// What no folder may be called is refused, never mapped to another name.
	const char16_t *refused[] = { u"", u"   ", u"a/b", u"a\\b", u"a:b", u"a*b", u"a?b", u"a\"b", u"a<b", u"a>b", u"a|b",
		u"a\tb", u"name.", u"..", u"CON", u"con", u"Nul.txt", u"LPT1", u"com9", u"COM¹", u"\xd800x" };
	for ( const char16_t *name : refused )
		CHECK( NProfile::NameFromTyped( std::u16string( name ) ).empty() );
	CHECK( !NProfile::NameFromTyped( std::u16string( u"COM10" ) ).empty() );
	CHECK( !NProfile::NameFromTyped( std::u16string( u"Console" ) ).empty() );
	CHECK( !NProfile::IsValidName( std::string( NProfile::MAX_NAME_BYTES + 1, 'a' ) ) );
	CHECK( !NProfile::IsValidName( "\xd0" ) );

	// The command line and active.cfg still go through Sanitize: every name an
	// existing ASCII profile can have is unchanged, and Cyrillic now survives.
	const char *ascii[] = { "Player", "Bob 2", "Hans-Peter_1", "MapEditorTest", "a.b" };
	for ( const char *name : ascii )
		CHECK( NProfile::Sanitize( name ) == name );
	CHECK( NProfile::Sanitize( IVAN ) == IVAN );
	CHECK( NProfile::Sanitize( YAMADA + "\r" ) == YAMADA );
	CHECK( NProfile::Sanitize( "a:b" ) == "ab" );
	CHECK( NProfile::Sanitize( " x. " ) == "x" );
	CHECK( NProfile::Sanitize( "\xd0\x98\xd0" ) == u8"И" );
	CHECK( NProfile::Sanitize( "" ) == "Player" );
	CHECK( NProfile::Sanitize( "CON" ) == "Player" );

	// One directory on NTFS and APFS, so one profile.
	CHECK( NProfile::NameEquals( "Player", "pLAYER" ) );
	CHECK( NProfile::NameEquals( PETR, PETR_UPPER ) );
	CHECK( NProfile::NameEquals( u8"Ärger", u8"ärger" ) );
	CHECK( !NProfile::NameEquals( IVAN, PETR ) );
	CHECK( NProfile::NameLess( "Anna", IVAN ) );
	CHECK( NProfile::NameLess( IVAN, u8"иванов" ) );
	CHECK( !NProfile::NameLess( u8"иванов", IVAN ) );
}

static void CheckFileHelpers( const std::string &root )
{
	// Folders and files named in Cyrillic and CJK, made, listed, opened,
	// renamed and deleted through NFile only.
	const std::string dir = root + "/" + u8"Моды" + "/" + YAMADA;
	NFile::CreatePath( dir.c_str() );
	CHECK( fs::is_directory( NPlatform::PathFromUtf8( dir ) ) );

	const std::string file = dir + "/" + u8"сохранение 1.sav";
	{
		NFile::CFile out( file.c_str(), NFile::CFile::modeCreate | NFile::CFile::modeWrite );
		CHECK( out.IsOpen() );
		CHECK( out.Write( "save", 4 ) == 4 );
		CHECK( out.GetFileName() == u8"сохранение 1.sav" );
		CHECK( out.GetFileTitle() == u8"сохранение 1" );
	}
	CHECK( NFile::IsFileExist( file.c_str() ) );
	{
		NFile::CFile in( file.c_str(), NFile::CFile::modeRead );
		CHECK( in.IsOpen() );
		char text[5] = {};
		CHECK( in.Read( text, 4 ) == 4 && std::string( text ) == "save" );
		NFile::CFile::SStatus status;
		CHECK( in.GetStatus( &status ) && status.nSize == 4 );
	}

	NFile::CFileIterator it( ( dir + "/*.sav" ).c_str() );
	CHECK( !it.IsEnd() && it.GetFileName() == u8"сохранение 1.sav" && it.GetLength() == 4 );
	CHECK( it.GetFilePath().find( YAMADA ) != std::string::npos );
	CHECK( NFile::IsFileExist( it.GetFilePath().c_str() ) );
	++it;
	CHECK( it.IsEnd() );

	std::list<std::string> files;
	NFile::GetFileNames( ( root + "/" + u8"Моды" ).c_str(), "*.sav", &files, true );
	CHECK( files.size() == 1 && files.front().find( u8"сохранение 1.sav" ) != std::string::npos );
	std::list<std::string> dirs;
	NFile::GetDirNames( ( root + "/" + u8"Моды" ).c_str(), &dirs, false );
	CHECK( dirs.size() == 1 && dirs.front().find( YAMADA ) != std::string::npos );
	CHECK( NFile::GetFullName( file ).find( u8"Моды" ) != std::string::npos );

	const std::string renamed = dir + "/" + u8"セーブ.sav";
	CHECK( NFile::CFile::Rename( file.c_str(), renamed.c_str() ) );
	CHECK( !NFile::IsFileExist( file.c_str() ) && NFile::IsFileExist( renamed.c_str() ) );
	CHECK( NFile::CFile::Remove( renamed.c_str() ) );
	CHECK( !NFile::IsFileExist( renamed.c_str() ) );

	// The plain helpers, as the rest of the engine reaches them.
	const std::string direct = root + "/" + u8"файл.txt";
	std::FILE *pFile = NPlatform::OpenFileUtf8( direct.c_str(), "wb" );
	CHECK( pFile != nullptr );
	std::fclose( pFile );
	CHECK( NFile::IsFileExist( direct.c_str() ) );
	CHECK( NPlatform::Utf8FromPath( NPlatform::PathFromUtf8( direct ).filename() ) == u8"файл.txt" );

	NFile::DeleteDirectory( ( root + "/" + u8"Моды" ).c_str() );
	CHECK( !fs::exists( NPlatform::PathFromUtf8( dir ) ) );
}

static void CheckProfiles( const std::string &root )
{
	// The profile registry is profiles/ under the working directory.
	const fs::path previous = fs::current_path();
	fs::current_path( NPlatform::PathFromUtf8( root ) );
	NPlatform::Paths::SetInjectedRootsForTest( root.c_str(), ( root + "/user" ).c_str() );

	NFile::CreatePath( ( "profiles/" + IVAN + "/saves" ).c_str() );
	NFile::CreatePath( ( "profiles/" + YAMADA + "/saves" ).c_str() );
	NFile::CreatePath( "profiles/Player/saves" );
	// A folder no profile could be called is not listed (it can exist on
	// every filesystem, so the check runs everywhere).
	const std::string tooLong( NProfile::MAX_NAME_BYTES + 1, 'a' );
	NFile::CreatePath( ( "profiles/" + tooLong ).c_str() );
	std::vector<std::string> names = NProfile::List();
	CHECK( names.size() == 3 && Contains( names, IVAN ) && Contains( names, YAMADA ) && Contains( names, "Player" ) );
	CHECK( names[0] == "Player" && names[1] == IVAN && names[2] == YAMADA );

	std::string error;
	CHECK( NProfile::Rename( IVAN, PETR, &error ) );
	CHECK( fs::is_directory( NProfile::Directory( PETR ) / "saves" ) );
	CHECK( !fs::exists( NProfile::Directory( IVAN ) ) );
	// Only the case changes: a real rename on case-insensitive filesystems too.
	CHECK( NProfile::Rename( PETR, PETR_UPPER, &error ) );
	names = NProfile::List();
	CHECK( Contains( names, PETR_UPPER ) && !Contains( names, PETR ) );
	// Another profile's name, and a name no folder may have, are refused.
	CHECK( !NProfile::Rename( PETR_UPPER, YAMADA, &error ) );
	CHECK( !NProfile::Rename( PETR_UPPER, "CON", &error ) );
	CHECK( fs::is_directory( NProfile::Directory( PETR_UPPER ) ) );

	CHECK( NProfile::Delete( PETR_UPPER, &error ) );
	CHECK( NProfile::Delete( YAMADA, &error ) );
	names = NProfile::List();
	CHECK( names.size() == 1 && names[0] == "Player" );
	NFile::DeleteDirectory( ( "profiles/" + tooLong ).c_str() );
	CHECK( NProfile::GeneratedDirectory( IVAN ).find( IVAN ) != std::string::npos );

	NPlatform::Paths::ClearInjectedRootsForTest();
	fs::current_path( previous );
}

int main()
{
	CheckNames();
	const std::string root = NPlatform::Utf8FromPath( fs::temp_directory_path() ) + "/" + u8"blitzkrieg-юникод-測試";
	fs::remove_all( NPlatform::PathFromUtf8( root ) );
	NFile::CreatePath( root.c_str() );
	CHECK( fs::is_directory( NPlatform::PathFromUtf8( root ) ) );
	CheckFileHelpers( root );
	CheckProfiles( root );
	NFile::DeleteDirectory( root.c_str() );
	CHECK( !fs::exists( NPlatform::PathFromUtf8( root ) ) );
	return 0;
}
