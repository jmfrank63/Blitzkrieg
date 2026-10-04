#pragma once
// FutureBlob: the opaque wrapper for an XML element whose tag the factory does
// not know. The project loader hands the raw NResourceXml::Node to a FutureBlob
// instead of dropping it; the writer serialises the stored node verbatim so a
// round-trip preserves tags, attributes, child-order and even comment nodes
// that a future version of the editor adds.
//
// FutureBlob is a CTreeItem subclass so the tree walker can hold it next to
// genuinely-typed children (nItemType = 0 / E_UNKNOWN_ITEM in the MFC enum).
// Values and children are empty: everything the blob needs lives in the stored
// Node. Writing the project back out branches on IsFutureBlob() and emits the
// node's own bytes rather than reconstructing them from CPropVector.

#include "tree_item.h"
#include "xml.h"

namespace NResourceModel
{

class FutureBlob : public CTreeItem
{
public:
	explicit FutureBlob( NResourceXml::Node node ) : m_node( std::move( node ) )
	{
		nItemType = 0; // E_UNKNOWN_ITEM; see domen_id.h / TreeItem.h ETreeItemType.
		szDefaultName = m_node.name;
	}

	const NResourceXml::Node &GetNode() const { return m_node; }
	NResourceXml::Node &MutableNode() { return m_node; }

	static bool IsFutureBlob( const CTreeItem &item ) { return item.GetItemType() == 0; }

private:
	NResourceXml::Node m_node;
};

}
