#pragma once
// MFC-free replacement for Sources/src/editor/TreeItem.h's CTreeItem base.
// What the port keeps:
//   * nItemType - the ETreeItemType the factory routes on.
//   * szDefaultName / szDisplayName - the authored and shown names.
//   * values - the SProp list the ObjectInspector drives.
//   * children - a vector of owning pointers (unique_ptr, not CPtr/IRefCount).
//   * defaultChilds - the list the MFC CreateDefaultChilds walked.
// What the port drops: SECTreeCtrl, HTREEITEM, pTreeCtrl, pItemParent, the
// DECLARE_SERIALIZE binary serialiser (replaced by project-XML round-trip),
// IsCompatibleWith / CopyItemTo / InsertChildItems - every one of these is a
// UI concern the Qt port will reintroduce at its own layer. The ETreeItemType
// values live in Sources/src/editor/TreeItem.h for now and will move to the
// port in T02 together with the first wave of registered classes.

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "prop.h"

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

	void AddChild( std::unique_ptr<CTreeItem> p ) { treeItemList.push_back( std::move( p ) ); }
	const CTreeItemList &GetChildren() const { return treeItemList; }
	CTreeItemList &MutableChildren() { return treeItemList; }

	const CPropVector &GetValues() const { return values; }
	CPropVector &MutableValues() { return values; }

	// The sub-editor overrides fill defaultValues / defaultChilds; the base
	// hands them back as a template for serialisation (unknown-prop fallback).
	const CChildItemsList &GetDefaultChilds() const { return defaultChilds; }
	const CPropVector &GetDefaultValues() const { return defaultValues; }

protected:
	int nItemType = 0;                        // ETreeItemType the factory keys on
	std::string szDefaultName;
	std::string szDisplayName;
	CChildItemsList defaultChilds;
	CPropVector defaultValues;
	CPropVector values;
	CTreeItemList treeItemList;

	virtual void InitDefaultValues() {}
};

}
