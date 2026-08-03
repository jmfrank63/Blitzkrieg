#ifndef BLITZKRIEG_PLATFORM_DYNAMIC_LIBRARY_H
#define BLITZKRIEG_PLATFORM_DYNAMIC_LIBRARY_H

#include <string>

namespace NPlatform
{
class DynamicLibrary
{
	void *handle;
	std::string path;
	std::string error;
public:
	DynamicLibrary();
	explicit DynamicLibrary( const char *utf8Path );
	~DynamicLibrary();
	DynamicLibrary( const DynamicLibrary & ) = delete;
	DynamicLibrary &operator=( const DynamicLibrary & ) = delete;
	DynamicLibrary( DynamicLibrary &&other ) noexcept;
	DynamicLibrary &operator=( DynamicLibrary &&other ) noexcept;
	bool Load( const char *utf8Path );
	void Unload();
	bool IsLoaded() const;
	void *GetFunction( const char *name );
	const char *GetError() const;
	const std::string &GetPath() const;
};
}

#endif
