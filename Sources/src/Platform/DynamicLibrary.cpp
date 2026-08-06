#include "DynamicLibrary.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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
#if defined(_WIN32)
	handle = reinterpret_cast<void *>( LoadLibraryA( utf8Path ) );
#else
	handle = dlopen( utf8Path, RTLD_NOW | RTLD_LOCAL );
#endif
	if ( handle == nullptr )
	{
		error = "dynamic library load failed";
#if !defined(_WIN32)
		if ( const char *detail = dlerror(); detail != nullptr ) error = detail;
#endif
	}
	else error.clear();
	return handle != nullptr;
}

void DynamicLibrary::Unload()
{
	if ( handle == nullptr ) return;
#if defined(_WIN32)
	FreeLibrary( reinterpret_cast<HMODULE>( handle ) );
#else
	dlclose( handle );
#endif
	handle = nullptr;
}

bool DynamicLibrary::IsLoaded() const { return handle != nullptr; }

void *DynamicLibrary::GetFunction( const char *name )
{
	if ( handle == nullptr ) { error = "dynamic library is not loaded"; return nullptr; }
	if ( name == nullptr ) { error = "dynamic library symbol name is null"; return nullptr; }
#if defined(_WIN32)
	void *function = reinterpret_cast<void *>( GetProcAddress( reinterpret_cast<HMODULE>( handle ), name ) );
#else
	void *function = dlsym( handle, name );
#endif
	if ( function == nullptr )
	{
		error = "dynamic library symbol lookup failed";
#if !defined(_WIN32)
		if ( const char *detail = dlerror(); detail != nullptr ) error = detail;
#endif
	}
	else error.clear();
	return function;
}

const char *DynamicLibrary::GetError() const { return error.c_str(); }
const std::string &DynamicLibrary::GetPath() const { return path; }
}
