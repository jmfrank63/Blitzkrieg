#include "DynamicLibrary.h"

#include <SDL3/SDL.h>

namespace NPlatform
{
DynamicLibrary::DynamicLibrary() : handle( nullptr ) {}
DynamicLibrary::DynamicLibrary( const char *utf8Path ) : handle( nullptr ) { Load( utf8Path ); }
DynamicLibrary::~DynamicLibrary() { Unload(); }

DynamicLibrary::DynamicLibrary( DynamicLibrary &&other ) noexcept
	: handle( other.handle ), path( std::move( other.path) ), error( std::move( other.error ) )
{
	other.handle = nullptr;
}

DynamicLibrary &DynamicLibrary::operator=( DynamicLibrary &&other ) noexcept
{
	if ( this == &other ) return *this;
	Unload();
	handle = other.handle;
	path = std::move( other.path );
	error = std::move( other.error );
	other.handle = nullptr;
	return *this;
}

bool DynamicLibrary::Load( const char *utf8Path )
{
	Unload();
	path = utf8Path != nullptr ? utf8Path : "";
	if ( utf8Path == nullptr )
	{
		error = "dynamic library path is null";
		return false;
	}
	handle = reinterpret_cast<void *>( SDL_LoadObject( utf8Path ) );
	if ( handle == nullptr )
	{
		error = SDL_GetError();
		if ( error.empty() ) error = "SDL_LoadObject failed";
	}
	else error.clear();
	return handle != nullptr;
}

void DynamicLibrary::Unload()
{
	if ( handle == nullptr ) return;
	SDL_UnloadObject( reinterpret_cast<SDL_SharedObject *>( handle ) );
	handle = nullptr;
}

bool DynamicLibrary::IsLoaded() const { return handle != nullptr; }

void *DynamicLibrary::GetFunction( const char *name )
{
	if ( handle == nullptr ) { error = "dynamic library is not loaded"; return nullptr; }
	if ( name == nullptr ) { error = "dynamic library symbol name is null"; return nullptr; }
	void *function = reinterpret_cast<void *>( SDL_LoadFunction( reinterpret_cast<SDL_SharedObject *>( handle ), name ) );
	if ( function == nullptr )
	{
		error = SDL_GetError();
		if ( error.empty() ) error = "SDL_LoadFunction failed";
	}
	else error.clear();
	return function;
}

const char *DynamicLibrary::GetError() const { return error.c_str(); }
const std::string &DynamicLibrary::GetPath() const { return path; }
}
