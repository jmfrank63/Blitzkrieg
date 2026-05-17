#ifndef __BUILDVERSION_H__
#define __BUILDVERSION_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SProject
{
	std::string szName;										// project name
	std::string szPathName;								// project path
	std::string szFileName;								// complete filename
	std::string szSourceControl;					// source control entry
	std::string szResourceFileName;				// resource file
	std::list<std::string> depends;				// all projects, this one depend from
	std::list<std::string> sources;				// source files of this project
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuildVersion
{
	typedef std::unordered_map<std::string, SProject> CProjectsMap;
	CProjectsMap mapProjects;							// all found projects
	//
	bool LoadDSW( const char *pszFileName, bool bGetLatestVersion = false );
	bool LoadDSP( SProject *pProject );
	bool CheckProjects( const std::list<SProject*> &projects, const SProject *pHeadProject, std::string *pVersion ) const;
	//
	bool UpdateVersion( const SProject *pProject, const std::string &szVersion ) const;
	bool LoadProjects( const char *pszWorkspaceName, const char *pszHeadProject, std::list<SProject*> &projects );
	//
public:
	CBuildVersion( const char *pszConfigFileName );
	//
	bool UpdateVersion( const char *pszWorkspaceName, const char *pszHeadProject );
	bool MakeBuild( const char *pszHeadProject );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BUILDVERSION_H__
