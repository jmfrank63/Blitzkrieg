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
    (void)required;
    std::puts( "portable input code mapping passed" );
    return 0;
}
