#include "../../Sources/src/Platform/Event.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#define CHECK(condition) \
	do { \
		if ( !(condition) ) { \
			std::fprintf( stderr, "platform display contract check failed: %s\n", #condition ); \
			return false; \
		} \
	} while ( false )

namespace
{
struct DisplayOptions
{
	std::int32_t width = 1024;
	std::int32_t height = 768;
	std::int32_t bpp = 16;
	std::int32_t stencil = 0;
	std::int32_t fullscreen = 0;
	std::int32_t frequency = 0;
};

void PutI32( std::vector<std::uint8_t> &bytes, const std::int32_t value )
{
	const std::uint32_t raw = static_cast<std::uint32_t>( value );
	for ( unsigned shift = 0; shift != 32; shift += 8 )
		bytes.push_back( static_cast<std::uint8_t>( raw >> shift ) );
}

std::vector<std::uint8_t> SerializeDisplayOptions( const DisplayOptions &options )
{
	// Preserve the legacy GFX.Mode scalar order and four-byte little-endian
	// integer representation: SizeX, SizeY, BPP, Stencil, FullScreen, Frequency.
	std::vector<std::uint8_t> bytes;
	bytes.reserve( 6 * sizeof( std::int32_t ) );
	PutI32( bytes, options.width );
	PutI32( bytes, options.height );
	PutI32( bytes, options.bpp );
	PutI32( bytes, options.stencil );
	PutI32( bytes, options.fullscreen );
	PutI32( bytes, options.frequency );
	return bytes;
}

class RecordingRenderer
{
public:
	void Resize( const NPlatform::PlatformEvent &event )
	{
		resize_count++;
		width = event.x;
		height = event.y;
		trace.push_back( "renderer-resize" );
	}

	int resize_count = 0;
	int width = 0;
	int height = 0;
	std::vector<std::string> trace;
};

class HeadlessResizeBoundary
{
public:
	void EnqueueResize( const std::uint64_t timestamp, const int width, const int height )
	{
		NPlatform::PlatformEvent event{};
		event.type = NPlatform::EventType::windowResized;
		event.timestamp = timestamp;
		event.x = width;
		event.y = height;
		events.push_back( event );
	}

	bool Pump( RecordingRenderer &renderer )
	{
		for ( const NPlatform::PlatformEvent &event : events )
		{
			if ( event.type != NPlatform::EventType::windowResized ) continue;
			CHECK( renderer.resize_count == 0 );
			renderer.trace.push_back( "normalized-event" );
			observed_width = event.x;
			observed_height = event.y;
			renderer.Resize( event );
		}
		events.clear();
		return true;
	}

	int observed_width = 0;
	int observed_height = 0;

private:
	std::vector<NPlatform::PlatformEvent> events;
};

bool TestResizeOrdering()
{
	HeadlessResizeBoundary boundary;
	RecordingRenderer renderer;
	boundary.EnqueueResize( 42, 1280, 720 );
	CHECK( boundary.Pump( renderer ) );
	CHECK( boundary.observed_width == 1280 && boundary.observed_height == 720 );
	CHECK( renderer.width == 1280 && renderer.height == 720 );
	CHECK( renderer.resize_count == 1 );
	CHECK( renderer.trace.size() == 2 );
	CHECK( renderer.trace[0] == "normalized-event" );
	CHECK( renderer.trace[1] == "renderer-resize" );
	return true;
}

bool TestDisplayOptionBytes()
{
	const DisplayOptions options{ 1024, 768, 16, 0, 1, 60 };
	const std::array<std::uint8_t, 24> accepted_windows_bytes = {
		0x00, 0x04, 0x00, 0x00,
		0x00, 0x03, 0x00, 0x00,
		0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x3c, 0x00, 0x00, 0x00,
	};
	const std::vector<std::uint8_t> actual = SerializeDisplayOptions( options );
	CHECK( actual.size() == accepted_windows_bytes.size() );
	CHECK( std::equal( actual.begin(), actual.end(), accepted_windows_bytes.begin() ) );
	return true;
}
}

int main()
{
	if ( !TestResizeOrdering() || !TestDisplayOptionBytes() ) return 1;
	std::puts( "platform display contract passed: normalized resize precedes renderer resize and bytes are stable" );
	return 0;
}
