#include <SDL3/SDL.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
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

static bool checkBaselineFixtures()
{
    const float position[4] = { 1.0f, -2.0f, 0.5f, 1.0f };
    const float diffuse[4] = { 0.2f, 0.4f, 0.6f, 0.8f };
    const float draw[4] = { 0.5f, 1.0f, 0.25f, 1.0f };
    const float expected_diffuse[4] = { 0.1f, 0.4f, 0.15f, 0.8f };
    const float texture[4] = { 0.5f, 0.25f, 1.0f, 0.5f };
    const float expected_textured[4] = { 0.05f, 0.1f, 0.15f, 0.4f };
    for ( int i = 0; i < 4; ++i )
    {
        if ( position[i] != position[i] ) return false;
        if ( std::fabs( diffuse[i] * draw[i] - expected_diffuse[i] ) > 1e-6f ) return false;
        if ( std::fabs( texture[i] * expected_diffuse[i] - expected_textured[i] ) > 1e-6f ) return false;
    }
    return true;
}

static bool readShader(const char *path, std::vector<unsigned char> &bytes)
{
    std::ifstream file( path, std::ios::binary | std::ios::ate );
    if ( !file ) return false;
    const std::streamsize size = file.tellg();
    if ( size <= 0 ) return false;
    bytes.resize( static_cast<size_t>( size ) );
    file.seekg( 0, std::ios::beg );
    return file.read( reinterpret_cast<char *>( bytes.data() ), size ).good();
}

static bool createBaselineShaders(SDL_GPUDevice *device)
{
    struct ShaderSpec { const char *path; const char *entry; SDL_GPUShaderStage stage; Uint32 samplers; };
    const ShaderSpec specs[] = {
        { "../shaders/untextured.vertex.dxil", "vs_untextured", SDL_GPU_SHADERSTAGE_VERTEX, 0 },
        { "../shaders/untextured.fragment.dxil", "ps_untextured", SDL_GPU_SHADERSTAGE_FRAGMENT, 0 },
        { "../shaders/textured.vertex.dxil", "vs_textured", SDL_GPU_SHADERSTAGE_VERTEX, 1 },
        { "../shaders/textured.fragment.dxil", "ps_textured", SDL_GPU_SHADERSTAGE_FRAGMENT, 1 },
    };
    SDL_GPUShader *shaders[4] = {};
    for ( int i = 0; i < 4; ++i )
    {
        std::vector<unsigned char> code;
        if ( !readShader( specs[i].path, code ) ) return false;
        SDL_GPUShaderCreateInfo info = {};
        info.code_size = code.size();
        info.code = code.data();
        info.entrypoint = specs[i].entry;
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.stage = specs[i].stage;
        info.num_samplers = specs[i].samplers;
        info.num_uniform_buffers = 2;
        shaders[i] = SDL_CreateGPUShader( device, &info );
        if ( shaders[i] == nullptr )
        {
            for ( int j = i - 1; j >= 0; --j ) SDL_ReleaseGPUShader( device, shaders[j] );
            return false;
        }
    }
    for ( int i = 3; i >= 0; --i ) SDL_ReleaseGPUShader( device, shaders[i] );
    return true;
}

int main()
{
    std::printf( "SDL smoke: entered main\n" );
    std::fflush( stdout );
    startSmokeWatchdog();
    if ( !checkBaselineFixtures() )
    {
        std::fprintf( stderr, "baseline shader CPU fixtures failed\n" );
        return 3;
    }
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

    SDL_GPUDevice *device = SDL_CreateGPUDevice( SDL_GPU_SHADERFORMAT_DXIL, false, nullptr );
    if ( device == nullptr )
    {
        std::fprintf( stderr, "SDL_CreateGPUDevice failed: %s\n", SDL_GetError() );
        SDL_DestroyWindow( window );
        SDL_Quit();
        return 4;
    }
    if ( !createBaselineShaders( device ) )
    {
        std::fprintf( stderr, "baseline shader creation failed: %s\n", SDL_GetError() );
        SDL_DestroyGPUDevice( device );
        SDL_DestroyWindow( window );
        SDL_Quit();
        return 5;
    }

    std::printf( "SDL revision: %s\n", SDL_GetRevision() );
    SDL_DestroyGPUDevice( device );
    SDL_DestroyWindow( window );
    SDL_Quit();
    smoke_finished.store( true );
    return 0;
}
