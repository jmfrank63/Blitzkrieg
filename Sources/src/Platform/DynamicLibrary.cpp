#include "DynamicLibrary.h"
#include "../PlatformABI/PlatformClient.h"

namespace {
void updateRuntimeError(std::string &error, const char *fallback) {
    char message[512] = {};
    uint32_t required = 0;
    if (BkPlatform::Client::LastError(message, sizeof(message), &required) == BK_PLATFORM_OK) error = message;
    else error = fallback;
}
BkPlatformUtf8Span span(const char *value) {
    return BkPlatformUtf8Span{sizeof(BkPlatformUtf8Span), value, value == nullptr ? 0u : static_cast<uint32_t>(std::char_traits<char>::length(value))};
}
}

namespace NPlatform
{
DynamicLibrary::DynamicLibrary() : handle( 0 ) {}
DynamicLibrary::DynamicLibrary( const char *utf8Path ) : handle( 0 ) { Load( utf8Path ); }
DynamicLibrary::~DynamicLibrary() { Unload(); }

DynamicLibrary::DynamicLibrary( DynamicLibrary &&other ) noexcept
	: handle( other.handle ), path( std::move( other.path) ), error( std::move( other.error ) )
{
	other.handle = 0;
}

DynamicLibrary &DynamicLibrary::operator=( DynamicLibrary &&other ) noexcept
{
	if ( this == &other ) return *this;
	Unload();
	handle = other.handle;
	path = std::move( other.path );
	error = std::move( other.error );
	other.handle = 0;
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
	if ( !BkPlatform::Client::IsAttached() && !BkPlatform::Client::Attach() )
	{
		error = "platform runtime is not attached";
		return false;
	}
	const BkPlatformResult result = BkPlatform::Client::LibraryOpen( span( utf8Path ), &handle );
	if ( result != BK_PLATFORM_OK )
	{
		handle = 0;
		updateRuntimeError( error, "dynamic library load failed" );
		return false;
	}
	error.clear();
	return true;
}

void DynamicLibrary::Unload()
{
	if ( handle == 0 ) return;
	if ( BkPlatform::Client::IsAttached() ) BkPlatform::Client::LibraryClose( handle );
	handle = 0;
}

bool DynamicLibrary::IsLoaded() const { return handle != 0; }

void *DynamicLibrary::GetFunction( const char *name )
{
	if ( handle == 0 ) { error = "dynamic library is not loaded"; return nullptr; }
	if ( name == nullptr ) { error = "dynamic library symbol name is null"; return nullptr; }
	void *function = nullptr;
	if ( !BkPlatform::Client::IsAttached() || BkPlatform::Client::LibrarySymbol( handle, span( name ), &function ) != BK_PLATFORM_OK )
	{
		updateRuntimeError( error, "dynamic library symbol lookup failed" );
		return nullptr;
	}
	error.clear();
	return function;
}

const char *DynamicLibrary::GetError() const { return error.c_str(); }
const std::string &DynamicLibrary::GetPath() const { return path; }
}
