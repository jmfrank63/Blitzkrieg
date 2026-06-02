#ifndef LINUX
#pragma once
#endif
#ifndef _MApiVersion
#define _MApiVersion

#ifndef MNoVersionString

#define _versionString1( _version ) _versionString2(_version)
#define _versionString2( _version ) PLUGIN_EXPORT char MApiVersion[] =  #_version

#ifdef __cplusplus
extern "C" {
#endif

_versionString1(MAYA_API_VERSION);

#ifdef __cplusplus
}
#endif

#undef _versionString1
#undef _versionString2

#endif // MNoVersionString

#endif /* _MApiVersion */
