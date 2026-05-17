#if !defined(__A7_MOD_COLLECTOR__)
#define __A7_MOD_COLLECTOR__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMODCollector
{
public:
	struct CMODNode
	{
		std::string szMODFolder;
		
		std::string szMODName;
		std::string szMODVersion;
	};
	typedef std::unordered_map<std::string, CMODNode> TMODNodesList;
	TMODNodesList availableMODs;

	static const std::string GetKey( const std::string &rszMODName, const std::string &rszMODVersion );
	void Collect();
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // !defined(__A7_MOD_COLLECTOR__)
