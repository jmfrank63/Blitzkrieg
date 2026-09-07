#include "../../Sources/src/Input/InputCodes.h"

#include <cstdio>
#include <fstream>
#include <string>

#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "input code check failed: %s\n", #value); return 1; } } while (false)

int main()
{
    std::size_t count = 0;
    const NInput::KeyCodeEntry *entries = NInput::KeyboardCodes( &count );
    CHECK( entries != nullptr && count > 70 );
    for ( std::size_t i = 0; i != count; ++i ) {
        CHECK( entries[i].name != nullptr && entries[i].code != 0 );
        CHECK( NInput::CodeForName( entries[i].name ) == entries[i].code );
        CHECK( std::string( NInput::NameForCode( entries[i].code ) ) == entries[i].name );
        for ( std::size_t j = i + 1; j != count; ++j ) CHECK( entries[i].code != entries[j].code );
    }
    CHECK( NInput::CodeForName( "UNKNOWN_INPUT" ) == 0 );
    CHECK( std::string( NInput::NameForCode( 0 ) ) == "UNKNOWN_KEY" );
    CHECK( NInput::SDLScancodeToLegacy( 4 ) == NInput::CodeForName( "A" ) );
    CHECK( NInput::SDLScancodeToLegacy( 41 ) == NInput::CodeForName( "ESC" ) );
    CHECK( NInput::SDLScancodeToLegacy( 40 ) == NInput::CodeForName( "ENTER" ) );

    // The keys the incomplete table used to drop: the digits behind Ctrl+1..9
    // unit groups, game speed on -/=, Tab, and the function keys.
    static const struct { std::uint32_t scancode; const char *name; } named[] = {
        { 30, "1" }, { 31, "2" }, { 38, "9" }, { 39, "0" },
        { 43, "TAB" }, { 42, "BACKSPACE" }, { 45, "-" }, { 46, "=" },
        { 58, "F1" }, { 67, "F10" }, { 68, "F11" }, { 69, "F12" },
        { 79, "RIGHT" }, { 80, "LEFT" }, { 81, "DOWN" }, { 82, "UP" },
        { 74, "HOME" }, { 77, "END" }, { 76, "DELETE" }, { 73, "INSERT" },
        { 89, "NUM_1" }, { 95, "NUM_7" }, { 98, "NUM_0" }, { 88, "NUM_ENTER" },
        { 224, "LCTRL" }, { 228, "RCTRL" }, { 225, "LSHIFT" }, { 53, "`" },
#if defined(__APPLE__)
        // Command is the modifier a Mac player reaches for, and the configs bind
        // LCTRL/RCTRL for the unit groups. Control still works alongside it.
        { 227, "LCTRL" }, { 231, "RCTRL" },
#else
        { 227, "LWIN" }, { 231, "RWIN" },
#endif
    };
    for ( const auto &entry : named )
        CHECK( NInput::SDLScancodeToLegacy( entry.scancode ) == NInput::CodeForName( entry.name ) );

    // Every legacy code the table produces must be a control the device
    // registers, or the key resolves to nothing once it reaches the bindings.
    for ( std::uint32_t scancode = 0; scancode != 256; ++scancode ) {
        const std::uint32_t legacy = NInput::SDLScancodeToLegacy( scancode );
        if ( legacy == 0 ) continue;
        CHECK( std::string( NInput::NameForCode( legacy ) ) != "UNKNOWN_KEY" );
    }

    // Virtual keys are what the screens switch on; 192 is the console toggle and
    // the modifiers must arrive side-independent, as MapVirtualKeyEx reported.
    CHECK( NInput::SDLScancodeToVirtualKey( 43 ) == 0x09 );   // VK_TAB
    CHECK( NInput::SDLScancodeToVirtualKey( 40 ) == 0x0d );   // VK_RETURN
    CHECK( NInput::SDLScancodeToVirtualKey( 42 ) == 0x08 );   // VK_BACK
    CHECK( NInput::SDLScancodeToVirtualKey( 76 ) == 0x2e );   // VK_DELETE
    CHECK( NInput::SDLScancodeToVirtualKey( 80 ) == 0x25 );   // VK_LEFT
    CHECK( NInput::SDLScancodeToVirtualKey( 41 ) == 0x1b );   // VK_ESCAPE
    CHECK( NInput::SDLScancodeToVirtualKey( 4 ) == 0x41 );    // 'A'
    CHECK( NInput::SDLScancodeToVirtualKey( 30 ) == 0x31 );   // '1'
    CHECK( NInput::SDLScancodeToVirtualKey( 39 ) == 0x30 );   // '0'
    CHECK( NInput::SDLScancodeToVirtualKey( 58 ) == 0x70 );   // VK_F1
    CHECK( NInput::SDLScancodeToVirtualKey( 69 ) == 0x7b );   // VK_F12
    CHECK( NInput::SDLScancodeToVirtualKey( 53 ) == 192 );    // VK_OEM_3
    CHECK( NInput::SDLScancodeToVirtualKey( 224 ) == NInput::SDLScancodeToVirtualKey( 228 ) );
    CHECK( NInput::SDLScancodeToVirtualKey( 225 ) == NInput::SDLScancodeToVirtualKey( 229 ) );
    CHECK( NInput::SDLScancodeToVirtualKey( 226 ) == NInput::SDLScancodeToVirtualKey( 230 ) );
#if defined(__APPLE__)
    CHECK( NInput::SDLScancodeToVirtualKey( 227 ) == 0x11 );  // VK_CONTROL
    CHECK( NInput::SDLScancodeToVirtualKey( 231 ) == 0x11 );
#endif
    CHECK( NInput::SDLScancodeToVirtualKey( 0 ) == 0 );

    // A key the bindings can reach must also reach the screens, so the two
    // tables have to agree on which scancodes exist at all.
    for ( std::uint32_t scancode = 0; scancode != 256; ++scancode )
        if ( NInput::SDLScancodeToLegacy( scancode ) != 0 )
            CHECK( NInput::SDLScancodeToVirtualKey( scancode ) != 0 );

    std::ifstream config( "Data/Configs/defconf.cfg" );
    CHECK( config.good() );
    const char *required[] = { "ESC", "LCTRL", "MOUSE_BUTTON0", "MOUSE_AXIS_X", "MOUSE_AXIS_Z" };
    std::string line;
    std::string contents;
    while ( std::getline( config, line ) ) contents += line + '\n';
    CHECK( contents.find( "<item>ESC</item>" ) != std::string::npos );
    CHECK( contents.find( "<item>LCTRL</item>" ) != std::string::npos );
    CHECK( contents.find( "MOUSE_BUTTON0" ) != std::string::npos );
    CHECK( contents.find( "MOUSE_AXIS_X" ) != std::string::npos );
    CHECK( contents.find( "MOUSE_AXIS_Z" ) != std::string::npos );
    // An edit box only inserts a character, so a printable key has to carry one
    // even when no SDL text event follows -- that is what left the save game
    // name field impossible to type into.
    CHECK( NInput::CharacterFromKeycode( 'a', 0 ) == 'a' );
    CHECK( NInput::CharacterFromKeycode( 'a', 0x0002 ) == 'A' );
    CHECK( NInput::CharacterFromKeycode( 'a', 0x2000 ) == 'A' );
    CHECK( NInput::CharacterFromKeycode( 'a', 0x2000 | 0x0001 ) == 'a' );
    CHECK( NInput::CharacterFromKeycode( '7', 0 ) == '7' );
    CHECK( NInput::CharacterFromKeycode( ' ', 0 ) == ' ' );
    CHECK( NInput::CharacterFromKeycode( '-', 0 ) == '-' );
    // Shifted punctuation is layout dependent, so it defers to the text event.
    CHECK( NInput::CharacterFromKeycode( '7', 0x0001 ) == 0 );
    // Non printable keys must stay silent or an edit box would insert controls.
    CHECK( NInput::CharacterFromKeycode( 13, 0 ) == 0 );
    CHECK( NInput::CharacterFromKeycode( 8, 0 ) == 0 );
    CHECK( NInput::CharacterFromKeycode( 0x40000050, 0 ) == 0 );

    // The paste chord: Ctrl+V or Cmd+V (either side), shift tolerated, but
    // never Alt -- AltGr arrives as Ctrl+Alt on Windows layouts and types a
    // character, which a paste must not eat. SDL keycodes are unshifted, so
    // the key is always lowercase 'v'.
    CHECK( NInput::IsPasteChord( 'v', 0x0040 ) );              // LCTRL
    CHECK( NInput::IsPasteChord( 'v', 0x00c0 ) );              // both ctrls
    CHECK( NInput::IsPasteChord( 'v', 0x0400 ) );              // LGUI (Cmd)
    CHECK( NInput::IsPasteChord( 'v', 0x0800 ) );              // RGUI
    CHECK( NInput::IsPasteChord( 'v', 0x0001 | 0x0400 ) );     // Shift+Cmd+V
    CHECK( !NInput::IsPasteChord( 'v', 0 ) );                  // plain v types
    CHECK( !NInput::IsPasteChord( 'v', 0x0040 | 0x0100 ) );    // AltGr combo
    CHECK( !NInput::IsPasteChord( 'v', 0x0200 ) );             // Alt alone
    CHECK( !NInput::IsPasteChord( 'c', 0x0040 ) );             // not the V key
    CHECK( !NInput::IsPasteChord( 'v', 0x2000 ) );             // caps lock only

    // Clipboard text is destined for single-line edit boxes: control bytes
    // (a password manager's trailing newline above all) are stripped, UTF-8
    // passes through, and a runaway clipboard is capped on a character
    // boundary so the decoder never sees a torn sequence.
    CHECK( NInput::SanitizeClipboardText( "sOXJ-08 pass" ) == "sOXJ-08 pass" );
    CHECK( NInput::SanitizeClipboardText( "secret\n" ) == "secret" );
    CHECK( NInput::SanitizeClipboardText( "a\r\nb\tc" ) == "abc" );
    CHECK( NInput::SanitizeClipboardText( "" ).empty() );
    CHECK( NInput::SanitizeClipboardText( "gr\xc3\xbc\xc3\x9f" "e" ) == "gr\xc3\xbc\xc3\x9f" "e" );
    {
        std::string huge( 5000, 'x' );
        huge += "\xc3\xbc";
        const std::string capped = NInput::SanitizeClipboardText( huge );
        CHECK( capped.size() <= 4096 );
        CHECK( ( static_cast<unsigned char>( capped[capped.size() - 1] ) & 0xc0 ) != 0x80 );
    }
    std::string boundary( 4095, 'x' );
    boundary += "\xc3\xbc";        // two-byte char straddles the cap
    CHECK( NInput::SanitizeClipboardText( boundary ) == std::string( 4095, 'x' ) );

    (void)required;
    std::puts( "portable input code mapping passed" );
    return 0;
}
