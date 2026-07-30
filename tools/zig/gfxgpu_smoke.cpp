#include <SDL3/SDL.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

static std::atomic<bool> smoke_finished{ false };

static void startSmokeWatchdog()
{
    const char *timeout_text = std::getenv( "GFXGPU_SMOKE_TIMEOUT_SECONDS" );
    const unsigned timeout_seconds = timeout_text != nullptr ? static_cast<unsigned>( std::strtoul( timeout_text, nullptr, 10 ) ) : 15u;
    std::thread( [timeout_seconds]() {
        std::this_thread::sleep_for( std::chrono::seconds( timeout_seconds ) );
        if ( !smoke_finished.load() )
        {
            std::fprintf( stderr, "SDL smoke watchdog: timeout after %u seconds\n", timeout_seconds );
            std::fflush( stderr );
            TerminateProcess( GetCurrentProcess(), 124 );
        }
    } ).detach();
}

int main()
{
    std::printf( "SDL smoke: entered main\n" );
    std::fflush( stdout );
    startSmokeWatchdog();
    const char *requested_driver = std::getenv( "SDL_VIDEO_DRIVER" );
    if ( requested_driver != nullptr )
        std::printf( "SDL requested video driver: %s\n", requested_driver );
    std::printf( "SDL smoke: initializing video\n" );
    std::fflush( stdout );
    if ( !SDL_Init( SDL_INIT_VIDEO ) )
    {
        std::fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
        return 1;
    }

    std::printf( "SDL smoke: video initialized; active driver: %s\n", SDL_GetCurrentVideoDriver() );
    std::fflush( stdout );

    SDL_WindowFlags window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
    if ( std::getenv( "GFXGPU_SMOKE_VISIBLE" ) != nullptr ) window_flags &= ~SDL_WINDOW_HIDDEN;
    if ( std::getenv( "GFXGPU_SMOKE_NO_RESIZE" ) != nullptr ) window_flags &= ~SDL_WINDOW_RESIZABLE;
    std::printf( "SDL smoke: creating window with flags 0x%llx\n", static_cast<unsigned long long>( window_flags ) );
    std::fflush( stdout );
    SDL_Window *window = SDL_CreateWindow( "Blitzkrieg GfxGpu smoke", 320, 200, window_flags );
    if ( window == nullptr )
    {
        std::fprintf( stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
        SDL_Quit();
        return 2;
    }

    std::printf( "SDL revision: %s\n", SDL_GetRevision() );
    SDL_DestroyWindow( window );
    SDL_Quit();
    smoke_finished.store( true );
    return 0;
}
