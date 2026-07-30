#include <SDL3/SDL.h>

#include <cstdio>

int main()
{
    std::printf( "SDL smoke: initializing video\n" );
    std::fflush( stdout );
    if ( !SDL_Init( SDL_INIT_VIDEO ) )
    {
        std::fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
        return 1;
    }

    std::printf( "SDL smoke: video initialized\n" );
    std::fflush( stdout );

    SDL_Window *window = SDL_CreateWindow( "Blitzkrieg GfxGpu smoke", 320, 200, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE );
    if ( window == nullptr )
    {
        std::fprintf( stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
        SDL_Quit();
        return 2;
    }

    std::printf( "SDL revision: %s\n", SDL_GetRevision() );
    SDL_DestroyWindow( window );
    SDL_Quit();
    return 0;
}
