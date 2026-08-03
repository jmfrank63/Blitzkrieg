#ifndef BLITZKRIEG_PLATFORM_SYSTEM_H
#define BLITZKRIEG_PLATFORM_SYSTEM_H

#include <string>
#include <vector>

namespace NPlatform
{
using UiHandler = bool (*)( const char *first, const char *second );

std::string ExecutablePath();
std::string GetEnvironment( const char *name );
bool SetEnvironment( const char *name, const char *value );
bool ShowError( const char *title, const char *text );
bool OpenUrl( const char *url );
bool OpenFile( const char *path );
void SetUiHandlers( UiHandler errorHandler, UiHandler openHandler );
bool RunProcess( const std::vector<std::string> &arguments, const std::string &workingDirectory, int *exitCode );
}

#endif
