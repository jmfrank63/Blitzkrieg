#include "tree_item.h"

// Prop-walking parse/serialise. MFC CTreeItem::operator&( IDataTree &ss )
// added each named prop under a <values> list and then recursed into
// treeItemList; the port's project-XML schema puts each prop under its own
// element named after SProp::szDefaultName. For the scaffold round-trip the
// Project loader keeps the raw NResourceXml::Node on the typed root so Save()
// can emit it byte-for-byte - these walks exist so a Qt-side authoring UI can
// edit the typed tree and still produce consistent XML.

namespace NResourceModel
{

void CTreeItem::parse( const NResourceXml::Node &node )
{
	// Fill `values` in declaration order from the stored defaultValues so a
	// subclass that overrode InitDefaultValues() carries the right type for
	// every slot even when the XML omits one.
	values = defaultValues;
	for ( SProp &prop : values )
	{
		const NResourceXml::Node *child = NResourceXml::FindChild( node, prop.szDefaultName );
		if ( !child )
			continue;
		prop.value = CVariant::FromString( prop.value.GetKind(), child->text );
	}
}

void CTreeItem::serialise( NResourceXml::Node &node ) const
{
	// Walk values in-order so the on-disk attribute order matches MFC
	// byte-for-byte. The caller is responsible for setting node.name /
	// node.kind; the base only emits the SProp children.
	for ( const SProp &prop : values )
	{
		NResourceXml::Node child;
		child.kind = NResourceXml::Node::Element;
		child.name = prop.szDefaultName;
		child.text = prop.value.ToString();
		node.children.push_back( std::move( child ) );
	}
}

}
