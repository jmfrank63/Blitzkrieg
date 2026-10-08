#pragma once
// MFC-free replacement for Sources/src/editor/TreeItem.h's CTreeItem base.
// What the port keeps:
//   * nItemType - the ETreeItemType the factory routes on.
//   * szDefaultName / szDisplayName - the authored and shown names.
//   * values - the SProp list the ObjectInspector drives.
//   * children - a vector of owning pointers (unique_ptr, not CPtr/IRefCount).
//   * defaultValues / defaultChilds - what InitDefaultValues sets up, and
//     CreateDefaultChilds, which reconciles a tree with them as MFC does.
//   * nNeedExpand, bSerializeChilds, bStaticElements - the flags that change
//     what is written or what CreateDefaultChilds does.
// What the port drops: SECTreeCtrl, HTREEITEM, pTreeCtrl, pItemParent, the
// DECLARE_SERIALIZE binary serialiser, IsCompatibleWith / CopyItemTo /
// InsertChildItems and the key and mouse handlers - UI concerns the editor
// reintroduces at its own layer.
//
// Serialisation is CTreeItem::operator&( IDataTree & ) ported to
// NResourceXml::Node: ReadData/WriteData are the reading and writing halves,
// and an item class with its own operator& overrides both, calling the base
// first as MFC's AddTypedSuper does. The XML shape is CDataTreeXML's: a
// string is an element holding text, a number an attribute, a container an
// element with one <item> per entry, an item in a container carries its
// ClassTypeID attribute.
//
// What MFC does not do, and the port adds so a load and save of an unedited
// project changes nothing: an item read from a file remembers the layout it
// was read with (attribute and element order, and every element its own
// operator& does not know, kept whole). Writing it again follows that layout:
// known fields are written fresh in their stored place and unknown ones are
// copied back. A field the item writes but the file did not have (an older
// writer's file) is added at the end only once its content differs from what
// it was right after the read, so an unedited file keeps its shape and an
// edit is never dropped. A new item has no stored layout and gets MFC's full
// shape.

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "prop.h"
#include "xml.h"

namespace NResourceModel
{

class CTreeItem
{
public:
	struct SChildItem
	{
		int nChildItemType = 0;
		std::string szDefaultName;
		std::string szDisplayName;
	};
	using CChildItemsList = std::list<SChildItem>;
	using CTreeItemList = std::vector<std::unique_ptr<CTreeItem>>;

	CTreeItem() = default;
	virtual ~CTreeItem() = default;

	// Non-copyable (owning children by unique_ptr); subeditor code moves them.
	CTreeItem( const CTreeItem & ) = delete;
	CTreeItem &operator=( const CTreeItem & ) = delete;
	CTreeItem( CTreeItem && ) = default;
	CTreeItem &operator=( CTreeItem && ) = default;

	int GetItemType() const { return nItemType; }
	const std::string &GetDefaultName() const { return szDefaultName; }
	const std::string &GetDisplayName() const { return szDisplayName; }
	void SetDisplayName( std::string s ) { szDisplayName = std::move( s ); }
	void SetDefaultName( std::string s ) { szDefaultName = std::move( s ); }
	// MFC's SetItemName, which the insert handlers call: both names.
	void SetItemName( const std::string &s ) { szDefaultName = s; szDisplayName = s; }

	bool GetExpand() const { return nNeedExpand != 0; }
	void SetExpand( bool bExpand ) { nNeedExpand = bExpand ? 1 : 0; }

	// MFC's AddChild: the child joins the list and gets its default values and
	// default children (CreateDefaultChilds), as an inserted item does in the
	// editor. AppendChild only appends; the loader uses it, because a read
	// item already carries what the file says.
	void AddChild( std::unique_ptr<CTreeItem> p );
	void AppendChild( std::unique_ptr<CTreeItem> p ) { treeItemList.push_back( std::move( p ) ); }
	const CTreeItemList &GetChildren() const { return treeItemList; }
	CTreeItemList &MutableChildren() { return treeItemList; }

	const CPropVector &GetValues() const { return values; }
	CPropVector &MutableValues() { return values; }

	const CChildItemsList &GetDefaultChilds() const { return defaultChilds; }
	const CPropVector &GetDefaultValues() const { return defaultValues; }

	// MFC's CreateDefaultChilds (TreeItem.cpp), run by the editor after a load
	// and on every inserted item: values not in defaultValues are dropped, the
	// rest sorted into the default order, missing ones added, and each takes
	// the default's id, label, type, widget and strings. An item with
	// bStaticElements gets the same treatment for its children against
	// defaultChilds. Recurses into the children.
	void CreateDefaultChilds();

	// An item as a container entry: the ClassTypeID attribute the container
	// writes (DTHelper.h, CPtrBase), then the item's operator&. The caller
	// names the element ("item").
	void serialise( NResourceXml::Node &node ) const;
	// The reading counterpart; the caller has already made the item from the
	// ClassTypeID. Remembers the node's layout for the next write.
	void parse( const NResourceXml::Node &node );
	// The same without ClassTypeID: a project's root item, which the frame
	// writes straight into the document element.
	void SerialiseRoot( NResourceXml::Node &node ) const;

protected:
	int nItemType = 0;                        // ETreeItemType the factory keys on
	std::string szDefaultName;
	std::string szDisplayName;
	int nNeedExpand = 0;                      // the "expand" attribute: tree node open
	bool bSerializeChilds = true;             // false: operator& does not write childs
	bool bStaticElements = false;             // children are exactly defaultChilds
	CChildItemsList defaultChilds;
	CPropVector defaultValues;
	CPropVector values;
	CTreeItemList treeItemList;

	virtual void InitDefaultValues() {}
	// True when MFC reads this item's bools through CVariant::operator bool before
	// a save, so they are written with flag 9 (EncodeMfcValue).
	virtual bool BoolsReadAsInt() const { return false; }

	// CTreeItem::operator&( IDataTree & ), reading and writing.
	virtual void ReadData( const NResourceXml::Node &node );
	virtual void WriteData( NResourceXml::Node &node ) const;
	// True for an attribute or element name this item's operator& reads and
	// writes; everything else in a read node is kept as it was.
	virtual bool OwnsField( const std::string &name ) const;

private:
	void MergeLayout( NResourceXml::Node &fresh, NResourceXml::Node &out ) const;

	NResourceXml::Node m_layout;              // the node as read, own fields emptied
	NResourceXml::Node m_absent;              // own fields the node lacked, as written right after the read
	bool m_hasLayout = false;
};

}
