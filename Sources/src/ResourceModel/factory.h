#pragma once
// Replacement for Sources/src/editor/TreeItemFactory.{h,cpp}, which used
// CBasicObjectFactory (MFC CMap-backed) and the REGISTER_CLASS macro. The port
// uses an unordered_map<int, Ctor> where Ctor is a std::function; sub-editor
// translation units register themselves at construction time with Register().
//
// This task registers nothing; T02 is where E_WEAPON_ROOT_ITEM, E_MINE_* etc.
// come in. The scaffold test only exercises an empty factory and the Create()
// fallthrough path, which returns nullptr for an unknown type.

#include <functional>
#include <memory>
#include <unordered_map>

#include "tree_item.h"

namespace NResourceModel
{

class CTreeItemFactory
{
public:
	using Ctor = std::function<std::unique_ptr<CTreeItem>()>;

	void Register( int nType, Ctor ctor ) { m_ctors[nType] = std::move( ctor ); }

	// Returns nullptr when nType is not registered; the project loader then
	// wraps the originating XML element in a FutureBlob so a round-trip
	// preserves it verbatim.
	std::unique_ptr<CTreeItem> Create( int nType ) const
	{
		auto it = m_ctors.find( nType );
		if ( it == m_ctors.end() )
			return nullptr;
		return it->second();
	}

	bool IsRegistered( int nType ) const { return m_ctors.find( nType ) != m_ctors.end(); }
	std::size_t Size() const { return m_ctors.size(); }

	// The singleton (file-scope in factory.cpp) every sub-editor registers into.
	static CTreeItemFactory &Instance();

private:
	std::unordered_map<int, Ctor> m_ctors;
};

}
