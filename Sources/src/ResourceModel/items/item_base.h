#pragma once
// Shared helpers for the 11 stats-only sub-editor item-class ports. Each
// subclass under items/<kind>/ sets nItemType in its constructor and chains
// InitDefaultValues() the way Sources/src/editor/<Kind>TreeItem.cpp did; the
// macros below cut the boilerplate so the per-kind files read as a vertical
// register against the MFC source.
//
// RM_ITEM_CTOR fills in the single pattern every sub-editor used:
//   nItemType = <enum>; InitDefaultValues();
// A real transcription of each InitDefaultValues() body (props, default
// children, combo strings) lands in a follow-up data-fidelity task; for the
// T02 scaffold the typed root items only need to carry nItemType so the
// factory can produce them and the Project loader can hand them the stored
// XML node for byte-identical round-trip.

#include "../tree_item.h"
#include "etree_item_type.h"

#define RM_DECLARE_ITEM( Class, Type )                                                        \
	class Class : public ::NResourceModel::CTreeItem                                          \
	{                                                                                         \
	public:                                                                                   \
		Class() { nItemType = ( Type ); InitDefaultValues(); }                                \
		~Class() override = default;                                                          \
		void InitDefaultValues() override;                                                    \
	}

#define RM_DEFINE_ITEM_EMPTY( Class )                                                         \
	void Class::InitDefaultValues() {}

