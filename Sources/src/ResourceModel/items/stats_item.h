#pragma once
// Shared base + helpers for the 11 stats-only sub-editors (Weapon, Mine, Trench,
// Squad, Sprite, Infantry, Mesh, Object, Fence, Building, Bridge). Each
// sub-editor ports its CTreeItem subclasses as thin shells: an nItemType value,
// an inherited serialise/parse walking SProp values, and (for the root item)
// an XML tag name the Project loader keys on. The prop vectors themselves
// (CWeaponCommonPropsItem has 9 of them, CWeaponDamagePropsItem 16, etc.) migrate
// in a later task - the current fixtures are minimal root+<fixture><name>minimal
// </name></fixture> files, so an empty values vector is correct and still
// rt-stable: the fixture subtree is handed to FutureBlob verbatim.
//
// Each per-kind header declares its class list; factory.cpp calls
// REGISTER_CLASS line-for-line to mirror Sources/src/editor/TreeItemFactory.cpp
// so an audit against the MFC file is a one-pass diff.

#include <string>

#include "../tree_item.h"

namespace NResourceModel
{

// Project loader map: XML root tag -> ETreeItemType the factory keys on.
// Populated by RegisterRootTag at factory bootstrap time (called inline from
// factory.cpp alongside REGISTER_CLASS). Used by project.cpp Load() to turn
// the parsed Document into a typed root instead of always wrapping it in a
// FutureBlob.
int LookupRootTag( const std::string &tag );
void RegisterRootTag( const std::string &tag, int nType );

// A thin CTreeItem subclass that records its ETreeItemType at construction
// so a factory-created instance is self-identifying before any Load() runs.
// Every stats-only item class derives from this; the roots additionally
// capture their XML tag so Save() can emit it even on an empty project.
class CStatsItem : public CTreeItem
{
public:
	explicit CStatsItem( int nType ) { nItemType = nType; }
	CStatsItem( int nType, std::string xmlTag )
	{
		nItemType = nType;
		m_xmlTag = std::move( xmlTag );
	}

	// Non-empty on the root items (Weapon_Composer_Project, Mine_Composer_Project,
	// etc.) so Save() knows what to emit. Non-root items leave this empty.
	const std::string &GetXmlTag() const { return m_xmlTag; }

private:
	std::string m_xmlTag;
};

}
