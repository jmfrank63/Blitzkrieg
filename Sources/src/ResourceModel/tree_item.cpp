#include "tree_item.h"

#include <algorithm>
#include <cstdlib>

#include "factory.h"
#include "future_blob.h"
#include "mfc_value.h"

// CTreeItem::operator&( IDataTree & ) (Sources/src/editor/TreeItem.cpp):
//
//	saver.Add( "default_name", &szDefaultName );
//	saver.Add( "display_name", &szDisplayName );
//	saver.Add( "values", &values );            // SProp: default_name, value
//	saver.Add( "expand", &nNeedExpand );
//	if ( bSerializeChilds )
//		saver.Add( "childs", &treeItemList );  // CPtr<CTreeItem>: ClassTypeID + operator&
//
// CDataTreeXML puts strings and containers in elements and ints in attributes,
// so expand lands next to ClassTypeID on the item element.

namespace NResourceModel
{

namespace
{

NResourceXml::Node Element( const std::string &name )
{
	NResourceXml::Node n;
	n.kind = NResourceXml::Node::Element;
	n.name = name;
	return n;
}

bool IsLayoutWhitespace( const NResourceXml::Node &n )
{
	return n.kind == NResourceXml::Node::Text && n.text.find_first_not_of( " \t\r\n" ) == std::string::npos;
}

bool SameNode( const NResourceXml::Node &a, const NResourceXml::Node &b )
{
	if ( a.kind != b.kind || a.name != b.name || a.text != b.text || a.attrs != b.attrs || a.children.size() != b.children.size() )
		return false;
	for ( size_t i = 0; i < a.children.size(); ++i )
		if ( !SameNode( a.children[i], b.children[i] ) )
			return false;
	return true;
}

// The item type as CPtrBase reads it: ClassTypeID, else the older type.
bool ReadItemType( const NResourceXml::Node &n, int &type )
{
	const std::string *v = FindAttr( n, "ClassTypeID" );
	if ( !v )
		v = FindAttr( n, "type" );
	if ( !v )
		return false;
	type = (int)std::strtol( v->c_str(), nullptr, 0 );
	return true;
}

}

void CTreeItem::AddChild( std::unique_ptr<CTreeItem> p )
{
	CTreeItem *pItem = p.get();
	treeItemList.push_back( std::move( p ) );
	pItem->CreateDefaultChilds();
}

void CTreeItem::CreateDefaultChilds()
{
	if ( values.empty() )
		values = defaultValues;
	else
	{
		values.erase( std::remove_if( values.begin(), values.end(), [this]( const SProp &v ) {
			return std::none_of( defaultValues.begin(), defaultValues.end(), [&v]( const SProp &d ) { return d.szDefaultName == v.szDefaultName; } );
		} ), values.end() );
		auto order = [this]( const SProp &v ) {
			return std::find_if( defaultValues.begin(), defaultValues.end(), [&v]( const SProp &d ) { return d.szDefaultName == v.szDefaultName; } ) - defaultValues.begin();
		};
		std::stable_sort( values.begin(), values.end(), [&order]( const SProp &a, const SProp &b ) { return order( a ) < order( b ); } );
		size_t i = 0;
		for ( const SProp &def : defaultValues )
		{
			if ( i == values.size() || values[i].szDefaultName != def.szDefaultName )
				values.insert( values.begin() + i, def );
			else
			{
				SProp &v = values[i];
				v.nId = def.nId;
				v.szDisplayName = def.szDisplayName;
				v.value.SetType( def.value.GetKind() );
				v.nDomenType = def.nDomenType;
				v.szStrings = def.szStrings;
			}
			++i;
		}
	}

	if ( bStaticElements )
	{
		auto &factory = CTreeItemFactory::Instance();
		auto make = [&factory]( const SChildItem &def ) {
			std::unique_ptr<CTreeItem> p = factory.Create( def.nChildItemType );
			if ( p )
			{
				p->szDefaultName = def.szDefaultName;
				p->szDisplayName = def.szDisplayName;
			}
			return p;
		};
		if ( treeItemList.empty() )
		{
			for ( const SChildItem &def : defaultChilds )
				if ( auto p = make( def ) )
					treeItemList.push_back( std::move( p ) );
		}
		else
		{
			auto match = []( const CTreeItem &item, const SChildItem &def ) {
				return item.szDefaultName == def.szDefaultName && item.nItemType == def.nChildItemType;
			};
			treeItemList.erase( std::remove_if( treeItemList.begin(), treeItemList.end(), [&]( const std::unique_ptr<CTreeItem> &p ) {
				return std::none_of( defaultChilds.begin(), defaultChilds.end(), [&]( const SChildItem &d ) { return match( *p, d ); } );
			} ), treeItemList.end() );
			auto order = [&]( const CTreeItem &item ) {
				return std::distance( defaultChilds.begin(), std::find_if( defaultChilds.begin(), defaultChilds.end(), [&]( const SChildItem &d ) { return match( item, d ); } ) );
			};
			std::stable_sort( treeItemList.begin(), treeItemList.end(), [&]( const std::unique_ptr<CTreeItem> &a, const std::unique_ptr<CTreeItem> &b ) { return order( *a ) < order( *b ); } );
			size_t i = 0;
			for ( const SChildItem &def : defaultChilds )
			{
				if ( i == treeItemList.size() || !match( *treeItemList[i], def ) )
					if ( auto p = make( def ) )
						treeItemList.insert( treeItemList.begin() + i, std::move( p ) );
				++i;
			}
		}
	}

	for ( auto &child : treeItemList )
		child->CreateDefaultChilds();
}

bool CTreeItem::OwnsField( const std::string &name ) const
{
	return name == "default_name" || name == "display_name" || name == "values" || name == "expand" ||
		( bSerializeChilds && name == "childs" );
}

void CTreeItem::ReadData( const NResourceXml::Node &node )
{
	if ( const NResourceXml::Node *e = FindElement( node, "default_name" ) )
		szDefaultName = ElementText( *e );
	if ( const NResourceXml::Node *e = FindElement( node, "display_name" ) )
		szDisplayName = ElementText( *e );

	// The values container replaces the list: MFC keeps what the file holds
	// and leaves the reconciliation with defaultValues to CreateDefaultChilds.
	// The widget metadata (id, label, DT_*, strings) is not in the file; it is
	// taken from the default of the same name, if any.
	if ( const NResourceXml::Node *list = FindElement( node, "values" ) )
	{
		values.clear();
		for ( const auto &entry : list->children )
		{
			if ( entry.kind != NResourceXml::Node::Element || entry.name != "item" )
				continue;
			SProp prop;
			if ( const NResourceXml::Node *e = FindElement( entry, "default_name" ) )
				prop.szDefaultName = ElementText( *e );
			for ( const SProp &def : defaultValues )
				if ( def.szDefaultName == prop.szDefaultName )
				{
					prop.nId = def.nId;
					prop.szDisplayName = def.szDisplayName;
					prop.nDomenType = def.nDomenType;
					prop.szStrings = def.szStrings;
					break;
				}
			if ( const NResourceXml::Node *v = FindElement( entry, "value" ) )
			{
				prop.value = DecodeMfcValue( *v );
				prop.mfcValue = *v;
				prop.bHasMfcValue = true;
			}
			values.push_back( std::move( prop ) );
		}
	}

	if ( const std::string *v = FindAttr( node, "expand" ) )
		nNeedExpand = (int)std::strtol( v->c_str(), nullptr, 0 );

	if ( !bSerializeChilds )
		return;
	if ( const NResourceXml::Node *list = FindElement( node, "childs" ) )
	{
		treeItemList.clear();
		auto &factory = CTreeItemFactory::Instance();
		for ( const auto &entry : list->children )
		{
			if ( IsLayoutWhitespace( entry ) )
				continue;
			// An entry the factory cannot make (an unknown ClassTypeID, or
			// not an <item> at all) is kept whole and written back as it was.
			int type = 0;
			std::unique_ptr<CTreeItem> child;
			if ( entry.kind == NResourceXml::Node::Element && entry.name == "item" && ReadItemType( entry, type ) )
				child = factory.Create( type );
			if ( !child )
			{
				treeItemList.push_back( std::make_unique<FutureBlob>( entry ) );
				continue;
			}
			child->parse( entry );
			treeItemList.push_back( std::move( child ) );
		}
	}
}

void CTreeItem::WriteData( NResourceXml::Node &node ) const
{
	node.children.push_back( StringElement( "default_name", szDefaultName ) );
	node.children.push_back( StringElement( "display_name", szDisplayName ) );

	NResourceXml::Node list = Element( "values" );
	for ( const SProp &prop : values )
	{
		NResourceXml::Node entry = Element( "item" );
		entry.children.push_back( StringElement( "default_name", prop.szDefaultName ) );
		NResourceXml::Node value;
		EncodeMfcValue( prop.value, prop.bHasMfcValue ? &prop.mfcValue : nullptr, value, BoolsReadAsInt() );
		entry.children.push_back( std::move( value ) );
		list.children.push_back( std::move( entry ) );
	}
	node.children.push_back( std::move( list ) );

	SetAttr( node, "expand", MfcInt( nNeedExpand ) );

	if ( !bSerializeChilds )
		return;
	NResourceXml::Node childs = Element( "childs" );
	for ( const auto &child : treeItemList )
	{
		if ( FutureBlob::IsFutureBlob( *child ) )
		{
			childs.children.push_back( static_cast<const FutureBlob &>( *child ).GetNode() );
			continue;
		}
		NResourceXml::Node entry = Element( "item" );
		child->serialise( entry );
		childs.children.push_back( std::move( entry ) );
	}
	node.children.push_back( std::move( childs ) );
}

void CTreeItem::parse( const NResourceXml::Node &node )
{
	ReadData( node );

	m_layout = Element( node.name );
	m_layout.kind = node.kind;
	m_layout.attrs = node.attrs;
	for ( const auto &c : node.children )
	{
		if ( IsLayoutWhitespace( c ) )
			continue;
		if ( c.kind == NResourceXml::Node::Element && OwnsField( c.name ) )
			m_layout.children.push_back( Element( c.name ) );
		else
			m_layout.children.push_back( c );
	}
	m_hasLayout = true;

	// What the item would write for the fields the node lacked, to tell an
	// edit from the defaults at the next write.
	m_absent = Element( node.name );
	NResourceXml::Node fresh = Element( node.name );
	WriteData( fresh );
	for ( const auto &a : fresh.attrs )
		if ( !FindAttr( node, a.first ) )
			m_absent.attrs.push_back( a );
	for ( auto &c : fresh.children )
		if ( c.kind == NResourceXml::Node::Element && !FindElement( node, c.name ) )
			m_absent.children.push_back( std::move( c ) );
}

void CTreeItem::MergeLayout( NResourceXml::Node &fresh, NResourceXml::Node &out ) const
{
	if ( !m_hasLayout )
	{
		out.attrs = std::move( fresh.attrs );
		out.children = std::move( fresh.children );
		return;
	}

	// Attributes in stored order. A file from the older writer names the
	// class "type"; it keeps that name.
	const bool bLegacyType = FindAttr( m_layout, "type" ) && !FindAttr( m_layout, "ClassTypeID" );
	out.attrs.clear();
	for ( const auto &a : m_layout.attrs )
	{
		const std::string key = ( bLegacyType && a.first == "type" ) ? "ClassTypeID" : a.first;
		const std::string *v = FindAttr( fresh, key );
		out.attrs.emplace_back( a.first, v ? *v : a.second );
	}
	for ( const auto &a : fresh.attrs )
	{
		if ( FindAttr( m_layout, a.first ) || ( bLegacyType && a.first == "ClassTypeID" ) )
			continue;
		const std::string *was = FindAttr( m_absent, a.first );
		if ( !was || *was != a.second )
			out.attrs.push_back( a );
	}

	// Elements in stored order: each known one written fresh, each unknown
	// one copied back.
	std::vector<bool> used( fresh.children.size(), false );
	out.children.clear();
	for ( const auto &c : m_layout.children )
	{
		if ( c.kind == NResourceXml::Node::Element && OwnsField( c.name ) )
		{
			for ( size_t i = 0; i < fresh.children.size(); ++i )
				if ( !used[i] && fresh.children[i].kind == NResourceXml::Node::Element && fresh.children[i].name == c.name )
				{
					used[i] = true;
					out.children.push_back( std::move( fresh.children[i] ) );
					break;
				}
			continue;
		}
		out.children.push_back( c );
	}
	for ( size_t i = 0; i < fresh.children.size(); ++i )
	{
		if ( used[i] || fresh.children[i].kind != NResourceXml::Node::Element )
			continue;
		const NResourceXml::Node *was = FindElement( m_absent, fresh.children[i].name );
		if ( !was || !SameNode( *was, fresh.children[i] ) )
			out.children.push_back( std::move( fresh.children[i] ) );
	}
}

void CTreeItem::serialise( NResourceXml::Node &node ) const
{
	NResourceXml::Node fresh = Element( node.name );
	SetAttr( fresh, "ClassTypeID", MfcInt( nItemType ) );
	WriteData( fresh );
	MergeLayout( fresh, node );
}

void CTreeItem::SerialiseRoot( NResourceXml::Node &node ) const
{
	NResourceXml::Node fresh = Element( node.name );
	WriteData( fresh );
	MergeLayout( fresh, node );
}

}
