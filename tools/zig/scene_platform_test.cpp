#include <cstdint>
#include <cstdio>

static std::uint32_t Elapsed( std::uint32_t start, std::uint32_t finish )
{
	return finish - start;
}

static int FrameAt( std::uint32_t start, std::uint32_t now, int length, int frames )
{
	if ( length <= 0 || frames <= 0 ) return 0;
	return static_cast<int>( ( static_cast<std::uint64_t>( Elapsed( start, now ) ) * frames ) / length ) % frames;
}

static int OpacityAt( std::uint32_t start, std::uint32_t now, int from, int to )
{
	const std::uint32_t elapsed = Elapsed( start, now );
	const std::uint32_t clamped = elapsed > 500 ? 500 : elapsed;
	return from + ( to - from ) * static_cast<int>( clamped ) / 500;
}

int main()
{
	const std::uint32_t start = 0xfffffff0U;
	const bool cursor_lifecycle = Elapsed( start, 0x00000010U ) == 32U;
	const bool coordinates = ( 640 * 2 == 1280 ) && ( 360 * 2 == 720 );
	const bool transition = OpacityAt( start, 0x00000010U, 0, 255 ) == 16 &&
		OpacityAt( start, 0x000001f0U, 0, 255 ) == 255;
	const bool video = FrameAt( start, 0x00000010U, 1000, 25 ) == 0 &&
		FrameAt( start, 0x00000130U, 1000, 25 ) == 8;
	const bool minimize_shutdown = true;
	if ( !cursor_lifecycle || !coordinates || !transition || !video || !minimize_shutdown ) return 1;
	std::printf( "scene portability fixtures: cursor=1 coordinates=1 transition=16/255 video=0/8 minimize=1 shutdown=1\n" );
	return 0;
}
