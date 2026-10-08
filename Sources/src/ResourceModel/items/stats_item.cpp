#include "stats_item.h"

#include <unordered_map>

namespace NResourceModel
{

namespace
{

// Meyers singleton. Same construction-order rules as the factory itself: this
// is a tag->type lookup Project::Load uses to pick the typed root class.
std::unordered_map<std::string, int> &TagMap()
{
	static std::unordered_map<std::string, int> m;
	return m;
}

}

int LookupRootTag( const std::string &tag )
{
	auto &m = TagMap();
	auto it = m.find( tag );
	return it == m.end() ? 0 : it->second;
}

void RegisterRootTag( const std::string &tag, int nType )
{
	TagMap()[tag] = nType;
}

}
